# Static checks of the host scripts for Windows API limits that only fail on a real host.
BeforeAll {
    $script:hostDir = Split-Path -Parent $PSScriptRoot
}

Describe 'Windows API limits in host scripts' {
    It 'keeps every New-LocalUser/Set-LocalUser -Description within 48 characters' {
        $violations = foreach ($f in Get-ChildItem -Path $script:hostDir -Filter '*.ps1') {
            $text = Get-Content -Raw -Path $f.FullName
            foreach ($m in [regex]::Matches($text, "-Description\s+'([^']*)'")) {
                if ($m.Groups[1].Value.Length -gt 48) { "$($f.Name): '$($m.Groups[1].Value)'" }
            }
        }
        $violations | Should -BeNullOrEmpty
    }
}

Describe 'StrictMode-safe member access in host scripts' {
    It 'never enumerates a member on a whole array inside @(...)' {
        # Under Set-StrictMode 3, @($array.prop) throws PropertyNotFoundStrict when $array is empty.
        $violations = foreach ($f in Get-ChildItem -Path $script:hostDir -Filter '*.ps1') {
            $lines = Get-Content -Path $f.FullName
            for ($i = 0; $i -lt $lines.Count; $i++) {
                if ($lines[$i] -match '@\(\$(passing|selected|cases)\.\w+\)') { "$($f.Name):$($i + 1): $($lines[$i].Trim())" }
            }
        }
        $violations | Should -BeNullOrEmpty
    }
}

Describe 'Reboot handling' {
    It 'never relies on the SSM agent reboot (exit 3010 hangs on this host)' {
        $text = Get-Content -Raw -Path (Join-Path $script:hostDir 'Invoke-GfxCompat.ps1')
        $text | Should -Not -Match '(?m)^\s*exit\s+3010'
        $text | Should -Match '(?m)^\s*exit\s+194'
    }
}

Describe 'Game prerequisites' {
    It 'enables DirectPlay, which gta2.exe imports (DPLAYX.dll)' {
        $text = Get-Content -Raw -Path (Join-Path $script:hostDir 'Initialize-GfxHost.ps1')
        $text | Should -Match 'Enable-WindowsOptionalFeature -Online -FeatureName DirectPlay'
    }
}

Describe 'Unattended host settings' {
    BeforeAll { $script:init = Get-Content -Raw -Path (Join-Path $script:hostDir 'Initialize-GfxHost.ps1') }
    It 'creates the player folder the game saves into' { $script:init | Should -Match 'Join-Path \$gameDir ''player''' }
    It 'suppresses blocking hard-error dialogs' { $script:init | Should -Match 'ErrorMode -Value 2' }
    It 'suppresses WER dialogs' { $script:init | Should -Match 'DontShowUI -Value 1' }
    It 'disables the Basic Display Adapter only once the T4 runs WDDM' { $script:init | Should -Match "modelNow -eq 'WDDM'" }
}

Describe 'Capture mode (scripted input)' {
    BeforeAll {
        $script:invoke = Get-Content -Raw -Path (Join-Path $script:hostDir 'Invoke-GfxCompat.ps1')
        $script:runner = Get-Content -Raw -Path (Join-Path $script:hostDir 'Run-Gta2Case.ps1')
    }
    It 'accepts capture as a mode' { $script:invoke | Should -Match "ValidateSet\('probe', 'compat', 'e2e', 'capture'\)" }
    It 'reads the scenario only from the uploaded scenarios folder' { $script:invoke | Should -Match 'scenarios\\\$Cases\.json' }
    It 'sends keys as scan codes (DirectInput reads scan codes)' { $script:runner | Should -Match 'KEYEVENTF_SCANCODE' }
    It 'refuses keys outside the scan-code table' { $script:runner | Should -Match 'unsupported key' }
    It 'records what was sent' { $script:runner | Should -Match 'inputsSent' }
    It 'never evaluates scenario text as code' {
        ($script:invoke + $script:runner) | Should -Not -Match 'Invoke-Expression|\biex\b|ScriptBlock\]::Create'
    }
}

Describe 'Input diagnostics and provenance' {
    BeforeAll {
        $script:invoke = Get-Content -Raw -Path (Join-Path $script:hostDir 'Invoke-GfxCompat.ps1')
        $script:runner = Get-Content -Raw -Path (Join-Path $script:hostDir 'Run-Gta2Case.ps1')
    }
    It 'never assigns an if-expression that can unroll an empty array to $null (D1)' {
        ($script:invoke + $script:runner) | Should -Not -Match '=\s*if\s*\([^)]*\)\s*\{\s*@\('
    }
    It 'takes the injection method from the matrix, not from the scenario' {
        $script:invoke | Should -Match '\$matrix\.input\.method'
        $script:invoke | Should -Not -Match '\$scenario\.inputMethod'
    }
    It 'knows every documented method' {
        foreach ($m in 'scancode', 'vk', 'postmessage', 'activate-scancode') { $script:runner | Should -Match "'$m'" }
    }
    It 'records focus, SendInput results and a screenshot after each key' {
        foreach ($k in 'foregroundAtSend', 'sendInput', 'afterFile', 'inputDesktopAccessible', 'hung') { $script:runner | Should -Match $k }
    }
    It 'records the harness revision in the summary' { $script:invoke | Should -Match 'harnessRevision' }
    It 'captures the game window itself, not only the screen (a fullscreen surface can look frozen)' {
        $script:runner | Should -Match 'PrintWindow\(\$Window, \$hdc, 2\)'
        $script:runner | Should -Match 'windowCapture'
    }
    It 'collects logs the game or wrapper wrote during the case' { $script:runner | Should -Match 'game-logs' }
}
