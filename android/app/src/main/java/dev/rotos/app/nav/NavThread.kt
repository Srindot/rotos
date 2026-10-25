package dev.rotos.app.nav

import kotlinx.coroutines.ExecutorCoroutineDispatcher
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.withContext
import java.util.concurrent.Executors

/**
 * The one thread allowed to touch [NativeNav] (the C library keeps a single
 * static session). Everything that calls into the library does so through
 * [NavThread.run].
 */
object NavThread {
    val dispatcher: ExecutorCoroutineDispatcher =
        Executors.newSingleThreadExecutor { r -> Thread(r, "rotos-nav") }.asCoroutineDispatcher()

    suspend fun <T> run(block: () -> T): T = withContext(dispatcher) { block() }
}
