#Requires -Version 5.1
<#
.SYNOPSIS
  GTA20 graphics test orchestrator. Runs as SYSTEM through SSM Run Command.

.DESCRIPTION
  Mode 'probe'  - bootstrap + environment report only (GPU, driver, sessions, dxdiag).
  Mode 'compat' - DirectDraw/Direct3D compatibility matrix for the original GTA2.
  Mode 'e2e'    - same runner restricted to the case(s) promoted after compat passed.
  Mode 'capture' - one scenario from scenarios\<Cases>.json: configuration, scripted key
                  input and screenshot times (reference captures of the original game).

  Exit codes: 0 = finished (verdict in summary.json), 194 = reboot requested (the caller
  reboots through the EC2 API and sends the command again), anything else = infrastructure error.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $Bucket,
    [Parameter(Mandatory)] [string] $Region,
    [Parameter(Mandatory)] [ValidatePattern('^[A-Za-z0-9-]{1,64}$')] [string] $RunId,
    [ValidateSet('probe', 'compat', 'e2e', 'capture')] [string] $Mode = 'compat',
    [ValidatePattern('^(all|[a-z0-9-]+(,[a-z0-9-]+)*)$')] [string] $Cases = 'all',
    [string] $Root = 'C:\GTA20',
    [string] $TestUser = 'gta2runner'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$scriptDir = $PSScriptRoot
$matrixPath = Join-Path $scriptDir 'compat-matrix.json'
$matrix = Get-Content -Raw -Path $matrixPath | ConvertFrom-Json
$resultsDir = Join-Path $Root "runs\$RunId\results"
New-Item -ItemType Directory -Force -Path $resultsDir | Out-Null
$gameDir = Join-Path $Root 'game'
$cacheDir = Join-Path $Root 'cache'

# ---------------------------------------------------------------- bootstrap (may request reboot)
Import-Module (Join-Path $scriptDir 'GfxAnalysis.psm1') -Force

$boot = & (Join-Path $scriptDir 'Initialize-GfxHost.ps1') -Bucket $Bucket -Region $Region `
    -MatrixPath $matrixPath -Root $Root -TestUser $TestUser | Select-Object -Last 1
if ($boot.RebootRequired) {
    # Not SSM's exit 3010: on this host the agent-initiated reboot hangs (seen 2026-10-13, twice).
    # Exit 194 tells the caller to reboot through the EC2 API and send the command again;
    # the counter stops an endless loop.
    $attempt = Register-RebootAttempt -StateDir (Join-Path $Root 'state') -RunId $RunId -MaxReboots 3
    Write-Host "REBOOT_REQUIRED: bootstrap changed drivers/autologon/WDDM - reboot $attempt/3 (exit 194)."
    exit 194
}

function Get-InteractiveSession([string] $User) {
    # explorer.exe owned by the test user == its console session is up.
    Get-CimInstance Win32_Process -Filter "Name='explorer.exe'" | ForEach-Object {
        $owner = Invoke-CimMethod -InputObject $_ -MethodName GetOwner
        if ($owner.User -eq $User) { [pscustomobject]@{ SessionId = $_.SessionId; Pid = $_.ProcessId } }
    } | Select-Object -First 1
}

function Get-NvidiaSmi {
    $smi = @("$env:ProgramFiles\NVIDIA Corporation\NVSMI\nvidia-smi.exe", "$env:SystemRoot\System32\nvidia-smi.exe") |
        Where-Object { Test-Path $_ } | Select-Object -First 1
    return $smi
}

# ---------------------------------------------------------------- environment probe
$session = $null
for ($i = 0; $i -lt 24 -and -not $session; $i++) {
    $session = Get-InteractiveSession $TestUser
    if (-not $session) { Start-Sleep -Seconds 5 }
}

$smi = Get-NvidiaSmi
$envReport = [ordered]@{
    runId       = $RunId
    mode        = $Mode
    collectedAt = (Get-Date).ToString('o')
    os          = (Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, BuildNumber)
    adapters    = @(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion, Status,
                    ConfigManagerErrorCode, CurrentHorizontalResolution, CurrentVerticalResolution,
                    CurrentBitsPerPixel, AdapterCompatibility, PNPDeviceID)
    driver      = $boot.Driver
    interactiveSession = $session
    nvidiaSmi   = $null
}
if ($smi) {
    $envReport.nvidiaSmi = (& $smi --query-gpu=name,driver_version,driver_model.current,display_mode,display_active --format=csv,noheader 2>&1) -join "`n"
}
$dxdiagOut = Join-Path $resultsDir 'dxdiag.txt'
$dx = Start-Process -FilePath "$env:SystemRoot\System32\dxdiag.exe" -ArgumentList '/whql:off', '/t', $dxdiagOut -Wait -PassThru
$envReport.dxdiagExitCode = $dx.ExitCode
$envReport | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 (Join-Path $resultsDir 'environment.json')

# ---------------------------------------------------------------- helpers for cases
function Get-Wrapper([string] $Name) {
    $w = $matrix.wrappers.$Name
    if (-not $w) { throw "unknown wrapper '$Name'" }
    if (-not $w.files -or @($w.files).Count -eq 0) { return $null }
    if (-not $w.sha256) { throw "wrapper '$Name' has no pinned sha256 - refusing to use it" }
    $dir = Join-Path $cacheDir "wrapper-$Name"
    $marker = Join-Path $dir '.sha256'
    if (-not (Test-Path $marker) -or (Get-Content -Raw $marker).Trim() -ne $w.sha256) {
        Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
        $zip = Join-Path $cacheDir "wrapper-$Name.zip"
        Invoke-WebRequest -UseBasicParsing -Uri $w.url -OutFile $zip
        $sha = (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLowerInvariant()
        if ($sha -ne $w.sha256) { throw "wrapper '$Name' SHA-256 mismatch: $sha" }
        Expand-Archive -Path $zip -DestinationPath $dir -Force
        Remove-Item -Force $zip
        Set-Content -Path $marker -Value $w.sha256
    }
    $config = if ($w.PSObject.Properties.Name -contains 'config') { $w.config } else { $null }
    return [pscustomobject]@{ Dir = $dir; Files = @($w.files); Config = $config }
}

function Set-ScreenSetting($Settings) {
    $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey('LocalMachine', 'Registry32')
    $key = $base.CreateSubKey("$($matrix.game.registryKey)\Screen")
    try {
        foreach ($p in $Settings.PSObject.Properties) {
            $key.SetValue($p.Name, [int]$p.Value, [Microsoft.Win32.RegistryValueKind]::DWord)
        }
    } finally { $key.Close(); $base.Close() }
}

function Reset-ScreenSetting {
    $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey('LocalMachine', 'Registry32')
    try { $base.DeleteSubKeyTree("$($matrix.game.registryKey)\Screen", $false) } finally { $base.Close() }
}

# GTA 2 developer options (registry subkey Debug). A flag is ON when its value exists, whatever
# the number, so only flags that should be on are written; names are allowlisted by the caller.
$script:DebugStrings = @('mapname', 'stylename', 'scriptname')
function Set-DebugSetting($Settings) {
    $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey('LocalMachine', 'Registry32')
    $key = $base.CreateSubKey("$($matrix.game.registryKey)\Debug")
    try {
        foreach ($p in $Settings.PSObject.Properties) {
            if ($script:DebugStrings -contains $p.Name) {
                $key.SetValue($p.Name, [string]$p.Value, [Microsoft.Win32.RegistryValueKind]::String)
            } else {
                $key.SetValue($p.Name, 1, [Microsoft.Win32.RegistryValueKind]::DWord)
            }
        }
    } finally { $key.Close(); $base.Close() }
}

function Reset-DebugSetting {
    $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey('LocalMachine', 'Registry32')
    try { $base.DeleteSubKeyTree("$($matrix.game.registryKey)\Debug", $false) } finally { $base.Close() }
}

function Get-ScreenSetting([string] $Name) {
    $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey('LocalMachine', 'Registry32')
    $key = $base.OpenSubKey("$($matrix.game.registryKey)\Screen")
    try { if ($key) { $key.GetValue($Name) } } finally { if ($key) { $key.Close() }; $base.Close() }
}

function Merge-Setting($A, $B) {
    $merged = [ordered]@{}
    foreach ($src in @($A, $B)) {
        if ($src) { foreach ($p in $src.PSObject.Properties) { $merged[$p.Name] = $p.Value } }
    }
    return [pscustomobject]$merged
}

# ---------------------------------------------------------------- case loop
# Key injection method (owner setting in the matrix; scenarios cannot choose it).
$inputMethod = 'scancode'
if (($matrix.PSObject.Properties.Name -contains 'input') -and $matrix.input.method) { $inputMethod = [string]$matrix.input.method }
# Which harness revision actually ran here (written by ec2-gpu-session.sh at upload).
$revisionFile = Join-Path $scriptDir 'harness-revision.txt'
$harnessRevision = if (Test-Path $revisionFile) { (Get-Content -Raw -Path $revisionFile).Trim() } else { 'unknown' }
$summary = [ordered]@{
    runId = $RunId; mode = $Mode; startedAt = (Get-Date).ToString('o')
    harnessRevision = $harnessRevision; inputMethod = $inputMethod
    gpu = @($envReport.adapters | Where-Object { $_.Name -like '*NVIDIA*' } | ForEach-Object { $_.Name }) -join '; '
    driverStrategy = $boot.Driver.strategy
    interactiveSession = [bool]$session
    cases = @()
}

if ($Mode -ne 'probe') {
    if (-not $session) { throw "NO_INTERACTIVE_SESSION: '$TestUser' is not logged on at the console (autologon failed)." }

    $observe = $matrix.observe
    $inputs = @()
    $selected = @($matrix.cases)
    if ($Mode -eq 'capture') {
        # $Cases names one scenario file; its id pattern was validated by the parameter.
        $scenarioPath = Join-Path $scriptDir "scenarios\$Cases.json"
        if (-not (Test-Path $scenarioPath)) { throw "unknown scenario '$Cases'" }
        $scenario = Get-Content -Raw -Path $scenarioPath | ConvertFrom-Json
        $names = $scenario.PSObject.Properties.Name
        $scenarioScreen = if ($names -contains 'screen') { $scenario.screen } else { [pscustomobject]@{} }
        $scenarioDebug = if ($names -contains 'debug') { $scenario.debug } else { $null }
        $selected = @([pscustomobject]@{ id = "capture-$($scenario.id)"; wrapper = $scenario.wrapper; screen = $scenarioScreen; debug = $scenarioDebug })
        $observe = [pscustomobject]@{
            warmupSeconds        = $matrix.observe.warmupSeconds
            screenshotsAtSeconds = @($scenario.screenshotsAtSeconds)
            minUniqueColors      = $matrix.observe.minUniqueColors
            maxBlackRatio        = $matrix.observe.maxBlackRatio
            minChangedRatio      = $matrix.observe.minChangedRatio
            caseTimeoutSeconds   = [int] $scenario.durationSeconds + 30
        }
        $inputs = @(if ($names -contains 'inputs') { $scenario.inputs })
    } elseif ($Cases -ne 'all') {
        $wanted = $Cases.Split(',')
        $selected = @($selected | Where-Object { $wanted -contains $_.id })
        $selectedIds = @($selected | ForEach-Object { $_.id })
        $missing = @($wanted | Where-Object { $_ -notin $selectedIds })
        if ($missing) { throw "unknown case id(s): $($missing -join ', ')" }
    }

    $taskName = 'GTA20-GfxCase'
    $runner = Join-Path $scriptDir 'Run-Gta2Case.ps1'
    foreach ($c in $selected) {
        $caseDir = Join-Path $resultsDir "cases\$($c.id)"
        # A rerun with the same RunId must never read the previous run's result.json/frames.
        Remove-Item -Recurse -Force -Path $caseDir -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force -Path $caseDir | Out-Null
        $installed = @()
        $entry = [ordered]@{ id = $c.id; wrapper = $c.wrapper; verdict = $null; detail = $null }
        $caseStart = Get-Date
        try {
            Get-Process -Name ([IO.Path]::GetFileNameWithoutExtension($matrix.game.exe)) -ErrorAction SilentlyContinue |
                Stop-Process -Force

            $wrapper = Get-Wrapper $c.wrapper
            if ($wrapper) {
                foreach ($f in $wrapper.Files) {
                    $src = Join-Path $wrapper.Dir $f.from
                    if (-not (Test-Path $src)) { throw "wrapper file '$($f.from)' not found in archive" }
                    $dst = Join-Path $gameDir $f.to
                    if (Test-Path $dst) { throw "refusing to overwrite existing game file '$($f.to)'" }
                    Copy-Item -Path $src -Destination $dst
                    $installed += $dst
                }
                # Wrapper configuration files from the matrix (e.g. DDrawCompat.ini), one line each.
                if ($wrapper.Config) {
                    foreach ($cfgFile in $wrapper.Config.PSObject.Properties) {
                        if ($cfgFile.Name -notmatch '^[A-Za-z0-9_.-]{1,64}\.(ini|conf)$') { throw "bad wrapper config name '$($cfgFile.Name)'" }
                        $dst = Join-Path $gameDir $cfgFile.Name
                        if (Test-Path $dst) { throw "refusing to overwrite existing game file '$($cfgFile.Name)'" }
                        Set-Content -Path $dst -Value @($cfgFile.Value) -Encoding ASCII
                        $installed += $dst
                    }
                }
            }

            # Each case starts from the base settings only: values a previous case wrote
            # (e.g. renderdevice=2) or the game itself rewrote must not leak into the next case.
            Reset-ScreenSetting
            Reset-DebugSetting
            $screen = Merge-Setting $matrix.baseScreen $c.screen
            Set-ScreenSetting $screen
            if (($c.PSObject.Properties.Name -contains 'debug') -and $c.debug) { Set-DebugSetting $c.debug }

            $caseFile = Join-Path $caseDir 'case.json'
            [ordered]@{
                id = $c.id; gameDir = $gameDir; exe = $matrix.game.exe
                outDir = $caseDir; observe = $observe; inputs = $inputs; inputMethod = $inputMethod
            } | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 $caseFile
            & icacls.exe $caseDir /grant "${env:COMPUTERNAME}\${TestUser}:(OI)(CI)M" /T /Q | Out-Null

            $action = New-ScheduledTaskAction -Execute 'powershell.exe' `
                -Argument "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$runner`" -CaseFile `"$caseFile`""
            $principal = New-ScheduledTaskPrincipal -UserId "$env:COMPUTERNAME\$TestUser" -LogonType Interactive -RunLevel Limited
            $settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Seconds ($observe.caseTimeoutSeconds + 30))
            Register-ScheduledTask -TaskName $taskName -Action $action -Principal $principal -Settings $settings -Force | Out-Null
            Start-ScheduledTask -TaskName $taskName

            $resultFile = Join-Path $caseDir 'result.json'
            $deadline = (Get-Date).AddSeconds($observe.caseTimeoutSeconds)
            $gpuSeen = $false
            while (-not (Test-Path $resultFile) -and (Get-Date) -lt $deadline) {
                if ($smi -and -not $gpuSeen) {
                    $apps = (& $smi 2>$null) -join "`n"
                    if ($apps -match [regex]::Escape($matrix.game.exe)) { $gpuSeen = $true }
                }
                Start-Sleep -Seconds 2
            }
            Stop-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue

            $res = if (Test-Path $resultFile) { Get-Content -Raw $resultFile | ConvertFrom-Json } else { $null }
            $startAfter = Get-ScreenSetting 'start_mode'
            $entry.verdict = Get-CaseVerdict -Result $res -Observe $observe -StartModeRequested $screen.start_mode -StartModeAfter $startAfter
            $entry.detail = [ordered]@{
                startModeRequested = $screen.start_mode
                startModeAfter     = $startAfter
                gpuProcessSeen     = $gpuSeen
                sessionId          = if ($res) { $res.sessionId } else { $null }
                frames             = if ($res) { $res.frames } else { @() }
                exitCode           = if ($res) { $res.exitCode } else { $null }
                error              = if ($res) { $res.error } else { 'result.json not produced before timeout' }
            }
        } catch {
            $entry.verdict = 'ERROR'
            $entry.detail = @{ error = $_.Exception.Message }
        } finally {
            Get-Process -Name ([IO.Path]::GetFileNameWithoutExtension($matrix.game.exe)) -ErrorAction SilentlyContinue |
                Stop-Process -Force
            foreach ($f in $installed) { Remove-Item -Force $f -ErrorAction SilentlyContinue }
            Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction SilentlyContinue
            # Crash evidence for this case window.
            try {
                Get-WinEvent -FilterHashtable @{ LogName = 'Application'; Level = 1, 2; StartTime = $caseStart } -ErrorAction Stop |
                    Where-Object { $_.Message -match [regex]::Escape($matrix.game.exe) } |
                    Select-Object TimeCreated, ProviderName, Id, Message |
                    ConvertTo-Json -Depth 3 | Set-Content -Encoding UTF8 (Join-Path $caseDir 'app-errors.json')
            } catch {
                Write-Warning "no Application log errors collected for $($c.id): $($_.Exception.Message)"
            }
        }
        $summary.cases += $entry
        Write-Host ("{0,-28} {1}" -f $c.id, $entry.verdict)
    }
}

$passing = @($summary.cases | Where-Object { $_.verdict -like 'PASS*' })
$summary.finishedAt = (Get-Date).ToString('o')
$summary.overall = Get-OverallVerdict -Mode $Mode -Cases @($summary.cases)
# StrictMode 3: no member enumeration on a possibly empty array.
$summary.passingCases = @($passing | ForEach-Object { $_.id })
$summary | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 (Join-Path $resultsDir 'summary.json')

# ---------------------------------------------------------------- upload
foreach ($m in @('AWS.Tools.S3', 'AWSPowerShell')) {
    if (Get-Module -ListAvailable -Name $m) { Import-Module $m; break }
}
Write-S3Object -BucketName $Bucket -Folder $resultsDir -Recurse -KeyPrefix "runs/$RunId/results" -Region $Region | Out-Null
Write-Host "overall: $($summary.overall)"
exit 0
