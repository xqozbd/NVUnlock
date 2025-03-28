/* SPDX-License-Identifier: MIT */
#ifndef __NOUVEAU_ABI16_H__
#define __NOUVEAU_ABI16_H__

#define ABI16_IOCTL_ARGS                                                       \
	struct drm_device *dev, void *data, struct drm_file *file_priv

int nouveau_abi16_ioctl_getparam(ABI16_IOCTL_ARGS);
int nouveau_abi16_ioctl_channel_alloc(ABI16_IOCTL_ARGS);
int nouveau_abi16_ioctl_channel_free(ABI16_IOCTL_ARGS);
int nouveau_abi16_ioctl_grobj_alloc(ABI16_IOCTL_ARGS);
int nouveau_abi16_ioctl_notifierobj_alloc(ABI16_IOCTL_ARGS);
int nouveau_abi16_ioctl_gpuobj_free(ABI16_IOCTL_ARGS);

struct nouveau_abi16_ntfy {
	struct nvif_object object;
	struct list_head head;
	struct nvkm_mm_node *node;
};

struct nouveau_abi16_chan {
	struct list_head head;
	struct nouveau_channel *chan;
	struct nvif_object ce;
	struct list_head notifiers;
	struct nouveau_bo *ntfy;
	struct nouveau_vma *ntfy_vma;
	struct nvkm_mm heap;
	struct nouveau_sched *sched;
};

struct nouveau_abi16 {
	struct nouveau_cli *cli;
	struct list_head channels;
	struct list_head objects;
};

struct nouveau_abi16 *nouveau_abi16_get(struct drm_file *);
int nouveau_abi16_put(struct nouveau_abi16 *, int);
void nouveau_abi16_fini(struct nouveau_abi16 *);
s32 nouveau_abi16_swclass(struct nouveau_drm *);
int nouveau_abi16_ioctl(struct drm_file *, void __user *user, u32 size);

#define NOUVEAU_GEM_DOMAIN_VRAM      (1 << 1)
#define NOUVEAU_GEM_DOMAIN_GART      (1 << 2)

struct drm_nouveau_grobj_alloc {
	int      channel;
	uint32_t handle;
	int      class;
};

struct drm_nouveau_setparam {
	uint64_t param;
	uint64_t value;
};

#define DRM_IOCTL_NOUVEAU_SETPARAM           DRM_IOWR(DRM_COMMAND_BASE + DRM_NOUVEAU_SETPARAM, struct drm_nouveau_setparam)
#define DRM_IOCTL_NOUVEAU_GROBJ_ALLOC        DRM_IOW (DRM_COMMAND_BASE + DRM_NOUVEAU_GROBJ_ALLOC, struct drm_nouveau_grobj_alloc)
#define DRM_IOCTL_NOUVEAU_NOTIFIEROBJ_ALLOC  DRM_IOWR(DRM_COMMAND_BASE + DRM_NOUVEAU_NOTIFIEROBJ_ALLOC, struct drm_nouveau_notifierobj_alloc)
#define DRM_IOCTL_NOUVEAU_GPUOBJ_FREE        DRM_IOW (DRM_COMMAND_BASE + DRM_NOUVEAU_GPUOBJ_FREE, struct drm_nouveau_gpuobj_free)

#endif /* __NOUVEAU_ABI16_H__ */