<#
Simple pre-commit hook: ensures you didn't forget to run some checks.
This sample hook writes the list of files you're committing and prevents empty commit messages.
#>
param()

# safest path
$ErrorActionPreference = 'Stop'

# Provide some trace
Write-Host "[pre-commit] Running simple checks..."

# If we are not in git repo root, find it
$gitRoot = (git rev-parse --show-toplevel 2>$null)
if (-not $gitRoot) { Write-Host "[pre-commit] Not a git repo?"; exit 0 }

Push-Location $gitRoot

# Ensure commit message is non-empty for normal commit
# We can't read the pending commit message easily in script; let's just ensure there are staged files
$staged = git diff --cached --name-only
if (-not $staged) {
    Write-Host "[pre-commit] No files are staged; aborting commit."; exit 1
}

Write-Host "[pre-commit] Staged files:"
Write-Host $staged

# Example check: ensure no 'TODO' remains in committed files
$bad = $false
foreach ($file in $staged) {
    if (Test-Path $file) {
        $content = Get-Content $file -ErrorAction SilentlyContinue
        if ($null -ne $content) {
            if ($content -join "`n" | Select-String -Pattern "\bTODO\b") {
                Write-Host "[pre-commit] Found TODO in $file"; $bad = $true
            }
        }
    }
}

if ($bad) {
    Write-Host "[pre-commit] Please remove TODO comments before committing (or ignore in urgent cases)"; exit 1
}

Pop-Location

exit 0
