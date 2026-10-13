#Requires -Version 5.1
<#
.SYNOPSIS
  Idempotent bootstrap of the GTA20 GPU test host. Runs as SYSTEM via SSM.

.DESCRIPTION
  - creates the low-privilege interactive test user and console autologon
    (SSM runs in session 0, which has no visible desktop - the game must run in
    the console session of a logged-on user),
  - grants that user access to the GTA2 registry key and game directory,
  - installs the game from the private artifacts bucket,
  - tries to install the NVIDIA driver (strategies from compat-matrix.json).

  Returns an object with RebootRequired=$true when a reboot is needed before
  tests can run. The caller maps that to SSM exit code 3010 (reboot + rerun).
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $Bucket,
    [Parameter(Mandatory)] [string] $Region,
    [Parameter(Mandatory)] [string] $MatrixPath,
    [string] $Root = 'C:\GTA20',
    [string] $TestUser = 'gta2runner'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$matrix = Get-Content -Raw -Path $MatrixPath | ConvertFrom-Json
$stateDir = Join-Path $Root 'state'
$gameDir = Join-Path $Root 'game'
$cacheDir = Join-Path $Root 'cache'
foreach ($d in @($Root, $stateDir, $gameDir, $cacheDir)) {
    New-Item -ItemType Directory -Force -Path $d | Out-Null
}

$rebootRequired = $false
$log = [System.Collections.Generic.List[string]]::new()
function Write-Step([string] $Message) {
    $line = '{0:o} {1}' -f (Get-Date), $Message
    $log.Add($line)
    Write-Host $line
}

function Import-AwsS3Module {
    foreach ($m in @('AWS.Tools.S3', 'AWSPowerShell')) {
        if (Get-Module -ListAvailable -Name $m) { Import-Module $m; return }
    }
    throw 'AWS Tools for PowerShell not found on the AMI.'
}

function Get-FileSha256([string] $Path) {
    (Get-FileHash -Algorithm SHA256 -Path $Path).Hash.ToLowerInvariant()
}

# Downloads a pinned NVIDIA installer over HTTPS and refuses it unless both the SHA-256 and the
# NVIDIA Authenticode signature match.
function Get-PinnedNvidiaInstaller([string] $Url, [string] $Sha256, [string] $Signer, [string] $OutFile) {
    if (-not $Url -or -not $Sha256) { throw 'installer url/sha256 not configured' }
    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $OutFile
    $sha = Get-FileSha256 $OutFile
    if ($sha -ne $Sha256.ToLowerInvariant()) { throw "installer SHA-256 mismatch ($sha)" }
    $sig = Get-AuthenticodeSignature -FilePath $OutFile
    if ($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Subject -notmatch [regex]::Escape($Signer)) {
        throw "installer Authenticode check failed: $($sig.Status) / $($sig.SignerCertificate.Subject)"
    }
}

function New-RandomPassword {
    $bytes = New-Object byte[] 24
    [System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($bytes)
    # Base64 plus fixed suffix satisfies the default complexity policy.
    return ([Convert]::ToBase64String($bytes) -replace '[+/=]', 'x') + 'aA1!'
}

# ---------------------------------------------------------------- watchdog sanity
if (-not (Get-ScheduledTask -TaskName 'GTA20-Watchdog' -ErrorAction SilentlyContinue)) {
    throw 'GTA20-Watchdog task is missing. Refusing to continue without the runtime guard.'
}

# ---------------------------------------------------------------- Windows Update: no surprise reboots
$au = 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate\AU'
New-Item -Force -Path $au | Out-Null
Set-ItemProperty -Path $au -Name NoAutoRebootWithLoggedOnUsers -Type DWord -Value 1
Set-ItemProperty -Path $au -Name AUOptions -Type DWord -Value 2   # notify only

# ---------------------------------------------------------------- interactive test user + autologon
$winlogon = 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon'
function Get-RegValue([string] $Path, [string] $Name) {
    try { return Get-ItemPropertyValue -Path $Path -Name $Name } catch { return $null }
}
$autologonOk = (Get-RegValue $winlogon 'AutoAdminLogon') -eq '1' -and
               (Get-RegValue $winlogon 'DefaultUserName') -eq $TestUser -and
               $null -ne (Get-RegValue $winlogon 'DefaultPassword')
$user = Get-LocalUser -Name $TestUser -ErrorAction SilentlyContinue

if (-not $user -or -not $autologonOk) {
    # The password never leaves the instance; it exists only for console autologon
    # on a host with no inbound network access.
    $plain = New-RandomPassword
    $secure = ConvertTo-SecureString $plain -AsPlainText -Force
    if (-not $user) {
        New-LocalUser -Name $TestUser -Password $secure -PasswordNeverExpires -AccountNeverExpires `
            -Description 'GTA20 graphics test (interactive console)' | Out-Null
        Add-LocalGroupMember -Group 'Users' -Member $TestUser -ErrorAction SilentlyContinue
        Write-Step "created local user $TestUser"
    } else {
        Set-LocalUser -Name $TestUser -Password $secure
        Write-Step "reset password of $TestUser for autologon"
    }
    Set-ItemProperty -Path $winlogon -Name AutoAdminLogon -Value '1'
    Set-ItemProperty -Path $winlogon -Name DefaultUserName -Value $TestUser
    Set-ItemProperty -Path $winlogon -Name DefaultDomainName -Value $env:COMPUTERNAME
    Set-ItemProperty -Path $winlogon -Name DefaultPassword -Value $plain
    Remove-Variable plain, secure
    $rebootRequired = $true
}

# No lock screen / display sleep on the console session.
$pers = 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\Personalization'
New-Item -Force -Path $pers | Out-Null
Set-ItemProperty -Path $pers -Name NoLockScreen -Type DWord -Value 1
& powercfg.exe /change monitor-timeout-ac 0 | Out-Null

# ---------------------------------------------------------------- GTA2 registry key (32-bit view) writable by the test user
$base = [Microsoft.Win32.RegistryKey]::OpenBaseKey('LocalMachine', 'Registry32')
$rootKeyPath = [string] $matrix.game.registryKey
$gtaKey = $base.CreateSubKey($rootKeyPath)
foreach ($sub in @('Screen', 'Sound', 'Debug', 'Option', 'Control', 'Network')) {
    $gtaKey.CreateSubKey($sub).Close()
}
$acl = $gtaKey.GetAccessControl()
$rule = New-Object System.Security.AccessControl.RegistryAccessRule(
    "$env:COMPUTERNAME\$TestUser", 'FullControl', 'ContainerInherit', 'None', 'Allow')
$acl.SetAccessRule($rule)
$gtaKey.SetAccessControl($acl)
$gtaKey.Close()
$base.Close()

# ---------------------------------------------------------------- game files from the private bucket
Import-AwsS3Module
$gameMarker = Join-Path $stateDir 'game.json'
$expectedGameSha = [string] $matrix.game.sha256
$installedSha = if (Test-Path $gameMarker) { (Get-Content -Raw $gameMarker | ConvertFrom-Json).sha256 } else { '' }
$gameExe = Join-Path $gameDir $matrix.game.exe

if (-not (Test-Path $gameExe) -or ($expectedGameSha -and $installedSha -ne $expectedGameSha)) {
    $zip = Join-Path $cacheDir 'gta2.zip'
    Write-Step "downloading s3://$Bucket/$($matrix.game.s3Key)"
    Read-S3Object -BucketName $Bucket -Key $matrix.game.s3Key -File $zip -Region $Region | Out-Null
    $sha = Get-FileSha256 $zip
    if ($expectedGameSha -and $sha -ne $expectedGameSha) {
        throw "Game archive SHA-256 mismatch: expected $expectedGameSha, got $sha"
    }
    Remove-Item -Recurse -Force -Path $gameDir -ErrorAction SilentlyContinue
    Expand-Archive -Path $zip -DestinationPath $gameDir -Force
    # Accept archives that wrap everything in a single top-level folder.
    if (-not (Test-Path $gameExe)) {
        $found = Get-ChildItem -Path $gameDir -Recurse -Filter $matrix.game.exe | Select-Object -First 1
        if (-not $found) { throw "$($matrix.game.exe) not found in the game archive." }
        Get-ChildItem -Path $found.DirectoryName -Force | Move-Item -Destination $gameDir -Force
    }
    Remove-Item -Force $zip
    @{ sha256 = $sha; installedAt = (Get-Date).ToString('o') } | ConvertTo-Json | Set-Content $gameMarker
    Write-Step "game installed (sha256 $sha)"
}
# gta2.exe writes player\plyslot*.dat and shows a blocking error when the folder is missing
# (the installer creates it empty; extracted archives carry no empty folders).
New-Item -ItemType Directory -Force -Path (Join-Path $gameDir 'player') | Out-Null
& icacls.exe $gameDir /grant "${env:COMPUTERNAME}\${TestUser}:(OI)(CI)M" /T /Q | Out-Null

# Unattended host: no blocking hard-error or WER dialogs. A failing game must exit (and be
# reported), not leave a modal box that also pollutes later screenshots.
Set-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\Windows' -Name ErrorMode -Value 2 -Type DWord
New-Item -Path 'HKLM:\SOFTWARE\Microsoft\Windows\Windows Error Reporting' -Force | Out-Null
Set-ItemProperty -Path 'HKLM:\SOFTWARE\Microsoft\Windows\Windows Error Reporting' -Name DontShowUI -Value 1 -Type DWord

# ---------------------------------------------------------------- DirectPlay (gta2.exe imports DPLAYX.dll)
# Disabled by default on Windows Server 2022; without it the loader stops gta2.exe with
# 0xc0000022 ("unable to start correctly") before any game code runs (seen 2026-10-13).
$dplay = Get-WindowsOptionalFeature -Online -FeatureName DirectPlay
if ($dplay.State -ne 'Enabled') {
    $r = Enable-WindowsOptionalFeature -Online -FeatureName DirectPlay -All -NoRestart
    Write-Step "DirectPlay enabled (restart needed: $($r.RestartNeeded))"
    if ($r.RestartNeeded) { $rebootRequired = $true }
}

# ---------------------------------------------------------------- NVIDIA driver
$driverMarker = Join-Path $stateDir 'driver.json'
function Test-NvidiaDriverActive {
    $gpu = Get-CimInstance Win32_VideoController | Where-Object { $_.Name -like '*NVIDIA*' }
    return [bool]($gpu | Where-Object { $_.Status -eq 'OK' -and $_.ConfigManagerErrorCode -eq 0 })
}

$driverConfig = ($matrix.driver | ConvertTo-Json -Compress -Depth 3)
$driverConfigHash = [BitConverter]::ToString(
    [System.Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes($driverConfig))).Replace('-', '')
$forceInstall = $false
if (Test-Path $driverMarker) {
    $previous = Get-Content -Raw $driverMarker | ConvertFrom-Json
    # A changed driver config (e.g. a new pinned driver) is installed once over the old one.
    if ($previous.configHash -ne $driverConfigHash) {
        Write-Step "driver config changed since the '$($previous.strategy)' install - reinstalling"
        Remove-Item -Force $driverMarker
        Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $stateDir 'wddm-switch-attempted')
        $forceInstall = $true
    }
}

if (-not (Test-Path $driverMarker)) {
    $outcome = [ordered]@{ strategy = 'none'; attempts = @(); configHash = $driverConfigHash }
    $smiExe = @("$env:SystemRoot\System32\nvidia-smi.exe", "$env:ProgramFiles\NVIDIA Corporation\NVSMI\nvidia-smi.exe") |
        Where-Object { Test-Path $_ } | Select-Object -First 1
    $activeVersion = if ($smiExe) { ((& $smiExe --query-gpu=driver_version --format=csv,noheader 2>$null) -join '').Trim() } else { '' }
    $gridVersion = if ($matrix.driver.PSObject.Properties.Name -contains 'grid') { [string] $matrix.driver.grid.version } else { '' }
    if (-not $forceInstall -and (Test-NvidiaDriverActive)) {
        $outcome.strategy = 'preinstalled'
    } elseif ($gridVersion -and $activeVersion -eq $gridVersion -and (Test-NvidiaDriverActive)) {
        # The pinned GRID driver is already the active one: record it, do not reinstall.
        $outcome.strategy = 'grid-public'
        $outcome.note = "already installed ($activeVersion)"
    } else {
        foreach ($strategy in $matrix.driver.strategies) {
            try {
                switch ($strategy) {
                    'grid-s3' {
                        $obj = Get-S3Object -BucketName 'ec2-windows-nvidia-drivers' -KeyPrefix 'latest/' -Region 'us-east-1' |
                            Where-Object { $_.Key -match 'server2022.*\.exe$|_grid_win10_win11_server2022.*\.exe$' } |
                            Sort-Object LastModified -Descending | Select-Object -First 1
                        if (-not $obj) { throw 'no Windows Server 2022 GRID driver found under latest/' }
                        $exe = Join-Path $cacheDir 'nvidia-grid.exe'
                        Read-S3Object -BucketName 'ec2-windows-nvidia-drivers' -Key $obj.Key -File $exe -Region 'us-east-1' | Out-Null
                    }
                    'grid-public' {
                        # AWS GRID driver (WDDM/Direct3D on T4), anonymous HTTPS download of the public
                        # object; pinned key + SHA-256 + NVIDIA signature.
                        $exe = Join-Path $cacheDir 'nvidia-grid.exe'
                        Get-PinnedNvidiaInstaller -Url $matrix.driver.grid.url -Sha256 $matrix.driver.grid.sha256 `
                            -Signer $matrix.driver.signer -OutFile $exe
                    }
                    'url' {
                        $exe = Join-Path $cacheDir 'nvidia-datacenter.exe'
                        Get-PinnedNvidiaInstaller -Url $matrix.driver.url -Sha256 $matrix.driver.sha256 `
                            -Signer $matrix.driver.signer -OutFile $exe
                    }
                    'none' { $outcome.strategy = 'none'; break }
                    default { throw "unknown driver strategy '$strategy'" }
                }
                if ($strategy -ne 'none') {
                    $p = Start-Process -FilePath $exe -ArgumentList '-s', '-noreboot', '-clean' -Wait -PassThru
                    if ($p.ExitCode -notin 0, 1, 3010) { throw "installer exit code $($p.ExitCode)" }
                    $outcome.strategy = $strategy
                    $rebootRequired = $true
                }
                $outcome.attempts += @{ strategy = $strategy; ok = $true }
                break
            } catch {
                $outcome.attempts += @{ strategy = $strategy; ok = $false; error = $_.Exception.Message }
                Write-Step "driver strategy '$strategy' failed: $($_.Exception.Message)"
            }
        }
    }
    $outcome.at = (Get-Date).ToString('o')
    $outcome | ConvertTo-Json -Depth 5 | Set-Content $driverMarker
    Write-Step "driver outcome: $($outcome.strategy)"
}

# ---------------------------------------------------------------- T4 must run in WDDM (TCC = compute only, no Direct3D/display)
$smi = @("$env:ProgramFiles\NVIDIA Corporation\NVSMI\nvidia-smi.exe", "$env:SystemRoot\System32\nvidia-smi.exe") |
    Where-Object { Test-Path $_ } | Select-Object -First 1
$wddmMarker = Join-Path $stateDir 'wddm-switch-attempted'
if ($smi -and -not (Test-Path $wddmMarker)) {
    $model = ((& $smi --query-gpu=driver_model.current --format=csv,noheader 2>$null) -join '').Trim()
    if ($model -eq 'TCC') {
        # nvidia-smi exits 0 even when the switch is refused ("Not Supported", e.g. data center
        # drivers), so trust only the pending model it reports afterwards.
        $answer = (& $smi -i 0 -dm 0 2>&1) -join ' '    # 0 = WDDM, applied after reboot
        $pending = ((& $smi --query-gpu=driver_model.pending --format=csv,noheader 2>$null) -join '').Trim()
        Set-Content -Path $wddmMarker -Value (@{ at = (Get-Date).ToString('o'); pending = $pending; answer = $answer } | ConvertTo-Json)
        if ($pending -eq 'WDDM') {
            Write-Step 'GPU was in TCC mode - WDDM pending, reboot required'
            $rebootRequired = $true
        } else {
            Write-Step "GPU stays in TCC: this driver refuses WDDM ($answer). Direct3D will not use the T4."
        }
    } else {
        Write-Step "GPU driver model: $model"
    }
}

# ---------------------------------------------------------------- single display on the T4
$wantSingle = ($matrix.driver.PSObject.Properties.Name -contains 'disableBasicDisplay') -and $matrix.driver.disableBasicDisplay
$modelNow = if ($smi) { ((& $smi --query-gpu=driver_model.current --format=csv,noheader 2>$null) -join '').Trim() } else { '' }
if ($wantSingle -and $modelNow -eq 'WDDM') {
    $basic = @(Get-PnpDevice -Class Display -Status OK -ErrorAction SilentlyContinue |
        Where-Object FriendlyName -eq 'Microsoft Basic Display Adapter')
    if ($basic) {
        $basic | Disable-PnpDevice -Confirm:$false
        Write-Step "disabled $($basic.Count) Basic Display Adapter(s): the T4 is the only display"
    }
}

Add-Content -Path (Join-Path $stateDir 'bootstrap.log') -Value $log
[pscustomobject]@{
    RebootRequired = $rebootRequired
    Driver         = (Get-Content -Raw $driverMarker | ConvertFrom-Json)
}
