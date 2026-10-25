/*
 * rt_notify.c - phone notifications on ROTOS.
 */
#include "rt_notify.h"

#include <string.h>

#include "rn/rn_types.h"

void rt_notify_init(RtNotify *n)
{
    if (!RN_ASSERT(n != NULL)) {
        return;
    }
    (void)memset(n, 0, sizeof *n);
}

/* Ring slot of the k-th newest item. */
static uint32_t slot(const RtNotify *n, uint32_t k)
{
    return (n->head + RT_NOTIFY_MAX - 1u - k) % RT_NOTIFY_MAX;
}

void rt_notify_on(RtNotify *n, const RnNotify *msg, uint32_t now_ms)
{
    if (!RN_ASSERT(n != NULL && msg != NULL)) {
        return;
    }
    n->toast_ms = (now_ms == 0u) ? 1u : now_ms;
    if (n->count > 0u && n->items[slot(n, 0u)].id == msg->id) {
        n->items[slot(n, 0u)] = *msg;              /* same notification, updated text */
        n->rx_ms[slot(n, 0u)] = now_ms;
        return;
    }
    n->items[n->head] = *msg;
    n->rx_ms[n->head] = now_ms;
    n->head = (n->head + 1u) % RT_NOTIFY_MAX;
    n->count = (n->count < RT_NOTIFY_MAX) ? n->count + 1u : RT_NOTIFY_MAX;
    n->unread = (n->unread < RT_NOTIFY_MAX) ? n->unread + 1u : RT_NOTIFY_MAX;
    n->cursor = 0u;
}

const RnNotify *rt_notify_get(const RtNotify *n, uint32_t k, uint32_t *rx_ms)
{
    if (!RN_ASSERT(n != NULL) || k >= n->count) {
        return NULL;
    }
    if (rx_ms != NULL) {
        *rx_ms = n->rx_ms[slot(n, k)];
    }
    return &n->items[slot(n, k)];
}

const RnNotify *rt_notify_toast(const RtNotify *n, uint32_t now_ms)
{
    if (!RN_ASSERT(n != NULL) || n->toast_ms == 0u || n->count == 0u ||
        now_ms - n->toast_ms >= RT_NOTIFY_TOAST_MS) {
        return NULL;
    }
    return &n->items[slot(n, 0u)];
}

void rt_notify_hide_toast(RtNotify *n)
{
    if (RN_ASSERT(n != NULL)) {
        n->toast_ms = 0u;
    }
}

void rt_notify_open(RtNotify *n)
{
    if (RN_ASSERT(n != NULL)) {
        n->unread = 0u;
        n->cursor = 0u;
        n->toast_ms = 0u;
    }
}

void rt_notify_older(RtNotify *n)
{
    if (RN_ASSERT(n != NULL) && n->cursor + 1u < n->count) {
        n->cursor++;
    }
}

void rt_notify_newer(RtNotify *n)
{
    if (RN_ASSERT(n != NULL) && n->cursor > 0u) {
        n->cursor--;
    }
}

void rt_notify_clear(RtNotify *n)
{
    rt_notify_init(n);
}
