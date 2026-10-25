package dev.rotos.app.call

import android.annotation.SuppressLint
import android.app.Activity
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.telecom.TelecomManager
import android.telephony.SmsManager
import androidx.core.content.ContextCompat
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.withTimeoutOrNull

/** Hang up (ANSWER_PHONE_CALLS) and text back (SEND_SMS). Android edge; failures are Results. */
class CallActions(private val context: Context) {
    private companion object {
        const val SMS_SENT = "dev.rotos.app.SMS_SENT"
        const val SMS_TIMEOUT_MS = 20_000L
    }

    @SuppressLint("MissingPermission")
    fun endCall(): Result<Unit> {
        if (Build.VERSION.SDK_INT < 28) return Result.failure(UnsupportedOperationException("endCall needs Android 9"))
        val tm = context.getSystemService(TelecomManager::class.java)
            ?: return Result.failure(IllegalStateException("no TelecomManager"))
        return runCatching {
            @Suppress("DEPRECATION")
            val ok = tm.endCall()
            check(ok) { "TelecomManager.endCall() returned false" }
        }
    }

    /** Sends [text] to [number] and waits for the radio's sent report. */
    suspend fun sendSms(number: String, text: String): Result<Unit> {
        val sm: SmsManager = (if (Build.VERSION.SDK_INT >= 31) context.getSystemService(SmsManager::class.java) else null)
            ?: @Suppress("DEPRECATION") SmsManager.getDefault()
        val done = CompletableDeferred<Int>()
        val receiver = object : BroadcastReceiver() {
            override fun onReceive(c: Context, i: Intent) {
                done.complete(resultCode)
            }
        }
        ContextCompat.registerReceiver(context, receiver, IntentFilter(SMS_SENT), ContextCompat.RECEIVER_NOT_EXPORTED)
        try {
            val sent = PendingIntent.getBroadcast(context, 0, Intent(SMS_SENT).setPackage(context.packageName),
                PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
            runCatching { sm.sendTextMessage(number, null, text, sent, null) }.onFailure { return Result.failure(it) }
            val code = withTimeoutOrNull(SMS_TIMEOUT_MS) { done.await() }
                ?: return Result.failure(IllegalStateException("no SMS sent report in 20 s"))
            return if (code == Activity.RESULT_OK) Result.success(Unit)
            else Result.failure(IllegalStateException("SMS failed (result $code)"))
        } finally {
            runCatching { context.unregisterReceiver(receiver) }
        }
    }
}
