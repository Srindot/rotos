package dev.rotos.app.link

import kotlinx.coroutines.sync.Mutex
import java.util.concurrent.atomic.AtomicInteger

/** The link's single u16 phone -> ROTOS sequence counter. */
class SeqCounter {
    private val n = AtomicInteger(0)
    fun next(): Int = n.getAndIncrement() and 0xFFFF

    /** Held from numbering a frame until it is sent, so frames leave in sequence order. */
    val order = Mutex()
}
