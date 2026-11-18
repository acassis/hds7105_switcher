/*
 * session.h - the device a control screen is talking to.
 */

#ifndef SWITCHER_SESSION_H
#define SWITCHER_SESSION_H

#include "net.h"
#include "devices.h"

typedef struct {
    sw_net_t *net;
    uint32_t ip;                /* switcher, host order               */
    uint8_t type, id, model;
    bool has_his;               /* streaming module paired?           */
    uint32_t his_ip;
    uint8_t his_type, his_id;
    uint32_t bcast;             /* != 0: send everything here instead
                                   (--broadcast-control)              */
} sw_session_t;

void sw_session_from_device(sw_session_t *s, sw_net_t *net,
                            const sw_device_t *d);

/* Send a ready frame to `ip` (the switcher or its streaming module), or
 * to the broadcast address in broadcast-control mode. */
int sw_session_send(sw_session_t *s, uint32_t ip, const uint8_t *f, size_t n);

/* Build helpers that send to the switcher ... */
int sw_tx_none(sw_session_t *s, uint8_t op);
int sw_tx_single(sw_session_t *s, uint8_t op, uint8_t v);
int sw_tx_double(sw_session_t *s, uint8_t op, uint8_t a, uint8_t b);
int sw_tx_u16(sw_session_t *s, uint8_t op, uint16_t v);   /* lo, hi */
int sw_tx_set_ip(sw_session_t *s, const uint8_t ip[4]);   /* op 05, mask/gw 0 */

#endif
