# Backup Tools for this Repository

This folder contains scripts to make daily backups of the repository and install helpful git hooks. Follow the steps below to set up and use them.

## Recommended daily backup
Run the daily backup script (manually or via scheduled task):

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\backup\git-backup-daily.ps1 -Message "Daily snapshot"
```

The script will:
- `git fetch origin`
- Create a commit (if new changes exist), create a timestamped branch `backup/daily-YYYYMMDD_HHMMSS`, and push it to `origin`.

## Install Git hooks
Install the hook configuration to use `.githooks` folder for hooks:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\backup\install-hooks.ps1
```

This will copy the default `pre-commit.ps1` to `.githooks` and set `git config core.hooksPath .githooks`.

### pre-commit
The included `pre-commit.ps1` checks that:
- There is at least one staged file; otherwise it aborts the commit.
- It warns (blocks) if `TODO` tokens are present in changed files (so you don't accidentally commit TODO items).

You can customize or replace `.githooks/pre-commit.ps1` as needed.

## Schedule Daily Backups (Windows)
Use the register schedule script to add a Windows scheduled task that runs the backup.

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\backup\register-schedule.ps1 -Time "02:00"
```

This generates a `schtasks` entry called `GitDailyBackupCoding` that runs the backup at the specified time daily.

## Tips / Best Practices
- Commit and push often. The script is a safety layer, not a substitute for commits.
- If your project uses large binary assets, add Git LFS and back those up separately if needed.
- For CI: you can add a GitHub Actions/CI job to run a build on PRs to ensure the code compiles.
- Test the scripts locally before relying on scheduled backups.

## Troubleshooting
- Ensure PowerShell execution policy allows scripts, or run with `-ExecutionPolicy Bypass`.
- If the scheduled task fails to run, check `Task Scheduler` > `Task Status` and user account privileges.

## Checklist rapide (en français)
- Avant de fermer VSCode : commit + push (git add -A ; git commit -m "message" ; git push)
- Si tu dois quitter sans finir : `git stash` puis `git stash pop` le lendemain pour récupérer les changements
- Exécuter `.\scripts\backup\git-backup-daily.ps1` tous les soirs ou utiliser le scheduled task
- Installe le hook via : `powershell -ExecutionPolicy Bypass -File .\scripts\backup\install-hooks.ps1`

## Commandes rapides
- Snapshot (manuel) :
	```powershell
	powershell -ExecutionPolicy Bypass -File .\scripts\backup\git-backup-daily.ps1 -Message "Snapshot avant grosse modif"
	```
- Commit & Push rapide :
	```powershell
	powershell -ExecutionPolicy Bypass -File .\scripts\backup\commit-push-quick.ps1 -Message "Mes changements rapides"
	```

---
Si tu veux que j'ajoute un job de CI pour builder automatiquement (GitHub Actions/Azure Pipelines), dis-moi ton provider (GitHub/Azure/etc.) et je créerai un workflow minimal qui testera la compilation (attention, builds UE sur CI demandent de grosses ressources).

