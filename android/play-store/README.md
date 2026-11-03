# Google Play preparation

Reviewed 3 November 2026. Developer: **Shinygati**. Public support and privacy contact: **anthoney1313@gmail.com**.

These are preparation documents, not evidence of Google approval. Nothing has been submitted or published. The repository's educational, source-build policy remains in effect until the owner deliberately changes it.

## Work included

- Target/compile API 36, minimum API 26; native arm64, ARMv7 and x86-64 builds use 16 KiB page alignment.
- Explicit optional physical-step permission explanation, optional timer, visible tracking notification with a Pause action.
- Local privacy, credits and GPL pages available in Settings, without a network request.
- First-sync disclosure of trainer/game/step data and the current unencrypted local Wi-Fi transport.
- Private recovery snapshots before replacing or returning a walker; export through Android's document picker.
- A pending return stays locked across process restarts until the console acknowledges it. No accounts, ads, analytics, crash-upload service or developer-operated sync server.

## Open production requirements

1. **Transport protection.** The current console protocol uses HTTP on port 30820. The session token is sent over that connection, so it does not provide encryption or protect against an observer on the local network. It transfers trainer information and step totals. Do not answer “all data encrypted in transit” in Data Safety. Assess and implement an authenticated encrypted transport compatible with the DS/3DS before requesting production review; a LAN-only disclaimer does not itself satisfy Google's sensitive-data policy.
2. **Rights and branding.** Review the app title, icon and listing with the owner and establish rights for any third-party material used. Do not publish the supplied real walker, retail artwork, game screenshots, ROMs or extracted Download Play clients. A fan-project disclaimer is not permission from a rights holder. Use original demonstration graphics and synthetic data for store materials.
3. **Release signing and bundle.** Establish a permanent upload key and Play App Signing, build a release AAB, inspect its generated APKs and test them. The current APK is a private test build. Never upload its private test keystore or a signing password to this repository. See `RELEASE-BUILD.md`.
4. **Public privacy URL.** Host `privacy.html` at a stable, public HTTPS URL. This copy has not been hosted. Keep it consistent with the actual shipping code and Data Safety answers.
5. **Account and review forms.** Complete the real account identity, app access, target audience, content rating, health/fitness, Data Safety and foreground-service declarations in Play Console. No answers have been submitted for the owner. Do not claim a medical or fitness benefit; steps drive a virtual-game companion.
6. **Review access.** Supply a legal synthetic demonstration walker and precise reviewer instructions, or an approved demonstration mode. Reviewers need to reach Radar, Dowsing, steps and sync without obtaining a copyrighted game or save. No test walker is bundled with this build.
7. **Device testing.** Run `TEST-PLAN.md` on Android devices and actual DS Lite/DSi/3DS consoles. CPU fault injection and a successful APK build do not replace radio, sleep, permissions or Play pre-launch testing.
8. **Testing track.** If the account is a personal developer account created after 13 November 2023, the current rule requires at least 12 testers opted in continuously for 14 days before applying for production access. Confirm which account rules apply; the account type has not been supplied.

## Official references

- [Target API rules](https://developer.android.com/google/play/requirements/target-sdk): new apps and updates require API 36 from 31 August 2026.
- [Data Safety definitions](https://support.google.com/googleplay/android-developer/answer/10787469): off-device transfer and encryption declarations require an accurate inventory, even without a developer server.
- [User data policy](https://support.google.com/googleplay/android-developer/answer/10144311): check transport, prominent disclosure and consent against the final implementation.
- [Intellectual property](https://support.google.com/googleplay/android-developer/answer/9888072).
- [Personal account testing](https://support.google.com/googleplay/android-developer/answer/14151465).
- [Foreground services](https://support.google.com/googleplay/android-developer/answer/18265346).

Policies can change. Recheck the official pages before submission.
