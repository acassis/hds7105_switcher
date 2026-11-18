/*
 * devices.c - discovery reply handling.
 */

#include "devices.h"
#include "proto.h"

#include <string.h>

void sw_devlist_clear(sw_devlist_t *l)
{
    memset(l, 0, sizeof *l);
}

const char *sw_model_name(uint8_t model)
{
    switch (model) {
    case SW_MODEL_HDS7105:      return "HDS7105";
    case SW_MODEL_HDS7106:      return "HDS7106";
    case SW_MODEL_HDS7105S:     return "HDS7105S";
    case SW_MODEL_HDS7105_2021: return "HDS7105(2021)";
    default:                    return NULL;
    }
}

/* showsearch::on_liswidget_clicked @0x703734: 21/24/25 -> set7105UI,
 * 22 -> set7106UI. */
bool sw_model_is_7105(uint8_t model)
{
    return model == SW_MODEL_HDS7105 || model == SW_MODEL_HDS7105S ||
           model == SW_MODEL_HDS7105_2021;
}

bool sw_model_is_7106(uint8_t model)
{
    return model == SW_MODEL_HDS7106;
}

/* The original "HDS7105" filter showed only model 0x21; the S/2021
 * variants use the same UI, so they are listed under it here too. */
bool sw_filter_match(sw_filter_t f, uint8_t model)
{
    switch (f) {
    case SW_FILTER_7105: return sw_model_is_7105(model);
    case SW_FILTER_7106: return sw_model_is_7106(model);
    default:             return sw_model_name(model) != NULL;
    }
}

static bool sign_eq(const uint8_t a[3], const uint8_t b[3])
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

static bool attach_his(sw_devlist_t *l, uint32_t ip, uint8_t type, uint8_t id,
                       const uint8_t sign[3])
{
    bool hit = false;
    int i;

    for (i = 0; i < l->count; i++) {
        sw_device_t *d = &l->dev[i];
        if (sign_eq(d->sign, sign)) {
            d->has_his = true;
            d->his_ip = ip;
            d->his_type = type;
            d->his_id = id;
            hit = true;
        }
    }
    return hit;
}

bool sw_devlist_handle(sw_devlist_t *l, uint32_t src_ip, const uint8_t *f,
                       size_t len)
{
    uint8_t sign[3];
    int i;

    if (len < 20)
        return false;
    sign[0] = f[len - 6];
    sign[1] = f[len - 5];
    sign[2] = f[len - 4];

    /* Switcher reply: type A5, id FF */
    if (f[4] == SW_TYPE_SWITCHER && f[5] == SW_ID_ANY) {
        sw_device_t *d = NULL;

        for (i = 0; i < l->count; i++)          /* de-duplicate by IP */
            if (l->dev[i].ip == src_ip)
                d = &l->dev[i];
        if (!d) {
            if (l->count >= SW_MAX_DEVICES)
                return false;
            d = &l->dev[l->count++];
            memset(d, 0, sizeof *d);
        }
        d->ip = src_ip;
        d->type = f[4];
        d->id = f[5];
        d->model = f[14];
        memcpy(d->sign, sign, 3);
        for (i = 0; i < l->his_count; i++)
            if (sign_eq(l->his[i].sign, sign)) {
                d->has_his = true;
                d->his_ip = l->his[i].ip;
                d->his_type = l->his[i].type;
                d->his_id = l->his[i].id;
            }
        return true;
    }

    /* Streaming module reply: type E0 or E5, id FF */
    if ((f[4] == SW_TYPE_HIS_E0 || f[4] == SW_TYPE_HIS_E5) &&
        f[5] == SW_ID_ANY) {
        for (i = 0; i < l->his_count; i++)
            if (l->his[i].ip == src_ip)
                break;
        if (i == l->his_count) {
            if (l->his_count >= SW_MAX_DEVICES)
                return attach_his(l, src_ip, f[4], f[5], sign);
            l->his_count++;
        }
        l->his[i].ip = src_ip;
        l->his[i].type = f[4];
        l->his[i].id = f[5];
        memcpy(l->his[i].sign, sign, 3);
        return attach_his(l, src_ip, f[4], f[5], sign);
    }
    return false;
}

bool sw_is_status_reply(const uint8_t *f, size_t len)
{
    if (len <= SW_HDR_LEN)
        return false;
    if (f[4] == SW_TYPE_SWITCHER)
        return f[SW_HDR_LEN] == 0x49;
    if (f[4] == SW_TYPE_HIS_E0 || f[4] == SW_TYPE_HIS_E5)
        return f[SW_HDR_LEN] == 0x24 || f[SW_HDR_LEN] == 0x6A;
    return false;
}

sw_device_t *sw_devlist_add_manual(sw_devlist_t *l, uint32_t ip, uint8_t model)
{
    sw_device_t *d = NULL;
    int i;

    for (i = 0; i < l->count; i++)
        if (l->dev[i].ip == ip)
            d = &l->dev[i];
    if (!d) {
        if (l->count >= SW_MAX_DEVICES)
            return NULL;
        d = &l->dev[l->count++];
        memset(d, 0, sizeof *d);
    }
    d->ip = ip;
    d->type = SW_TYPE_SWITCHER;     /* every switcher reply carries A5/FF */
    d->id = SW_ID_ANY;
    d->model = model;
    return d;
}
