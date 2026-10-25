package dev.rotos.app.call

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import androidx.core.content.ContextCompat
import dev.rotos.app.nav.CallAction
import dev.rotos.app.nav.Packets
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.withTimeoutOrNull
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.time.LocalTime

/**
 * Incoming calls on ROTOS (docs/PROTOCOL.md "Calls"), while the link runs:
 *   telephony -> CallTracker -> CALL_STATE frames (RINGING now and every 2 s,
 *   ACTIVE when answered, IDLE when over, MISSED if never answered)
 *   CALL_ACTION REJECT_REPLY -> endCall() -> SMS with the ETA -> IDLE(REPLIED)
 * Missing permissions degrade the feature (no number / name / hang-up / SMS)
 * and are reported through [log]. All callbacks run on the main thread.
 */
class CallController(
    private val context: Context,
    private val send: suspend (CallFrame) -> Unit,
    private val eta: suspend () -> LocalTime?,
    private val log: (String) -> Unit,
) {
    private companion object {
        const val REPEAT_MS = 2_000L
        const val UNKNOWN = "UNKNOWN"
        /** The number broadcast trails the call-state callback by ~50 ms on API 31+. */
        const val NUMBER_GRACE_MS = 500L
    }

    private var tracker = CallTracker()
    private val contacts = ContactLookup(context)
    private val actions = CallActions(context)
    private val ringer = RingerMute(context, log)

    private fun has(p: String) = ContextCompat.checkSelfPermission(context, p) == PackageManager.PERMISSION_GRANTED

    /** Runs until [scope] is cancelled (the ride / link ends). */
    fun start(scope: CoroutineScope) {
        if (!has(Manifest.permission.READ_PHONE_STATE)) {
            log("call alerts off: READ_PHONE_STATE not granted")
            return
        }
        val missing = listOf(
            Manifest.permission.READ_CALL_LOG to "caller number",
            Manifest.permission.READ_CONTACTS to "contact names",
            Manifest.permission.ANSWER_PHONE_CALLS to "hang-up",
            Manifest.permission.SEND_SMS to "SMS reply",
        ).filter { !has(it.first) }.map { it.second }
        log("call alerts on" + if (missing.isEmpty()) "" else " (without ${missing.joinToString(", ")}: permission missing)")
        val events = Channel<PhoneCallEvent>(64)
        scope.launch {
            runCatching { CallMonitor(context).events().collect { events.send(it) } }
                .onFailure { if (it !is CancellationException) log("call monitor stopped: ${it.message}") }
            events.close()
        }
        scope.launch { consume(events) }
        scope.coroutineContext[kotlinx.coroutines.Job]?.invokeOnCompletion { ringer.restore() }   // never leave it muted
        scope.launch {
            while (isActive) {
                delay(REPEAT_MS)
                tracker.repeat()?.let { send(it) }
            }
        }
    }

    /** Applies telephony events strictly in order (ends when the monitor closes the channel). */
    private suspend fun consume(events: Channel<PhoneCallEvent>) {
        var pending: PhoneCallEvent? = null
        while (true) {
            val e = pending ?: events.receiveCatching().getOrNull() ?: return
            pending = null
            if (waitsForNumber(e)) {
                // A new call without its number yet: give the number up to 500 ms so
                // ROTOS does not flash "UNKNOWN"; whatever comes next replaces this ring.
                val next = withTimeoutOrNull(NUMBER_GRACE_MS) { events.receiveCatching().getOrNull() }
                if (next != null) {
                    pending = next
                    continue
                }
            }
            apply(e)
        }
    }

    private fun waitsForNumber(e: PhoneCallEvent): Boolean =
        e is PhoneCallEvent.Ringing && e.number == null && tracker.current.let { it == null || it.ended } &&
            has(Manifest.permission.READ_CALL_LOG)

    private suspend fun apply(e: PhoneCallEvent) {
        val name = if (e is PhoneCallEvent.Ringing) nameOf(e.number) else ""   // suspends: before reading tracker
        val step = when (e) {
            is PhoneCallEvent.Ringing -> tracker.onRinging(e.number, name)
            PhoneCallEvent.OffHook -> tracker.onOffHook()
            PhoneCallEvent.Idle -> tracker.onIdle()
        }
        tracker = step.tracker
        if (e !is PhoneCallEvent.Ringing) ringer.restore()            // answered or over: ring again next time
        step.frame?.let {
            log("call ${it.callId}: ${stateName(it.state)} ${it.name}${if (it.flags and Packets.CALL_F_MISSED != 0) " (missed)" else ""}")
            send(it)
        }
    }

    private suspend fun nameOf(number: String?): String {
        if (number.isNullOrBlank()) return UNKNOWN
        if (!has(Manifest.permission.READ_CONTACTS)) return number
        return contacts.name(number).getOrElse { log("contact lookup failed: ${it.message}"); null } ?: number
    }

    suspend fun onAction(a: CallAction) {
        when (a.action) {
            Packets.ACTION_DISMISS -> {
                log("call ${a.callId}: ignored on ROTOS")
                ringer.mute()
            }
            Packets.ACTION_REJECT_REPLY -> reject(a.callId)
            else -> log("call ${a.callId}: unknown action ${a.action}")
        }
    }

    private suspend fun reject(id: Int) {
        val (t, call) = tracker.startReject(id)
        tracker = t
        if (call == null) {
            log("call $id: REJECT_REPLY ignored (not the ringing call)")
            return
        }
        val hung = if (has(Manifest.permission.ANSWER_PHONE_CALLS)) actions.endCall()
        else Result.failure(SecurityException("ANSWER_PHONE_CALLS not granted"))
        log("call $id: hang up " + hung.fold({ "OK" }, { "failed: ${it.message}" }))
        val text = ReplyText.of(eta())
        val sms = when {
            call.number.isNullOrBlank() -> Result.failure(IllegalStateException("caller number unknown"))
            !has(Manifest.permission.SEND_SMS) -> Result.failure(SecurityException("SEND_SMS not granted"))
            else -> actions.sendSms(call.number, text)
        }
        log("call $id: SMS \"$text\" " + sms.fold({ "sent" }, { "not sent: ${it.message}" }))
        val step = tracker.finishReject(id, sms.isSuccess)
        tracker = step.tracker
        step.frame?.let { send(it) }
    }

    private fun stateName(s: Int) = when (s) {
        Packets.CALL_RINGING -> "RINGING"
        Packets.CALL_ACTIVE -> "ACTIVE"
        else -> "IDLE"
    }
}
