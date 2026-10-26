import java.util.Properties

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

// Google Maps / Routes API key: android/local.properties `MAPS_API_KEY=...` or the
// MAPS_API_KEY environment variable. Never committed; empty = OSRM + no map.
val mapsApiKey: String = run {
    val props = Properties()
    val f = rootProject.file("local.properties")
    if (f.exists()) f.inputStream().use { props.load(it) }
    (props.getProperty("MAPS_API_KEY") ?: System.getenv("MAPS_API_KEY") ?: "").trim()
}

// Release signing: android/keystore.properties (storeFile, storePassword, keyAlias,
// keyPassword), created by `android/scripts/build.sh release`. Never committed.
val releaseKeys: Properties? = rootProject.file("keystore.properties").takeIf { it.exists() }?.let { f ->
    Properties().also { p -> f.inputStream().use { p.load(it) } }
}

android {
    namespace = "dev.rotos.app"
    compileSdk = 34

    // Screens, layouts and theme live in the shared ui/ folder (ui/android).
    sourceSets["main"].java.srcDir("../../ui/android/java")
    sourceSets["main"].res.setSrcDirs(listOf("../../ui/android/res"))
    ndkVersion = "26.3.11579264"

    defaultConfig {
        applicationId = "dev.rotos.app"
        minSdk = 26
        targetSdk = 34
        versionCode = 5
        versionName = "3.0-beta"
        manifestPlaceholders["MAPS_API_KEY"] = mapsApiKey
        buildConfigField("String", "MAPS_API_KEY", "\"$mapsApiKey\"")
        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=none")
            }
        }
    }

    buildFeatures {
        buildConfig = true
    }

    signingConfigs {
        if (releaseKeys != null) {
            create("release") {
                storeFile = rootProject.file(releaseKeys.getProperty("storeFile"))
                storePassword = releaseKeys.getProperty("storePassword")
                keyAlias = releaseKeys.getProperty("keyAlias")
                keyPassword = releaseKeys.getProperty("keyPassword")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            signingConfig = signingConfigs.findByName("release")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
        allWarningsAsErrors = false
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("com.google.android.material:material:1.12.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.8.1")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.4")
    implementation("com.google.android.gms:play-services-maps:19.0.0")
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.json:json:20240303")          // the real org.json for JVM tests
}
