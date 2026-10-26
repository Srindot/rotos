package dev.rotos.app.net

import java.io.ByteArrayOutputStream
import java.net.HttpURLConnection
import java.net.URL

/**
 * Minimal blocking HTTP (call from Dispatchers.IO). Every request has connect
 * and read timeouts; every failure is returned as a Result, never thrown.
 */
object Http {
    const val USER_AGENT = "ROTOS/1.0 (personal motorcycle navigation project)"
    private const val MAX_BODY = 32 * 1024 * 1024

    class HttpError(val code: Int, message: String) : Exception(message)

    fun get(url: String, timeoutMs: Int, headers: Map<String, String> = emptyMap()): Result<ByteArray> =
        request("GET", url, null, timeoutMs, headers)

    fun post(
        url: String, body: ByteArray, contentType: String, timeoutMs: Int,
        headers: Map<String, String> = emptyMap(),
    ): Result<ByteArray> = request("POST", url, body, timeoutMs, headers + ("Content-Type" to contentType))

    private fun request(
        method: String, url: String, body: ByteArray?, timeoutMs: Int, headers: Map<String, String>,
    ): Result<ByteArray> = runCatching {
        val c = URL(url).openConnection() as HttpURLConnection
        try {
            c.requestMethod = method
            c.connectTimeout = timeoutMs
            c.readTimeout = timeoutMs
            c.instanceFollowRedirects = true
            c.setRequestProperty("User-Agent", USER_AGENT)
            headers.forEach { (k, v) -> c.setRequestProperty(k, v) }
            if (body != null) {
                c.doOutput = true
                c.outputStream.use { it.write(body) }
            }
            val code = c.responseCode
            if (code !in 200..299) {
                val err = c.errorStream?.use { readCapped(it.readBytes()) } ?: ByteArray(0)
                throw HttpError(code, "HTTP $code: ${String(err).take(300)}")
            }
            c.inputStream.use { input ->
                val out = ByteArrayOutputStream()
                val buf = ByteArray(16 * 1024)
                while (true) {                                   // bounded by MAX_BODY, however small the chunks
                    val n = input.read(buf)
                    if (n < 0) break
                    out.write(buf, 0, n)
                    check(out.size() <= MAX_BODY) { "response too large" }
                }
                out.toByteArray()
            }
        } finally {
            c.disconnect()
        }
    }

    private fun readCapped(b: ByteArray): ByteArray = if (b.size > 4096) b.copyOf(4096) else b
}
