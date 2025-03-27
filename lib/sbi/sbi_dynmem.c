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
#include <sbi/sbi_hart.h>
#include <sbi/sbi_hfence.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <libfdt.h>

static unsigned long start_1G_addr[SBI_DOMAIN_MAX_INDEX];
static unsigned long start_XM_addr[SBI_DOMAIN_MAX_INDEX];
static unsigned long start_4K_addr[SBI_DOMAIN_MAX_INDEX];
static unsigned long dram_base, dram_size, dram_max;

void memory_region(struct sbi_scratch *scratch)
{
    int rc, i;
    uint64_t base, size;

    rc = fdt_path_offset((const void *)scratch->next_arg1, "/memory");

	fdt_get_node_addr_size((void *)scratch->next_arg1, rc, 0, &base, &size);

    dram_base = base;
    dram_size = size;
    dram_max = base + size;

    for(i = 0; i < SBI_DOMAIN_MAX_INDEX; i++)
    {
        start_4K_addr[i] = base;
        start_XM_addr[i] = base;
        start_1G_addr[i] = base;
    }
}

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
    unsigned long info = 0, index;
    mttl2_entry_t *entry;

    base = base & PA_1G;
    index = EXTRACT_FIELD(base, PA_PN2);
    entry = &mttl2[index];

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

    for (index = 0; index < MTTL1_ENTRIES; index++)
        mttl1[index] = field;

    // Determine index and offset in mttl1 that this address belongs to
    index = EXTRACT_FIELD(base, PA_PN1);
    offset = EXTRACT_FIELD(base, PA_PN0);

	field = MTT_PERM_FIELD(offset);

	// Set new permissions
    perms = mttl1_perms_from_flags(flags);
	mttl1[index] = INSERT_FIELD(mttl1[index], field, perms);

    return SBI_OK;
}

static int modify_1G_page(mttl2_entry_t *mttl2, mttl2_entry_t *entry, unsigned long base, unsigned long size, unsigned long flags)
{
    smmtt_type type = entry->type;
    unsigned long offset, field, info, index;
    smmtt_xm_perms perms;
    int rc = 0;

    switch (size)
    {
    case GiB:
        // only need to modify entry->type to new flags.
        type = mttl2_1g_type_from_flags(flags);
        for (index = 0; index < 32; index++)
            (entry + index)->type = type;

        return SBI_OK;
    case XM_SIZE: 
        /* Besides this special entry,
         * we also need to modify all other entry from 1G type to XM type
         */
        rc = modify_1G_XM(mttl2, base, type);

        offset = EXTRACT_FIELD(base, PA_XM_OFFS);
        field = MTT_PERM_FIELD(offset);
        info = entry->info;
    
        perms = xm_perms_from_flags(flags);
        info = INSERT_FIELD(info, field, perms);
        entry->info = info;

        return rc;
    case PAGE_SIZE:
        /*
         * modify all 32 entries to XM type
         * modify this entry from XM type to TYPE_MTTL1_DIR
         * modify this entry in MTTL1 
         */
        rc = modify_1G_XM(mttl2, base, type);
        if (rc) return rc;

        rc = modify_XM_4K(entry, base, type, flags);
        if (rc) return rc;
    default:
        return SBI_EINVAL;
    }
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

        return SBI_OK;
    case PAGE_SIZE:
        return modify_XM_4K(entry, base, type, flags);
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

        return SBI_OK;
    }
    else
        return SBI_EINVAL;
}

/* @return 0 on success and -1 on failure*/
int modify(unsigned long base, unsigned long size, unsigned long flags)
{
    smmtt_mode_t mode;
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

    if ((size & (size - 1)) != 0)
        return SBI_EINVAL;

    // no privilege for this domain at this PA, no need for modify 
    if (!mttl2)
        return SBI_OK;

    // mttl2 entry of this PA. 
    index = EXTRACT_FIELD(base, PA_PN2);
    entry = &mttl2[index];

    switch (entry->type)
    {
    case TYPE_1G_DISALLOW:
        return SBI_OK;
    case TYPE_1G_ALLOW_RWX:
    case TYPE_1G_ALLOW_RW:
    case TYPE_1G_ALLOW_RX:
        if (modify_1G_page(mttl2, entry, base, size, flags))
            return SBI_EINVAL;
        break;
#if __riscv_xlen == 32
    case TYPE_4M_PAGE:
#else
    case TYPE_2M_PAGE:
#endif
        if (modify_XM_page(entry, base, size, flags))
            return SBI_EINVAL;
        break;
    case TYPE_MTTL1_DIR:
        if (modify_4K_page(entry, base, size, flags))
            return SBI_EINVAL;
        break;
    default:
        return SBI_EINVAL;
    }

    if (misa_extension('S')) {
        __asm__ __volatile__("sfence.vma");

        /*
         * If hypervisor mode is supported, flush caching
         * structures in guest mode too.
         */
        if (misa_extension('H'))
            __sbi_hfence_gvma_all();
    }

    return SBI_OK;
}

/* @return 0 on success and -1 on failure*/
int remove(unsigned long base, unsigned long size)
{
    int rc = modify(base, size, 0);

    return rc;
}

static unsigned long allocate_1G_page(mttl2_entry_t *mttl2, unsigned int sdid, unsigned long flags, unsigned long min, unsigned long max)
{
    int count = 0;
    int start = *index;
    int pos;

    // find free space. 
    record = EXTRACT_FIELD(addr, PA_PN2);
    index = record;
    do
    {
        count = (mttl2[index].type == TYPE_1G_DISALLOW) ? count + 1 : 0;

        if (count == 32)
        {
            index = index - 31;
            break;
        }

        index = (index < max) ? index + 1 : min;
    } while (index == record);

    if (count != 32)    
    {
        return 0;
    }

    type = mttl2_1g_type_from_flags(flags);
    for (i = 0; i < 32; i++)
        mttl2[index + i].type = type;

    // head entry of free space. 
    addr = INSERT_FIELD(addr, PA_PN2, index);
    return addr;
}

static unsigned long allocate_XM_page(mttl2_entry_t *mttl2, unsigned int sdid, unsigned long flags, unsigned long min, unsigned long max)
{
    unsigned long index, offset, field, addr, info, record;
    mttl2_entry_t *entry;
    smmtt_type type;
    smmtt_xm_perms perms = xm_perms_from_flags(flags);

    // get the base address to search for free space. 
    addr = start_XM_addr[sdid];
    index = EXTRACT_FIELD(addr, PA_PN2);
    record = index;
    
#if __riscv_xlen == 32
    type = TYPE_4M_PAGE;
#else
    type = TYPE_2M_PAGE;
#endif

    do
    {
        entry = &mttl2[index];
        
        // allocate this entry if all 32M is free.
        if (entry->type == TYPE_1G_DISALLOW)
        {
            entry->type = type;
            offset = EXTRACT_FIELD(addr, PA_XM_OFFS);
            field = MTT_PERM_FIELD(offset);

            info = entry->info;
            info = INSERT_FIELD(info, field, perms);
            entry->info = info;

            return addr;
        }

        // XMs has been allocated from this entry, find whether free space has left.
        else if (entry->type == type)
        {
            info = entry->info;
            for (offset = 0; offset < MTTL1_FIELD; offset++)
            {
                if (((info >> 2 * offset) & 0b11) == PERMS_XM_DISALLOW)
                {
                    entry->info = INSERT_FIELD(entry->info, MTT_PERM_FIELD(offset), perms);
                    addr += XM_SIZE * (offset + 1);
                    return addr;
                }
            }
            index++;
        }
        else
        {
            index += (entry->type == TYPE_MTTL1_DIR) ? 1 : 32;
        }

        if (index >= max) 
        {
            index = min;
        }

    } while(index != record);

    return 0;
}

static unsigned long allocate_4K_page(mttl2_entry_t *mttl2, unsigned int sdid, unsigned long flags, unsigned long min, unsigned long max)
{
    mttl1_entry_t *mttl1;
    unsigned long index, offset, field, addr, base_index;
    mttl2_entry_t *entry;
    perms_mttl1 perms = mttl1_perms_from_flags(flags);

    // get the base address to search for free space. 
    addr = start_4K_addr[sdid];
    index = EXTRACT_FIELD(addr, PA_PN2);
    base_index = index;
    
    do
    {
        entry = &mttl2[index];      // get entry of 'this base address'.
        
        if (entry->type == TYPE_1G_DISALLOW)
        {
            entry->type = TYPE_MTTL1_DIR;
            mttl1 = mttl1_from_mttl2(entry);
        
            index = EXTRACT_FIELD(addr, PA_PN1);
            offset = EXTRACT_FIELD(addr, PA_PN0);

            field = MTT_PERM_FIELD(offset);
            mttl1[index] = INSERT_FIELD(mttl1[index], field, perms);

            return addr;
        }
        else if (entry->type == TYPE_MTTL1_DIR)
        {
            mttl1 = mttl1_from_mttl2(entry);
            
            for (index = 0; index < MTTL1_ENTRIES; index++)
            {
                field = mttl1[index];
                for (offset = 0; offset < MTTL1_FIELD; offset++)
                {
                    if (((field >> (2 * offset)) & 0b11) == PERMS_MTTL1_DISALLOWED)
                    {
                        field = MTT_PERM_FIELD(offset);
                        mttl1[index] = INSERT_FIELD(mttl1[index], field, perms);

                        addr = (addr & ~(PAGE_SIZE * MTTL1_FIELD - 1)) + 
                               (index * PAGE_SIZE * MTTL1_FIELD) + ((offset + 1) * PAGE_SIZE);
                        return addr;
                    }
                }
            }
            index++;
        }
        if (entry->type == TYPE_1G_ALLOW_RW || 
            entry->type == TYPE_1G_ALLOW_RWX ||
            entry->type == TYPE_1G_ALLOW_RX) 
        {
            index += 32;
        } 
        else
        {
            index++;
        }
    
        if (index >= max) 
            index = min;

    } while(index != base_index);

    return 0;
}

/* traverse mttl2 only */
#if __riscv_xlen == 32
unsigned long allocate(unsigned long size, unsigned long flags)
{
    unsigned long base, max;
    mttl2_entry_t *mttl2;
    
    mttl2 = (mttl2_entry_t *)(ppn << PAGE_SHIFT);
    base = EXTRACT_FIELD(dram_base, PA_PN2);
    max = EXTRACT_FIELD(dram_max, PA_PN2);
    switch (size) {
        case GiB:
            base = allocate_1G_page(mttl2, sdid, flags, base, max);
            if (base) {
                start_1G_addr[sdid] = (base + GiB) > dram_max ? (base + GiB): dram_base;
                return base;
            }
            break;
        case XM_SIZE:
            base = allocate_XM_page(mttl2, sdid, flags, base, max);
            if (base) {
                start_XM_addr[sdid] = (base + XM_SIZE) > dram_max ? (base + XM_SIZE): dram_base;
                return base;
            }
            break;
        case PAGE_SIZE:
            base = allocate_4K_page(mttl2, sdid, flags, base, max);
            if (base) {
                start_4K_addr[sdid] = (base + PAGE_SIZE) > dram_max ? (base + PAGE_SIZE): dram_base;
                return base;
            }
            break;
        default: 
            return 0;
    }
}
#else

unsigned long allocate(unsigned long size, unsigned long flags)
{
    smmtt_mode_t mode;
    unsigned int sdid;
    unsigned long ppn, base, max;
    mttl2_entry_t *mttl2;
    unsigned long record_index, increment, record = 0, *start_addr;

    if ((size & (size - 1)) != 0)
        return SBI_EINVAL;

    // get the base address to search for free space. 
    mttp_get(&mode, &sdid, &ppn);

    // Set up size-dependent variables
    switch (size) {
        case GiB:      
            start_addr = &start_1G_addr[sdid];
            increment = GiB;
            break;
        case XM_SIZE:  
            start_addr = &start_XM_addr[sdid];
            increment = XM_SIZE;
            break;
        case PAGE_SIZE: 
            start_addr = &start_4K_addr[sdid];
            increment = PAGE_SIZE;
            break;
        default: 
            return SBI_EINVAL;
    }

    // Get record and record_index
    record = *start_addr;
    record_index = EXTRACT_FIELD(record, PA_PN3);

    if (mode == SMMTT_46) {
        // For SMMTT46 mode, we only have one MTTL2 table
        mttl2 = (mttl2_entry_t *)(ppn << PAGE_SHIFT);
        base = EXTRACT_FIELD(dram_base, PA_PN2);
        max = EXTRACT_FIELD(dram_max, PA_PN2);

        switch (size) {
            case GiB:      base = allocate_1G_page(mttl2, sdid, flags, base, max); break;
            case XM_SIZE:  base = allocate_XM_page(mttl2, sdid, flags, base, max); break;
            case PAGE_SIZE: base = allocate_4K_page(mttl2, sdid, flags, base, max); break;
        }
        return base;
    }

    // SMMTT56 mode: handle multiple MTTL2 tables
    unsigned long mttl3_index = record_index;
    mttl3_entry_t *mttl3 = (mttl3_entry_t *)(ppn << PAGE_SHIFT);

    do {
        ppn = mttl3[mttl3_index].mttl2_ppn;
        mttl2 = (mttl2_entry_t *)(ppn << PAGE_SHIFT);

        base = (mttl3_index == EXTRACT_FIELD(dram_base, PA_PN3)) 
                    ? EXTRACT_FIELD(dram_base, PA_PN2) : 0;
        max = (mttl3_index == EXTRACT_FIELD(dram_max, PA_PN3)) 
                    ? EXTRACT_FIELD(dram_max, PA_PN2) : MTTL2_ENTRIES - 1;
        
        // Try allocation based on size
        switch (size) {
            case GiB:      base = allocate_1G_page(mttl2, sdid, flags, base, max); break;
            case XM_SIZE:  base = allocate_XM_page(mttl2, sdid, flags, base, max); break;
            case PAGE_SIZE: base = allocate_4K_page(mttl2, sdid, flags, base, max); break;
        }

        if (base) {
            // Update start address on successful allocation
            *start_addr = (base + increment) > dram_max ? (base + increment) : dram_base;
            return base;
        }

        if (mttl3_index == EXTRACT_FIELD(dram_max, PA_PN3)) {
            // Reset to dram_base
            *start_addr = dram_base;
            mttl3_index = EXTRACT_FIELD(dram_base, PA_PN3);
        } else {
            // Move to next index
            mttl3_index += 1;
            *start_addr = INSERT_FIELD(*start_addr, PA_PN3, mttl3_index);
        }
    } while (mttl3_index != record_index);

    // Restore record value
    *start_addr = record;
    return 0;
}
#endif