# Repository Workflow

This repository is the source of truth for the WoW server code. Do not make code
changes directly in the live server tree except for emergency rollback.

## Required Change Flow

1. Make source changes locally in this repository.
2. Build or syntax-check locally when practical.
3. Commit the local changes.
4. Push the branch to GitHub:

```bash
git push -u origin playerbot-ike3-upgrade
```

5. Sync the live server from GitHub by pulling on the server checkout.
6. Rebuild on the server.
7. Deploy the rebuilt binary only after a successful build.
8. Verify logs and process status after restart.

## Active Server Layout

- SSH alias: `wow-server`
- Active source: `/home/mangos/mangos/zero/server-ike3-upgrade-test`
- Active build: `/home/mangos/mangos/zero/build-ike3-upgrade-test`
- Runtime binaries: `/home/mangos/mangos/zero/bin`
- Logs: `/home/mangos/mangos/zero/logs`

## Server Sync Rule

Local pushes use the configured `origin` remote. The server source tree should be
a Git checkout of the read-only HTTPS URL:

```text
https://github.com/weilai1988/mangoszero-server.git
```

on branch:

```text
playerbot-ike3-upgrade
```

The normal server update command should be:

```bash
ssh wow-server 'sudo -n -u mangos git -C /home/mangos/mangos/zero/server-ike3-upgrade-test pull --ff-only origin playerbot-ike3-upgrade'
```

If the server checkout is dirty, stop and inspect the diff. Do not overwrite live
server changes until they have been copied back locally, reviewed, committed, and
pushed.

## Build And Deploy

Use the installed local helper for routine operations:

```bash
/Users/ashleyshuyaoliu/.codex/skills/wow-server-ops/scripts/wow-server.sh status
/Users/ashleyshuyaoliu/.codex/skills/wow-server-ops/scripts/wow-server.sh build
/Users/ashleyshuyaoliu/.codex/skills/wow-server-ops/scripts/wow-server.sh deploy-built
```

Before replacing `/home/mangos/mangos/zero/bin/mangosd`, keep the helper's
timestamped binary backup. After restart, check `world-server.log` for:

- `AI Playerbot initialized`
- `MaNGOS Server: World Initialization Complete`
- `World Updater Thread started`

## Database Safety

Database reads and writes should use the `wow-server-ops` helper. For writes,
show the SQL first, create a backup, apply the SQL, and validate with a read-only
query.

Never store or print DB credentials in this repository or in final notes.
