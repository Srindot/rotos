# JNI: rotos_jni.c binds Java_dev_rotos_app_nav_NativeNav_* by name.
-keep class dev.rotos.app.nav.NativeNav { *; }
-keepclasseswithmembernames class * { native <methods>; }
# Stack traces in the status log stay readable.
-keepattributes SourceFile,LineNumberTable
-renamesourcefileattribute SourceFile
