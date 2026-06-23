#!/bin/bash
# Build Android APK for DawApp
# Usage: ./build-android-apk.sh <path-to-ndk> <path-to-sdk>

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
JUCE_DIR="$SCRIPT_DIR/../../.."
BUILD_DIR="$SCRIPT_DIR/build-android"
OUTPUT_DIR="$SCRIPT_DIR/android-apk"

NDK_PATH="${1:-$ANDROID_NDK_HOME}"
SDK_PATH="${2:-$ANDROID_HOME}"

if [ -z "$NDK_PATH" ] || [ ! -d "$NDK_PATH" ]; then
    echo "Error: NDK not found at '$NDK_PATH'. Set ANDROID_NDK_HOME or pass as first argument."
    exit 1
fi

if [ -z "$SDK_PATH" ] || [ ! -d "$SDK_PATH" ]; then
    echo "Error: SDK not found at '$SDK_PATH'. Set ANDROID_HOME or pass as second argument."
    exit 1
fi

echo "=== Step 1: Build native .so with CMake ==="
export ANDROID_NDK="$NDK_PATH"
export ANDROID_NDK_HOME="$NDK_PATH"

cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=Android \
    -DCMAKE_ANDROID_NDK="$NDK_PATH" \
    -DANDROID_NDK="$NDK_PATH" \
    -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a \
    -DCMAKE_ANDROID_STL_TYPE=c++_shared \
    -DCMAKE_ANDROID_API=30

cmake --build "$BUILD_DIR" --target DawAppExample -j"$(nproc)"

echo "=== Step 2: Create Android Gradle project ==="
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR/app/src/main/java/com/rmsl/juce"
mkdir -p "$OUTPUT_DIR/app/src/main/jniLibs/arm64-v8a"
mkdir -p "$OUTPUT_DIR/app/src/main/res/values"
mkdir -p "$OUTPUT_DIR/gradle/wrapper"

# Copy the native library (renamed to juce_jni for the Java loader)
# CMake outputs to <target>_artefacts/<config>/lib<output_name>.so
cp "$BUILD_DIR/DawAppExample_artefacts/Release/libjuce_jni.so" "$OUTPUT_DIR/app/src/main/jniLibs/arm64-v8a/libjuce_jni.so"

# Copy the C++ shared library (required by c++_shared STL)
CPP_SHARED=$(find "$NDK_PATH" -path "*/aarch64-linux-android/libc++_shared.so" 2>/dev/null | head -1)
if [ -n "$CPP_SHARED" ]; then
    cp "$CPP_SHARED" "$OUTPUT_DIR/app/src/main/jniLibs/arm64-v8a/"
fi

# Copy required Java files from JUCE modules
# The module Java dirs use app/ or init/ prefixes that we strip
copy_java() {
    local src_dir="$JUCE_DIR/modules/$1"
    local dst="$OUTPUT_DIR/app/src/main/java"
    if [ -d "$src_dir" ]; then
        find "$src_dir" -name "*.java" -type f | while read -r f; do
            local rel="${f#$src_dir/}"
            rel="${rel#app/}"
            rel="${rel#init/}"
            local target="$dst/$rel"
            mkdir -p "$(dirname "$target")"
            cp "$f" "$target"
        done
    fi
}

# Core files (always needed)
copy_java "juce_core/native/javacore"
copy_java "juce_core/native/java"
# GUI basics
copy_java "juce_gui_basics/native/java"
copy_java "juce_gui_basics/native/javaopt"
# Audio devices
copy_java "juce_audio_devices/native/java"
copy_java "juce_audio_devices/native/javaopt"

# Generate AndroidManifest.xml
cat > "$OUTPUT_DIR/app/src/main/AndroidManifest.xml" << 'MANIFEST'
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.dawapp.example">

    <uses-permission android:name="android.permission.INTERNET" />
    <uses-permission android:name="android.permission.MODIFY_AUDIO_SETTINGS" />
    <uses-permission android:name="android.permission.RECORD_AUDIO" />
    <uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />
    <uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />
    <uses-permission android:name="android.permission.READ_MEDIA_AUDIO" />
    <uses-permission android:name="android.permission.BLUETOOTH" />

    <application
        android:name="com.rmsl.juce.JuceApp"
        android:label="Music Theory DAW"
        android:icon="@mipmap/ic_launcher"
        android:allowBackup="false"
        android:hardwareAccelerated="true"
        android:largeHeap="true"
        android:supportsRtl="true"
        android:theme="@style/JuceTheme">

        <activity
            android:name="com.rmsl.juce.JuceActivity"
            android:configChanges="orientation|screenSize|screenLayout|keyboardHidden"
            android:exported="true"
            android:hardwareAccelerated="true"
            android:launchMode="singleTask"
            android:screenOrientation="userLandscape"
            android:windowSoftInputMode="adjustResize">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>
</manifest>
MANIFEST

# Generate theme/style resources
cat > "$OUTPUT_DIR/app/src/main/res/values/styles.xml" << 'STYLES'
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <style name="JuceTheme" parent="@android:style/Theme.DeviceDefault.NoActionBar">
        <item name="android:windowBackground">@android:color/black</item>
    </style>
</resources>
STYLES

# Generate launcher icon (simple placeholder)
mkdir -p "$OUTPUT_DIR/app/src/main/res/mipmap-hdpi"
mkdir -p "$OUTPUT_DIR/app/src/main/res/mipmap-mdpi"
mkdir -p "$OUTPUT_DIR/app/src/main/res/mipmap-xhdpi"
mkdir -p "$OUTPUT_DIR/app/src/main/res/mipmap-xxhdpi"
mkdir -p "$OUTPUT_DIR/app/src/main/res/mipmap-xxxhdpi"

# Generate a simple app icon (a colored rectangle)
create_png() {
    local file="$1"
    local size="$2"
    # Use Python to create a simple icon PNG
    python3 -c "
import struct, zlib
def create_png(path, w, h, r, g, b):
    def chunk(ct, data):
        c = ct + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    header = b'\\x89PNG\\r\\n\\x1a\\n'
    ihdr = chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    raw = b''
    for y in range(h):
        raw += b'\\x00'
        for x in range(w):
            raw += bytes([r, g, b])
    idat = chunk(b'IDAT', zlib.compress(raw))
    iend = chunk(b'IEND', b'')
    with open(path, 'wb') as f:
        f.write(header + ihdr + idat + iend)
create_png('$file', $size, $size, 102, 126, 234)
"
}

create_png "$OUTPUT_DIR/app/src/main/res/mipmap-mdpi/ic_launcher.png" 48
create_png "$OUTPUT_DIR/app/src/main/res/mipmap-hdpi/ic_launcher.png" 72
create_png "$OUTPUT_DIR/app/src/main/res/mipmap-xhdpi/ic_launcher.png" 96
create_png "$OUTPUT_DIR/app/src/main/res/mipmap-xxhdpi/ic_launcher.png" 144
create_png "$OUTPUT_DIR/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png" 192

# Generate build.gradle (project-level)
cat > "$OUTPUT_DIR/build.gradle" << 'BUILD'
buildscript {
    repositories {
        google()
        mavenCentral()
    }
    dependencies {
        classpath 'com.android.tools.build:gradle:8.7.3'
    }
}

allprojects {
    repositories {
        google()
        mavenCentral()
    }
}
BUILD

# Generate settings.gradle
cat > "$OUTPUT_DIR/settings.gradle" << 'SETTINGS'
plugins {
    id 'org.gradle.toolchains.foojay-resolver-convention' version '0.9.0'
}
rootProject.name = 'DawApp'
include ':app'
SETTINGS

# Generate gradle.properties
cat > "$OUTPUT_DIR/gradle.properties" << 'PROPS'
org.gradle.jvmargs=-Xmx4096m -XX:MaxMetaspaceSize=512m
android.useAndroidX=true
android.enableJetifier=true
PROPS

# Generate app/build.gradle
cat > "$OUTPUT_DIR/app/build.gradle" << 'APPBUILD'
apply plugin: 'com.android.application'

java.toolchain.languageVersion = JavaLanguageVersion.of(17)

android {
    compileSdk 35
    namespace = "com.dawapp.example"

    defaultConfig {
        applicationId "com.dawapp.example"
        minSdkVersion 24
        targetSdkVersion 35
        versionCode 1
        versionName "1.0"
    }

    buildTypes {
        debug {
            debuggable true
        }
    }

    sourceSets {
        main {
            jniLibs.srcDirs = ['src/main/jniLibs']
            java.srcDirs = ['src/main/java']
        }
    }
}
APPBUILD

# Generate Gradle wrapper properties
cat > "$OUTPUT_DIR/gradle/wrapper/gradle-wrapper.properties" << 'WRAPPER'
distributionBase=GRADLE_USER_HOME
distributionPath=wrapper/dists
distributionUrl=https\://services.gradle.org/distributions/gradle-8.11.1-bin.zip
zipStoreBase=GRADLE_USER_HOME
zipStorePath=wrapper/dists
WRAPPER

echo "=== Step 3: Build APK ==="
cd "$OUTPUT_DIR"

# Download gradle wrapper if not present
if [ ! -f "gradlew" ]; then
    gradle wrapper --gradle-version=8.11.1 2>/dev/null || true
fi
if [ ! -f "gradlew" ]; then
    # Download gradle wrapper manually
    curl -sL "https://services.gradle.org/distributions/gradle-8.11.1-bin.zip" -o /tmp/gradle.zip
    unzip -q /tmp/gradle.zip -d /tmp/gradle-extract
    /tmp/gradle-extract/gradle-8.11.1/bin/gradle wrapper --gradle-version=8.11.1
    rm -rf /tmp/gradle.zip /tmp/gradle-extract
fi

# Build the APK
export ANDROID_HOME="$SDK_PATH"
export ANDROID_SDK_ROOT="$SDK_PATH"
./gradlew assembleDebug

echo "=== Done! ==="
APK_PATH=$(find "$OUTPUT_DIR/app/build/outputs/apk" -name "*.apk" 2>/dev/null | head -1)
if [ -n "$APK_PATH" ]; then
    echo "APK built at: $APK_PATH"
    ls -lh "$APK_PATH"
fi
