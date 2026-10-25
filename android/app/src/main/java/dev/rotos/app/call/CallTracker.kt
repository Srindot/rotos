package dev.rotos.app.call

import dev.rotos.app.nav.Packets

/**
 * Pure state machine: telephony events and reply results in, CALL_STATE
 * frames out (docs/PROTOCOL.md "Calls"). call_id is a u16 counter per call
 * (never 0). Immutable: every step returns the next tracker.
 */
data class CallTracker(val nextId: Int = 1, val current: Call? = null) {
    data class Step(val tracker: CallTracker, val frame: CallFrame?)

    fun onRinging(number: String?, name: String): Step {
        val c = current
        if (c == null || c.ended) {
            val call = Call(nextId, number, name)
            val next = if (nextId >= 0xFFFF) 1 else nextId + 1
            return Step(CallTracker(next, call), ringing(call))
        }
        if (c.handled || c.answered) return Step(this, null)
        val updated = c.copy(number = number ?: c.number, name = name)
        return Step(copy(current = updated), if (updated.name != c.name) ringing(updated) else null)
    }

    fun onOffHook(): Step {
        val c = current ?: return Step(this, null)              // outgoing call: not ours
        if (c.answered || c.ended) return Step(this, null)
        val updated = c.copy(answered = true)
        val quiet = c.handled || c.rejecting
        return Step(copy(current = updated), if (quiet) null else CallFrame(c.id, Packets.CALL_ACTIVE, 0, c.name))
    }

    fun onIdle(): Step {
        val c = current ?: return Step(this, null)
        if (c.rejecting) return Step(copy(current = c.copy(ended = true)), null)
        if (c.handled) return Step(copy(current = null), null)
        val flags = if (c.answered) 0 else Packets.CALL_F_MISSED
        return Step(copy(current = null), CallFrame(c.id, Packets.CALL_IDLE, flags, c.name))
    }

    /** REJECT_REPLY for [id]: the call to act on, or null when it is not the current one. */
    fun startReject(id: Int): Pair<CallTracker, Call?> {
        val c = current
        if (c == null || c.id != id || c.handled || c.rejecting || c.answered) return this to null
        return copy(current = c.copy(rejecting = true)) to c
    }

    /** Hang-up + SMS done: IDLE with REPLIED when the SMS went out. */
    fun finishReject(id: Int, smsSent: Boolean): Step {
        val c = current
        if (c == null || c.id != id || !c.rejecting) return Step(this, null)
        val frame = CallFrame(c.id, Packets.CALL_IDLE, if (smsSent) Packets.CALL_F_REPLIED else 0, c.name)
        val next = if (c.ended) null else c.copy(rejecting = false, handled = true)
        return Step(copy(current = next), frame)
    }

    /** The periodic RINGING repeat (every 2 s), or null when nothing is ringing. */
    fun repeat(): CallFrame? {
        val c = current ?: return null
        return if (c.answered || c.ended || c.handled || c.rejecting) null else ringing(c)
    }

    private fun ringing(c: Call) = CallFrame(c.id, Packets.CALL_RINGING, 0, c.name)
}
