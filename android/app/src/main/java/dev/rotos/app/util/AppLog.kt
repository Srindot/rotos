package dev.rotos.app.util

import android.content.Context
import android.util.Log
import java.io.File
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

/**
 * The app's own log: logcat (tag ROTOS) plus rotos.log in the app's external
 * files folder, kept across restarts (the phone's logcat buffer only holds
 * minutes), rotated at 1 MB to rotos.log.1. Readable over USB for release
 * builds too:
 *   adb shell cat /sdcard/Android/data/dev.rotos.app/files/rotos.log
 */
object AppLog {
    private const val TAG = "ROTOS"
    private const val MAX_BYTES = 1L shl 20
    private val STAMP: DateTimeFormatter = DateTimeFormatter.ofPattern("MM-dd HH:mm:ss.SSS")
    @Volatile private var file: File? = null

    fun init(ctx: Context) {
        if (file == null) {
            val dir = ctx.applicationContext.getExternalFilesDir(null) ?: ctx.applicationContext.filesDir
            file = File(dir, "rotos.log")
        }
    }

    @Synchronized
    fun i(msg: String) {
        Log.i(TAG, msg)
        val f = file ?: return
        runCatching {
            if (f.length() > MAX_BYTES) f.renameTo(File(f.path + ".1"))
            f.appendText(LocalDateTime.now().format(STAMP) + "  " + msg + "\n")
        }
    }
}
