/*
 * rt_notify.h - phone notifications on ROTOS: the last RT_NOTIFY_MAX
 * NOTIFY packets (newest first), a toast for each new one, and a cursor
 * for the notifications page.
 *
 *   NOTIFY --> stored, toast for RT_NOTIFY_TOAST_MS (C opens it)
 *   page:  B = older, C = newer, hold B = clear all
 *
 * Pure state: rt_app routes buttons here and rt_screen_notify draws it.
 */
#ifndef RT_NOTIFY_H
#define RT_NOTIFY_H

#include <stdbool.h>
#include <stdint.h>

#include "rn/rn_protocol.h"

#define RT_NOTIFY_MAX      8u
#define RT_NOTIFY_TOAST_MS 6000u

typedef struct {
    RnNotify items[RT_NOTIFY_MAX];   /* ring buffer                         */
    uint32_t rx_ms[RT_NOTIFY_MAX];
    uint32_t head;                   /* next slot to write                  */
    uint32_t count;                  /* stored, <= RT_NOTIFY_MAX            */
    uint32_t unread;                 /* arrived since the page was last seen */
    uint32_t cursor;                 /* page: 0 = newest                    */
    uint32_t toast_ms;               /* arrival of the toast, 0 = none      */
} RtNotify;

void rt_notify_init(RtNotify *n);
/* A NOTIFY packet; the same id again only refreshes the toast. */
void rt_notify_on(RtNotify *n, const RnNotify *msg, uint32_t now_ms);
/* The k-th newest (0 = newest), or NULL; *rx_ms receives its arrival time. */
const RnNotify *rt_notify_get(const RtNotify *n, uint32_t k, uint32_t *rx_ms);
/* The toast to show now, or NULL. */
const RnNotify *rt_notify_toast(const RtNotify *n, uint32_t now_ms);
void rt_notify_hide_toast(RtNotify *n);
/* The page is open: everything counts as read, the cursor starts at the newest. */
void rt_notify_open(RtNotify *n);
void rt_notify_older(RtNotify *n);
void rt_notify_newer(RtNotify *n);
void rt_notify_clear(RtNotify *n);

#endif /* RT_NOTIFY_H */
