#Requires -Version 5.1
<#
.SYNOPSIS
  Runs ONE graphics case inside the interactive console session of the test user.

.DESCRIPTION
  Started by Invoke-GfxCompat.ps1 through a scheduled task with LogonType
  Interactive. Launches the game, captures the desktop at fixed offsets,
  measures whether anything was rendered and writes result.json.
  Never runs as SYSTEM: session 0 has no visible desktop.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $CaseFile
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$case = Get-Content -Raw -Path $CaseFile | ConvertFrom-Json
$outDir = $case.outDir
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type -Namespace OpenGta2 -Name Native -MemberDefinition @'
[DllImport("user32.dll")] public static extern bool GetWindowRect(System.IntPtr hWnd, out RECT rect);
[DllImport("user32.dll")] public static extern bool IsWindowVisible(System.IntPtr hWnd);
[DllImport("user32.dll")] public static extern System.IntPtr GetForegroundWindow();
[DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
[System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
'@
[OpenGta2.Native]::SetProcessDPIAware() | Out-Null

Import-Module (Join-Path $PSScriptRoot 'GfxAnalysis.psm1') -Force

function Get-ScreenSample([string] $Path) {
    $bounds = [System.Windows.Forms.SystemInformation]::VirtualScreen
    $bmp = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $gfx = [System.Drawing.Graphics]::FromImage($bmp)
    try {
        $gfx.CopyFromScreen($bounds.Left, $bounds.Top, 0, 0, $bmp.Size)
        $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
        $rect = New-Object System.Drawing.Rectangle 0, 0, $bmp.Width, $bmp.Height
        $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $bytes = New-Object byte[] ($data.Stride * $data.Height)
            [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
            return Get-FrameSample -Bytes $bytes -Width $data.Width -Height $data.Height -Stride $data.Stride
        } finally {
            $bmp.UnlockBits($data)
        }
    } finally {
        $gfx.Dispose(); $bmp.Dispose()
    }
}

$result = [ordered]@{
    caseId     = $case.id
    sessionId  = [System.Diagnostics.Process]::GetCurrentProcess().SessionId
    user       = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
    startedAt  = (Get-Date).ToString('o')
    frames     = @()
    window     = $null
    alive      = $false
    exitCode   = $null
    error      = $null
}

$proc = $null
try {
    # Desktop before the game starts: frames must differ from it to count as "rendered",
    # otherwise a live process over an unchanged desktop would look like success.
    $baseline = Get-ScreenSample (Join-Path $outDir 'baseline-desktop.png')
    $result.baseline = [ordered]@{ width = $baseline.Width; height = $baseline.Height;
        uniqueColors = $baseline.UniqueColors; blackRatio = $baseline.BlackRatio }

    $exe = Join-Path $case.gameDir $case.exe
    $proc = Start-Process -FilePath $exe -WorkingDirectory $case.gameDir -PassThru
    $result.pid = $proc.Id
    $t0 = Get-Date

    foreach ($at in $case.observe.screenshotsAtSeconds) {
        $wait = [double]$at - ((Get-Date) - $t0).TotalSeconds
        if ($wait -gt 0) { Start-Sleep -Milliseconds ([int]($wait * 1000)) }
        $png = Join-Path $outDir ('frame-{0:D3}s.png' -f [int]$at)
        $m = Get-ScreenSample $png
        $diff = Compare-FrameSample -Baseline $baseline -Frame $m
        $proc.Refresh()
        $result.frames += [ordered]@{
            atSeconds    = [int]$at
            file         = (Split-Path -Leaf $png)
            width        = $m.Width
            height       = $m.Height
            uniqueColors = $m.UniqueColors
            blackRatio   = $m.BlackRatio
            changedRatio = $diff.ChangedRatio
            sizeChanged  = $diff.SizeChanged
            processAlive = -not $proc.HasExited
            foreground   = ([OpenGta2.Native]::GetForegroundWindow() -eq $proc.MainWindowHandle)
        }
    }

    $proc.Refresh()
    $result.alive = -not $proc.HasExited
    if ($proc.HasExited) {
        $result.exitCode = $proc.ExitCode
    } elseif ($proc.MainWindowHandle -ne [IntPtr]::Zero) {
        $r = New-Object OpenGta2.Native+RECT
        [OpenGta2.Native]::GetWindowRect($proc.MainWindowHandle, [ref] $r) | Out-Null
        $result.window = [ordered]@{
            title   = $proc.MainWindowTitle
            visible = [OpenGta2.Native]::IsWindowVisible($proc.MainWindowHandle)
            left = $r.Left; top = $r.Top; right = $r.Right; bottom = $r.Bottom
        }
    }
} catch {
    $result.error = $_.Exception.Message
} finally {
    if ($proc -and -not $proc.HasExited) {
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    }
    $result.finishedAt = (Get-Date).ToString('o')
    $result | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -Path (Join-Path $outDir 'result.json')
}
