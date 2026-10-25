package dev.rotos.app.link

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull

/**
 * A RotosLink that survives drop-outs: when the underlying link (BLE or TCP,
 * made fresh by [factory] each time) disconnects, it reconnects in the
 * background with a gentle backoff until [close]. Meanwhile [state] reads
 * CONNECTING and [send] fails fast, so callers simply carry on; [incoming]
 * is one continuous flow across reconnects.
 *
 * ROTOS keeps the map pack in flash, so after a reconnect the next NAV_STATE
 * (same route id) resumes navigation on the device by itself.
 */
class ReconnectingLink(
    private val scope: CoroutineScope,
    private val factory: () -> RotosLink,
    private val onEvent: (String) -> Unit,
    private val onReconnected: suspend () -> Unit,
) : RotosLink {
    companion object {
        private val BACKOFF_MS = longArrayOf(1_000, 2_000, 3_000, 4_000)     // + a 15 s scan: under Android's 5 scans / 30 s
        const val WAIT_CONNECTED_MS = 60_000L
    }

    private val _state = MutableStateFlow(LinkState.IDLE)
    override val state: StateFlow<LinkState> = _state
    private val _incoming = MutableSharedFlow<ByteArray>(extraBufferCapacity = 256, onBufferOverflow = BufferOverflow.DROP_OLDEST)
    override val incoming: SharedFlow<ByteArray> = _incoming
    override val peer: String get() = inner?.peer ?: "ROTOS"

    @Volatile private var inner: RotosLink? = null
    @Volatile private var closed = false
    private var pump: Job? = null
    private var watch: Job? = null
    private var retry: Job? = null

    /** First connection: one attempt, the caller decides about retries. */
    override suspend fun connect(): Result<Unit> {
        _state.value = LinkState.CONNECTING
        val r = attach()
        if (r.isFailure) _state.value = LinkState.FAILED
        return r
    }

    private suspend fun attach(): Result<Unit> {
        val l = factory()
        val r = l.connect()
        if (r.isFailure) {
            l.close()
            return r
        }
        inner = l
        pump = scope.launch { l.incoming.collect { _incoming.emit(it) } }
        watch = scope.launch {
            l.state.first { it == LinkState.DISCONNECTED || it == LinkState.FAILED }
            if (!closed) dropped(l)
        }
        _state.value = LinkState.CONNECTED
        return r
    }

    private fun dropped(l: RotosLink) {
        pump?.cancel()
        l.close()
        inner = null
        _state.value = LinkState.CONNECTING
        onEvent("link to ROTOS lost: reconnecting")
        retry = scope.launch {
            var n = 0
            while (isActive && !closed) {
                delay(BACKOFF_MS[minOf(n, BACKOFF_MS.size - 1)])
                n++
                val r = try { attach() } catch (e: kotlinx.coroutines.CancellationException) { throw e } catch (e: Exception) { Result.failure(e) }
                if (r.isSuccess) {
                    onEvent("reconnected to ROTOS (attempt $n)")
                    runCatching { onReconnected() }
                    return@launch
                }
            }
        }
    }

    /** Waits (up to [timeoutMs]) until the link is up again; false on timeout or close. */
    suspend fun awaitConnected(timeoutMs: Long = WAIT_CONNECTED_MS): Boolean =
        withTimeoutOrNull(timeoutMs) { _state.first { it == LinkState.CONNECTED || closed } } != null && !closed

    override suspend fun send(bytes: ByteArray): Result<Unit> {
        val l = inner
        if (l == null || _state.value != LinkState.CONNECTED) {
            return Result.failure(IllegalStateException("ROTOS link down (reconnecting)"))
        }
        return l.send(bytes)
    }

    override fun close() {
        closed = true
        retry?.cancel()
        watch?.cancel()
        pump?.cancel()
        inner?.close()
        inner = null
        _state.value = LinkState.DISCONNECTED
    }
}
