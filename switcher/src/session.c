/*
 * session.c - send helpers bound to one switcher.
 */

#include "session.h"
#include "proto.h"

#include <string.h>

void sw_session_from_device(sw_session_t *s, sw_net_t *net,
                            const sw_device_t *d)
{
    memset(s, 0, sizeof *s);
    s->net = net;
    s->ip = d->ip;
    s->type = d->type;
    s->id = d->id;
    s->model = d->model;
    s->has_his = d->has_his;
    s->his_ip = d->his_ip;
    s->his_type = d->his_type;
    s->his_id = d->his_id;
}

int sw_session_send(sw_session_t *s, uint32_t ip, const uint8_t *f, size_t n)
{
    if (ip == 0)
        return -1;
    return sw_net_send(s->net, s->bcast ? s->bcast : ip, f, n);
}

int sw_tx_none(sw_session_t *s, uint8_t op)
{
    uint8_t f[32];
    return sw_session_send(s, s->ip, f, sw_build_none(f, s->type, s->id, op));
}

int sw_tx_single(sw_session_t *s, uint8_t op, uint8_t v)
{
    uint8_t f[32];
    return sw_session_send(s, s->ip, f,
                       sw_build_single(f, s->type, s->id, op, v));
}

int sw_tx_double(sw_session_t *s, uint8_t op, uint8_t a, uint8_t b)
{
    uint8_t f[32];
    return sw_session_send(s, s->ip, f,
                       sw_build_double(f, s->type, s->id, op, a, b));
}

int sw_tx_u16(sw_session_t *s, uint8_t op, uint16_t v)
{
    return sw_tx_double(s, op, (uint8_t)(v & 0xFF), (uint8_t)(v >> 8));
}

/* HDS7105_workdata::SendCMD_SetIP @0x8d08dc / HDS7106_workdata::SendCMD_IP
 * @0x8d46e7: only the address is filled in, mask and gateway stay 0. */
int sw_tx_set_ip(sw_session_t *s, const uint8_t ip[4])
{
    static const uint8_t zero[4];
    uint8_t f[32];
    size_t n = sw_build_ipcom(f, s->type, s->id, 0x05, ip, zero, zero);
    return sw_session_send(s, s->ip, f, n);
}
