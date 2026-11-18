/*
 * devices.h - discovered switchers and their streaming ("HIS") modules.
 * Port of SWITCHER_DATA::Get_Bind_Dev (@0x82320c) and the list logic in
 * showsearch::setBtnall/setBtn7105/setBtn7106.
 */

#ifndef SWITCHER_DEVICES_H
#define SWITCHER_DEVICES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define SW_MAX_DEVICES 128      /* DevInfo[128] in the original */

typedef struct {
    uint32_t ip;                /* host order                         */
    uint8_t type;               /* reply byte 4 (A5)                  */
    uint8_t id;                 /* reply byte 5 (FF)                  */
    uint8_t model;              /* reply byte 14 ("Dev_Child_Type")   */
    uint8_t sign[3];            /* last 3 payload bytes: pairing key  */
    bool has_his;
    uint32_t his_ip;
    uint8_t his_type, his_id;
} sw_device_t;

typedef struct {
    sw_device_t dev[SW_MAX_DEVICES];
    int count;
    /* HIS replies that arrived before their switcher */
    struct { uint32_t ip; uint8_t type, id, sign[3]; } his[SW_MAX_DEVICES];
    int his_count;
} sw_devlist_t;

typedef enum { SW_FILTER_ALL = 0, SW_FILTER_7105 = 1, SW_FILTER_7106 = 2 } sw_filter_t;

void sw_devlist_clear(sw_devlist_t *l);

/* Feed a validated discovery reply; returns true if the list changed.
 * Like the original, this does not look at the opcode: callers route
 * only frames from the discovery socket here (see sw_is_status_reply). */
bool sw_devlist_handle(sw_devlist_t *l, uint32_t src_ip, const uint8_t *f,
                       size_t len);

/* True for state/info replies that share type/id with discovery replies
 * (A5 + 0x49, E0/E5 + 0x24 or 0x6A). */
bool sw_is_status_reply(const uint8_t *f, size_t len);

/* Add a device by hand (no discovery reply), e.g. across a router. */
sw_device_t *sw_devlist_add_manual(sw_devlist_t *l, uint32_t ip, uint8_t model);

/* "HDS7105", "HDS7106", "HDS7105S", "HDS7105(2021)" or NULL if unknown. */
const char *sw_model_name(uint8_t model);
bool sw_model_is_7105(uint8_t model);   /* opens the HDS7105 control UI */
bool sw_model_is_7106(uint8_t model);   /* opens the HDS7106 control UI */
bool sw_filter_match(sw_filter_t f, uint8_t model);

#endif
