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
#include <sbi/sbi_console.h>

static unsigned long start_1G_addr;
static unsigned long start_XM_addr;
static unsigned long start_4K_addr;
static unsigned long dram_base, dram_size, dram_max;

bool check_mem(struct sbi_domain_memregion *reg)
{
    unsigned long base, size;
    base = reg->base;
    size = reg->size;
    if (size == 0)  return false;
    if (base >= dram_base && base < dram_max)
        return true;
    
    return false;
}

void memory_region(struct sbi_scratch *scratch)
{
    int rc;
    uint64_t base, size;

    rc = fdt_path_offset((const void *)scratch->next_arg1, "/memory");

    fdt_get_node_addr_size((void *)scratch->next_arg1, rc, 0, &base, &size);

    dram_base = base;
    dram_size = size;
    dram_max = base + size;

    start_4K_addr = base + size - 32 * MiB;
    start_XM_addr = base + size - 32 * MiB;
    start_1G_addr = base;
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
// now we cannot do this, because every XM entry can be used by multiple domains.
// static int modify_XM_4K(mttl2_entry_t *entry, unsigned long base, smmtt_type type, unsigned long flags)
// {
//     mttl1_entry_t *mttl1 = NULL;
//     unsigned long index, field, offset;
//     perms_mttl1 perms;

//     entry->type = TYPE_MTTL1_DIR;

//     // Allocate new mttl1
//     mttl1 = sbi_aligned_alloc_from(smmtt_hpctrl, PAGE_SIZE, PAGE_SIZE);
//     if (!mttl1) {
//         return SBI_ENOMEM;
//     }

//     entry->info = ((uintptr_t)mttl1) >> PAGE_SHIFT;
//     entry->zero = 0;

//     field = 0;
//     for (index = 0; index < MTTL1_FIELD; index++)
//         field = INSERT_FIELD(field, MTT_PERM_FIELD(index), type);

//     for (index = 0; index < MTTL1_ENTRIES; index++)
//         mttl1[index] = field;

//     // Determine index and offset in mttl1 that this address belongs to
//     index = EXTRACT_FIELD(base, PA_PN1);
//     offset = EXTRACT_FIELD(base, PA_PN0);

//     field = MTT_PERM_FIELD(offset);

//     // Set new permissions
//     perms = mttl1_perms_from_flags(flags);
//     mttl1[index] = INSERT_FIELD(mttl1[index], field, perms);

//     return SBI_OK;
// }

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
       return SBI_EINVAL;
        // rc = modify_1G_XM(mttl2, base, type);
        // if (rc) return rc;

        // rc = modify_XM_4K(entry, base, type, flags);
        // if (rc) return rc;
    default:
        return SBI_EINVAL;
    }
}

static int modify_XM_page(mttl2_entry_t *entry, unsigned long base, unsigned long size, unsigned long flags)
{
    int rc;
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
        return SBI_EINVAL;
    // return modify_XM_4K(entry, base, type, flags);
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
    mttl2_entry_t *mttl2, *entry, *Np1_mttl2;
    unsigned long ppn, index, Np1_ppn;

    mttp_get(&mode, NULL, &ppn);

#if __riscv_xlen == 64
    // ppn is mttl3_ppn, if __riscv_xlen == 64 and mode == SMMTT_56 
    if (mode == SMMTT_56)
    {
        ppn = mttl3_get_mttl2(ppn, base);
        mttl3_entry_t *mttl3 = Np1;
        index = EXTRACT_FIELD(base, PA_PN3);
        Np1_ppn = mttl3[index].mttl2_ppn;

        Np1_mttl2 = (mttl2_entry_t *)(Np1_ppn << PAGE_SHIFT);
        mttl2 = (mttl2_entry_t *)(ppn << PAGE_SHIFT);
    }
    else
    {
        mttl2 = (mttl2_entry_t *)(ppn << PAGE_SHIFT);
        Np1_mttl2 = (mttl2_entry_t *)Np1;
    }
#endif
#if __riscv_xlen == 32
    mttl2 = (mttl2_entry_t *)(ppn << PAGE_SHIFT);
    Np1_mttl2 = (mttl2_entry_t *)Np1;
#endif

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
        if (modify_1G_page(mttl2, entry, base, size, flags) || modify_1G_page(Np1_mttl2, &Np1_mttl2[index], base, size, flags))
            return SBI_EINVAL;
        break;
#if __riscv_xlen == 32
    case TYPE_4M_PAGE:
#else
    case TYPE_2M_PAGE:
#endif
        if (modify_XM_page(entry, base, size, flags) || modify_XM_page(&Np1_mttl2[index], base, size, flags))
            return SBI_EINVAL;
        break;
    case TYPE_MTTL1_DIR:
        if (modify_4K_page(entry, base, size, flags) || modify_4K_page(&Np1_mttl2[index], base, size, flags))
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

static unsigned long allocate_1G_page(mttl2_entry_t *Np1_mttl2, mttl2_entry_t *mttl2, unsigned int sdid, unsigned long flags, unsigned long min, unsigned long max)
{
    unsigned long index, addr, count, record;
    count = 0;
    addr = start_1G_addr;

    // find free space. 
    record = EXTRACT_FIELD(addr, PA_PN2);
    index = record;
    sbi_printf("initial index %ld\n", index);
    do
    {
        count = (Np1_mttl2[index].type == TYPE_1G_DISALLOW) ? count + 1 : 0;

        if (count == 32)
        {
            index = index - 31;
            addr = addr - GiB + 32 * MiB;
            break;
        }

        index = (index < max) ? index + 1 : min;
        addr = (index < max) ? addr + 32 * MiB : start_1G_addr;
    } while (index != record);

    if (count != 32)    
    {
        return 0;
    }
    sbi_printf("Allocate 1G page at %lx, index: %ld\n", addr, index);

    add_1g_region(&Np1_mttl2[index], flags);
    add_1g_region(&mttl2[index], flags);
    // set the type of this entry to 1G page.

    // head entry of free space. 
    // addr = INSERT_FIELD(addr, PA_PN2, index);
    return addr;
}

static unsigned long allocate_XM_page(mttl2_entry_t *Np1_mttl2, mttl2_entry_t *mttl2, unsigned int sdid, unsigned long flags, unsigned long min, unsigned long max)
{
    unsigned long index, offset, addr, info, record;
    mttl2_entry_t *entry;
    smmtt_type type;
    smmtt_xm_perms perms = xm_perms_from_flags(flags);

    // get the base address to search for free space. 
    addr = start_XM_addr;
    index = EXTRACT_FIELD(addr, PA_PN2);
    record = index;
    
#if __riscv_xlen == 32
    type = TYPE_4M_PAGE;
#else
    type = TYPE_2M_PAGE;
#endif

    do
    {
        entry = &Np1_mttl2[index];
        
        // allocate this entry if all 32M is free.
        if (entry->type == TYPE_1G_DISALLOW)
        {
            add_xm_region(entry, addr, flags);
            add_xm_region(&mttl2[index], addr, flags);

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
                    mttl2[index].info = INSERT_FIELD(mttl2[index].info, MTT_PERM_FIELD(offset), perms);
                    return (addr + XM_SIZE * offset);
                }
            }
            index--;
            addr -= 32 * MiB;
        }
        else
        {
            index -= (entry->type == TYPE_MTTL1_DIR) ? 1 : 32;
            addr -= (entry->type == TYPE_MTTL1_DIR) ? 32 * MiB : GiB;
        }

        if (index <= min) 
        {
            index = max;
            addr = start_XM_addr;
        }

    } while(index != record);

    return 0;
}

int add_mttl1_entry(mttl2_entry_t *entry, unsigned long base, unsigned long flags)
{
	unsigned long index, offset, field, tmp = 0;
	perms_mttl1 perms;
	mttl1_entry_t *mttl1;

	MTTL2_FIELD_ENSURE_EQUAL(entry, type, TYPE_MTTL1_DIR);

	// Allocate or get an existing mttl1 table
	mttl1 = mttl1_from_mttl2(entry);
	if (!mttl1) {
		// Failed to allocate, reset entry
		entry->info = 0;
		entry->type = 0;
		entry->zero = 0;
		return SBI_ENOMEM;
	}

    perms = mttl1_perms_from_flags(flags);
	// Determine index and offset in mttl1 that this address belongs to
	index = EXTRACT_FIELD(base, PA_PN1);
    for (offset = 0; offset < MTTL1_FIELD; offset++)
    {
        field = MTT_PERM_FIELD(offset);
        ENSURE_ZERO(EXTRACT_FIELD(mttl1[index], field));
        mttl1[index] = INSERT_FIELD(mttl1[index], field, perms);
        tmp = tmp << 2 | perms;
    }
    // ENSURE_ZERO(EXTRACT_FIELD(mttl1[index], field));
    // mttl1[index] = INSERT_FIELD(mttl1[index], field, tmp);
	return SBI_OK;
}

static unsigned long allocate_4K_page(mttl2_entry_t *Np1_mttl2, mttl2_entry_t *mttl2, unsigned int sdid, unsigned long flags, unsigned long min, unsigned long max)
{
    mttl1_entry_t *Np1_mttl1;
    mttl1_entry_t *mttl1;
    unsigned long index, offset, field, addr, base_index, Np1_field, tmp = 0, i;
    mttl2_entry_t *entry;
    perms_mttl1 perms = mttl1_perms_from_flags(flags);

    // get the base address to search for free space. 
    addr = start_4K_addr;
    index = EXTRACT_FIELD(addr, PA_PN2);
    base_index = index;
    
    do
    {
        entry = &Np1_mttl2[index];      // get entry of 'this base address'.
        
        if (entry->type == TYPE_1G_DISALLOW)
        {
            add_mttl1_region(&mttl2[index], addr, flags);       // domain's MTT, initialize as normal.
            add_mttl1_entry(entry, addr, flags);       // N+1 MTT, initialize as normal.
            
            return start_4K_addr - index * 32 * MiB;
        }
        else if (entry->type == TYPE_MTTL1_DIR)
        {
            Np1_mttl1 = mttl1_from_mttl2(entry);
            mttl1 = mttl1_from_mttl2(&mttl2[index]);
            
            for (i = 0; i < MTTL1_ENTRIES; i++)
            {
                Np1_field = Np1_mttl1[i];
                field = mttl1[i];
                perms = mttl1_perms_from_flags(flags);
                if (Np1_field == 0)     // have not been used.
                {
                    // Determine index and offset in mttl1 that this address belongs to
                    for (offset = 0; offset < MTTL1_FIELD; offset++)
                    {
                        tmp = tmp << 2 | perms;
                    }
                    Np1_mttl1[i] = INSERT_FIELD(Np1_mttl1[i], field, tmp);
                    mttl1[i] = INSERT_FIELD(mttl1[i], field, perms);
                    return addr + i * 64 * KiB;
                }
                else                    // have been used.
                {
                    if (field == 0)     // used by other domain.
                        continue;
                    else                // used by this domain.
                    {
                        for (offset = 0; offset < MTTL1_FIELD; offset++)
                        {
                            if (((field >> (2 * offset)) & 0b11) == PERMS_MTTL1_DISALLOWED)
                            {
                                field = MTT_PERM_FIELD(offset);
                                mttl1[i] = INSERT_FIELD(mttl1[i], field, perms);
        
                                return addr + i * 64 * KiB + offset * PAGE_SIZE;
                            }
                        }
                    }
                }
            }
            index--;
            addr -= 32 * MiB;
        }
        if (entry->type == TYPE_1G_ALLOW_RW || 
            entry->type == TYPE_1G_ALLOW_RWX ||
            entry->type == TYPE_1G_ALLOW_RX) 
        {
            index -= 32;
            addr -= GiB;
        } 
        else
        {
            index--;
            addr -= 32 * MiB;
        }
    
        if (index <= min) 
        {
            index = max;
            addr = start_4K_addr;
        }

    } while(index != base_index);

    return 0;
}

unsigned long allocate_with_fallback(unsigned long (*alloc_fn)(void *, void *, int, int, unsigned long, unsigned long),
                                  void *Np1_mtt, void *mtt, int sdid, int flags,
                                  unsigned long base, unsigned long max,
                                  unsigned long increment, unsigned long dram_base, unsigned long dram_max,
                                  unsigned long *start_addr, unsigned long *mttl3_index,
                                  bool is_forward) {
    unsigned long result = alloc_fn(Np1_mtt, mtt, sdid, flags, base, max);
    if (result) {
        *start_addr = is_forward
                        ? ((result + increment) > dram_max ? (result + increment) : dram_base)
                        : ((result - increment) < dram_base ? (result - increment) : dram_max);
        return result;
    }

    unsigned long boundary = is_forward ? dram_max : dram_base;
    unsigned long other_side = is_forward ? dram_base : dram_max;
    uint64_t index_field = EXTRACT_FIELD(boundary, PA_PN3);
    
    if (*mttl3_index == index_field) {
        *start_addr = other_side;
        *mttl3_index = EXTRACT_FIELD(other_side, PA_PN3);
    } else {
        *mttl3_index += is_forward ? 1 : -1;
        *start_addr = INSERT_FIELD(*start_addr, PA_PN3, *mttl3_index);
    }

    return 0;
}
unsigned long wrapped_allocate_1G_page(void *Np1_mtt, void *mtt, int sdid, int flags, unsigned long base, unsigned long max) {
    return allocate_1G_page((mttl2_entry_t *)Np1_mtt, (mttl2_entry_t *)mtt, (unsigned int)sdid, (unsigned long)flags, base, max);
}

unsigned long wrapped_allocate_XM_page(void *Np1_mtt, void *mtt, int sdid, int flags, unsigned long base, unsigned long max) {
    return allocate_XM_page((mttl2_entry_t *)Np1_mtt, (mttl2_entry_t *)mtt, (unsigned int)sdid, (unsigned long)flags, base, max);
}

unsigned long wrapped_allocate_4K_page(void *Np1_mtt, void *mtt, int sdid, int flags, unsigned long base, unsigned long max) {
    return allocate_4K_page((mttl2_entry_t *)Np1_mtt, (mttl2_entry_t *)mtt, (unsigned int)sdid, (unsigned long)flags, base, max);
}

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
            start_addr = &start_1G_addr;
            increment = GiB;
            break;
        case XM_SIZE:  
            start_addr = &start_XM_addr;
            increment = XM_SIZE;
            break;
        case PAGE_SIZE: 
            start_addr = &start_4K_addr;
            increment = PAGE_SIZE;
            break;
        default: 
            return SBI_EINVAL;
    }

    // Get record and record_index
    record = *start_addr;
    record_index = EXTRACT_FIELD(record, PA_PN3);

#if __riscv_xlen == 64
    if (mode == SMMTT_46) 
    {
#endif
        // For SMMTT46 mode, we only have one MTTL2 table
        mttl2 = (mttl2_entry_t *)(ppn << PAGE_SHIFT);
        base = EXTRACT_FIELD(dram_base, PA_PN2);
        max = EXTRACT_FIELD(dram_max, PA_PN2);

        switch (size) {
            case GiB:      base = allocate_1G_page(Np1, mttl2, sdid, flags, base, max); break;
            case XM_SIZE:  base = allocate_XM_page(Np1, mttl2, sdid, flags, base, max); break;
            case PAGE_SIZE: base = allocate_4K_page(Np1, mttl2, sdid, flags, base, max); break;
        }
        return base;
#if __riscv_xlen == 64
    }
#endif

// #if __riscv_xlen == 64
    // SMMTT56 mode: handle multiple MTTL2 tables
    unsigned long mttl3_index = record_index;
    mttl2_entry_t *Np1_mttl2;
    mttl3_entry_t *Np1_mttl3 = (mttl3_entry_t *)(Np1);
    mttl3_entry_t *mttl3 = (mttl3_entry_t *)(ppn << PAGE_SHIFT);

    do {
        ppn = Np1_mttl3[mttl3_index].mttl2_ppn;
        Np1_mttl2 = (mttl2_entry_t *)(ppn << PAGE_SHIFT);
        ppn = mttl3[mttl3_index].mttl2_ppn;
        mttl2 = (mttl2_entry_t *)(ppn << PAGE_SHIFT);

        base = (mttl3_index == EXTRACT_FIELD(dram_base, PA_PN3)) 
                    ? EXTRACT_FIELD(dram_base, PA_PN2) : 0;
        max = (mttl3_index == EXTRACT_FIELD(dram_max, PA_PN3)) 
                    ? EXTRACT_FIELD(dram_max, PA_PN2) : MTTL2_ENTRIES - 1;

        switch (size) {
            case GiB:
                return allocate_with_fallback(wrapped_allocate_1G_page, Np1_mttl2, mttl2, sdid, flags, base, max,
                                                increment, dram_base, dram_max, start_addr, &mttl3_index, true);
            case XM_SIZE:
                return allocate_with_fallback(wrapped_allocate_XM_page, Np1_mttl2, mttl2, sdid, flags, base, max,
                                                increment, dram_base, dram_max, start_addr, &mttl3_index, false);
            case PAGE_SIZE:
                return allocate_with_fallback(wrapped_allocate_4K_page, Np1_mttl2, mttl2, sdid, flags, base, max,
                                                increment, dram_base, dram_max, start_addr, &mttl3_index, false);
        }
        // // Try allocation based on size
        // switch (size) {
        //     case GiB:
        //         base = allocate_1G_page(mttl2, sdid, flags, base, max);
        //         if (base) {
        //             // Update start address on successful allocation
        //             *start_addr = (base + increment) > dram_max ? (base + increment) : dram_base;
        //             return base;
        //         }
        
        //         if (mttl3_index == EXTRACT_FIELD(dram_max, PA_PN3)) {
        //             // Reset to dram_base
        //             *start_addr = dram_base;
        //             mttl3_index = EXTRACT_FIELD(dram_base, PA_PN3);
        //         } else {
        //             // Move to next index
        //             mttl3_index += 1;
        //             *start_addr = INSERT_FIELD(*start_addr, PA_PN3, mttl3_index);
        //         }
        //         break;
        //     case XM_SIZE:
        //         base = allocate_XM_page(mttl2, sdid, flags, base, max);
        //         if (base) {
        //             // Update start address on successful allocation
        //             *start_addr = (base - increment) < dram_base ? (base - increment) : dram_max;
        //             return base;
        //         }
        
        //         if (mttl3_index == EXTRACT_FIELD(dram_base, PA_PN3)) {
        //             // Reset to dram_max
        //             *start_addr = dram_max;
        //             mttl3_index = EXTRACT_FIELD(dram_max, PA_PN3);
        //         } else {
        //             // Move to next index
        //             mttl3_index -= 1;
        //             *start_addr = INSERT_FIELD(*start_addr, PA_PN3, mttl3_index);
        //         }
        //     break;
        //     case PAGE_SIZE: 
        //         base = allocate_4K_page(mttl2, sdid, flags, base, max); 
        //         if (base) {
        //             // Update start address on successful allocation
        //             *start_addr = (base - increment) < dram_base ? (base - increment) : dram_max;
        //             return base;
        //         }
        
        //         if (mttl3_index == EXTRACT_FIELD(dram_base, PA_PN3)) {
        //             // Reset to dram_max
        //             *start_addr = dram_max;
        //             mttl3_index = EXTRACT_FIELD(dram_max, PA_PN3);
        //         } else {
        //             // Move to next index
        //             mttl3_index -= 1;
        //             *start_addr = INSERT_FIELD(*start_addr, PA_PN3, mttl3_index);
        //         }
        //         break;
        // }

    } while (mttl3_index != record_index);
// #endif
    // Restore record value
    *start_addr = record;
    return 0;
}