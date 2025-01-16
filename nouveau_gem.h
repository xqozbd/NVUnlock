/* SPDX-License-Identifier: MIT */
#ifndef __NOUVEAU_GEM_H__
#define __NOUVEAU_GEM_H__

#include <linux/types.h>
#include <linux/dma-buf.h>
#include <drm/drm_gem.h>
#include <drm/drm_device.h>
#include <drm/drm_file.h>

#include "nouveau_drv.h"
#include "nouveau_bo.h"

extern const struct drm_gem_object_funcs nouveau_gem_object_funcs;

/**
 * nouveau_gem_object - Convert DRM GEM object to Nouveau BO
 * @gem: DRM GEM object
 *
 * Safely converts a DRM GEM object to a Nouveau buffer object
 */
static inline struct nouveau_bo *
nouveau_gem_object(struct drm_gem_object *gem)
{
    return gem ? container_of(gem, struct nouveau_bo, bo.base) : NULL;
}

/* GEM object management functions */
int nouveau_gem_new(struct nouveau_cli *cli, u64 size, int align,
                    uint32_t domain, uint32_t tile_mode,
                    uint32_t tile_flags, struct nouveau_bo **pnvbo);

void nouveau_gem_object_del(struct drm_gem_object *gem);
int nouveau_gem_object_open(struct drm_gem_object *gem, struct drm_file *file_priv);
void nouveau_gem_object_close(struct drm_gem_object *gem, struct drm_file *file_priv);

/* IOCTL handlers */
int nouveau_gem_ioctl_new(struct drm_device *dev, void *data, struct drm_file *file_priv);
int nouveau_gem_ioctl_pushbuf(struct drm_device *dev, void *data, struct drm_file *file_priv);
int nouveau_gem_ioctl_cpu_prep(struct drm_device *dev, void *data, struct drm_file *file_priv);
int nouveau_gem_ioctl_cpu_fini(struct drm_device *dev, void *data, struct drm_file *file_priv);
int nouveau_gem_ioctl_info(struct drm_device *dev, void *data, struct drm_file *file_priv);

/* Prime (DMA-BUF) related functions */
int nouveau_gem_prime_pin(struct drm_gem_object *obj);
void nouveau_gem_prime_unpin(struct drm_gem_object *obj);
struct sg_table *nouveau_gem_prime_get_sg_table(struct drm_gem_object *obj);
struct drm_gem_object *nouveau_gem_prime_import_sg_table(
    struct drm_device *dev, 
    struct dma_buf_attachment *attach, 
    struct sg_table *sgt);
struct dma_buf *nouveau_gem_prime_export(struct drm_gem_object *obj, int flags);

#endif /* __NOUVEAU_GEM_H__ */