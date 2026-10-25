package dev.rotos.app.ui

import android.view.View
import androidx.core.content.ContextCompat
import dev.rotos.app.BuildConfig
import dev.rotos.app.R
import dev.rotos.app.quota.Level
import dev.rotos.app.quota.Usage

/** The "Google usage" card: calls today and this month against ROTOS's caps. */
object UsageCard {
    fun render(v: MainViews, usage: List<Usage>) {
        val ctx = v.root.context
        if (BuildConfig.MAPS_API_KEY.isBlank()) {
            v.usageBody.visibility = View.GONE
            v.usageNote.setText(R.string.usage_no_key)
            return
        }
        v.usageBody.visibility = View.VISIBLE
        v.usageBody.text = table(usage)
        val worst = usage.maxOfOrNull { it.level } ?: Level.OK
        v.usageNote.setText(when (worst) {
            Level.OK -> R.string.usage_ok
            Level.WARN -> R.string.usage_warn
            Level.CAPPED -> R.string.usage_capped
        })
        val color = when (worst) {
            Level.OK -> R.color.ctp_subtext0
            Level.WARN -> R.color.ctp_peach
            Level.CAPPED -> R.color.ctp_red
        }
        v.usageNote.setTextColor(ContextCompat.getColor(ctx, color))
    }

    /** "Routes      3 / 200     40 / 2,500" per API, under a today / month header. Pure. */
    fun table(usage: List<Usage>): String {
        val head = "%-14s %13s %15s".format("", "today", "month")
        val rows = usage.map { u ->
            "%-14s %13s %15s".format(u.api.label,
                "%,d / %,d".format(u.counts.day, u.api.dayCap),
                "%,d / %,d".format(u.counts.month, u.api.monthCap))
        }
        return (listOf(head) + rows).joinToString("\n")
    }
}
