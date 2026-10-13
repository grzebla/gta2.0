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
Add-Type -Namespace Gta20 -Name Native -MemberDefinition @'
[DllImport("user32.dll")] public static extern bool GetWindowRect(System.IntPtr hWnd, out RECT rect);
[DllImport("user32.dll")] public static extern bool IsWindowVisible(System.IntPtr hWnd);
[DllImport("user32.dll")] public static extern System.IntPtr GetForegroundWindow();
[DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
[DllImport("user32.dll")] public static extern bool SetForegroundWindow(System.IntPtr hWnd);
[System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
public struct KEYBDINPUT { public ushort wVk; public ushort wScan; public uint dwFlags; public uint time; public System.IntPtr dwExtraInfo; }
[System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
public struct MOUSEINPUT { public int dx; public int dy; public uint mouseData; public uint dwFlags; public uint time; public System.IntPtr dwExtraInfo; }
[System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Explicit)]
public struct InputUnion { [System.Runtime.InteropServices.FieldOffset(0)] public MOUSEINPUT mi; [System.Runtime.InteropServices.FieldOffset(0)] public KEYBDINPUT ki; }
[System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
public struct INPUT { public uint type; public InputUnion u; }
[DllImport("user32.dll", SetLastError = true)] public static extern uint SendInput(uint n, INPUT[] inputs, int size);
public delegate bool EnumProc(System.IntPtr hWnd, System.IntPtr lParam);
[DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, System.IntPtr lParam);
[DllImport("user32.dll")] public static extern bool EnumChildWindows(System.IntPtr parent, EnumProc cb, System.IntPtr lParam);
[DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(System.IntPtr hWnd, out uint pid);
[DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(System.IntPtr hWnd, System.Text.StringBuilder s, int n);
[DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(System.IntPtr hWnd, System.Text.StringBuilder s, int n);
[System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
'@
[Gta20.Native]::SetProcessDPIAware() | Out-Null

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
    dialogs    = @()
    inputsSent = @()
    alive      = $false
    exitCode   = $null
    error      = $null
}

# Scan codes (set 1) for scripted input. DirectInput games read scan codes, not window
# messages, so input goes through SendInput with KEYEVENTF_SCANCODE. [code, extended]
$script:ScanCodes = @{
    ENTER = @(0x1C, $false); ESC = @(0x01, $false); SPACE = @(0x39, $false); TAB = @(0x0F, $false)
    UP = @(0x48, $true); DOWN = @(0x50, $true); LEFT = @(0x4B, $true); RIGHT = @(0x4D, $true)
    LCTRL = @(0x1D, $false); LSHIFT = @(0x2A, $false); LALT = @(0x38, $false); BACKSPACE = @(0x0E, $false)
}
$letters = 'QWERTYUIOP'; for ($i = 0; $i -lt 10; $i++) { $script:ScanCodes[[string]$letters[$i]] = @((0x10 + $i), $false) }
$letters = 'ASDFGHJKL'; for ($i = 0; $i -lt 9; $i++) { $script:ScanCodes[[string]$letters[$i]] = @((0x1E + $i), $false) }
$letters = 'ZXCVBNM'; for ($i = 0; $i -lt 7; $i++) { $script:ScanCodes[[string]$letters[$i]] = @((0x2C + $i), $false) }
$digits = '1234567890'; for ($i = 0; $i -lt 10; $i++) { $script:ScanCodes[[string]$digits[$i]] = @((0x02 + $i), $false) }

function Send-GameKey([string] $Key, [int] $HoldMs, [IntPtr] $Window) {
    if (-not $script:ScanCodes.ContainsKey($Key)) { throw "unsupported key '$Key'" }
    $code = $script:ScanCodes[$Key]
    if ($Window -ne [IntPtr]::Zero) { [Gta20.Native]::SetForegroundWindow($Window) | Out-Null }
    $flags = [uint32]0x0008                     # KEYEVENTF_SCANCODE
    if ($code[1]) { $flags = $flags -bor 0x0001 } # KEYEVENTF_EXTENDEDKEY
    $sent = 0
    foreach ($up in @($false, $true)) {
        $in = New-Object Gta20.Native+INPUT
        $in.type = 1
        $ki = New-Object Gta20.Native+KEYBDINPUT
        $ki.wScan = [uint16] $code[0]
        $ki.dwFlags = if ($up) { $flags -bor 0x0002 } else { $flags } # KEYEVENTF_KEYUP
        $in.u.ki = $ki
        $sent += [Gta20.Native]::SendInput(1, @($in), [System.Runtime.InteropServices.Marshal]::SizeOf([type] [Gta20.Native+INPUT]))
        if (-not $up) { Start-Sleep -Milliseconds ([math]::Max(30, $HoldMs)) }
    }
    return $sent
}

# Visible dialog boxes (class #32770) owned by a process, with their static texts.
# A game that stops at its own message box must not count as rendered.
function Get-ProcessDialog([int] $ProcessId) {
    $found = [System.Collections.Generic.List[object]]::new()
    $cb = [Gta20.Native+EnumProc] {
        param($h, $l)
        $null = $l
        $owner = [uint32]0
        [Gta20.Native]::GetWindowThreadProcessId($h, [ref] $owner) | Out-Null
        if ($owner -eq $ProcessId -and [Gta20.Native]::IsWindowVisible($h)) {
            $cls = New-Object System.Text.StringBuilder 64
            [Gta20.Native]::GetClassName($h, $cls, 64) | Out-Null
            if ($cls.ToString() -eq '#32770') {
                $title = New-Object System.Text.StringBuilder 256
                [Gta20.Native]::GetWindowText($h, $title, 256) | Out-Null
                $texts = [System.Collections.Generic.List[string]]::new()
                $child = [Gta20.Native+EnumProc] {
                    param($c, $l2)
                    $null = $l2
                    $t = New-Object System.Text.StringBuilder 512
                    [Gta20.Native]::GetWindowText($c, $t, 512) | Out-Null
                    if ($t.Length -gt 0) { $texts.Add($t.ToString()) }
                    return $true
                }
                [Gta20.Native]::EnumChildWindows($h, $child, [IntPtr]::Zero) | Out-Null
                $found.Add([ordered]@{ title = $title.ToString(); texts = @($texts) })
            }
        }
        return $true
    }
    [Gta20.Native]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
    return @($found)
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

    # One timeline: screenshots and (capture mode) scripted key presses, in time order.
    $events = @()
    foreach ($at in $case.observe.screenshotsAtSeconds) { $events += [pscustomobject]@{ at = [double]$at; kind = 'shot'; key = $null; hold = 0 } }
    if (($case.PSObject.Properties.Name -contains 'inputs') -and $case.inputs) {
        foreach ($k in @($case.inputs)) {
            $hold = if ($k.PSObject.Properties.Name -contains 'holdMs') { [int]$k.holdMs } else { 80 }
            $events += [pscustomobject]@{ at = [double]$k.atSeconds; kind = 'key'; key = [string]$k.key; hold = $hold }
        }
    }
    $result.inputsSent = @()
    foreach ($ev in @($events | Sort-Object at, @{ Expression = { if ($_.kind -eq 'key') { 0 } else { 1 } } })) {
        $wait = $ev.at - ((Get-Date) - $t0).TotalSeconds
        if ($wait -gt 0) { Start-Sleep -Milliseconds ([int]($wait * 1000)) }
        if ($ev.kind -eq 'key') {
            $proc.Refresh()
            $n = Send-GameKey -Key $ev.key -HoldMs $ev.hold -Window $proc.MainWindowHandle
            $result.inputsSent += [ordered]@{ atSeconds = $ev.at; key = $ev.key; events = $n }
            continue
        }
        $at = $ev.at
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
            foreground   = ([Gta20.Native]::GetForegroundWindow() -eq $proc.MainWindowHandle)
        }
    }

    $proc.Refresh()
    $result.alive = -not $proc.HasExited
    $result.dialogs = if ($result.alive) { @(Get-ProcessDialog -ProcessId $proc.Id) } else { @() }
    if ($proc.HasExited) {
        $result.exitCode = $proc.ExitCode
    } elseif ($proc.MainWindowHandle -ne [IntPtr]::Zero) {
        $r = New-Object Gta20.Native+RECT
        [Gta20.Native]::GetWindowRect($proc.MainWindowHandle, [ref] $r) | Out-Null
        $result.window = [ordered]@{
            title   = $proc.MainWindowTitle
            visible = [Gta20.Native]::IsWindowVisible($proc.MainWindowHandle)
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
