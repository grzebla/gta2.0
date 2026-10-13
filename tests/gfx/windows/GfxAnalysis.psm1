#Requires -Version 5.1
<#
  Pure analysis functions for the GTA 2 graphics compatibility test.
  No Windows APIs here, so the logic is unit-tested with Pester on any OS
  (tests/gfx/windows/tests/GfxAnalysis.Tests.ps1).
#>
Set-StrictMode -Version 3.0

function Get-FrameSample {
    <#
      .SYNOPSIS Samples a 32bpp BGRA buffer on a grid; returns packed RGB ints plus summary stats.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)] [byte[]] $Bytes,
        [Parameter(Mandatory)] [int] $Width,
        [Parameter(Mandatory)] [int] $Height,
        [Parameter(Mandatory)] [int] $Stride,
        [int] $Step = 4
    )
    $cols = [int][math]::Ceiling($Width / $Step)
    $rows = [int][math]::Ceiling($Height / $Step)
    $pixels = New-Object int[] ($cols * $rows)
    $colors = New-Object 'System.Collections.Generic.HashSet[int]'
    $black = 0; $n = 0
    for ($y = 0; $y -lt $Height; $y += $Step) {
        $row = $y * $Stride
        for ($x = 0; $x -lt $Width; $x += $Step) {
            $i = $row + $x * 4
            $b = [int]$Bytes[$i]; $g = [int]$Bytes[$i + 1]; $r = [int]$Bytes[$i + 2]
            $rgb = ($r -shl 16) -bor ($g -shl 8) -bor $b
            $pixels[$n] = $rgb
            [void] $colors.Add($rgb)
            if (($r + $g + $b) -lt 24) { $black++ }
            $n++
        }
    }
    [pscustomobject]@{
        Width        = $Width
        Height       = $Height
        Step         = $Step
        Pixels       = $pixels
        UniqueColors = $colors.Count
        BlackRatio   = [math]::Round($black / [math]::Max($n, 1), 4)
    }
}

function Compare-FrameSample {
    <#
      .SYNOPSIS Fraction of sampled pixels that differ from the pre-launch desktop baseline.
      A different resolution (fullscreen mode switch) counts as a full change.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)] $Baseline,
        [Parameter(Mandatory)] $Frame,
        [int] $ChannelThreshold = 24
    )
    if ($Baseline.Width -ne $Frame.Width -or $Baseline.Height -ne $Frame.Height -or $Baseline.Step -ne $Frame.Step) {
        return [pscustomobject]@{ ChangedRatio = 1.0; SizeChanged = $true }
    }
    $changed = 0
    $count = $Frame.Pixels.Length
    for ($i = 0; $i -lt $count; $i++) {
        $a = $Baseline.Pixels[$i]; $b = $Frame.Pixels[$i]
        if ($a -ne $b) {
            $dr = [math]::Abs((($a -shr 16) -band 255) - (($b -shr 16) -band 255))
            $dg = [math]::Abs((($a -shr 8) -band 255) - (($b -shr 8) -band 255))
            $db = [math]::Abs(($a -band 255) - ($b -band 255))
            if ([math]::Max($dr, [math]::Max($dg, $db)) -gt $ChannelThreshold) { $changed++ }
        }
    }
    [pscustomobject]@{
        ChangedRatio = [math]::Round($changed / [math]::Max($count, 1), 4)
        SizeChanged  = $false
    }
}

function Test-FrameRendered {
    <# A frame counts as "game rendered" only if it is non-trivial AND differs from the desktop. #>
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Frame, [Parameter(Mandatory)] $Observe)
    $minChanged = if ($Observe.PSObject.Properties.Name -contains 'minChangedRatio') { [double]$Observe.minChangedRatio } else { 0.25 }
    return ($Frame.uniqueColors -ge $Observe.minUniqueColors) -and
           ($Frame.blackRatio -le $Observe.maxBlackRatio) -and
           ($Frame.sizeChanged -or $Frame.changedRatio -ge $minChanged)
}

function Get-CaseVerdict {
    <#
      .SYNOPSIS Classifies one case. Distinguishes "process running" from "game actually drawn".
    #>
    [CmdletBinding()]
    param(
        $Result,
        [Parameter(Mandatory)] $Observe,
        $StartModeRequested,
        $StartModeAfter
    )
    if (-not $Result) { return 'ERROR_NO_RESULT' }
    if ($Result.error) { return 'ERROR' }
    $frames = @($Result.frames)
    if ($frames.Count -eq 0) { return 'ERROR_NO_FRAMES' }
    if (-not $Result.alive) { return 'FAIL_PROCESS_EXITED' }
    # The game stopped at its own message box (e.g. "Videomode 16x16x16 is not available").
    # @($null).Count is 1: count real dialog entries only (defect D1 gave false FAIL_GAME_DIALOG).
    if (($Result.PSObject.Properties.Name -contains 'dialogs') -and @($Result.dialogs | Where-Object { $null -ne $_ }).Count -gt 0) { return 'FAIL_GAME_DIALOG' }

    $rendered = @($frames | Where-Object { Test-FrameRendered -Frame $_ -Observe $Observe })
    if ($rendered.Count -eq 0) {
        $nonTrivial = @($frames | Where-Object { $_.uniqueColors -ge $Observe.minUniqueColors -and $_.blackRatio -le $Observe.maxBlackRatio })
        # Desktop still visible and unchanged: the process lives but draws nothing on screen.
        if ($nonTrivial.Count -gt 0) { return 'FAIL_NO_VISIBLE_CHANGE' }
        return 'FAIL_BLANK_SCREEN'
    }
    if (-not (Test-FrameRendered -Frame $frames[-1] -Observe $Observe)) { return 'FAIL_RENDER_STOPPED' }
    if ($StartModeRequested -eq 0 -and $StartModeAfter -eq 1) { return 'PASS_FELL_BACK_TO_FULLSCREEN' }
    return 'PASS'
}

function Get-OverallVerdict {
    [CmdletBinding()]
    param([string] $Mode, [object[]] $Cases = @())
    if ($Mode -eq 'probe') { return 'PROBE_ONLY' }
    if (@($Cases | Where-Object { $_.verdict -like 'PASS*' }).Count -gt 0) { return 'COMPATIBLE' }
    if (@($Cases).Count -gt 0 -and @($Cases | Where-Object { $_.verdict -notlike 'ERROR*' }).Count -eq 0) { return 'INFRASTRUCTURE_ERROR' }
    return 'INCOMPATIBLE'
}

function Register-RebootAttempt {
    <#
      .SYNOPSIS Counts bootstrap reboots per run; throws when the limit is exceeded (reboot-loop guard).
    #>
    [CmdletBinding()]
    param([Parameter(Mandatory)] [string] $StateDir, [Parameter(Mandatory)] [string] $RunId, [int] $MaxReboots = 3)
    $file = Join-Path $StateDir "reboots-$RunId.txt"
    $count = if (Test-Path $file) { [int](Get-Content -Raw $file) } else { 0 }
    if ($count -ge $MaxReboots) {
        throw "REBOOT_LOOP: bootstrap requested more than $MaxReboots reboots in run $RunId."
    }
    Set-Content -Path $file -Value ($count + 1)
    return $count + 1
}

Export-ModuleMember -Function Get-FrameSample, Compare-FrameSample, Test-FrameRendered, Get-CaseVerdict, Get-OverallVerdict, Register-RebootAttempt
