#include "hud.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include "font.h"
#include "icons.h"

namespace rotos {

void formatDistance(float m, char* value, char* unit) {
    if (m < 1000.f) {
        int r = m < 100.f ? int(m / 5.f + 0.5f) * 5 : int(m / 10.f + 0.5f) * 10;
        if (r >= 1000) { std::strcpy(value, "1.0"); std::strcpy(unit, "KM"); return; }
        std::snprintf(value, 16, "%d", r);
        std::strcpy(unit, "M");
    } else if (m < 10000.f) {
        std::snprintf(value, 16, "%.1f", m / 1000.f);
        std::strcpy(unit, "KM");
    } else {
        std::snprintf(value, 16, "%d", int(m / 1000.f + 0.5f));
        std::strcpy(unit, "KM");
    }
}

// Copy `s` into `out`, cutting it with ".." to fit `maxW` pixels.
static const char* fit(const char* s, int scale, int maxW, char* out, size_t cap) {
    std::snprintf(out, cap, "%s", s);
    size_t n = std::strlen(out);
    if (textWidth(out, scale) <= maxW) return out;
    while (n > 0) {
        out[--n] = 0;
        char tmp[64];
        std::snprintf(tmp, sizeof tmp, "%s..", out);
        if (textWidth(tmp, scale) <= maxW) { std::snprintf(out, cap, "%s", tmp); break; }
    }
    return out;
}

static void chip(Canvas& cv, const Theme& t, int cx, int y, const char* text, uint8_t col) {
    int w = textWidth(text, 1) + 12;
    cv.panel(cx - w / 2, y, w, 15, t);
    drawText(cv, cx - w / 2 + 6, y + 4, text, 1, col);
}

void drawHud(Canvas& cv, const Theme& t, const HudData& d, uint32_t now_ms) {
    char v[16], u[8], buf[64];

    // ── Top: next maneuver ───────────────────────────────────────────
    cv.panel(6, 6, 228, 64, t, true);
    cv.fillRect(12, 12, 52, 52, C_ACCENT);
    drawStatus(cv, 228, 11, d.status, false, now_ms);
    if (d.next != M_NONE) {
        drawManeuverIcon(cv, 14, 14, d.next, 3, C_PANEL);
        formatDistance(d.nextDist, v, u);
        int x = drawText(cv, 74, 14, v, 4, C_TEXT);
        drawText(cv, x + 6, 28, u, 2, C_TEXT_DIM);
        const char* road = d.nextRoad && *d.nextRoad ? d.nextRoad : (d.next == M_ARRIVE ? "DESTINATION" : "");
        drawText(cv, 74, 52, fit(road, 1, 154, buf, sizeof buf), 1, C_ACCENT);
    }

    // ── Under it: follow-up maneuver / warnings ──────────────────────
    if (d.then != M_NONE) {
        cv.panel(6, 74, 62, 24, t);
        drawText(cv, 12, 83, "THEN", 1, C_TEXT_DIM);
        drawManeuverIcon(cv, 44, 78, d.then, 1, C_TEXT);
    }
    bool blink = (now_ms / 500) & 1;
    if (d.linkLost) chip(cv, t, 172, 78, blink ? "NO PHONE LINK" : "", C_HILITE);
    else if (d.offRoute) chip(cv, t, 172, 78, "OFF ROUTE", C_HILITE);
    else if (d.flyover != FLY_NONE && d.nextDist < 600.f) {
        // Flyover or service road: arrow + words, the decision riders get wrong.
        static const char* TXT[] = {"", "TAKE FLYOVER", "STAY BELOW", "EXIT FLYOVER"};
        const char* txt = TXT[d.flyover];
        int w = textWidth(txt, 1) + 24, x = 172 - w / 2;
        cv.panel(x, 76, w, 19, t);
        drawFlyoverArrow(cv, x + 6, 82, d.flyover == FLY_TAKE, d.flyover == FLY_EXIT ? C_TEXT : C_HILITE);
        drawText(cv, x + 17, 82, txt, 1, d.flyover == FLY_EXIT ? C_TEXT : C_HILITE);
    }

    // ── Current road, like a Pokemon location sign ───────────────────
    if (d.currentRoad && *d.currentRoad)
        chip(cv, t, 120, 240, fit(d.currentRoad, 1, 200, buf, sizeof buf), C_TEXT);

    // ── Bottom: speed | ETA | remaining ──────────────────────────────
    cv.panel(6, 260, 228, 54, t, true);
    for (int y = 268; y < 306; y += 2) {       // dotted: lighter over glass
        cv.pixel(82, y, C_TEXT_DIM);
        cv.pixel(158, y, C_TEXT_DIM);
    }
    std::snprintf(v, sizeof v, "%d", d.speedKmh);
    drawTextCentered(cv, 44, 268, v, 3, C_TEXT);
    drawTextCentered(cv, 44, 297, "KM/H", 1, C_TEXT_DIM);

    std::snprintf(v, sizeof v, "%02u:%02u", unsigned(d.etaS / 3600 % 24), unsigned(d.etaS / 60 % 60));
    drawTextCentered(cv, 120, 272, v, 2, C_TEXT);
    drawTextCentered(cv, 120, 297, "ETA", 1, C_TEXT_DIM);

    formatDistance(d.remainingM, v, u);
    drawTextCentered(cv, 196, 272, v, 2, C_TEXT);
    std::snprintf(buf, sizeof buf, "%s LEFT", u);
    drawTextCentered(cv, 196, 297, buf, 1, C_TEXT_DIM);
}

void drawStatus(Canvas& cv, int rightX, int y, const StatusView& s, bool withPct, uint32_t now_ms) {
    bool blink = (now_ms / 500) & 1;
    // Battery: 13x7 body + 1x3 nub, fill proportional; blinks below 15%.
    int bx = rightX - 15;
    uint8_t outline = s.known ? C_TEXT : C_TEXT_DIM;
    cv.rect(bx, y, 13, 7, outline);
    cv.fillRect(bx + 13, y + 2, 2, 3, outline);
    if (s.known) {
        int fill = (s.battery * 11 + 50) / 100;
        bool low = s.battery < 15;
        if (!(low && blink)) cv.fillRect(bx + 1, y + 1, fill, 5, s.charging ? C_ACCENT : (low ? C_HILITE : C_TEXT));
    }
    int x = bx - 4;
    if (withPct && s.known) {
        char pct[8];
        std::snprintf(pct, sizeof pct, "%u%%", unsigned(s.battery));
        x -= textWidth(pct, 1);
        drawText(cv, x, y, pct, 1, C_TEXT);
        x -= 6;
    }
    // Signal: 4 bars (phone's mobile network). Link to the phone lost: bars
    // go dim with a blinking X over them.
    int sx = x - 11;
    for (int i = 0; i < 4; ++i) {
        int h = 2 + i * 5 / 3 + (i == 3);
        bool lit = s.known && s.linkOk && i < s.bars;
        cv.fillRect(sx + i * 3, y + 7 - h, 2, h, lit ? C_TEXT : C_ROUTE_DONE);
    }
    if (s.known && !s.linkOk && blink)
        for (int k = 0; k < 5; ++k) { cv.pixel(sx + 3 + k, y + 1 + k, C_HILITE); cv.pixel(sx + 7 - k, y + 1 + k, C_HILITE); }
}

static void dotGrid(Canvas& cv, int phase) {
    for (int y = 4 + phase; y < Canvas::H; y += 8)
        for (int x = 4; x < Canvas::W; x += 8) cv.pixel(x, y, C_BUILDING);
}

void drawDashboard(Canvas& cv, const Theme& t, const DashData& d, uint32_t now_ms) {
    static const char* DAYS[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
    static const char* MONTHS[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    char buf[48];
    cv.clear(C_LAND);
    dotGrid(cv, 0);

    drawText(cv, 10, 10, "ROTOS", 1, C_TEXT_DIM);
    drawStatus(cv, 230, 10, d.status, true, now_ms);

    // Mascot: gentle bob, blinks every ~4 s.
    int bob = ((now_ms / 600) & 1) ? 2 : 0;
    int mood = (now_ms % 4000) < 160 ? 1 : 0;
    drawMascot(cv, 88, 34 + bob, mood, 4);

    if (d.haveClock) {
        std::snprintf(buf, sizeof buf, "%02u:%02u", unsigned(d.timeS / 3600 % 24), unsigned(d.timeS / 60 % 60));
        drawTextCentered(cv, 120, 112, buf, 6, C_TEXT);
        std::snprintf(buf, sizeof buf, "%s %02u %s", DAYS[d.weekday % 7], unsigned(d.day), MONTHS[(d.month + 11) % 12]);
        drawTextCentered(cv, 120, 162, buf, 2, C_TEXT_DIM);
    } else {
        drawTextCentered(cv, 120, 112, "--:--", 6, C_TEXT_DIM);
    }

    cv.panel(12, 190, 216, 74, t, true);
    if (d.haveWeather) {
        const Weather& w = d.weather;
        drawWeatherIcon(cv, 22, 203, w.code, w.is_day, 3);
        std::snprintf(buf, sizeof buf, "%d~C", int(w.temp_c + (w.temp_c < 0 ? -0.5f : 0.5f)));
        drawText(cv, 82, 200, buf, 4, C_TEXT);
        drawText(cv, 82, 236, weatherName(w.code), 1, C_ACCENT);
        std::snprintf(buf, sizeof buf, "RAIN %u%%  WIND %u KM/H", unsigned(w.rain_prob), unsigned(w.wind_kmh));
        drawText(cv, 82, 248, buf, 1, C_TEXT_DIM);
    } else {
        drawTextCentered(cv, 120, 224, "NO WEATHER DATA", 1, C_TEXT_DIM);
    }

    const char* foot = !d.status.known ? "WAITING FOR PHONE"
                     : d.status.linkOk ? "READY - START A ROUTE ON YOUR PHONE" : "PHONE LINK LOST";
    drawTextCentered(cv, 120, 292, foot, 1, d.status.linkOk ? C_TEXT_DIM : C_HILITE);
}

static void durationText(uint32_t s, char* out, size_t n) {
    if (s >= 3600) std::snprintf(out, n, "%uH %02uM", unsigned(s / 3600), unsigned(s / 60 % 60));
    else std::snprintf(out, n, "%uM %02uS", unsigned(s / 60), unsigned(s % 60));
}

void drawSummary(Canvas& cv, const Theme& t, const RideSummary& r, int secondsLeft, uint32_t now_ms) {
    (void)now_ms;
    char v[24], u[16];
    cv.panel(10, 24, 220, 272, t, true);
    drawTextCentered(cv, 120, 36, "RIDE COMPLETE", 2, C_TEXT);
    drawMascot(cv, 96, 56, 2, 3);

    int y = 112;
    auto row = [&](const char* label, const char* value, const char* unit) {
        drawText(cv, 24, y + 4, label, 1, C_TEXT_DIM);
        int wu = unit && *unit ? textWidth(unit, 1) + 4 : 0;
        int x = 216 - wu - textWidth(value, 2);
        int end = drawText(cv, x, y, value, 2, C_TEXT);
        if (wu) drawText(cv, end + 4, y + 7, unit, 1, C_TEXT_DIM);
        for (int i = 24; i < 216; i += 2) cv.pixel(i, y + 19, C_ROUTE_DONE);   // dotted rule
        y += 26;
    };
    formatDistance(r.distanceM, v, u);
    row("DISTANCE", v, u);
    durationText(r.rideS, v, sizeof v);
    row("RIDE TIME", v, "");
    durationText(r.movingS, v, sizeof v);
    row("MOVING", v, "");
    std::snprintf(v, sizeof v, "%d", int(r.avgKmh + 0.5f));
    row("AVG SPEED", v, "KM/H");
    std::snprintf(v, sizeof v, "%d", int(r.maxKmh + 0.5f));
    row("TOP SPEED", v, "KM/H");
    if (r.haveClock) {
        std::snprintf(v, sizeof v, "%02u:%02u-%02u:%02u", unsigned(r.startS / 3600 % 24), unsigned(r.startS / 60 % 60),
                      unsigned(r.endS / 3600 % 24), unsigned(r.endS / 60 % 60));
        row("TIME", v, "");
    }
    std::snprintf(v, sizeof v, "DASHBOARD IN %d", secondsLeft);
    drawTextCentered(cv, 120, 282, v, 1, C_TEXT_DIM);
}

static float clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

void drawBoot(Canvas& cv, const Theme& t, uint32_t ms) {
    cv.clear(C_LAND);
    // Background: slow scrolling dot grid.
    int off = int(ms / 40) % 8;
    for (int y = off; y < Canvas::H; y += 8)
        for (int x = 4; x < Canvas::W; x += 8) cv.pixel(x, y, C_BUILDING);

    // Game Boy style: the logo slides down and stops dead in the centre.
    float p = clamp01(ms / 900.f);
    int y = int(-50 + (118 + 50) * p);
    drawTextCentered(cv, 120, y, "ROTOS", 6, C_ACCENT, true);
    if (ms > 1000) {
        drawTextCentered(cv, 120, 172, "MOTORCYCLE SYSTEM", 1, C_TEXT);
        // Loading bar: border + stepped fill, 12 chunky segments.
        int segs = int(clamp01((ms - 1000) / 800.f) * 12);
        cv.panel(58, 196, 124, 14, t);
        for (int i = 0; i < segs; ++i) cv.fillRect(62 + i * 10, 200, 8, 6, i % 2 ? C_HILITE : C_ACCENT);
    }
    drawTextCentered(cv, 120, 300, "V0.1  SPEED 400", 1, C_TEXT_DIM);
}

void drawWaiting(Canvas& cv, const Theme& t, uint32_t ms) {
    cv.clear(C_LAND);
    dotGrid(cv, 0);
    // Pulsing rings around a phone glyph.
    int r = int(ms / 30) % 48;
    for (int k = 0; k < 3; ++k) {
        int rr = (r + k * 16) % 48 + 14;
        for (int a = 0; a < 64; ++a) {
            float th = a * 6.2831853f / 64;
            cv.pixel(120 + int(rr * std::cos(th)), 130 + int(rr * std::sin(th)), k == 0 ? C_ACCENT : C_ROUTE_DONE);
        }
    }
    cv.fillRect(111, 116, 18, 28, C_SHADOW);
    cv.rect(111, 116, 18, 28, C_ACCENT);
    cv.fillRect(114, 120, 12, 18, C_ACCENT);
    char s[32];
    int dots = int(ms / 400) % 4;
    std::snprintf(s, sizeof s, "WAITING FOR PHONE%.*s", dots, "...");
    cv.panel(36, 210, 168, 24, t);
    drawText(cv, 50, 219, s, 1, C_TEXT);
}

} // namespace rotos
