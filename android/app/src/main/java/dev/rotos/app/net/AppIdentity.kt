package dev.rotos.app.net

import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import java.security.MessageDigest

/**
 * Who is calling Google: an API key restricted to "Android apps" is only
 * accepted when REST requests say which app (package name) and which signing
 * certificate (SHA-1) they come from. Google's own SDKs add these headers;
 * ROTOS calls Routes / Map Tiles directly, so it adds them itself.
 */
object AppIdentity {
    @Volatile private var headers: Map<String, String> = emptyMap()

    fun init(ctx: Context) {
        if (headers.isNotEmpty()) return
        val app = ctx.applicationContext
        val sha1 = runCatching { signingSha1(app) }.getOrNull() ?: return
        headers = mapOf("X-Android-Package" to app.packageName, "X-Android-Cert" to sha1)
        dev.rotos.app.util.AppLog.i("google identity: ${app.packageName} SHA-1 ${sha1.chunked(2).joinToString(":")}")
    }

    /** X-Android-Package / X-Android-Cert, or empty before init. */
    fun googleHeaders(): Map<String, String> = headers

    @Suppress("DEPRECATION")
    private fun signingSha1(ctx: Context): String? {
        val pm = ctx.packageManager
        val certs = if (Build.VERSION.SDK_INT >= 28) {
            val info = pm.getPackageInfo(ctx.packageName, PackageManager.GET_SIGNING_CERTIFICATES)
            info.signingInfo?.apkContentsSigners
        } else {
            pm.getPackageInfo(ctx.packageName, PackageManager.GET_SIGNATURES).signatures
        }
        val cert = certs?.firstOrNull() ?: return null
        return MessageDigest.getInstance("SHA-1").digest(cert.toByteArray()).joinToString("") { "%02X".format(it) }
    }
}
