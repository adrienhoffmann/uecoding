<#
Sets the repository to use .githooks directory for Git hooks and installs a default pre-commit hook.
Run from repo root: powershell -ExecutionPolicy Bypass -File .\scripts\backup\install-hooks.ps1
#>

$ErrorActionPreference = 'Stop'

$gitRoot = (git rev-parse --show-toplevel 2>$null)
if (-not $gitRoot) { Write-Error "Not inside a git repository"; exit 1 }
Push-Location $gitRoot

# Create .githooks directory if needed
$hooksPath = "$gitRoot\.githooks"
if (-not (Test-Path $hooksPath)) { New-Item -ItemType Directory -Path $hooksPath | Out-Null }

# Copy template hook if not exists
$preCommitTemplate = Join-Path $PSScriptRoot "pre-commit.ps1"
$targetPreCommit = Join-Path $hooksPath "pre-commit.ps1"
if (Test-Path $preCommitTemplate -and -not (Test-Path $targetPreCommit)) {
    Copy-Item -Path $preCommitTemplate -Destination $targetPreCommit
    Write-Host "Installed pre-commit hook script to .githooks/pre-commit.ps1"
}

# Configure Git to use .githooks
git config core.hooksPath ".githooks"
Write-Host "Git hooks path set to .githooks (core.hooksPath)"

Pop-Location
exit 0
