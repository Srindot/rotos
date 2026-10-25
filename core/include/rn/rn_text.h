/*
 * rn_text.h - bounded string helpers. ROTOS's font is upper-case ASCII, so
 * every road name is normalised once on the phone side.
 */
#ifndef RN_TEXT_H
#define RN_TEXT_H

#include "rn/rn_types.h"

/* Copy at most cap-1 chars and NUL-terminate. Returns chars copied. */
uint32_t rn_text_copy(char *dst, uint32_t cap, const char *src);

/* Upper-case ASCII, non-ASCII bytes dropped, runs of spaces collapsed,
 * trimmed, cut to cap-1 chars. Returns the resulting length. */
uint32_t rn_text_ascii_upper(char *dst, uint32_t cap, const char *src);

/* Road name from a Google instruction: "Turn left onto Road No. 36" ->
 * "ROAD NO. 36". Prefers the text after "onto", then "on", then "toward";
 * the name stops at " toward". Empty string when no road is mentioned. */
uint32_t rn_text_road_from_instruction(char *dst, uint32_t cap, const char *instruction);

/* Case-insensitive ASCII substring test (bounded by RN_MAX_POLYLINE_LEN). */
bool rn_text_contains_ci(const char *haystack, const char *needle);

#endif /* RN_TEXT_H */
