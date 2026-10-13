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
