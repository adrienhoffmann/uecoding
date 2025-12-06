<#
Register a scheduled Windows task to run the backup script daily at the given time.
Usage: ./register-schedule.ps1 -Time "02:00" (24h format)
#>
param(
    [string]$Time = "02:00"
)

$ErrorActionPreference = 'Stop'

$gitRoot = (git rev-parse --show-toplevel 2>$null)
if (-not $gitRoot) { Write-Error "Not a git repo"; exit 1 }

$taskName = "GitDailyBackupCoding"
$scriptFull = Join-Path $gitRoot "scripts\backup\git-backup-daily.ps1"
$escaped = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$scriptFull`" -Message 'Daily auto-backup'"

# Create scheduled task
Write-Host "Registering task $taskName to run daily at $Time"
# Use schtasks to create the task
schtasks /Create /SC DAILY /TN $taskName /TR $escaped /ST $Time /F

Write-Host "Task registered. Use: schtasks /Query /TN $taskName"; exit 0
