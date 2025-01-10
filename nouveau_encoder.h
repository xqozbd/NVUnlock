/*
 * Copyright (C) 2023 NVUnlock Project
 * All Rights Reserved.
 */

#ifndef __NOUVEAU_ENCODER_H__
#define __NOUVEAU_ENCODER_H__
#include <nvif/outp.h>
#include <subdev/bios/dcb.h>

#include <drm/display/drm_dp_helper.h>
#include <drm/display/drm_dp_mst_helper.h>
#include <drm/drm_encoder_slave.h>

#include "dispnv04/disp.h"

#define NV_DPMS_CLEARED 0x80

// RTX-specific Encoder Capabilities
#define RTX_ENCODER_HDMI_2_1     (1 << 0)
#define RTX_ENCODER_DP_1_4       (1 << 1)
#define RTX_ENCODER_DSC_SUPPORT  (1 << 2)
#define RTX_ENCODER_VRR_SUPPORT  (1 << 3)

// New RTX Display Features
typedef enum {
    RTX_DISPLAY_MODE_STANDARD,
    RTX_DISPLAY_MODE_HDR,
    RTX_DISPLAY_MODE_GSYNC,
    RTX_DISPLAY_MODE_SUPERRES
} rtx_display_mode;

struct nv50_head_atom;
struct nouveau_connector;
struct nvkm_i2c_port;

struct nouveau_encoder {
    struct drm_encoder_slave base;

    struct dcb_output *dcb;
    struct nvif_outp outp;
    int or;

    struct nouveau_connector *conn;

    struct i2c_adapter *i2c;

    struct drm_crtc *crtc;
    u32 ctrl;

    /* Protected by nouveau_drm.audio.lock */
    struct {
        bool enabled;
    } audio;

    struct drm_display_mode mode;
    int last_dpms;

    struct nv04_output_reg restore;

    struct {
        struct {
            bool enabled;
        } hdmi;

        struct {
            struct nv50_mstm *mstm;

            struct {
                u8 caps[DP_LTTPR_COMMON_CAP_SIZE];
                u8 nr;
            } lttpr;

            u8 dpcd[DP_RECEIVER_CAP_SIZE];

            struct nvif_outp_dp_rate rate[8];
            int rate_nr;

            int link_nr;
            int link_bw;

            struct {
                bool mst;
                u8   nr;
                u32  bw;
            } lt;

            struct mutex hpd_irq_lock;

            u8 downstream_ports[DP_MAX_DOWNSTREAM_PORTS];
            struct drm_dp_desc desc;

            u8 sink_count;
        } dp;
    };

    struct {
        bool dp_interlace : 1;
    } caps;

    // RTX-specific extensions
    struct {
        u32 capabilities;
        rtx_display_mode current_mode;
        
        struct {
            bool vrr_enabled;
            u32 min_refresh_rate;
            u32 max_refresh_rate;
        } adaptive_sync;

        struct {
            bool dsc_enabled;
            u32 dsc_version;
            u32 max_compressed_bw;
        } compression;

        struct {
            bool hdr_enabled;
            u32 max_luminance;
            u32 min_luminance;
            u32 max_content_light_level;
        } hdr;
    } rtx_features;

    void (*enc_save)(struct drm_encoder *encoder);
    void (*enc_restore)(struct drm_encoder *encoder);
    void (*update)(struct nouveau_encoder *, u8 head,
                   struct nv50_head_atom *, u8 proto, u8 depth);
};

struct nv50_mstm {
    struct nouveau_encoder *outp;

    struct drm_dp_mst_topology_mgr mgr;

    bool can_mst;
    bool is_mst;
    bool suspended;

    bool modified;
    bool disabled;
    int links;
};

struct nouveau_encoder *
find_encoder(struct drm_connector *connector, int type);

static inline struct nouveau_encoder *nouveau_encoder(struct drm_encoder *enc)
{
    struct drm_encoder_slave *slave = to_encoder_slave(enc);
    return container_of(slave, struct nouveau_encoder, base);
}

static inline struct drm_encoder *to_drm_encoder(struct nouveau_encoder *enc)
{
    return &enc->base.base;
}

static inline const struct drm_encoder_slave_funcs *
get_slave_funcs(struct drm_encoder *enc)
{
    return to_encoder_slave(enc)->slave_funcs;
}

// RTX-specific inline functions
static inline bool nouveau_rtx_is_hdmi_2_1(struct nouveau_encoder *encoder) {
    return encoder->rtx_features.capabilities & RTX_ENCODER_HDMI_2_1;
}

static inline bool nouveau_rtx_is_dp_1_4(struct nouveau_encoder *encoder) {
    return encoder->rtx_features.capabilities & RTX_ENCODER_DP_1_4;
}

// Existing function prototypes
enum nouveau_dp_status {
    NOUVEAU_DP_NONE,
    NOUVEAU_DP_SST,
    NOUVEAU_DP_MST,
};

int nouveau_dp_detect(struct nouveau_connector *, struct nouveau_encoder *);
bool nouveau_dp_train(struct nouveau_encoder *, bool mst, u32 khz, u8 bpc);
void nouveau_dp_power_down(struct nouveau_encoder *);
bool nouveau_dp_link_check(struct nouveau_connector *);
void nouveau_dp_irq(struct work_struct *);
enum drm_mode_status nv50_dp_mode_valid(struct nouveau_encoder *,
                    const struct drm_display_mode *,
                    unsigned *clock);

struct nouveau_connector *
nv50_outp_get_new_connector(struct drm_atomic_state *state, struct nouveau_encoder *outp);
struct nouveau_connector *
nv50_outp_get_old_connector(struct drm_atomic_state *state, struct nouveau_encoder *outp);

int nv50_mstm_detect(struct nouveau_encoder *encoder);
void nv50_mstm_remove(struct nv50_mstm *mstm);
bool nv50_mstm_service(struct nouveau_drm *drm,
               struct nouveau_connector *nv_connector,
               struct nv50_mstm *mstm);

#endif /* __NOUVEAU_ENCODER_H__ */