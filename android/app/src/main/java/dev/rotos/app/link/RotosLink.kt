package dev.rotos.app.link

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.StateFlow

enum class LinkState { IDLE, CONNECTING, CONNECTED, DISCONNECTED, FAILED }

/** A byte pipe to ROTOS (BLE on the bike, TCP to the simulator). */
interface RotosLink {
    val state: StateFlow<LinkState>
    /** Raw inbound bytes, in order, any fragmentation. */
    val incoming: Flow<ByteArray>
    /** Human-readable peer, e.g. "10.0.2.2:7400" or a BLE address. */
    val peer: String

    suspend fun connect(): Result<Unit>
    /** Sends one whole frame (the link splits it as its transport needs). */
    suspend fun send(bytes: ByteArray): Result<Unit>
    fun close()
}
