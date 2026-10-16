# Local backups

The launcher automatically copies game saves before launch and after the game
exits into `runtime/user/savedata_backups/`. A failed initial backup cancels launch.
Folders ending in `.partial` are incomplete and must not be used for recovery.

Pit Stop inventories and coin balances are still owned by the selected lobby host.
The launcher saves signed copies of your account on joining, every 15 seconds
while the lobby connection is available, and after game exit:

`runtime/user/online/backups/hosts/<host hash>/<account>/`

The host must also run an updated launcher. An older or unreachable host cannot
provide these copies. Identical snapshots are stored once; an empty inventory never
overwrites an older snapshot. Abrupt disconnection can lose changes since the last
successful copy. These files do not make the wallet automatically follow you to a
different host. Restore only from the intended account, host and time; never add
coin balances from different snapshots.

The local server also archives each existing wallet before replacing it, under
`runtime/user/online/backups/local/`. If this backup fails, the wallet update fails
and the previous state is retained. Backups are not deleted automatically.

To recover game saves, close the game, preserve the current `savedata` folder, then
copy the contents of the selected completed save backup into it. Pit Stop recovery
requires selecting and validating the correct wallet snapshot separately; copying
game saves alone does not restore server-held purchases.

The command-line `play_gpu.py` runner makes the same pre/post-run game-save copies.
Remote wallet polling requires the launcher. Keep the launcher open while playing.
