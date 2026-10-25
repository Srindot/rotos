@file:Suppress("DEPRECATION") // PhoneStateListener: the API < 31 fallback the feature needs
package dev.rotos.app.call

import android.annotation.SuppressLint
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.telephony.PhoneStateListener
import android.telephony.TelephonyCallback
import android.telephony.TelephonyManager
import androidx.core.content.ContextCompat
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.callbackFlow

/**
 * Telephony call state as a flow (Android edge). API 31+: TelephonyCallback
 * for the state, plus the PHONE_STATE broadcast for the incoming number
 * (delivered only with READ_CALL_LOG). Older APIs: PhoneStateListener, which
 * carries both. Needs READ_PHONE_STATE (checked by CallController).
 */
class CallMonitor(private val context: Context) {
    @SuppressLint("MissingPermission")
    fun events(): Flow<PhoneCallEvent> = callbackFlow {
        val tm = context.getSystemService(TelephonyManager::class.java)
        if (tm == null) {
            close(IllegalStateException("no telephony"))
            return@callbackFlow
        }
        var lastNumber: String? = null
        var ringing = false
        fun onState(state: Int, number: String?) {
            if (!number.isNullOrEmpty()) lastNumber = number
            when (state) {
                TelephonyManager.CALL_STATE_RINGING -> { ringing = true; trySend(PhoneCallEvent.Ringing(lastNumber)) }
                TelephonyManager.CALL_STATE_OFFHOOK -> trySend(PhoneCallEvent.OffHook)
                TelephonyManager.CALL_STATE_IDLE -> { ringing = false; lastNumber = null; trySend(PhoneCallEvent.Idle) }
            }
        }
        val receiver = object : BroadcastReceiver() {
            override fun onReceive(c: Context, i: Intent) {
                @Suppress("DEPRECATION")
                val n = i.getStringExtra(TelephonyManager.EXTRA_INCOMING_NUMBER)
                val st = i.getStringExtra(TelephonyManager.EXTRA_STATE)
                if (st != TelephonyManager.EXTRA_STATE_RINGING || n.isNullOrEmpty() || n == lastNumber) return
                lastNumber = n                                // may arrive before or after the callback
                if (ringing) trySend(PhoneCallEvent.Ringing(n))
            }
        }
        val unregister: () -> Unit
        if (Build.VERSION.SDK_INT >= 31) {
            val cb = object : TelephonyCallback(), TelephonyCallback.CallStateListener {
                override fun onCallStateChanged(state: Int) = onState(state, null)
            }
            val ok = runCatching { tm.registerTelephonyCallback(ContextCompat.getMainExecutor(context), cb) }
            if (ok.isFailure) {
                close(ok.exceptionOrNull())
                return@callbackFlow
            }
            ContextCompat.registerReceiver(context, receiver, IntentFilter(TelephonyManager.ACTION_PHONE_STATE_CHANGED),
                ContextCompat.RECEIVER_EXPORTED)
            unregister = {
                tm.unregisterTelephonyCallback(cb)
                runCatching { context.unregisterReceiver(receiver) }
            }
        } else {
            @Suppress("DEPRECATION")
            val l = object : PhoneStateListener() {
                @Deprecated("API < 31 path")
                override fun onCallStateChanged(state: Int, phoneNumber: String?) = onState(state, phoneNumber)
            }
            @Suppress("DEPRECATION")
            tm.listen(l, PhoneStateListener.LISTEN_CALL_STATE)
            @Suppress("DEPRECATION")
            unregister = { tm.listen(l, PhoneStateListener.LISTEN_NONE) }
        }
        awaitClose { unregister() }
    }
}
