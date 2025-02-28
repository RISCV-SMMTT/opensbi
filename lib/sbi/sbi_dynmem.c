/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Xiao Xu <xiao_xu@mail.sdu.edu.cn>
 */

#include <sbi/sbi_dynmem.h>
#include <sbi/sbi_smmtt.c>

static physical_addr_t get_mttl2_ppn(physical_addr_t mttl3_ppn, physical_addr_t base)
{
    mttl3_entry_t *mttl3;
    mttl2_entry_t *mttl2;
    mttl3 = (mttl3_entry_t *)(mttl3_ppn << PAGE_SHIFT);
    unsigned long index = EXTRACT_FIELD(base, PA_PN3);

    if (!mttl3)
        return SBI_ENOMEM;
    
    if (mttl3[index].mttl2_ppn == 0)
    {
        mttl2 = sbi_aligned_alloc_from(smmtt_hpctrl, MTTL2_SIZE, MTTL2_SIZE);

        if (!mttl2)
            return SBI_ENOMEM;

        mttl3[index].mttl2_ppn = ((uintptr_t)mttl2) >> PAGE_SHIFT;
        mttl3[index].zero = 0;
        /* need to add mttl2 table. */
    }
    
    return mttl3[index].mttl2_ppn;
}

static mttl2_entry_t* get_mttl2_entry(const struct sbi_domain *dom,  physical_addr_t base)
{
    int rc, i;
    mttp_mode_t mode;
    unsigned int sdid;
    physical_addr_t ppn, mttl2_ppn;
    mttl2_entry_t *mttl2, *entry;
    uintptr_t index;

    mttp_get(mode, sdid, ppn);

#if __riscv_xlen == 64
    if (mode == SMMTT_56)
        mttl2_ppn = get_mttl2_ppn(ppn, base);
#endif
    
    mttl2_ppn = ppn;
    mttl2 = (mttl2_entry_t *)(mttl2_ppn << PAGE_SHIFT);

    index = EXTRACT_FIELD(base, PA_PN2);
    entry = &mttl2[index];
}

int sbi_domain_modify_1G_page(const struct sbi_domain *dom, 
        physical_addr_t base, uint64_t flags)
{
    int rc, i;
    mttl2_entry_t *entry = get_mttl2_entry(dom, base);
    mttl1_entry_t *mttl1;

    /* clean the old entry; */
    if (entry->type & 0b100 != 0)
    {
        for (i = 0; i < 32; i++)
        {
#if __riscv_xlen == 32
            if ((entry + i)->type == TYPE_4M_PAGE)
#else 
            if ((entry + i)->type == TYPE_2M_PAGE)
#endif
            {
                (entry + i)->info = 0;
                (entry + i)->type = 0;
            }
            else 
            {
                mttl1 = (mttl1_entry_t *)(entry->info << PAGE_SHIFT);
                sbi_free_from(smmtt_hpctrl, mttl1);
                (entry + i)->type = 0;
                (entry + i)->info = 0;
            }
        }
    }
    rc = add_1g_region(entry, flags, true);
    if (rc)
        return rc;

    return SBI_OK;
}

int sbi_domain_modify_XM_page(const struct sbi_domain *dom, 
        physical_addr_t base, uint64_t flags)
{
    int rc;
    mttl2_entry_t *entry = get_mttl2_entry(dom, base);
    mttl1_entry_t *mttl1;

    /* form 1G page to XM page, we cannot do this cause of alignment. */
    if (entry->type & 0b100 == 0)
        return SBI_EINVAL;

    if (entry->type == TYPE_MTTL1_DIR)
    {
        mttl1 = (mttl1_entry_t *)(entry->info << PAGE_SHIFT);
        sbi_free_from(smmtt_hpctrl, mttl1);
        entry->type = 0;
        entry->info = 0;
    }

    rc = add_xm_region(entry, base, flags, true);

    return SBI_OK;
}

int sbi_domain_modify_4K_page(const struct sbi_domain *dom, 
        physical_addr_t base, unsigned long size, uint64_t flags)
{
    int rc;
    mttl2_entry_t *entry = get_mttl2_entry(dom, base);

    /* form 1G page or XM page to 4K page, we cannot do this cause of alignment. */
    if (entry->type != TYPE_MTTL1_DIR)
        return SBI_EINVAL;

    rc = add_mttl1_region(entry, base, flags, true);

    return SBI_OK;
}
