# Release and recovery test plan

Use synthetic data or copies, never the sole copy of a valued walker. Record device, Android version, companion version, build hash and outcome. Keep the original supplied walker unchanged.

## Ownership and interruption

1. Console A → Android → console A: matching Pokémon, route, steps, watts and pack; console locked only while phone owns it.
2. Interrupt GET before data, midway, after body and before local journal commit; retry the same pending session. No second active phone session, no silent overwrite.
3. Interrupt POST before body, midway, after console commit and before response; restart both apps and retry. One effective return, matching CRC, no repeated overwrite.
4. After a completed return, make newer console progress in RAM and replay the old POST. It should acknowledge without reloading the old disk file or discarding newer RAM.
5. Force-recover, create another handoff, then replay the first revoked token. It must remain revoked.
6. While Android owns walker A, reset HGSS and produce walker B on the console. Neither a new GET nor returning A may silently replace B. Preserve both recovery copies and give a clear conflict result.
7. Fill storage / deny writes during snapshot and journal updates. No successful ownership transition without required persisted data. Verify existing backups are unchanged.
8. Damage an existing snapshot with the expected name. Refuse to overwrite it; preserve the current live file.
9. Export histories through the system document picker. Exports do not silently reactivate revoked sessions.

## Walking and UI

- Physical mode: permission grant/deny, unavailable sensor, screen off, reboot, service restart, cumulative counter reset and vendor battery restrictions.
- Timer: default off, one step per ten seconds when enabled, mode changes without double credit, Pause from notification, returnPending prevents steps/games/settings mutation.
- Radar: visible grass, accessible focus/touch targets, timely rustles, spending, escape/catch, full pack replacement/cancel.
- Dowsing: visible grass, two guesses, successful/failed search and full item pack replacement/cancel.
- Font scaling, TalkBack labels, landscape, small display, contrast and privacy/credits/licence navigation.
- Android restart reads both previous-version journal and new pending-return journal.

## Packaging and policy evidence

- Release AAB signed with the real upload key; generated split APK installation, 16 KiB page compatibility, arm64 and other retained ABIs.
- Manifest/permission review, dependency and native license notices, no retail/private files or signing keys.
- Public privacy URL opens without sign-in and matches code; Data Safety accurately describes local unencrypted transport until replaced.
- Foreground-service demonstration video: user enables mode, notification appears, steps accrue, user presses Pause and service stops. Explain `health` for sensor walking and `specialUse` for optional virtual timer; Google decides whether the requested type is accepted.
- Real DS Lite legacy Wi-Fi, DSi and 3DS sync, including reconnects, packet loss, Android background/foreground and simultaneous connection attempts.
- Closed-test feedback and Play pre-launch report triaged; owner approves final rights, screenshots, forms and publication policy.

## DS Lite Wi-Fi scan regression

Attempt Wi-Fi setup while a transfer peer is active and while a walker is phone-owned: setup must be refused. The scan screen must never poll/accept a handoff listener. Interrupt a return before the stored-file checksum response; Android must retain its pending copy, including after restart. Inject a rename that reports success but leaves an empty final file: the server must refuse success. Update both apps and retry the same session from Phone sync. Test empty registered checkout, automatic companion joining on the phone and returning that expected companion; reject a different trainer/course/companion without replacing either file.
