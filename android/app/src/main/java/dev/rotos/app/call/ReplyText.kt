package dev.rotos.app.call

import java.time.LocalTime
import java.time.format.DateTimeFormatter

/** The SMS sent when the rider rejects a call from ROTOS. Pure. */
object ReplyText {
    fun of(eta: LocalTime?): String =
        if (eta == null) "I'm riding right now, I'll call you back later."
        else "I'm riding right now, I'll call you back later (around ${eta.format(DateTimeFormatter.ofPattern("HH:mm"))})."
}
