package dev.rotos.app.call

/** What the phone's telephony stack reports (Android edge -> pure logic). */
sealed class PhoneCallEvent {
    /** Incoming call ringing; number null while unknown (or without READ_CALL_LOG). */
    data class Ringing(val number: String?) : PhoneCallEvent()
    object OffHook : PhoneCallEvent()
    object Idle : PhoneCallEvent()
}

/** One CALL_STATE packet to send (rn_protocol.h RnCallState). */
data class CallFrame(val callId: Int, val state: Int, val flags: Int, val name: String)

/** The call ROTOS currently knows about. */
data class Call(
    val id: Int,
    val number: String?,
    val name: String,
    val answered: Boolean = false,
    /** REJECT_REPLY in progress: hang-up / SMS running, telephony IDLE is held back. */
    val rejecting: Boolean = false,
    /** Reply finished and IDLE(REPLIED) sent: nothing more goes to ROTOS for this call. */
    val handled: Boolean = false,
    /** Telephony already reported the end of the call. */
    val ended: Boolean = false,
)
