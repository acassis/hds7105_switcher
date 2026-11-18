/*
 * net.c - UDP transport for the SWITCHER protocol.
 */

#define _DEFAULT_SOURCE
#include "net.h"
#include "proto.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int make_udp(bool bind_7000, bool *bound, char *err, size_t errlen)
{
    int one = 1;
    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

    if (fd < 0) {
        snprintf(err, errlen, "socket: %s", strerror(errno));
        return -1;
    }
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof one);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    if (bind_7000) {
        struct sockaddr_in a = { 0 };
        a.sin_family = AF_INET;
        a.sin_port = htons(SW_PORT);
        a.sin_addr.s_addr = htonl(INADDR_ANY);
        *bound = bind(fd, (struct sockaddr *)&a, sizeof a) == 0;
        if (!*bound) {
            /* Fall back to an ephemeral port: replies to our source port
             * still arrive, only unsolicited traffic to :7000 is lost. */
            a.sin_port = 0;
            if (bind(fd, (struct sockaddr *)&a, sizeof a) != 0) {
                snprintf(err, errlen, "bind: %s", strerror(errno));
                close(fd);
                return -1;
            }
        }
    }
    return fd;
}

static uint16_t bound_port(int fd)
{
    struct sockaddr_in a;
    socklen_t l = sizeof a;
    return getsockname(fd, (struct sockaddr *)&a, &l) == 0 ? ntohs(a.sin_port) : 0;
}

static void load_local_addrs(sw_net_t *n)
{
    struct ifaddrs *ifa, *i;

    n->nlocal = 0;
    if (getifaddrs(&ifa) != 0)
        return;
    for (i = ifa; i && n->nlocal < 16; i = i->ifa_next)
        if (i->ifa_addr && i->ifa_addr->sa_family == AF_INET)
            n->local_ip[n->nlocal++] =
                ntohl(((struct sockaddr_in *)i->ifa_addr)->sin_addr.s_addr);
    freeifaddrs(ifa);
}

uint32_t sw_net_bcast_for(uint32_t ip)
{
    struct ifaddrs *ifa, *i;
    uint32_t b = 0xFFFFFFFFu;

    if (getifaddrs(&ifa) != 0)
        return b;
    for (i = ifa; i; i = i->ifa_next) {
        uint32_t a, m;
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || !i->ifa_netmask ||
            !(i->ifa_flags & IFF_BROADCAST) || (i->ifa_flags & IFF_LOOPBACK))
            continue;
        a = ntohl(((struct sockaddr_in *)i->ifa_addr)->sin_addr.s_addr);
        m = ntohl(((struct sockaddr_in *)i->ifa_netmask)->sin_addr.s_addr);
        if (m && (a & m) == (ip & m)) {
            b = (a & m) | ~m;
            break;
        }
    }
    freeifaddrs(ifa);
    return b;
}

int sw_net_open(sw_net_t *n, char *err, size_t errlen)
{
    bool dummy;

    memset(n, 0, sizeof *n);
    n->ctl_fd = n->disc_fd = -1;
    n->dev_port = SW_PORT;
    if (err && errlen)
        err[0] = 0;

    n->ctl_fd = make_udp(true, &n->ctl_on_7000, err, errlen);
    if (n->ctl_fd < 0)
        return -1;
    n->disc_fd = make_udp(false, &dummy, err, errlen);
    if (n->disc_fd < 0) {
        close(n->ctl_fd);
        n->ctl_fd = -1;
        return -1;
    }
    n->ctl_port = bound_port(n->ctl_fd);
    load_local_addrs(n);
    if (!n->ctl_on_7000 && err && errlen)
        snprintf(err, errlen,
                 "UDP port %d is busy (another instance?); using an ephemeral port",
                 SW_PORT);
    return 0;
}

void sw_net_close(sw_net_t *n)
{
    if (n->ctl_fd >= 0)
        close(n->ctl_fd);
    if (n->disc_fd >= 0)
        close(n->disc_fd);
    n->ctl_fd = n->disc_fd = -1;
}

static int send_to(sw_net_t *n, int fd, uint32_t ip, const uint8_t *buf,
                   size_t len)
{
    struct sockaddr_in a = { 0 };
    ssize_t r;

    a.sin_family = AF_INET;
    a.sin_port = htons(n->dev_port);
    a.sin_addr.s_addr = htonl(ip);
    r = sendto(fd, buf, len, 0, (struct sockaddr *)&a, sizeof a);
    if (n->log)
        n->log(n->log_user, r == (ssize_t)len ? "TX" : "TX!", ip, n->dev_port,
               buf, len);
    return r == (ssize_t)len ? 0 : -1;
}

int sw_net_send(sw_net_t *n, uint32_t ip, const uint8_t *buf, size_t len)
{
    if (n->ctl_fd < 0 || ip == 0)
        return -1;
    return send_to(n, n->ctl_fd, ip, buf, len);
}

static void note_disc_port(sw_net_t *n)
{
    if (!n->disc_port)
        n->disc_port = bound_port(n->disc_fd);
}

int sw_net_search_broadcast(sw_net_t *n)
{
    uint8_t f[32];
    size_t len = sw_build_search(f);
    struct ifaddrs *ifa, *i;
    uint32_t sent[32];
    int nsent = 0, ok = 0, k;

    if (send_to(n, n->disc_fd, 0xFFFFFFFFu, f, len) == 0)
        ok++;

    /* Linux routes 255.255.255.255 out of one interface only; also hit
     * each interface's directed broadcast so multi-homed hosts work. */
    if (getifaddrs(&ifa) == 0) {
        for (i = ifa; i; i = i->ifa_next) {
            uint32_t b;
            bool dup = false;

            if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET)
                continue;
            if (!(i->ifa_flags & IFF_UP) || !(i->ifa_flags & IFF_BROADCAST) ||
                (i->ifa_flags & IFF_LOOPBACK) || !i->ifa_broadaddr)
                continue;
            b = ntohl(((struct sockaddr_in *)i->ifa_broadaddr)->sin_addr.s_addr);
            if (b == 0 || b == 0xFFFFFFFFu)
                continue;
            for (k = 0; k < nsent; k++)
                dup |= sent[k] == b;
            if (dup || nsent >= 32)
                continue;
            sent[nsent++] = b;
            if (send_to(n, n->disc_fd, b, f, len) == 0)
                ok++;
        }
        freeifaddrs(ifa);
    }
    note_disc_port(n);
    return ok;
}

int sw_net_search_unicast(sw_net_t *n, uint32_t ip)
{
    uint8_t f[32];
    size_t len = sw_build_search(f);
    int r = send_to(n, n->disc_fd, ip, f, len) == 0 ? 1 : 0;
    note_disc_port(n);
    return r;
}

struct rx_ctx {
    sw_rx_cb cb;
    void *user;
    uint32_t ip;
    uint16_t port;
    bool discovery;
};

static void on_frame(void *u, const uint8_t *frame, size_t len)
{
    struct rx_ctx *c = u;
    c->cb(c->user, c->ip, c->port, c->discovery, frame, len);
}

/* Our own broadcasts come back to the socket bound to the same port. */
static bool is_self(sw_net_t *n, uint32_t ip, uint16_t port)
{
    int i;

    if (port != n->ctl_port && port != n->disc_port)
        return false;
    for (i = 0; i < n->nlocal; i++)
        if (n->local_ip[i] == ip)
            return true;
    return false;
}

static int drain(sw_net_t *n, int fd, sw_rx_cb cb, void *user)
{
    uint8_t buf[2048];
    int frames = 0, guard = 256;

    while (fd >= 0 && guard-- > 0) {
        struct sockaddr_in from;
        socklen_t fl = sizeof from;
        ssize_t r = recvfrom(fd, buf, sizeof buf, 0,
                             (struct sockaddr *)&from, &fl);
        struct rx_ctx c;

        if (r < 0)
            break;                    /* EAGAIN: nothing left */
        c.cb = cb;
        c.user = user;
        c.ip = ntohl(from.sin_addr.s_addr);
        c.port = ntohs(from.sin_port);
        c.discovery = fd == n->disc_fd;
        if (is_self(n, c.ip, c.port))
            continue;
        if (n->log)
            n->log(n->log_user, "RX", c.ip, c.port, buf, (size_t)r);
        frames += sw_parse_datagram(buf, (size_t)r, on_frame, &c);
    }
    return frames;
}

int sw_net_poll(sw_net_t *n, sw_rx_cb cb, void *user)
{
    return drain(n, n->ctl_fd, cb, user) + drain(n, n->disc_fd, cb, user);
}

bool sw_ip_parse(const char *s, uint32_t *ip)
{
    struct in_addr a;
    unsigned b0, b1, b2, b3;
    char tail;

    /* inet_aton accepts shorthand like "10.1"; require four octets. */
    if (sscanf(s, "%u.%u.%u.%u%c", &b0, &b1, &b2, &b3, &tail) != 4 ||
        b0 > 255 || b1 > 255 || b2 > 255 || b3 > 255)
        return false;
    if (inet_aton(s, &a) == 0)
        return false;
    *ip = ntohl(a.s_addr);
    return true;
}

void sw_ip_str(uint32_t ip, char out[16])
{
    snprintf(out, 16, "%u.%u.%u.%u", (ip >> 24) & 255, (ip >> 16) & 255,
             (ip >> 8) & 255, ip & 255);
}
