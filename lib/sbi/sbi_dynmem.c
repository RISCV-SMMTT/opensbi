/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Xiao Xu <xiao_xu@mail.sdu.edu.cn>
 */

#include <sbi/sbi_dynmem.h>
#include <sbi/sbi_smmtt.h>
#include <sbi/sbi_heap.h>


#if __riscv_xlen == 64
unsigned long mttl3_get_mttl2(unsigned long ppn, uint64_t base)
{
    mttl3_entry_t *mttl3 = (mttl3_entry_t *)(ppn << PAGE_SHIFT);
    unsigned long index = EXTRACT_FIELD(base, PA_PN3);

    /* there is no mttl3, but at least we have a mttl3 table. */
    if (!mttl3)
        return SBI_ENOMEM;

    return mttl3[index].mttl2_ppn;
}
#endif

static int modify_1G_XM(mttl2_entry_t *mttl2, unsigned long base, smmtt_type type)
{
    unsigned long info, index;
    mttl2_entry_t *entry;

    base = base & PA_1G;
    index = EXTRACT_FIELD(base, PA_PN2);
    entry = &mttl2[index];

    info = 0;
    for (index = 0; index < MTTL2_FIELD; index++)
        info = INSERT_FIELD(info, MTT_PERM_FIELD(index), type);

    // no matter rv32 or rv64, we will modify 32 entry from 1G type to XM type*/
    for (index = 0; index < 32; index++)
    {
#if __riscv_xlen == 32  
        (entry + index)->type = TYPE_4M_PAGE;
#else
        (entry + index)->type = TYPE_2M_PAGE;
#endif
        (entry + index)->info = info;
    }

    return SBI_OK;
}

static int modify_XM_4K(mttl2_entry_t *entry, unsigned long base, smmtt_type type, unsigned long flags)
{
    mttl1_entry_t *mttl1 = NULL;
    unsigned long index, field, offset;
    perms_mttl1 perms;

    entry->type = TYPE_MTTL1_DIR;

    // Allocate new mttl1
    mttl1 = sbi_aligned_alloc_from(smmtt_hpctrl, PAGE_SIZE, PAGE_SIZE);
    if (!mttl1) {
        return SBI_ENOMEM;
    }

    entry->info = ((uintptr_t) mttl1) >> PAGE_SHIFT;
    entry->zero = 0;

    field = 0;
    for (index = 0; index < MTTL1_FIELD; index++)
        field = INSERT_FIELD(field, MTT_PERM_FIELD(index), type);

#if __riscv_xlen == 32
    for (index = 0; index < 1024; index++)
#else
    for (index = 0; index < 512; index++)
#endif
        mttl1[index] = field;

    // Determine index and offset in mttl1 that this address belongs to
    index = EXTRACT_FIELD(base, PA_PN1);
    offset = EXTRACT_FIELD(base, PA_PN0);

	// Generate the bitfield for the permissions and ensure it is not set
	field = MTT_PERM_FIELD(offset);

	// Set the new permissions
    perms = mttl1_perms_from_flags(flags);
	mttl1[index] = INSERT_FIELD(mttl1[index], field, perms);

    return SBI_OK;
}

static int modify_1G_page(mttl2_entry_t *mttl2, mttl2_entry_t *entry, unsigned long base, unsigned long size, unsigned long flags)
{
    int rc;
    smmtt_type type = entry->type;
    unsigned long offset, field, info, index;
    smmtt_xm_perms perms;

    switch (size)
    {
        case GiB:
            type = mttl2_1g_type_from_flags(flags);
            // only need to modify entry->type to MTTL2_1G_DISALLOW
            for (index = 0; index < 32; index++)
                (entry + index)->type = type;
            rc = SBI_OK;
            break;
        case XM_SIZE: 
            /* Besides this special entry,
             * we also need to modify all other entry from 1G type to XM type
             * use base & PA_1G to get the first entry of this 32 entries
             */
            modify_1G_XM(mttl2, base, type);

            offset = EXTRACT_FIELD(base, PA_XM_OFFS);
            field = MTT_PERM_FIELD(offset);
            info = entry->info;
        
            perms = xm_perms_from_flags(flags);
            info = INSERT_FIELD(info, field, perms);
            entry->info = info;

            rc = SBI_OK;
            break;
        case PAGE_SIZE:
            /*
             * modify all 32 entries to XM type
             * modify this entry from XM type to TYPE_MTTL1_DIR
             * modify this entry in MTTL1 
             */
            modify_1G_XM(mttl2, base, type);

            rc = modify_XM_4K(entry, base, type, flags);
            break;
        default:
            return SBI_EINVAL;
    }
    return rc;
}

static int modify_XM_page(mttl2_entry_t *entry, unsigned long base, unsigned long size, unsigned long flags)
{
    int rc;
    smmtt_type type = entry->type;
    unsigned long field, offset, info;
    smmtt_xm_perms perms;

    switch (size)
    {
        case XM_SIZE: 
            offset = EXTRACT_FIELD(base, PA_XM_OFFS);
            field = MTT_PERM_FIELD(offset);
            info = entry->info;
        
            perms = xm_perms_from_flags(flags);
            info = INSERT_FIELD(info, field, perms);
            entry->info = info;

            break;
        case PAGE_SIZE:
            rc = modify_XM_4K(entry, base, type, flags);
            break;
        default:
            return SBI_EINVAL;
    }
    
    return rc;
}

static int modify_4K_page(mttl2_entry_t *entry, unsigned long base, unsigned long size, unsigned long flags)
{
    unsigned long field, offset, index, mttl1_ppn;
    mttl1_entry_t *mttl1;
    perms_mttl1 perms;

    mttl1_ppn = entry->info;
    mttl1 = (mttl1_entry_t *)(mttl1_ppn << PAGE_SHIFT);

    if (size == PAGE_SIZE)
    {
        index = EXTRACT_FIELD(base, PA_PN1);
        offset = EXTRACT_FIELD(base, PA_PN0);

        perms = mttl1_perms_from_flags(flags);
        field = MTT_PERM_FIELD(offset);
        mttl1[index] = INSERT_FIELD(mttl1[index], field, perms);
    }
    else
        return SBI_EINVAL;

    return SBI_OK;
}

/* @return 0 on success and -1 on failure*/
int modify(unsigned long base, unsigned long size, unsigned long flags)
{
    int rc;
    mttp_mode_t mode;
    mttl2_entry_t *mttl2, *entry;
    unsigned long mttl2_ppn, ppn, index;

    mttp_get(&mode, NULL, &ppn);

#if __riscv_xlen == 64
    // ppn is mttl3_ppn, if __riscv_xlen == 64 and mode == SMMTT_56 
    if (mode == SMMTT_56)
        ppn = mttl3_get_mttl2(ppn, base);
#endif
    
    mttl2_ppn = ppn;
    mttl2 = (mttl2_entry_t *)(mttl2_ppn << PAGE_SHIFT);

    // no privilege for this domain at this PA, no need for modify 
    if (!mttl2)
        return SBI_OK;

    // mttl2 entry of this PA. 
    index = EXTRACT_FIELD(base, PA_PN2);
    entry = &mttl2[index];

    switch (entry->type)
    {
    case TYPE_1G_DISALLOW:
        rc = SBI_OK;
    case TYPE_1G_ALLOW_RWX:
    case TYPE_1G_ALLOW_RW:
    case TYPE_1G_ALLOW_RX:
        rc = modify_1G_page(mttl2, entry, base, size, flags);
#if __riscv_xlen == 32
    case TYPE_4M_PAGE:
#else
    case TYPE_2M_PAGE:
#endif
        rc = modify_XM_page(entry, base, size, flags);
    case TYPE_MTTL1_DIR:
        rc = modify_4K_page(entry, base, size, flags);
    }

    return rc;
}

/* @return 0 on success and -1 on failure*/
int remove(unsigned long base, unsigned long size)
{
    return modify(base, size, 0);
}

static int find_unused_entries(mttl2_entry_t *mttl2, int *index)
{
    int count = 0;
    int start = *index;
    int pos;

    // must aligned with 1G
    for (int i = 0; i < MTTL2_ENTRIES; i+=32)
    {
        if ((pos + i) >= MTTL2_ENTRIES) count = 0;

        pos = (start + i) % MTTL2_ENTRIES;
        if (mttl2[pos].info == TYPE_1G_DISALLOW)
            count++;
        else
            count = 0;

        if (count == 32)
        {
            *index = (pos - 32 + 1 + MTTL2_ENTRIES) % MTTL2_ENTRIES;
            return SBI_OK;
        }
    }
    return SBI_ENOMEM;
}

static unsigned long allocate_1G_page(mttl2_entry_t *mttl2, unsigned int sdid, unsigned long flags)
{
    int rc, index;
    unsigned long base;
    smmtt_type type;

    base = start_1G_index[sdid];

    // find free space. 
    index = EXTRACT_FIELD(base, PA_PN2);
    rc = find_unused_entries(mttl2, &index);
    if (rc)
        return 0;

    type = mttl2_1g_type_from_flags(flags);
    for (int i = 0; i < 32; i++)
        mttl2[index + i].type = type;

    // head entry of free space. 
    base = INSERT_FIELD(base, PA_PN2, index);
    start_1G_index[sdid] = base;
    return base;
}

static void get_next_base(unsigned long *base, unsigned int sdid)
{
    *base = *base > start_4K_index[sdid] ? 
        *base : start_4K_index[sdid];
    *base = *base > start_1G_index[sdid] ?
        *base + 32 * MiB : start_1G_index[sdid] + 1 * GiB;
    if (*base >= USER_MAX_ADDR) *base = USER_BASE_ADDR;
}

static unsigned long allocate_XM_page(mttl2_entry_t *mttl2, unsigned int sdid, unsigned long flags)
{
    unsigned long index, offset, field, base, info;
    mttl2_entry_t *entry;
    smmtt_type type;
    smmtt_xm_perms perms = xm_perms_from_flags(flags);

    // get the base address to search for free space. 
    base = start_1G_index[sdid];
    
#if __riscv_xlen == 32
            type = TYPE_4M_PAGE;
#else
            type = TYPE_2M_PAGE;
#endif

    do
    {
        index = EXTRACT_FIELD(base, PA_PN2);
        entry = &mttl2[index];
        
        // allocate this entry if all 32M is free.
        if (entry->type == TYPE_1G_DISALLOW)
        {
            entry->type = type;
            offset = EXTRACT_FIELD(base, PA_XM_OFFS);
            field = MTT_PERM_FIELD(offset);

            info = entry->info;
            info = INSERT_FIELD(info, field, perms);
            entry->info = info;

            return base;
        }

        // XMs has been allocated from this entry, find whether free space has left.
        else if (entry->type == type)
        {
            info = entry->info;
            for (offset = 0; offset < MTTL1_FIELD; offset++)
            {
                if (((info >> 2 * offset) & 0b11) == PERMS_XM_DISALLOW)
                {
                    field = MTT_PERM_FIELD(offset);
                    info = entry->info;
        
                    info = INSERT_FIELD(info, field, perms);
                    entry->info = info;

                    base += XM_SIZE * offset;
                    start_XM_index[sdid] = base;
                    return base;
                }
            }
        }

        // if this entry is not available, then use the least unused address as base.
        get_next_base(&base, sdid);

    } while(base != start_4K_index[sdid]);

    return 0;
}

static unsigned long allocate_4K_page(mttl2_entry_t *mttl2, unsigned int sdid, unsigned long flags)
{
    mttl1_entry_t *mttl1;
    unsigned long index, offset, field, base;
    mttl2_entry_t *entry;
    perms_mttl1 perms = mttl1_perms_from_flags(flags);

    // get the base address to search for free space. 
    base = start_1G_index[sdid];
    
    do
    {
        // get entry of 'this base address'.
        index = EXTRACT_FIELD(base, PA_PN2);
        entry = &mttl2[index];
        
        if (entry->type == TYPE_1G_DISALLOW)
        {
            mttl1 = mttl1_from_mttl2(entry);
            entry->type = TYPE_MTTL1_DIR;
        
            index = EXTRACT_FIELD(base, PA_PN1);
            offset = EXTRACT_FIELD(base, PA_PN0);

            field = MTT_PERM_FIELD(offset);
            mttl1[index] = INSERT_FIELD(mttl1[index], field, perms);

            return base;
        }

        else if (entry->type == TYPE_MTTL1_DIR)
        {
            mttl1 = mttl1_from_mttl2(entry);
            for (index = 0; index < MTTL1_ENTRIES; index++)
            {
                field = mttl1[index];
                for (offset = 0; offset < MTTL1_FIELD; offset++)
                {
                    if (((field >> 2 * offset) & 0b11) == PERMS_MTTL1_DISALLOWED)
                    {
                        field = MTT_PERM_FIELD(offset);
                        mttl1[index] = INSERT_FIELD(mttl1[index], field, perms);

                        base += (MTTL1_FIELD * PAGE_SIZE * index) + PAGE_SIZE * offset;
                        start_4K_index[sdid] = base;
                        return base;
                    }
                }
            }
        }

        // if this entry is not available, then use the least unused address as base.
        get_next_base(&base, sdid);

    } while(base != start_4K_index[sdid]);

    return 0;
}

unsigned long allocate_user_memory(unsigned long size, unsigned long flags)
{
    mttp_mode_t mode;
    unsigned int sdid;
    unsigned long ppn, index;
    mttl2_entry_t *mttl2;

    // get the base address to search for free space. 
    mttp_get(&mode, &sdid, &ppn);

#if __riscv_xlen == 64
    if (mode == SMMTT_56)
    {
        mttl3_entry_t *mttl3 = (mttl3_entry_t *)(ppn << PAGE_SHIFT);
        index = EXTRACT_FIELD(USER_BASE_ADDR, PA_PN3);

        ppn = mttl3[index].mttl2_ppn;
    }
#endif
    mttl2 = (mttl2_entry_t *)(ppn << PAGE_SHIFT);
    unsigned long addr;

    switch (size)
    {
    case GiB:
        addr = allocate_1G_page(mttl2, sdid, flags);
        break;
    case XM_SIZE:
        addr = allocate_XM_page(mttl2, sdid, flags);
        break;
    case PAGE_SIZE:
        addr = allocate_4K_page(mttl2, sdid, flags);
        break;
    default:
        break;
    }

    return addr;
}
