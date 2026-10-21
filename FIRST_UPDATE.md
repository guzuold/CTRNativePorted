# Updating to v0.5.6 / Aggiornamento a v0.5.6

## With Check updates / Con Check updates

If your launcher has Check updates (including the refreshed v0.4.0), use it to download and install v0.5.6. Close the game and disable Multiplayer Online before installing. The updater closes the launcher, replaces program files and restarts it. Keep launcher.exe, launcher_update.ps1 and update-version.json together at the installation root.

Se hai Check updates, cerca e installa v0.5.6 dal launcher. Prima chiudi il gioco e disattiva Multiplayer Online. Il launcher si chiude per l'installazione e si riavvia al termine.

## First manual update / Primo aggiornamento manuale

For an older launcher without Check updates:
1. Download CTRNativePorted-v0.5.6-win64.zip from the GitHub release.
2. Close the game, launcher and any standalone online server. Back up runtime/user.
3. Extract into the existing installation folder and replace program files, including build/CTRPort-gpu/ctr_hle.dll. Do not create an extra nested folder.
4. Preserve runtime/user, your game dump and mods. This ZIP does not contain or replace your identity, friend list, saves or settings.
5. Run launcher.exe and confirm Launcher 0.5.6 and Check updates. Upgrade both PCs before testing.

Se il vecchio launcher non ha Check updates, scarica lo ZIP v0.5.6 e installalo manualmente una volta. Chiudi gioco, launcher e server standalone, fai un backup di runtime/user, quindi estrai nella cartella esistente sostituendo i file del programma. Conserva runtime/user, dump e mod. Avvia launcher.exe e verifica Launcher 0.5.6. Aggiorna entrambi i PC.

## Test and logs / Prova e log

Test presence first with both launchers online and games closed, then after restarting one launcher. Start both games and try one invitation. If acceptance fails, note the displayed error; Retry repeats the request, Cancel stops automatic prompts for that invitation. A click on Yes alone does not confirm party membership.

I log si trovano in runtime/user/logs. Per indagare un problema servono gli online_server_*.jsonl e i log stdout/stderr del gioco relativi allo stesso tentativo, su entrambi i PC.