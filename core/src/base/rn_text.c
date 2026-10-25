/*
 * rn_text.c - bounded string helpers.
 */
#include "rn/rn_text.h"

#define TEXT_SCAN_MAX 4096u   /* longest instruction we look at */

static char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

static uint32_t bounded_len(const char *s, uint32_t max)
{
    uint32_t n = 0u;
    while (n < max && s[n] != '\0') {
        n++;
    }
    return n;
}

uint32_t rn_text_copy(char *dst, uint32_t cap, const char *src)
{
    if (!RN_ASSERT(dst != NULL && cap > 0u)) {
        return 0u;
    }
    uint32_t n = 0u;
    if (src != NULL) {
        while (n + 1u < cap && src[n] != '\0') {
            dst[n] = src[n];
            n++;
        }
    }
    dst[n] = '\0';
    return n;
}

uint32_t rn_text_ascii_upper(char *dst, uint32_t cap, const char *src)
{
    if (!RN_ASSERT(dst != NULL && cap > 0u)) {
        return 0u;
    }
    uint32_t n = 0u;
    bool pending_space = false;
    uint32_t len = (src != NULL) ? bounded_len(src, TEXT_SCAN_MAX) : 0u;
    for (uint32_t i = 0u; i < len && n + 1u < cap; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c >= 0x80u || c < 0x20u) {
            continue;                         /* drop non-ASCII / control  */
        }
        if (c == (unsigned char)' ') {
            pending_space = (n > 0u);
            continue;
        }
        if (pending_space && n + 2u < cap) {
            dst[n] = ' ';
            n++;
        }
        pending_space = false;
        dst[n] = (c >= (unsigned char)'a' && c <= (unsigned char)'z') ? (char)(c - 32u) : (char)c;
        n++;
    }
    dst[n] = '\0';
    return n;
}

/* Index just after the first whole-word, case-insensitive match of `word`
 * followed by a space, or -1. */
static int32_t find_word(const char *s, uint32_t len, const char *word)
{
    uint32_t wl = bounded_len(word, 16u);
    for (uint32_t i = 0u; i + wl < len; i++) {
        bool start_ok = (i == 0u) || (s[i - 1u] == ' ');
        uint32_t k = 0u;
        while (k < wl && lower(s[i + k]) == word[k]) {
            k++;
        }
        if (start_ok && k == wl && s[i + wl] == ' ') {
            return (int32_t)(i + wl + 1u);
        }
    }
    return -1;
}

uint32_t rn_text_road_from_instruction(char *dst, uint32_t cap, const char *instruction)
{
    if (!RN_ASSERT(dst != NULL && cap > 0u)) {
        return 0u;
    }
    dst[0] = '\0';
    if (instruction == NULL) {
        return 0u;
    }
    uint32_t len = bounded_len(instruction, TEXT_SCAN_MAX);
    int32_t at = find_word(instruction, len, "onto");
    if (at < 0) {
        at = find_word(instruction, len, "on");
    }
    if (at < 0) {
        at = find_word(instruction, len, "toward");
    }
    if (at < 0) {
        at = find_word(instruction, len, "towards");
    }
    if (at < 0) {
        return 0u;
    }
    char tmp[128];
    uint32_t n = 0u;
    const char *rest = &instruction[at];
    uint32_t rest_len = len - (uint32_t)at;
    int32_t cut = find_word(rest, rest_len, "toward");
    uint32_t end = (cut > 0) ? (uint32_t)cut - 7u : rest_len;   /* drop " toward" */
    while (n < end && n + 1u < (uint32_t)sizeof tmp) {
        tmp[n] = rest[n];
        n++;
    }
    tmp[n] = '\0';
    return rn_text_ascii_upper(dst, cap, tmp);
}

bool rn_text_contains_ci(const char *haystack, const char *needle)
{
    if (!RN_ASSERT(haystack != NULL && needle != NULL)) {
        return false;
    }
    uint32_t hl = bounded_len(haystack, TEXT_SCAN_MAX);
    uint32_t nl = bounded_len(needle, 64u);
    if (nl == 0u) {
        return true;
    }
    for (uint32_t i = 0u; i + nl <= hl; i++) {
        uint32_t k = 0u;
        while (k < nl && lower(haystack[i + k]) == lower(needle[k])) {
            k++;
        }
        if (k == nl) {
            return true;
        }
    }
    return false;
}
