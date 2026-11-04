# Data inventory for owner review

This inventory describes the current source. It is not a submitted Data Safety form. Google treats transfer off the device as collection in its form definitions; a user-initiated transfer may have a sharing exception but is not automatically exempt from collection. Review the current official definitions and the final app behaviour before answering.

| Data | Where stored | Transmission | Purpose / control |
| --- | --- | --- | --- |
| Walker EEPROM, trainer name, Pokémon, items, route and game identifiers | Android private app journal and recovery copies | Entire 64 KiB walker to/from the selected console | Core game and sync; initiated by user |
| Physical step totals and sensor baseline | Private journal; resulting totals inside walker data | Included in the returned walker | Optional physical walking; permission and Pause control |
| Time-based step progress | Journal and walker | Included in the returned walker | Optional simulation; off by default |
| Console IP addresses | Private preferences / journal | Used to contact selected console | User-entered saved console list |
| Session token and pending-return flag | Private journal; console lease metadata | Token in local HTTP request/response headers | Ownership and retry protection |
| Recovery export | App-private history; user-selected document destination | Destination depends on user's document provider | Explicit user export; do not assume every document provider is local |
| Support email message, if user sends one | User's email app / mail provider | Only if user sends an email themselves | Developer support; no automatic report sent |

No accounts, passwords, purchase data, advertising IDs, device location, contacts, microphone, camera, advertising or analytics SDK are used by this source. No developer-operated sync or telemetry endpoint is configured. This does not justify answering “no data collected” when the app sends data to a console.

The Android manifest disables Android automatic app backup. Private journals and recovery files remain until the user removes them or clears/uninstalls the app. Uninstalling while a walker is on the phone can strand current progress; return and export it first. Console recovery revokes the old session and restores its earlier recovery point, not progress that existed only on a lost phone.

## Fields requiring a final policy decision

- Health and fitness: physical step information is handled. Describe it accurately even though it is used for a game rather than health advice.
- Personal information: trainer names may be aliases or identifying names; inspect the complete EEPROM inventory rather than assuming it is anonymous.
- App activity: game progress and interactions exist in the EEPROM.
- Sharing: evaluate Google's user-initiated transfer exception for the user's own selected console. Do not imply distribution to an advertising or analytics service.
- Encryption: **No** for the current raw HTTP console protocol. HTTPS support links do not encrypt walker transfers.
- Deletion: no developer server holds walker data. Local deletion/export and recovery retention must be described separately from any future account-deletion requirement.

Revisit every row if transport, telemetry, permissions, cloud sync or SDK dependencies change.


Version 1.5 adds foreground Bluetooth LE visits and friendly battles. Connected
phones exchange compact partner/trainer records, uploaded sprite/name bitmaps,
move data, step counters and activity messages. The full walker ownership image
is not transferred by Bluetooth. Local entry/reward receipts prevent repeated
charges and rewards. Discovery stops when leaving the nearby connection screen.
The current apps do not require authenticated Bluetooth pairing or link
encryption; do not claim that all data is encrypted in transit.
