#Requires -Version 5.1
<#
.SYNOPSIS
  OpenGTA2 graphics test orchestrator. Runs as SYSTEM through SSM Run Command.

.DESCRIPTION
  Mode 'probe'  - bootstrap + environment report only (GPU, driver, sessions, dxdiag).
  Mode 'compat' - DirectDraw/Direct3D compatibility matrix for the original GTA2.
  Mode 'e2e'    - same runner restricted to the case(s) promoted after compat passed.

  Exit codes: 0 = finished (verdict in summary.json), 3010 = reboot requested
  (SSM reboots and re-runs the command), anything else = infrastructure error.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $Bucket,
    [Parameter(Mandatory)] [string] $Region,
    [Parameter(Mandatory)] [ValidatePattern('^[A-Za-z0-9-]{1,64}$')] [string] $RunId,
    [ValidateSet('probe', 'compat', 'e2e')] [string] $Mode = 'compat',
    [ValidatePattern('^(all|[a-z0-9-]+(,[a-z0-9-]+)*)$')] [string] $Cases = 'all',
    [string] $Root = 'C:\OpenGTA2',
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
    # SSM reboots on 3010 and re-runs this whole command; the counter stops an endless loop.
    $attempt = Register-RebootAttempt -StateDir (Join-Path $Root 'state') -RunId $RunId -MaxReboots 3
    Write-Host "Bootstrap changed drivers/autologon/WDDM - requesting reboot $attempt/3 (SSM exit 3010)."
    exit 3010
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
    return [pscustomobject]@{ Dir = $dir; Files = @($w.files) }
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
$summary = [ordered]@{
    runId = $RunId; mode = $Mode; startedAt = (Get-Date).ToString('o')
    gpu = @($envReport.adapters | Where-Object { $_.Name -like '*NVIDIA*' } | ForEach-Object { $_.Name }) -join '; '
    driverStrategy = $boot.Driver.strategy
    interactiveSession = [bool]$session
    cases = @()
}

if ($Mode -ne 'probe') {
    if (-not $session) { throw "NO_INTERACTIVE_SESSION: '$TestUser' is not logged on at the console (autologon failed)." }

    $selected = @($matrix.cases)
    if ($Cases -ne 'all') {
        $wanted = $Cases.Split(',')
        $selected = @($selected | Where-Object { $wanted -contains $_.id })
        $missing = @($wanted | Where-Object { $_ -notin @($selected.id) })
        if ($missing) { throw "unknown case id(s): $($missing -join ', ')" }
    }

    $taskName = 'OpenGTA2-GfxCase'
    $runner = Join-Path $scriptDir 'Run-Gta2Case.ps1'
    foreach ($c in $selected) {
        $caseDir = Join-Path $resultsDir "cases\$($c.id)"
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
            }

            $screen = Merge-Setting $matrix.baseScreen $c.screen
            Set-ScreenSetting $screen

            $caseFile = Join-Path $caseDir 'case.json'
            [ordered]@{
                id = $c.id; gameDir = $gameDir; exe = $matrix.game.exe
                outDir = $caseDir; observe = $matrix.observe
            } | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 $caseFile
            & icacls.exe $caseDir /grant "${env:COMPUTERNAME}\${TestUser}:(OI)(CI)M" /T /Q | Out-Null

            $action = New-ScheduledTaskAction -Execute 'powershell.exe' `
                -Argument "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$runner`" -CaseFile `"$caseFile`""
            $principal = New-ScheduledTaskPrincipal -UserId "$env:COMPUTERNAME\$TestUser" -LogonType Interactive -RunLevel Limited
            $settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Seconds ($matrix.observe.caseTimeoutSeconds + 30))
            Register-ScheduledTask -TaskName $taskName -Action $action -Principal $principal -Settings $settings -Force | Out-Null
            Start-ScheduledTask -TaskName $taskName

            $resultFile = Join-Path $caseDir 'result.json'
            $deadline = (Get-Date).AddSeconds($matrix.observe.caseTimeoutSeconds)
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
            $entry.verdict = Get-CaseVerdict -Result $res -Observe $matrix.observe -StartModeRequested $screen.start_mode -StartModeAfter $startAfter
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
$summary.passingCases = @($passing.id)
$summary | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 (Join-Path $resultsDir 'summary.json')

# ---------------------------------------------------------------- upload
foreach ($m in @('AWS.Tools.S3', 'AWSPowerShell')) {
    if (Get-Module -ListAvailable -Name $m) { Import-Module $m; break }
}
Write-S3Object -BucketName $Bucket -Folder $resultsDir -Recurse -KeyPrefix "runs/$RunId/results" -Region $Region | Out-Null
Write-Host "overall: $($summary.overall)"
exit 0
