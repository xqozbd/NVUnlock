/*
 * Copyright 2017 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#include <drm/ttm/ttm_tt.h>

#include "nouveau_mem.h"
#include "nouveau_drv.h"
#include "nouveau_bo.h"

#include <nvif/class.h>
#include <nvif/if000a.h>
#include <nvif/if500b.h>
#include <nvif/if500d.h>
#include <nvif/if900b.h>
#include <nvif/if900d.h>

#define RTX_MEM_KIND_MASK        0x0F
#define RTX_MEM_COMPRESSION_MASK 0xF0

int
nouveau_mem_map(struct nouveau_mem *mem,
                struct nvif_vmm *vmm, struct nvif_vma *vma)
{
    union {
        struct nv50_vmm_map_v0 nv50;
        struct gf100_vmm_map_v0 gf100;
        struct rtx_vmm_map_v0 {
            u8 version;
            u8 ro;
            u8 priv;
            u8 kind;
            u8 comp;
            u8 vol;
        } rtx;
    } args;
    u32 argc = 0;

    switch (vmm->object.oclass) {
    case NVIF_CLASS_VMM_NV04:
        break;
    case NVIF_CLASS_VMM_NV50:
        args.nv50.version = 0;
        args.nv50.ro = 0;
        args.nv50.priv = 0;
        args.nv50.kind = mem->kind;
        args.nv50.comp = mem->comp;
        argc = sizeof(args.nv50);
        break;
    case NVIF_CLASS_VMM_GF100:
    case NVIF_CLASS_VMM_GM200:
    case NVIF_CLASS_VMM_GP100:
    case NVIF_CLASS_VMM_RTX:
        args.rtx.version = 0;
        args.rtx.vol = (mem->mem.type & NVIF_MEM_VRAM) ? 0 : 1;
        args.rtx.ro = 0;
        args.rtx.priv = 0;
        args.rtx.kind = mem->kind & RTX_MEM_KIND_MASK;
        args.rtx.comp = mem->comp & RTX_MEM_COMPRESSION_MASK;
        argc = sizeof(args.rtx);
        break;
    default:
        WARN_ON(1);
        return -ENOSYS;
    }

    return nvif_vmm_map(vmm, vma->addr, mem->mem.size, &args, argc, &mem->mem, 0);
}

void
nouveau_mem_fini(struct nouveau_mem *mem)
{
    nvif_vmm_put(&mem->drm->client.vmm.vmm, &mem->vma[1]);
    nvif_vmm_put(&mem->drm->client.vmm.vmm, &mem->vma[0]);
    mutex_lock(&mem->drm->client_mutex);
    nvif_mem_dtor(&mem->mem);
    mutex_unlock(&mem->drm->client_mutex);
}

int
nouveau_mem_host(struct ttm_resource *reg, struct ttm_tt *tt)
{
    struct nouveau_mem *mem = nouveau_mem(reg);
    struct nouveau_drm *drm = mem->drm;
    struct nvif_mmu *mmu = &drm->mmu;
    struct nvif_mem_ram_v0 args = {};
    u8 type;
    int ret;

    // Enhanced type selection for RTX cards
    if (drm->client.device.info.family >= NV_DEVICE_INFO_V0_RTX) {
        type = drm->ttm.type_host[!!mem->kind] | NVIF_MEM_KIND;
    } else {
        type = !nouveau_drm_use_coherent_gpu_mapping(drm) ?
               drm->ttm.type_ncoh[!!mem->kind] :
               drm->ttm.type_host[0];
    }

    if (mem->kind && !(mmu->type[type].type & NVIF_MEM_KIND))
        mem->comp = mem->kind = 0;
    
    if (mem->comp && !(mmu->type[type].type & NVIF_MEM_COMP)) {
        if (mmu->object.oclass >= NVIF_CLASS_MMU_GF100)
            mem->kind = mmu->kind[mem->kind] & RTX_MEM_KIND_MASK;
        mem->comp = 0;
    }

    if (tt->sg)
        args.sgl = tt->sg->sgl;
    else
        args.dma = tt->dma_address;

    mutex_lock(&drm->client_mutex);
    ret = nvif_mem_ctor_type(mmu, "ttmHostMem", mmu->mem, type, PAGE_SHIFT,
                             reg->size, &args, sizeof(args), &mem->mem);
    mutex_unlock(&drm->client_mutex);
    return ret;
}

int
nouveau_mem_vram(struct ttm_resource *reg, bool contig, u8 page)
{
    struct nouveau_mem *mem = nouveau_mem(reg);
    struct nouveau_drm *drm = mem->drm;
    struct nvif_mmu *mmu = &drm->mmu;
    u64 size = ALIGN(reg->size, 1 << page);
    int ret;

    mutex_lock(&drm->client_mutex);
    switch (mmu->mem) {
    case NVIF_CLASS_MEM_GF100:
        ret = nvif_mem_ctor_type(mmu, "ttmVram", mmu->mem,
                                 drm->ttm.type_vram, page, size,
                                 &(struct gf100_mem_v0) {
                                     .contig = contig,
                                 }, sizeof(struct gf100_mem_v0),
                                 &mem->mem);
        break;
    case NVIF_CLASS_MEM_NV50:
        ret = nvif_mem_ctor_type(mmu, "ttmVram", mmu->mem,
                                 drm->ttm.type_vram, page, size,
                                 &(struct nv50_mem_v0) {
                                     .bankswz = mmu->kind[mem->kind] == 2,
                                     .contig = contig,
                                 }, sizeof(struct nv50_mem_v0),
                                 &mem->mem);
        break;
    case NVIF_CLASS_MEM_NV04:
        ret = nvif_mem_ctor_type(mmu, "ttmVram", mmu->mem,
                                 drm->ttm.type_vram, page, size,
                                 &(struct nv04_mem_v0) {
                                     .contig = contig,
                                 }, sizeof(struct nv04_mem_v0),
                                 &mem->mem);
        break;
    default:
        ret = -EINVAL;
        break;
    }
    mutex_unlock(&drm->client_mutex);
    return ret;
}

void
nouveau_mem_free(struct nouveau_mem *mem)
{
    if (!mem)
        return;

    nouveau_mem_fini(mem);
    kfree(mem);
}

bool
nouveau_mem_validate(struct ttm_resource *res, const struct ttm_place *place, size_t size)
{
    u32 num_pages = PFN_UP(size);

    // Enhanced validation for memory resources
    if (res->start < place->fpfn ||
        (place->lpfn && (res->start + num_pages) > place->lpfn))
        return false;

    return true;
}

int
nouveau_mem_init(struct nouveau_drm *drm)
{
    // Initialize memory management features
    drm->mem_manager.max_allocations = 1024;
    drm->mem_manager.default_page_size = PAGE_SIZE;
    drm->mem_manager.supported_compression_levels = 
        (1 << RTX_COMP_NONE) | 
        (1 << RTX_COMP_STANDARD) | 
        (1 << RTX_COMP_HIGH) | 
        (1 << RTX_COMP_LOSSLESS);

    return 0;
}

void
nouveau_mem_cleanup(struct nouveau_drm *drm)
{
    // Cleanup memory management resources
    memset(&drm->mem_manager, 0, sizeof(drm->mem_manager));
}