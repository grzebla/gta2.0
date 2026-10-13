#Requires -Modules @{ ModuleName = 'Pester'; ModuleVersion = '5.0.0' }
# Unit tests for the pure analysis/verdict logic of the GTA 2 graphics test.
# All frames here are SYNTHETIC byte buffers, not screenshots of GTA 2.

BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..' 'GfxAnalysis.psm1') -Force

    function New-Frame([int] $W, [int] $H, [scriptblock] $Pixel) {
        $stride = $W * 4
        $bytes = New-Object byte[] ($stride * $H)
        for ($y = 0; $y -lt $H; $y++) {
            for ($x = 0; $x -lt $W; $x++) {
                $rgb = & $Pixel $x $y
                $i = $y * $stride + $x * 4
                $bytes[$i] = $rgb[2]; $bytes[$i + 1] = $rgb[1]; $bytes[$i + 2] = $rgb[0]; $bytes[$i + 3] = 255
            }
        }
        Get-FrameSample -Bytes $bytes -Width $W -Height $H -Stride $stride -Step 4
    }

    # "Desktop": many colours (like the EC2 wallpaper with instance info).
    $script:Desktop = New-Frame 64 48 { param($x, $y) @((($x * 4) % 256), (($y * 5) % 256), 90) }
    $script:Game = New-Frame 64 48 { param($x, $y) @(200, ((($x + $y) * 7) % 256), (($x * 9) % 256)) }
    $script:Black = New-Frame 64 48 { @(0, 0, 0) }
    $script:Observe = [pscustomobject]@{ minUniqueColors = 24; maxBlackRatio = 0.97; minChangedRatio = 0.25 }

    function Frame($sample, $baseline) {
        $d = Compare-FrameSample -Baseline $baseline -Frame $sample
        [pscustomobject]@{ uniqueColors = $sample.UniqueColors; blackRatio = $sample.BlackRatio;
            changedRatio = $d.ChangedRatio; sizeChanged = $d.SizeChanged }
    }
    function Result([object[]] $frames, [bool] $alive = $true) {
        [pscustomobject]@{ error = $null; alive = $alive; frames = $frames }
    }
}

Describe 'Get-FrameSample' {
    It 'counts colours and black pixels on the sampling grid' {
        $Black.UniqueColors | Should -Be 1
        $Black.BlackRatio | Should -Be 1
        $Desktop.UniqueColors | Should -BeGreaterThan 24
        $Desktop.Pixels.Length | Should -Be (16 * 12)
    }
}

Describe 'Compare-FrameSample' {
    It 'reports no change for an identical frame' {
        (Compare-FrameSample -Baseline $Desktop -Frame $Desktop).ChangedRatio | Should -Be 0
    }
    It 'reports a large change when the game covers the desktop' {
        (Compare-FrameSample -Baseline $Desktop -Frame $Game).ChangedRatio | Should -BeGreaterThan 0.5
    }
    It 'treats a resolution switch as a full change' {
        $small = New-Frame 32 24 { @(1, 2, 3) }
        $d = Compare-FrameSample -Baseline $Desktop -Frame $small
        $d.SizeChanged | Should -BeTrue
        $d.ChangedRatio | Should -Be 1
    }
    It 'ignores tiny per-channel noise below the threshold' {
        $noisy = New-Frame 64 48 { param($x, $y) @(((($x * 4) % 256) -bxor 1), (($y * 5) % 256), 90) }
        (Compare-FrameSample -Baseline $Desktop -Frame $noisy).ChangedRatio | Should -Be 0
    }
}

Describe 'Get-CaseVerdict' {
    It 'PASS when the game is drawn over the desktop until the end' {
        $f = Frame $Game $Desktop
        Get-CaseVerdict -Result (Result @($f, $f, $f)) -Observe $Observe | Should -Be 'PASS'
    }
    It 'FAIL_NO_VISIBLE_CHANGE when the process lives but the desktop is unchanged' {
        $f = Frame $Desktop $Desktop
        Get-CaseVerdict -Result (Result @($f, $f, $f)) -Observe $Observe | Should -Be 'FAIL_NO_VISIBLE_CHANGE'
    }
    It 'FAIL_BLANK_SCREEN when every frame is black' {
        $f = Frame $Black $Desktop
        Get-CaseVerdict -Result (Result @($f, $f)) -Observe $Observe | Should -Be 'FAIL_BLANK_SCREEN'
    }
    It 'FAIL_RENDER_STOPPED when rendering stops before the last frame' {
        Get-CaseVerdict -Result (Result @((Frame $Game $Desktop), (Frame $Black $Desktop))) -Observe $Observe |
            Should -Be 'FAIL_RENDER_STOPPED'
    }
    It 'FAIL_PROCESS_EXITED when the game quit, whatever the screen shows' {
        Get-CaseVerdict -Result (Result @((Frame $Game $Desktop)) -alive $false) -Observe $Observe |
            Should -Be 'FAIL_PROCESS_EXITED'
    }
    It 'PASS_FELL_BACK_TO_FULLSCREEN when windowed mode was rewritten to fullscreen' {
        $f = Frame $Game $Desktop
        Get-CaseVerdict -Result (Result @($f)) -Observe $Observe -StartModeRequested 0 -StartModeAfter 1 |
            Should -Be 'PASS_FELL_BACK_TO_FULLSCREEN'
    }
    It 'FAIL_GAME_DIALOG when the game shows its own message box, even over a changed screen' {
        $f = Frame $Game $Desktop
        $r = Result @($f, $f, $f)
        $r | Add-Member -NotePropertyName dialogs -NotePropertyValue @([pscustomobject]@{ title = 'Error!'; texts = @('Videomode 16x16x16 is not available') })
        Get-CaseVerdict -Result $r -Observe $Observe | Should -Be 'FAIL_GAME_DIALOG'
    }
    It 'ERROR variants for missing or failed results' {
        Get-CaseVerdict -Result $null -Observe $Observe | Should -Be 'ERROR_NO_RESULT'
        Get-CaseVerdict -Result ([pscustomobject]@{ error = 'boom'; alive = $true; frames = @() }) -Observe $Observe | Should -Be 'ERROR'
        Get-CaseVerdict -Result (Result @()) -Observe $Observe | Should -Be 'ERROR_NO_FRAMES'
    }
}

Describe 'Get-OverallVerdict' {
    It 'is COMPATIBLE with at least one PASS*' {
        Get-OverallVerdict -Mode compat -Cases @(@{ verdict = 'FAIL_BLANK_SCREEN' }, @{ verdict = 'PASS_FELL_BACK_TO_FULLSCREEN' }) | Should -Be 'COMPATIBLE'
    }
    It 'is INFRASTRUCTURE_ERROR when every case errored' {
        Get-OverallVerdict -Mode compat -Cases @(@{ verdict = 'ERROR' }, @{ verdict = 'ERROR_NO_RESULT' }) | Should -Be 'INFRASTRUCTURE_ERROR'
    }
    It 'is INCOMPATIBLE when cases ran and none passed' {
        Get-OverallVerdict -Mode compat -Cases @(@{ verdict = 'FAIL_NO_VISIBLE_CHANGE' }, @{ verdict = 'ERROR' }) | Should -Be 'INCOMPATIBLE'
    }
    It 'is PROBE_ONLY in probe mode' {
        Get-OverallVerdict -Mode probe | Should -Be 'PROBE_ONLY'
    }
}

Describe 'Register-RebootAttempt' {
    It 'allows a bounded number of reboots per run' {
        $dir = Join-Path ([IO.Path]::GetTempPath()) ([guid]::NewGuid())
        New-Item -ItemType Directory -Path $dir | Out-Null
        try {
            1..3 | ForEach-Object { Register-RebootAttempt -StateDir $dir -RunId 'r1' -MaxReboots 3 | Should -Be $_ }
            { Register-RebootAttempt -StateDir $dir -RunId 'r1' -MaxReboots 3 } | Should -Throw '*REBOOT_LOOP*'
            Register-RebootAttempt -StateDir $dir -RunId 'r2' -MaxReboots 3 | Should -Be 1
        } finally {
            Remove-Item -Recurse -Force $dir
        }
    }
}
