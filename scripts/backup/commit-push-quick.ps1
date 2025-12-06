<#
Quick helper to stage all and commit with message then push current branch.
#>
param(
    [string]$Message = "WIP - quick commit"
)

$ErrorActionPreference = 'Stop'
$gitRoot = (git rev-parse --show-toplevel 2>$null)
if (-not $gitRoot) { Write-Error "Not a git repo"; exit 1 }
Push-Location $gitRoot

# Stage and commit
git add -A
if (-not git diff --cached --quiet) {
    git commit -m $Message
    git push
    Write-Host "Committed and pushed: $Message"
} else {
    Write-Host "No staged changes to commit."
}

Pop-Location
exit 0
