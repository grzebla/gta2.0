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
[DllImport("user32.dll", SetLastError = true)] public static extern bool PostMessage(System.IntPtr hWnd, uint msg, System.IntPtr wParam, System.IntPtr lParam);
[DllImport("user32.dll")] public static extern bool IsHungAppWindow(System.IntPtr hWnd);
[DllImport("user32.dll")] public static extern bool AttachThreadInput(uint idAttach, uint idAttachTo, bool fAttach);
[DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
[DllImport("user32.dll")] public static extern System.IntPtr SetFocus(System.IntPtr hWnd);
[DllImport("user32.dll")] public static extern bool BringWindowToTop(System.IntPtr hWnd);
[DllImport("user32.dll", SetLastError = true)] public static extern System.IntPtr OpenInputDesktop(uint flags, bool inherit, uint access);
[DllImport("user32.dll")] public static extern bool CloseDesktop(System.IntPtr hDesk);
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

# Scan codes (set 1) for scripted input. [code, extended]. DirectInput games read scan codes;
# the injection method comes from the compat matrix (owner setting, see Send-GameKey).
$script:ScanCodes = @{
    ENTER = @(0x1C, $false); ESC = @(0x01, $false); SPACE = @(0x39, $false); TAB = @(0x0F, $false)
    UP = @(0x48, $true); DOWN = @(0x50, $true); LEFT = @(0x4B, $true); RIGHT = @(0x4D, $true)
    LCTRL = @(0x1D, $false); LSHIFT = @(0x2A, $false); LALT = @(0x38, $false); BACKSPACE = @(0x0E, $false)
}
$letters = 'QWERTYUIOP'; for ($i = 0; $i -lt 10; $i++) { $script:ScanCodes[[string]$letters[$i]] = @((0x10 + $i), $false) }
$letters = 'ASDFGHJKL'; for ($i = 0; $i -lt 9; $i++) { $script:ScanCodes[[string]$letters[$i]] = @((0x1E + $i), $false) }
$letters = 'ZXCVBNM'; for ($i = 0; $i -lt 7; $i++) { $script:ScanCodes[[string]$letters[$i]] = @((0x2C + $i), $false) }
$digits = '1234567890'; for ($i = 0; $i -lt 10; $i++) { $script:ScanCodes[[string]$digits[$i]] = @((0x02 + $i), $false) }

# Virtual-key codes for the same keys (methods 'vk' and 'postmessage').
$script:VirtualKeys = @{
    ENTER = 0x0D; ESC = 0x1B; SPACE = 0x20; TAB = 0x09; UP = 0x26; DOWN = 0x28; LEFT = 0x25; RIGHT = 0x27
    LCTRL = 0xA2; LSHIFT = 0xA0; LALT = 0xA4; BACKSPACE = 0x08
}
foreach ($c in [char[]] 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789') { $script:VirtualKeys[[string]$c] = [int][char]$c }
$script:InputMethods = @('scancode', 'vk', 'postmessage', 'activate-scancode')

function Get-WindowOwner([IntPtr] $Window) {
    $owner = [uint32]0
    $thread = [Gta20.Native]::GetWindowThreadProcessId($Window, [ref] $owner)
    return [pscustomobject]@{ Pid = [int]$owner; Thread = $thread }
}

function Send-KeyEvent([uint16] $Vk, [uint16] $Scan, [uint32] $Flags) {
    $in = New-Object Gta20.Native+INPUT
    $in.type = 1
    $ki = New-Object Gta20.Native+KEYBDINPUT
    $ki.wVk = $Vk
    $ki.wScan = $Scan
    $ki.dwFlags = $Flags
    $in.u.ki = $ki
    $n = [Gta20.Native]::SendInput(1, @($in), [System.Runtime.InteropServices.Marshal]::SizeOf([type] [Gta20.Native+INPUT]))
    return [pscustomobject]@{ Sent = [int]$n; Error = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error() }
}

# Sends one key press to the game and returns diagnostics (never proof that the game consumed it).
# Methods: scancode = SendInput with KEYEVENTF_SCANCODE; vk = SendInput with virtual key + scan
# code; postmessage = WM_KEYDOWN/WM_KEYUP posted to the game window; activate-scancode = attach
# to the game's input thread, bring it to the top and focus it, then scancode.
function Send-GameKey([string] $Key, [int] $HoldMs, [IntPtr] $Window, [string] $Method, [int] $GamePid) {
    if (-not $script:ScanCodes.ContainsKey($Key)) { throw "unsupported key '$Key'" }
    if ($script:InputMethods -notcontains $Method) { throw "unsupported input method '$Method'" }
    $code = $script:ScanCodes[$Key]
    $vk = [uint16] $script:VirtualKeys[$Key]
    $scan = [uint16] $code[0]
    $ext = [bool] $code[1]
    $diag = [ordered]@{ method = $Method; window = [int64]$Window; sendInput = @(); postMessage = @() }
    $before = [Gta20.Native]::GetForegroundWindow()
    $diag.foregroundBefore = [ordered]@{ window = [int64]$before; pid = (Get-WindowOwner $before).Pid }
    $desk = [Gta20.Native]::OpenInputDesktop(0, $false, 0x0100)  # DESKTOP_SWITCHDESKTOP
    $diag.inputDesktopAccessible = ($desk -ne [IntPtr]::Zero)
    if ($desk -ne [IntPtr]::Zero) { [Gta20.Native]::CloseDesktop($desk) | Out-Null }
    if ($Window -ne [IntPtr]::Zero) {
        $diag.hung = [Gta20.Native]::IsHungAppWindow($Window)
        $attached = $false
        $target = Get-WindowOwner $Window
        if ($Method -eq 'activate-scancode') {
            $attached = [Gta20.Native]::AttachThreadInput([Gta20.Native]::GetCurrentThreadId(), $target.Thread, $true)
            [Gta20.Native]::BringWindowToTop($Window) | Out-Null
        }
        $diag.setForeground = [Gta20.Native]::SetForegroundWindow($Window)
        if ($attached) {
            [Gta20.Native]::SetFocus($Window) | Out-Null
            [Gta20.Native]::AttachThreadInput([Gta20.Native]::GetCurrentThreadId(), $target.Thread, $false) | Out-Null
        }
        $diag.attachedThreadInput = $attached
    }
    $fg = [Gta20.Native]::GetForegroundWindow()
    $diag.foregroundAtSend = [ordered]@{ window = [int64]$fg; pid = (Get-WindowOwner $fg).Pid; isGame = ((Get-WindowOwner $fg).Pid -eq $GamePid) }
    foreach ($up in @($false, $true)) {
        if ($Method -eq 'postmessage') {
            $lp = [int64]1 -bor ([int64]$scan -shl 16)
            if ($ext) { $lp = $lp -bor (1 -shl 24) }
            if ($up) { $lp = $lp -bor 0xC0000000L }
            $ok = [Gta20.Native]::PostMessage($Window, $(if ($up) { 0x0101 } else { 0x0100 }), [IntPtr][int64]$vk, [IntPtr]$lp)
            $diag.postMessage += [ordered]@{ up = $up; ok = $ok; error = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error() }
        } else {
            if ($Method -eq 'vk') {
                $flags = [uint32]0; $v = $vk
            } else {
                $flags = [uint32]0x0008; $v = [uint16]0       # KEYEVENTF_SCANCODE
            }
            if ($ext) { $flags = $flags -bor 0x0001 }       # KEYEVENTF_EXTENDEDKEY
            if ($up) { $flags = $flags -bor 0x0002 }        # KEYEVENTF_KEYUP
            $r = Send-KeyEvent -Vk $v -Scan $scan -Flags $flags
            $diag.sendInput += [ordered]@{ up = $up; sent = $r.Sent; error = $r.Error }
        }
        if (-not $up) { Start-Sleep -Milliseconds ([math]::Max(30, $HoldMs)) }
    }
    return $diag
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
    $lastSample = $baseline
    $proc = Start-Process -FilePath $exe -WorkingDirectory $case.gameDir -PassThru
    $result.pid = $proc.Id
    $t0 = Get-Date

    # Input method: one name, or 'cycle' (key i uses method i mod n - diagnosis of which
    # injection the game accepts). Set by the owner in the compat matrix, never by a scenario.
    $inputMethod = 'scancode'
    if (($case.PSObject.Properties.Name -contains 'inputMethod') -and $case.inputMethod) { $inputMethod = [string]$case.inputMethod }
    $keyIndex = 0
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
            $method = if ($inputMethod -eq 'cycle') { $script:InputMethods[$keyIndex % $script:InputMethods.Count] } else { $inputMethod }
            $keyIndex++
            $cpuBefore = $proc.TotalProcessorTime.TotalMilliseconds
            $diag = Send-GameKey -Key $ev.key -HoldMs $ev.hold -Window $proc.MainWindowHandle -Method $method -GamePid $proc.Id
            # What the screen shows shortly after the key (evidence, not proof of consumption).
            Start-Sleep -Milliseconds 1500
            $afterPng = Join-Path $outDir ('key-{0:D3}s-{1}-{2}.png' -f [int]$ev.at, $ev.key, $method)
            $after = Get-ScreenSample $afterPng
            $proc.Refresh()
            $diag.atSeconds = $ev.at
            $diag.key = $ev.key
            $diag.afterFile = (Split-Path -Leaf $afterPng)
            $diag.afterChangedRatio = (Compare-FrameSample -Baseline $lastSample -Frame $after).ChangedRatio
            $diag.gameCpuMsDuring = [math]::Round($proc.TotalProcessorTime.TotalMilliseconds - $cpuBefore)
            $result.inputsSent += $diag
            $lastSample = $after
            continue
        }
        $at = $ev.at
        $png = Join-Path $outDir ('frame-{0:D3}s.png' -f [int]$at)
        $m = Get-ScreenSample $png
        $lastSample = $m
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
    # @(if ...) - an if-expression would unroll an empty array to $null (defect D1).
    $result.dialogs = @(if ($result.alive) { Get-ProcessDialog -ProcessId $proc.Id })
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
