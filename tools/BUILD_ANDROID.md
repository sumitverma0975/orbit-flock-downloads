# Building the Android release

Use Godot **4.6.3** and its matching Android export templates, JDK 17, and Android SDK platform-tools plus build-tools **36.0.0**. Configure the JDK and Android SDK directories in Godot's Editor Settings under Export → Android. The standard APK template export does not require Gradle or an NDK.

Create a release keystore outside the project, or reuse the private key for an existing release. Keep the keystore and its password out of Git and source archives. Android updates must use the same signing key as the installed release.

Set `JAVA_HOME` and `ANDROID_HOME`, then provide these signing environment variables through your local environment or your CI secret store:

- `GODOT_ANDROID_KEYSTORE_RELEASE_PATH`: absolute path to the private `.keystore` or `.jks` file
- `GODOT_ANDROID_KEYSTORE_RELEASE_USER`: key alias
- `GODOT_ANDROID_KEYSTORE_RELEASE_PASSWORD`: keystore password

Run `bash tools/build_android.sh` from a shell with those settings. The script imports project assets, exports a signed release, verifies its APK signature and validates the package and native libraries. Outputs are `build/DelhiDrive-Mobile.apk`, its SHA256 checksum, and a JSON verification report. You can pass a different APK output path as the first argument. `GODOT_BIN` overrides the Godot executable; `ANDROID_BUILD_TOOLS_VERSION` overrides build-tools 36.0.0.

The release contains ARM64 and ARMv7 libraries, uses package `com.sumit.delhidrive.mobile`, and targets the prebuilt Godot template's Android SDK levels. It requests no Internet, microphone or external-storage permissions. Saved career data stays in the app's own storage. Signature/archive checks do not replace gameplay testing on an Android phone.
