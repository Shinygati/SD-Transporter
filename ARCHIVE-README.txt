Complete SD Transporter source and compiled snapshot.

SD Transporter companion 1.4 - source and compiled snapshot

Copy the contents of SD/ over the root of your working card. All three TWiLight
theme loaders and the matching existing modified bootstrap are included.
Install android/Pokewalker-Android.apk over your current same-key Android app.
Update the console and phone companions together for trainer binding checks.
Read howtouse.txt for installation, walking, activities, sync and recovery.

Walker profiles: add/select/rename profiles on the phone. All active profiles
earn steps. Journals, progress, trainer bindings and recovery folders are separate.
The existing single walker remains Walker 1. Profile receives check trainer
name and public Trainer ID; identical name/ID pairs are treated as one trainer.
Session checks continue to protect returns to the original handoff.

Recovery: file list SELECT or Settings X opens histories. Y changes walker,
L/R selects a snapshot. START opens Restore selected backup; A confirms it.
SELECT inside Recovery opens missing-file candidates. Current progress is
preserved first and an outstanding phone lease is revoked. Restore old states
only when needed: they may duplicate Pokemon already received in HGSS.

Return confirmation requires the final .walker to exist and verify after lock
cleanup. A receipt alone never confirms a missing/empty file. NDS storage uses
direct media access for its save checks. Ten recovery snapshots per walker are
retained; active/in-flight/ownership files are protected separately.

Phone battle displays use original walker-supplied two-frame sprites with
recreated attack/evade/catch timing. Stored catches commit before the animation.
Android Console Wi-Fi sync remains below Settings; scroll down to access it.
3DS recovery dates use the native SD timestamp API.

Extensive testing has been performed. Use at your own risk and keep independent
save/walker backups. iPhone is SOURCE ONLY: an Apple SDK build is required for
an IPA. No retail ROMs, saves, personal walkers, signing keys or SDKs are included.

See README.md, howtouse.txt, BUILDING-WINDOWS.md and CREDITS.md.
