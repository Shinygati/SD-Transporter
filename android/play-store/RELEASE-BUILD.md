# Android release bundle — Windows and Linux

The normal Gradle project is `android/`. The private rapid-build script produces a test APK; it does not prove a Play release bundle builds. These instructions need a real release build and device verification before submission.

Install JDK 17, Android SDK command-line tools or Android Studio, platform 36, current platform/build tools, NDK `29.0.14206865`, CMake and a Gradle version compatible with the Android Gradle Plugin in `android/build.gradle`. Set `ANDROID_HOME` to the SDK. Keep `shared/` beside `android/`, because the native bridge uses it. The repository currently has no Gradle wrapper; install a matching Gradle distribution or create and review a wrapper before relying on `gradlew`.

On Windows PowerShell, from `android/`:

```powershell
gradle --version
gradle clean :app:bundleRelease
```

On Linux, from `android/`:

```sh
gradle --version
gradle clean :app:bundleRelease
```

Expected unsigned release bundle: `android/app/build/outputs/bundle/release/app-release.aab`. Release signing is not configured in the source. Do not describe an unsigned output as ready to upload.

## Signing

Create a permanent upload key outside the checkout using JDK `keytool`, back it up securely and enrol in Play App Signing when the owner is ready. Sign the bundle through Android Studio's Generate Signed Bundle flow or a reviewed local Gradle signing configuration that reads secrets outside source control. Keep keystores, passwords and signing properties out of source ZIPs, logs and Git.

Do not change the existing installed test APK signing identity during rapid testing. Play signing may be different: return the walker to the console and export recovery copies before uninstalling an incompatible test build. App-private data is removed on uninstall.

## Verify the actual artifact

Use official `bundletool` to validate the AAB, generate/install APKs for a test device and inspect their manifests/ABIs. Confirm API 36, only required permissions, 16 KiB native segment alignment and ZIP alignment, release signing, privacy assets and source/license availability. Check that no ROM, private walker, test keystore or debug fixture has entered the bundle.

Increment versionCode for each uploaded build. Select a stable release versionName only after tests pass. Complete the transport, rights and policy requirements in README.md before requesting a public review.

Official documentation: [App bundles](https://developer.android.com/guide/app-bundle), [Signing](https://developer.android.com/studio/publish/app-signing), [16 KiB pages](https://developer.android.com/guide/practices/page-sizes), [bundletool](https://developer.android.com/tools/bundletool).
