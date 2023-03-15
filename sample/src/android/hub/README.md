This is the root folder of Android build. Open this folder in Android Studio then build as usual.

Build from command line is also supported. To be able to do that, you'll need to:
  - Install NDK via Android Studio GUI. (...or via sdkmanager script, if you know how)
  - set `JAVA_HOME` and `ANDROID_SDK_ROOT` environment variable to point to your JRE and Android SDK installation.

Now you should be able to invoke the `gradlew` script in this folder to build. Run `./gradlew tasks` to see all available tasks.

We also created a few convenient scripts to make running those most commonly used tasks easier:

- `clean.sh/bat`  : Clears all existing build files to ensure your next build has a fresh start.
- `compile.sh/bat`: Build debug flavor. Use "-r" to build release flavor.
- `launch.py/bat` : Deploy the debug build (or "-r" for release build) APK to your connected Android device, then launch it.