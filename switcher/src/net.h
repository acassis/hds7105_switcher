/*
 * net.h - UDP transport for the SWITCHER protocol (Linux sockets).
 *
 * Mirrors the original's two sockets:
 *  - control socket bound to 0.0.0.0:7000 (HDS7105_workdata::initUdp),
 *    used for unicast commands to the device and its replies;
 *  - discovery socket on an ephemeral port that broadcasts the search
 *    frame to port 7000 (SWITCHER_DATA::setSearcheQuipment).
 * Both are non-blocking and polled once per GUI frame.
 */

#ifndef SWITCHER_NET_H
#define SWITCHER_NET_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int ctl_fd;            /* bound to SW_PORT if possible           */
    int disc_fd;           /* ephemeral, SO_BROADCAST                */
    bool ctl_on_7000;      /* false if port 7000 was busy            */
    uint16_t ctl_port, disc_port;   /* our bound ports                */
    uint32_t local_ip[16];          /* our IPv4 addresses (host order) */
    int nlocal;
    uint16_t dev_port;     /* destination port, normally SW_PORT     */
    void (*log)(void *user, const char *dir, uint32_t ip, uint16_t port,
                const uint8_t *buf, size_t n);
    void *log_user;
} sw_net_t;

/* Called for every complete, valid frame.  ip is host byte order;
 * `discovery` is true for frames that arrived on the discovery socket. */
typedef void (*sw_rx_cb)(void *user, uint32_t src_ip, uint16_t src_port,
                         bool discovery, const uint8_t *frame, size_t len);

int  sw_net_open(sw_net_t *n, char *err, size_t errlen);
void sw_net_close(sw_net_t *n);

/* Unicast a frame to ip:dev_port from the control socket. */
int  sw_net_send(sw_net_t *n, uint32_t ip, const uint8_t *buf, size_t len);

/* Send the search frame to 255.255.255.255 and to every interface's
 * directed broadcast address.  Returns number of successful sends. */
int  sw_net_search_broadcast(sw_net_t *n);

/* Directed broadcast address of the local interface whose subnet holds
 * `ip`, or 255.255.255.255 if none does. */
uint32_t sw_net_bcast_for(uint32_t ip);

/* Send the search frame directly to one host (for routed networks). */
int  sw_net_search_unicast(sw_net_t *n, uint32_t ip);

/* Drain both sockets; returns number of frames delivered. */
int  sw_net_poll(sw_net_t *n, sw_rx_cb cb, void *user);

/* Helpers: host-order IPv4 <-> dotted string. */
bool sw_ip_parse(const char *s, uint32_t *ip);
void sw_ip_str(uint32_t ip, char out[16]);

#endif
