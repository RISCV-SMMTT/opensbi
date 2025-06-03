/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Xiao Xu <xiao_xu@mail.sdu.edu.cn>
 */

#include <sbi/sbi_smmtt.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_bitops.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_heap.h>
#include <sbi/sbi_types.h>
#include <sbi/sbi_console.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <libfdt.h>
#include <sbi/sbi_math.h>
#include <sbi/sbi_dynmem.h>

#if __riscv_xlen == 32
#define OFFSET_ENTRIES	8
#else
#define OFFSET_ENTRIES	16
#endif

void print_mttl1(mttl1_entry_t *mttl1, uintptr_t base_addr)
{
    if (!mttl1) return;

    sbi_printf("\n========== MTTL1 Table ==========\n");
    sbi_printf("|  Physical Addr  |  MTTL1 Entry  |  Info  |\n");
    sbi_printf("------------------------------------------\n");

    for (int i = 0; i < MTTL1_ENTRIES; i++) {
        unsigned long addr = base_addr + (i << 16);
        unsigned long info = mttl1[i];

        if (info == 0) continue;

        sbi_printf("|  0x%013lx  |  0x%016lx  |  [", addr, info);


        for (int j = 0; j < OFFSET_ENTRIES; j++) {
            unsigned long perms = (info >> (j * 2)) & 0x3; 

            if (perms == PERMS_MTTL1_ALLOW_RWX) sbi_printf("RWX ");
            else if (perms == PERMS_MTTL1_ALLOW_RW) sbi_printf("RW- ");
            else if (perms == PERMS_MTTL1_ALLOW_RX) sbi_printf("R-X ");
            else sbi_printf("--- "); 
        }

        sbi_printf("]\n"); 
    }

    sbi_printf("------------------------------------------\n");
}

void print_mttl2(mttl2_entry_t *mttl2, uintptr_t base_addr)
{
    unsigned long i;
    if (!mttl2) return;
    mttl1_entry_t *mttl1_list[512];  
    uintptr_t mttl1_base_addrs[512];
    int mttl1_count = 0;   
    const char *type_str;
    uintptr_t ppn;

    sbi_printf("\n========== MTTL2 Table ==========\n");
    sbi_printf("MTTL2 Table Address: 0x%lx\n", (uintptr_t)mttl2);
    sbi_printf("|  Physical Addr  |  Type   |  Info (if any)  |\n");
    sbi_printf("----------------------------------------------\n");

    for (i = 0; i < MTTL2_ENTRIES; i++) {
        mttl2_entry_t *entry = &mttl2[i];
        // Calculate the actual physical address by combining the base_addr with the index
        unsigned long addr = base_addr + (i << 25); // 32MB step

        if (entry->type == 0) continue; 

        unsigned long info = entry->info;
        type_str = "UNKNOWN"; 

        if (entry->type == TYPE_MTTL1_DIR) {
            ppn = info << PAGE_SHIFT;
            mttl1_list[mttl1_count] = (mttl1_entry_t *)ppn;
            mttl1_base_addrs[mttl1_count] = addr;
            mttl1_count++;

            type_str = "MTTL1 DIR";
            sbi_printf("|  0x%013lx  |  %s  |  0x%lx  |\n", addr, type_str, info);
        } else {
            switch (entry->type) {
                case TYPE_1G_ALLOW_RWX: type_str = "1GB  RWX"; break;
                case TYPE_1G_ALLOW_RW:  type_str = "1GB  RW-"; break;
                case TYPE_1G_ALLOW_RX:  type_str = "1GB  R-X"; break;
                case TYPE_2M_PAGE:      type_str = "2MB PAGE"; break;
                default:                type_str = "UNKNOWN "; break;
            }

			if (entry->type == TYPE_2M_PAGE)
			{
				sbi_printf("|  0x%013lx  |  %s  |  ", addr, type_str);
				for (int j = 0; j < OFFSET_ENTRIES; j++) {
					unsigned long perms = (info >> (j * 2)) & 0x3; 
		
					if (perms == PERMS_MTTL1_ALLOW_RWX) sbi_printf("RWX ");
					else if (perms == PERMS_MTTL1_ALLOW_RW) sbi_printf("RW- ");
					else if (perms == PERMS_MTTL1_ALLOW_RX) sbi_printf("R-X ");
					else sbi_printf("--- "); 
				}
				sbi_printf("\n");
			}
			else
	            sbi_printf("|  0x%013lx  |  %s  |  0x%lx  |\n", addr, type_str, info);

            if (entry->type == TYPE_1G_ALLOW_RWX || entry->type == TYPE_1G_ALLOW_RW || entry->type == TYPE_1G_ALLOW_RX) {
                i += 31; 
            }
        }
    }
    sbi_printf("-----------------------------------------\n");

    for (int j = 0; j < mttl1_count; j++) {
        print_mttl1(mttl1_list[j], mttl1_base_addrs[j]);
    }
}

void print_mttl3(mttl3_entry_t *mttl3)
{
    unsigned long ppn;
    unsigned long i;
    if (!mttl3) {
        sbi_printf("Error: MTTL3 table is NULL!\n");
        return;
    }

    sbi_printf("\n========== MTTL3 Table ==========\n");
    sbi_printf("|   Index   |  Physical Addr  |  MTTL2 Addr  |\n");
    sbi_printf("--------------------------------------------\n");


    for (i = 0; i < 512; i++) {
        mttl3_entry_t *entry = &mttl3[i];
        uintptr_t base_addr = i << 39; // 512GB step

        if (entry->mttl2_ppn == 0) continue; // Skip invalid entries

        ppn = entry->mttl2_ppn << PAGE_SHIFT;
        mttl2_entry_t *mttl2 = (mttl2_entry_t *)(ppn);
        sbi_printf("| %8ld |  0x%012lx  |  0x%lx  |\n", 
                  i, base_addr, (uintptr_t)mttl2);
    }
    sbi_printf("--------------------------------------------\n");

    // Second pass: Print all MTTL2 tables
    for (i = 0; i < 512; i++) {
        mttl3_entry_t *entry = &mttl3[i];
        if (entry->mttl2_ppn == 0) continue;

        ppn = entry->mttl2_ppn << PAGE_SHIFT;
        mttl2_entry_t *mttl2 = (mttl2_entry_t *)(ppn);
        uintptr_t base_addr = i << 46; // 512GB step for MTTL2 base address
        
        sbi_printf("\nMTTL2 Table for region 0x%012lx:\n", base_addr);
        print_mttl2(mttl2, base_addr);
    }
}

void sbi_smmtt_print_table(struct sbi_domain *dom)
{
    if (!dom) {
        sbi_printf("Error: Domain is NULL!\n");
        return;
    }

    if (!dom->mtt) {
        sbi_printf("Error: SMMTT Table is not initialized!\n");
        return;
    }

    switch (dom->smmtt_mode) {
        case SMMTT_BARE:
            sbi_printf("SMMTT Mode: BARE (No translation)\n");
            break;

#if __riscv_xlen == 32
        case SMMTT_34:
#else
        case SMMTT_46:
#endif
            print_mttl2((mttl2_entry_t *)dom->mtt, 0);
            break;

#if __riscv_xlen == 64
        case SMMTT_56:
            print_mttl3((mttl3_entry_t *)dom->mtt);
            break;
#endif

        default:
            sbi_printf("Error: Unsupported SMMTT mode: %d\n", dom->smmtt_mode);
            break;
    }
}

void *Np1 = NULL;

void sbi_smmtt_print_Np1(struct sbi_domain *dom)
{
    if (!dom) {
        sbi_printf("Error: Domain is NULL!\n");
        return;
    }

    if (!Np1) {
        sbi_printf("Error: Np1 Table is not initialized!\n");
        return;
    }

    switch (dom->smmtt_mode) {
        case SMMTT_BARE:
            sbi_printf("SMMTT Mode: BARE (No translation)\n");
            break;

#if __riscv_xlen == 32
        case SMMTT_34:
#else
        case SMMTT_46:
#endif
            print_mttl2((mttl2_entry_t *)Np1, 0);
            break;

#if __riscv_xlen == 64
        case SMMTT_56:
            print_mttl3((mttl3_entry_t *)Np1);
            break;
#endif

        default:
            sbi_printf("Error: Unsupported SMMTT mode: %d\n", dom->smmtt_mode);
            break;
    }
}


struct sbi_heap_control *smmtt_hpctrl = NULL;
uint64_t smmtt_table_base, smmtt_table_size;

/* MTTP handling */
unsigned int mttp_get_sdidlen()
{
	smmtt_mode_t mode;
	unsigned int sdid, sdidlen;
	uintptr_t ppn;

	// Save current values in mttp
	mttp_get(&mode, &sdid, &ppn);

	// Write all ones to SDID and get values back
	mttp_set(SMMTT_BARE, (unsigned int)-1, 0);
	mttp_get(NULL, &sdidlen, NULL);

	// Reset back old values
	mttp_set(mode, sdid, ppn);

	if (sdidlen == 0) {
		return 0;
	} else {
		return sbi_fls(sdidlen) + 1;
	}
}

void mttp_set(smmtt_mode_t mode, unsigned int sdid, physical_addr_t ppn)
{
	uintptr_t mttp = INSERT_FIELD(0, MTTP_PPN_MASK, ppn);
	mttp	       = INSERT_FIELD(mttp, MTTP_SDID_MASK, sdid);
	mttp	       = INSERT_FIELD(mttp, MTTP_MODE_MASK, mode);
	csr_write(CSR_MTTP, mttp);
}

void mttp_get(smmtt_mode_t *mode, unsigned int *sdid, physical_addr_t *ppn)
{
	uintptr_t mttp = csr_read(CSR_MTTP);
	if (mode) {
		*mode = EXTRACT_FIELD(mttp, MTTP_MODE_MASK);
	}

	if (sdid) {
		*sdid = EXTRACT_FIELD(mttp, MTTP_SDID_MASK);
	}

	if (ppn) {
		*ppn = EXTRACT_FIELD(mttp, MTTP_PPN_MASK);
	}
}

static int get_mtt_level(smmtt_mode_t mode, int *level)
{
	int tmp = -1;

	switch (mode) {
	case SMMTT_BARE:
		tmp = -1;
		break;

#if __riscv_xlen == 32
	case SMMTT_34:
#else
	case SMMTT_46:
#endif
		tmp = 2;
		break;

#if __riscv_xlen == 64
	case SMMTT_56:
		tmp = 3;
		break;
#endif
	default:
		return SBI_EINVAL;
	}

	if (level) {
		*level = tmp;
	}

	return SBI_OK;
}

smmtt_type mttl2_1g_type_from_flags(unsigned long flags)
{
	if (flags & SBI_DOMAIN_MEMREGION_SU_READABLE) 
	{
		if (flags & SBI_DOMAIN_MEMREGION_SU_WRITABLE) 
		{
			if (flags & SBI_DOMAIN_MEMREGION_SU_EXECUTABLE) 
				return TYPE_1G_ALLOW_RWX;
			else
				return TYPE_1G_ALLOW_RW;
		}
		else
		{ 
			if (flags & SBI_DOMAIN_MEMREGION_SU_EXECUTABLE) 
				return TYPE_1G_ALLOW_RX;
			else 
				return TYPE_1G_DISALLOW;
		}
	}
	else 
		return TYPE_1G_DISALLOW;
}

int add_1g_region(mttl2_entry_t *entry, unsigned long flags)
{
	int i;
	smmtt_type type = mttl2_1g_type_from_flags(flags);
	for (i = 0; i < 32; i++)
	{
		MTTL2_FIELD_ENSURE_EQUAL(entry, type, type);

		entry->info = 0;
		entry->zero = 0;
		entry = entry + 1;
	}
	return SBI_OK;
}

smmtt_xm_perms xm_perms_from_flags(unsigned long flags)
{
	if (flags & SBI_DOMAIN_MEMREGION_SU_READABLE)
	{
		if (flags & SBI_DOMAIN_MEMREGION_SU_WRITABLE) 
		{
			if (flags & SBI_DOMAIN_MEMREGION_SU_EXECUTABLE) 
				return PERMS_XM_ALLOW_RWX;
			else 
				return PERMS_XM_ALLOW_RW;
		}
		else 
		{
			if (flags & SBI_DOMAIN_MEMREGION_SU_EXECUTABLE) 
				return PERMS_XM_ALLOW_RX;
			else 
				return PERMS_XM_DISALLOW;
		}
	}
	else 
		return PERMS_XM_DISALLOW;
}

int add_xm_region(mttl2_entry_t *entry, unsigned long base, unsigned long flags)
{
	unsigned long offset, info, perms, field;

#if __riscv_xlen == 32
	smmtt_type type = TYPE_4M_PAGE;
#else
	smmtt_type type = TYPE_2M_PAGE;
#endif

	// Ensure we're not trying to change the type of this mttl2 entry
	MTTL2_FIELD_ENSURE_EQUAL(entry, type, type);

	offset = EXTRACT_FIELD(base, PA_XM_OFFS);
	field = MTT_PERM_FIELD(offset);
	info = entry->info;
	ENSURE_ZERO(EXTRACT_FIELD(entry->info, field));

	perms = xm_perms_from_flags(flags);
	info = INSERT_FIELD(info, field, perms);
	entry->info = info;

	entry->zero = 0;
	return SBI_OK;
}

mttl1_entry_t *mttl1_from_mttl2(mttl2_entry_t *entry)
{
	unsigned long mttl1_ppn;
	mttl1_entry_t *mttl1 = NULL;

	// Make sure this entry is the correct type to have an mttl1
	if (entry->type != TYPE_MTTL1_DIR) {
		return NULL;
	}

	if (entry->info) {
		// mttl1 already allocated, extract from mttl2
		mttl1_ppn = entry->info;
		mttl1 = (mttl1_entry_t *)(mttl1_ppn << PAGE_SHIFT);
	} else {
		// Allocate new mttl1
		mttl1 = sbi_aligned_alloc_from(smmtt_hpctrl, PAGE_SIZE, PAGE_SIZE);
		if (!mttl1) {
			return NULL;
		}

		// Link to mttl2
		entry->info = ((uintptr_t) mttl1) >> PAGE_SHIFT;

		// Ensure zero field is zero
		entry->zero = 0;
	}

	return mttl1;
}

perms_mttl1 mttl1_perms_from_flags(unsigned long flags)
{
	if (flags & SBI_DOMAIN_MEMREGION_SU_READABLE) 
	{
		if (flags & SBI_DOMAIN_MEMREGION_SU_WRITABLE) 
		{
			if (flags & SBI_DOMAIN_MEMREGION_SU_EXECUTABLE) 
				return PERMS_MTTL1_ALLOW_RWX; 
			else 
				return PERMS_MTTL1_ALLOW_RW;
		} 
		else 
		{
			if (flags & SBI_DOMAIN_MEMREGION_SU_EXECUTABLE) 
				return PERMS_MTTL1_ALLOW_RX;
			else
				return PERMS_MTTL1_DISALLOWED;
		}
	} else 
		return PERMS_MTTL1_DISALLOWED;
}

int add_mttl1_region(mttl2_entry_t *entry, unsigned long base, unsigned long flags)
{
	unsigned long index, offset, field;
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

	// Determine index and offset in mttl1 that this address belongs to
	index = EXTRACT_FIELD(base, PA_PN1);
	offset = EXTRACT_FIELD(base, PA_PN0);

	// Generate the bitfield for the permissions and ensure it is not set
	field = MTT_PERM_FIELD(offset);
	ENSURE_ZERO(EXTRACT_FIELD(mttl1[index], field));

	// Set the new permissions
	perms = mttl1_perms_from_flags(flags);
	mttl1[index] = INSERT_FIELD(mttl1[index], field, perms);
	return SBI_OK;
}

int add_mttl2_region(mttl2_entry_t *mttl2, unsigned long base,
                  unsigned long size, unsigned long flags)
{
    int rc = 0;
    uintptr_t index;
    mttl2_entry_t *entry;

    while(size != 0)
    {
        index = EXTRACT_FIELD(base, PA_PN2);
        entry = &mttl2[index];
        entry->zero = 0;

        if (FITS(base, size, GiB))
        {
            rc = add_1g_region(&mttl2[index], flags);
            if (rc) {
                sbi_printf("Failed to add 1GB region\n");
                return rc;
            }
            size -= GiB;
            base += GiB;
        }
        else if (FITS(base, size, XM_SIZE))
        {
            rc = add_xm_region(entry, base, flags);
            if (rc) {
                sbi_printf("Failed to add XM region\n");
                return rc;
            }
            size -= XM_SIZE;
            base += XM_SIZE;
        }
        else
        {
            rc = add_mttl1_region(entry, base, flags);
            if (rc) {
                sbi_printf("Failed to add MTTL1 region\n");
                return rc;
            }
            size -= PAGE_SIZE;
            base += PAGE_SIZE;
        }
    }

    return rc;
}

#if __riscv_xlen == 64
static int add_mttl3_region(mttl3_entry_t *mttl3, unsigned long base,
				  unsigned long size, unsigned long flags)
{
	unsigned long mttl2_ppn;
	mttl2_entry_t *mttl2;
	unsigned long index = EXTRACT_FIELD(base, PA_PN3);

	if (mttl3[index].mttl2_ppn == 0)
	{
		mttl2 = sbi_aligned_alloc_from(smmtt_hpctrl, MTTL2_SIZE, MTTL2_SIZE);

		if (!mttl2)
			return SBI_ENOMEM;

		mttl3[index].mttl2_ppn = ((uintptr_t)mttl2) >> PAGE_SHIFT;
		mttl3[index].zero = 0;
	}
	else
	{
		mttl2_ppn = mttl3[index].mttl2_ppn;
		mttl2 = (mttl2_entry_t *)(mttl2_ppn << PAGE_SHIFT);
	}
	
	return add_mttl2_region(mttl2, base, size, flags);
}
#endif

static int initialize_mtt(struct sbi_domain *dom, struct sbi_scratch *scratch)
{
    int level;
    int rc = 0;
    struct sbi_domain_memregion *reg;

	if (!dom->mtt)
	{
		if (dom->smmtt_mode == SMMTT_BARE)
			dom->smmtt_mode = SMMTT_DEFAULT_MODE;

		if (!sbi_hart_has_smmtt_mode(scratch, dom->smmtt_mode)) {
			sbi_printf("Error: HART does not support SMMTT mode %d\n", dom->smmtt_mode);
			return SBI_EINVAL;
		}

		rc = get_mtt_level(dom->smmtt_mode, &level);
		if (rc) {
			sbi_printf("Error: Invalid MTT level for mode %d\n", dom->smmtt_mode);
			return rc;
		}

		if (level == 3)
		{
			dom->mtt = sbi_aligned_alloc_from(smmtt_hpctrl, MTTL3_SIZE, MTTL3_SIZE);
			if (!dom->mtt)
			{
				sbi_printf("Error: Failed to allocate MTT table\n");
				return SBI_ENOMEM;
			}
			memset(dom->mtt, 0, MTTL3_SIZE);
		}
		else
		{
			dom->mtt = sbi_aligned_alloc_from(smmtt_hpctrl, MTTL2_SIZE, MTTL2_SIZE);
			if (!dom->mtt)
			{
				sbi_printf("Error: Failed to allocate MTT table\n");
				return SBI_ENOMEM;
			}
			memset(dom->mtt, 0, MTTL2_SIZE);
		}
	
		if (!dom->mtt) {
			sbi_printf("Error: Failed to allocate MTT table\n");
			return SBI_ENOMEM;
		}

		sbi_domain_for_each_memregion(dom, reg)
		{
			// Skip regions without any permissions
			if (!(reg->flags & SBI_DOMAIN_MEMREGION_SU_RWX)) {
				continue;
			}

			// Ensure region is properly aligned
			if (reg->base & (PAGE_SIZE - 1)) {
				sbi_printf("    Error: Region base not aligned\n");
				return SBI_EINVAL;
			}

			// Ensure region size is properly aligned
			if (reg->size & (PAGE_SIZE - 1)) {
				sbi_printf("    Error: Region size not aligned\n");
				return SBI_EINVAL;
			}

#if __riscv_xlen == 64
			if (level == 3) {
				rc = add_mttl3_region(dom->mtt, reg->base, reg->size, reg->flags);
				if (rc) {
					sbi_printf("    Failed to add MTTL3 region\n");
					return rc;
				}
			}
#endif

			if (level == 2) {
				rc = add_mttl2_region(dom->mtt, reg->base, reg->size, reg->flags);
				if (rc) {
					sbi_printf("    Failed to add MTTL2 region\n");
					return rc;
				}
			}
		}
	}


	return rc;
}

static int initialize_Np1(struct sbi_domain *dom, struct sbi_scratch *scratch)
{
    int level;
    int rc = 0;
    struct sbi_domain_memregion *reg;

	if (!Np1)
	{
		if (dom->smmtt_mode == SMMTT_BARE)
			dom->smmtt_mode = SMMTT_DEFAULT_MODE;

		if (!sbi_hart_has_smmtt_mode(scratch, dom->smmtt_mode)) {
			sbi_printf("Error: HART does not support SMMTT mode %d\n", dom->smmtt_mode);
			return SBI_EINVAL;
		}

		rc = get_mtt_level(dom->smmtt_mode, &level);
		if (rc) {
			sbi_printf("Error: Invalid MTT level for mode %d\n", dom->smmtt_mode);
			return rc;
		}

		if (level == 3)
		{
			Np1 = sbi_aligned_alloc_from(smmtt_hpctrl, MTTL3_SIZE, MTTL3_SIZE);
			if (!Np1)
			{
				sbi_printf("Error: Failed to allocate N+1 MTT table\n");
				return SBI_ENOMEM;
			}
			memset(Np1, 0, MTTL3_SIZE);
		}
		else
		{
			Np1 = sbi_aligned_alloc_from(smmtt_hpctrl, MTTL2_SIZE, MTTL2_SIZE);
			if (!Np1)
			{
				sbi_printf("Error: Failed to allocate N+1 MTT table\n");
				return SBI_ENOMEM;
			}
			memset(Np1, 0, MTTL2_SIZE);
		}
	
		if (!Np1) {
			sbi_printf("Error: Failed to allocate N+1 MTT table\n");
			return SBI_ENOMEM;
		}

		sbi_domain_for_each_memregion(dom, reg)
		{
			// Skip regions without any permissions
			if (!(reg->flags & SBI_DOMAIN_MEMREGION_SU_RWX)) {
				continue;
			}

			if (!check_mem(reg))
			{
				continue;
			}

			// Ensure region is properly aligned
			if (reg->base & (PAGE_SIZE - 1)) {
				sbi_printf("    Error: Region base not aligned\n");
				return SBI_EINVAL;
			}

			// Ensure region size is properly aligned
			if (reg->size & (PAGE_SIZE - 1)) {
				sbi_printf("    Error: Region size not aligned\n");
				return SBI_EINVAL;
			}

#if __riscv_xlen == 64
			if (level == 3) {
				rc = add_mttl3_region(Np1, reg->base, reg->size, reg->flags);
				if (rc) {
					sbi_printf("    Failed to add MTTL3 region\n");
					return rc;
				}
			}
#endif

			if (level == 2) {
				rc = add_mttl2_region(Np1, reg->base, reg->size, reg->flags);
				if (rc) {
					sbi_printf("    Failed to add MTTL2 region\n");
					return rc;
				}
			}
		}
	}

	return rc;
}


int sbi_hart_smmtt_configure(struct sbi_scratch *scratch)
{
	int rc;
	struct sbi_domain *dom = sbi_domain_thishart_ptr();
	unsigned int pmp_count = sbi_hart_pmp_count(scratch);

	rc = initialize_Np1(dom, scratch);
	if (rc)
		return rc;

	/* initialize MTT table */
	rc = initialize_mtt(dom, scratch);
	if (rc)
		return rc;

	/* use PMP to protect MTT table */
	pmp_set(pmp_count - 1, PMP_R | PMP_W | PMP_X, 0, __riscv_xlen);
	pmp_set(0, 0, smmtt_table_base, log2roundup(smmtt_table_size));

	mttp_set(dom->smmtt_mode, dom->index, ((uintptr_t)dom->mtt) >> PAGE_SHIFT);
	// sbi_smmtt_print_table(sbi_domain_thishart_ptr());
	return SBI_OK;
}

static int setup_mtt_table()
{
	int len;
	void* fdt;
	uint64_t chosen_offset, smmtt_order;
	const int* order_prop;
	const int* base_prop;

	fdt = fdt_get_address();
	chosen_offset = fdt_path_offset(fdt, "/chosen/opensbi-domains/smmtt_table");
	if (chosen_offset < 0)
		return SBI_ENOMEM;

	order_prop = fdt_getprop(fdt, chosen_offset, "order", &len);
	base_prop = fdt_getprop(fdt, chosen_offset, "base", &len);

	smmtt_order = fdt32_to_cpu(order_prop[0]);
	smmtt_table_size = 1ULL << smmtt_order;
	smmtt_table_base = ((uint64_t)fdt32_to_cpu(base_prop[0]) << 32) | fdt32_to_cpu(base_prop[1]);

	// Ensure NAPOT so we can later fit this in a single PMP register
	if ((smmtt_table_size & (smmtt_table_size - 1)) != 0) {
		return SBI_EINVAL;
	}

	if ((smmtt_table_base & (smmtt_table_size - 1)) != 0) {
		return SBI_EINVAL;
	}

	// Initialize the SMMTT table heap
	sbi_heap_alloc_new(&smmtt_hpctrl);
	sbi_heap_init_new(smmtt_hpctrl, smmtt_table_base, smmtt_table_size);

	return SBI_OK;
}

#define SECURE_DEVICE(status, sstatus) \
	(!strcmp(status, "disabled") && !strcmp(sstatus, "okay"))

#define NONSECURE_DEVICE(status, sstatus) \
	(!strcmp(status, "okay") && !strcmp(sstatus, "disabled"))

#define DISABLED_DEVICE(status, sstatus) \
	(!strcmp(status, "disabled") && !strcmp(sstatus, "disabled"))

#define AVAILABLE_DEVICE(status, sstatus) \
	(!strcmp(status, "okay") && !strcmp(sstatus, "okay"))

static int device_get_flags(const void *fdt, int dev, unsigned long *flags)
{
	const char *status, *sstatus, *name;

	status = fdt_getprop(fdt, dev, "status", NULL);
	if (!status)
		status = "okay";

	sstatus = fdt_getprop(fdt, dev, "secure-status", NULL);
	if (!sstatus)
		sstatus = status;

	*flags = SBI_DOMAIN_MEMREGION_MMIO;

	if (SECURE_DEVICE(status, sstatus) ||
	    DISABLED_DEVICE(status, sstatus)) {
		*flags |= (SBI_DOMAIN_MEMREGION_M_READABLE |
			  SBI_DOMAIN_MEMREGION_M_WRITABLE);
	} else if (NONSECURE_DEVICE(status, sstatus)) {
		*flags |= (SBI_DOMAIN_MEMREGION_SU_READABLE |
			  SBI_DOMAIN_MEMREGION_SU_WRITABLE);
	} else if (AVAILABLE_DEVICE(status, sstatus)) {
		*flags |= (SBI_DOMAIN_MEMREGION_M_READABLE |
			  SBI_DOMAIN_MEMREGION_M_WRITABLE |
			  SBI_DOMAIN_MEMREGION_SU_READABLE |
			  SBI_DOMAIN_MEMREGION_SU_WRITABLE);
	} else {
		name = fdt_get_name(fdt, dev, NULL);
		if (name) {
			sbi_printf("%s: invalid security specification "
				   "for device %s\n", __func__ , name);
		} else {
			sbi_printf("%s: invalid security specification\n",
				   __func__);
		}

		return SBI_EINVAL;
	}

	return SBI_OK;
}

static int create_regions_for_devices()
{
	int soc, dev, ret, i;
	uint64_t base, size;
	unsigned long flags;
	// struct sbi_domain *dom;
	struct sbi_domain_memregion reg;

	const void *fdt = fdt_get_address();
	soc = fdt_path_offset(fdt, "/soc");
	if (soc < 0) {
		return SBI_EINVAL;
	}

	fdt_for_each_subnode(dev, fdt, soc) {
		if (fdt_get_property(fdt, dev, "reg", NULL)) {
			ret = device_get_flags(fdt, dev, &flags);
			if (ret < 0) {
				return ret;
			}

			i = 0;
			while(1) {
				ret = fdt_get_node_addr_size(fdt, dev, i++,
							     &base, &size);
				if (ret < 0) {
					break;
				}

				sbi_domain_memregion_init(base, size, flags, &reg);
				ret = sbi_domain_add_memregion(&root, &reg);
			}
		}
	}

	return 0;
}

int sbi_smmtt_init(struct sbi_scratch *scratch, bool cold_boot)
{
	int rc = 0;
	if (!sbi_hart_has_extension(scratch, SBI_HART_EXT_SMMTT)) 
		return SBI_OK;
	
	if (cold_boot)
	{
		rc = setup_mtt_table();
		if (rc < 0)
			return rc;

		memory_region(scratch);

		rc = create_regions_for_devices();
		if (rc < 0)
			return rc;
	}
	// sbi_smmtt_print_Np1(&root);
	return rc;
}