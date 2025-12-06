<#
Simple daily backup script for local git repo.
Saves current working changes into a timestamped branch and pushes it to origin.
Usage: .\git-backup-daily.ps1 [-Message "Optional note"]
#>
param(
    [string]$Message = "Daily backup",
    [switch]$SkipIfNoChanges = $true
)

# Abort on error
$ErrorActionPreference = "Stop"

Write-Host "Running Git backup script..."

# Ensure we're in a git repo root (or find the root)
$gitRoot = (git rev-parse --show-toplevel 2>$null)
if (-not $gitRoot)
{
    Write-Error "Not in a git repository. Please run this script from inside a repository."; exit 1
}

Push-Location $gitRoot

try {
    # Fetch latest from origin
    Write-Host "Fetching origin..."
    git fetch origin

    # Get current branch
    $currentBranch = (git rev-parse --abbrev-ref HEAD).Trim()
    Write-Host "Current branch: $currentBranch"

    # See if there are working tree changes
    $status = (git status --porcelain)
    if ($status -eq "" -and $SkipIfNoChanges)
    {
        Write-Host "No working changes detected; nothing to snapshot."; exit 0
    }

    # Add anything and make a WIP commit
    git add -A

    $date = Get-Date -Format yyyyMMdd_HHmmss
    $safeBranch = "backup/daily-$date"
    $commitMsg = "$Message ($date)"

    # Create temporary commit if there are staged changes
    git commit -m $commitMsg

    # Create backup branch from current commit
    git branch $safeBranch

    # Push the backup branch to origin
    Write-Host "Pushing $safeBranch to origin..."
    git push -u origin $safeBranch

    Write-Host "Backup completed: $safeBranch pushed"
}
catch {
    Write-Error "Backup failed: $_"
}
finally {
    Pop-Location
}

exit 0
