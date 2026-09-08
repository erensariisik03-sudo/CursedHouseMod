# Android NDK ARMv7 GitHub Template

This is a clean Android Studio/Gradle + CMake project that builds a JNI shared library for `armeabi-v7a`.

## Build locally

```bash
./gradlew assembleDebug
```

The native library is produced under the app's build intermediates/outputs for `armeabi-v7a`.

## GitHub Actions

Push the folder to a GitHub repository. The workflow in `.github/workflows/build.yml` builds the project and uploads the debug APK as an artifact.

## Important

The template intentionally contains no code for injecting into or modifying a third-party game's `libil2cpp.so`, bypassing anti-cheat/anti-ban protections, forcing multiplayer authority, or altering another app's runtime behavior.
