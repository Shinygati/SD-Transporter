# Android storage and profile checks

Run `python tests/run_store_tests.py` from the Android source directory with
JDK 17 or later on PATH (or JAVA_HOME set). No Android SDK or retail data is
needed for these host checks. The runner compiles the production WalkerStore,
WalkerProfiles and WalkerSync classes with small Android framework test doubles.

The cases cover legacy journals, immutable history, failed publication,
completed/deleted-session replay, stored-return receipts, stale worker epochs,
ten histories per walker, profile migration and separate journals, trainer
bindings, physical/timer credit for all active profiles, pending-return and
deletion isolation, selected-profile persistence and failed catalogue writes.

The normal application build also compiles the actual Android service/UI and
native bridge. Host checks do not emulate Android sensors or lifecycle delivery.
