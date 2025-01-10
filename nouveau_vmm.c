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
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "nouveau_vmm.h"
#include "nouveau_drv.h"
#include "nouveau_bo.h"
#include "nouveau_svm.h"
#include "nouveau_mem.h"

// Unmap a VMA
void nouveau_vma_unmap(struct nouveau_vma *vma) {
    if (vma->mem) {
        nvif_vmm_unmap(&vma->vmm->vmm, vma->addr);
        vma->mem = NULL;
    }
}

// Map a VMA
int nouveau_vma_map(struct nouveau_vma *vma, struct nouveau_mem *mem) {
    struct nvif_vma tmp = { .addr = vma->addr };
    int ret = nouveau_mem_map(mem, &vma->vmm->vmm, &tmp);
    if (ret)
        return ret;
    vma->mem = mem;
    return 0;
}

// Find a VMA associated with a buffer object
struct nouveau_vma *nouveau_vma_find(struct nouveau_bo *nvbo, struct nouveau_vmm *vmm) {
    struct nouveau_vma *vma;

    list_for_each_entry(vma, &nvbo->vma_list, head) {
        if (vma->vmm == vmm)
            return vma;
    }

    return NULL;
}

// Delete a VMA
void nouveau_vma_del(struct nouveau_vma **pvma) {
    struct nouveau_vma *vma = *pvma;
    if (vma && --vma->refs <= 0) {
        if (likely(vma->addr != ~0ULL)) {
            struct nvif_vma tmp = { .addr = vma->addr, .size = 1 };
            nvif_vmm_put(&vma->vmm->vmm, &tmp);
        }
        list_del(&vma->head);
        kfree(*pvma);
    }
    *pvma = NULL;
}

// Create a new VMA
int nouveau_vma_new(struct nouveau_bo *nvbo, struct nouveau_vmm *vmm, struct nouveau_vma **pvma) {
    struct nouveau_mem *mem = nouveau_mem(nvbo->bo.resource);
    struct nouveau_vma *vma;
    struct nvif_vma tmp;
    int ret;

    if ((vma = *pvma = nouveau_vma_find(nvbo, vmm))) {
        vma->refs++;
        return 0;
    }

    if (!(vma = *pvma = kmalloc(sizeof(*vma), GFP_KERNEL)))
        return -ENOMEM;

    vma->vmm = vmm;
    vma->refs = 1;
    vma->addr = ~0ULL;
    vma->mem = NULL;
    vma->fence = NULL;
    list_add_tail(&vma->head, &nvbo->vma_list);

    if (nvbo->bo.resource->mem_type != TTM_PL_SYSTEM &&
        mem->mem.page == nvbo->page) {
        ret = nvif_vmm_get(&vmm->vmm, LAZY, false, mem->mem.page, 0,
                           mem->mem.size, &tmp);
        if (ret)
            goto done;

        vma->addr = tmp.addr;
        ret = nouveau_vma_map(vma, mem);
    } else {
        ret = nvif_vmm_get(&vmm->vmm, PTES, false, mem ->mem.page, 0,
                           mem->mem.size, &tmp);
        if (ret)
            goto done;

        vma->addr = tmp.addr;
    }

done:
    if (ret)
        nouveau_vma_del(pvma);
    return ret;
}

// Finalize the VMM
void nouveau_vmm_fini(struct nouveau_vmm *vmm) {
    nouveau_svmm_fini(&vmm->svmm);
    nvif_vmm_dtor(&vmm->vmm);
    vmm->cli = NULL;
}

// Initialize the VMM
int nouveau_vmm_init(struct nouveau_cli *cli, s32 oclass, struct nouveau_vmm *vmm) {
    int ret = nvif_vmm_ctor(&cli->mmu, "drmVmm", oclass, UNMANAGED,
                            PAGE_SIZE, 0, NULL, 0, &vmm->vmm);
    if (ret)
        return ret;

    vmm->cli = cli;
    return 0;
}

// RTX-specific memory management functions
int nouveau_vmm_rtx_init(struct nouveau_vmm *vmm) {
    // Initialize RTX-specific memory types
    int ret;
    struct nvif_vmm rtx_vmm;

    ret = nvif_vmm_ctor(&vmm->cli->mmu, "drmRtxVmm", RTX_OCLASS, UNMANAGED,
                        RTX_PAGE_SIZE, 0, NULL, 0, &rtx_vmm);
    if (ret)
        return ret;

    // Set up RTX-specific configurations
    vmm->rtx_vmm = rtx_vmm;
    return 0;
}

void nouveau_vmm_rtx_fini(struct nouveau_vmm *vmm) {
    // Clean up RTX-specific resources
    nvif_vmm_dtor(&vmm->rtx_vmm);
}

// Ensure to call RTX init and fini in the appropriate places