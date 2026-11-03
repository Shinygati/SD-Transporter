# NDS companion checks

Build the app with `python build.py`, then install the optional host test dependency
with `python -m pip install unicorn` and run `python tests/test_native.py`.
`DEVKITPRO` and `DEVKITARM` select the same SDK/compiler used by the build.

The test executes the compiled ARM Wi-Fi setup and renderer. Wireless services,
network scan results, filesystem calls and time are simulated. It checks DS Lite
hardware gating, asynchronous scans, WPA refusal, open-network selection,
40/104/128-bit WEP key parsing, profile save/reload/corruption fallback, timeout,
cancellation and redraws after menu changes. Hidden-network checks cover blank SSID results, the printable-ASCII editor (including spaces/case/symbols), empty-name rejection, the 32-byte limit, exact names in association/saved profiles, directed scans and cancellation. It also confirms unchanged idle
screens are reused. No real network, ROM, save or walker file is required.

This does not verify radio behavior or frame timing on DS hardware. Test those
on the intended DS Lite, flashcart and access point.

Run `python tests/test_transport.py` for the native socket regression checks. It
runs the Wi-Fi/UI suite first, then executes the installed DS `accept()` and
`ioctl()` code with simulated lower-level TCP connections. It reproduces the
legacy null-address rejection, checks that the server accepts a queued Android
connection using valid output buffers, and verifies that NDS peers/listeners use
`closesocket()` rather than the filesystem `close()` API.
