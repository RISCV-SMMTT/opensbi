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
#include <sbi/sbi_domain.h>
#include <sbi/sbi_console.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <libfdt.h>
#include <sbi/sbi_math.h>

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

static int add_1g_region(mttl2_entry_t *entry, unsigned long flags)
{
	int i;
	for (i = 0; i < 32; i++)
	{
		smmtt_type type = mttl2_1g_type_from_flags(flags);

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

static int add_xm_region(mttl2_entry_t *entry, unsigned long base, unsigned long flags)
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

static int add_mttl1_region(mttl2_entry_t *entry, unsigned long base, unsigned long flags)
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

static int add_mttl2_region(mttl2_entry_t *mttl2, unsigned long base,
				  unsigned long size, unsigned long flags)
{
	int rc;
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
			if (rc)
				return rc;
			size -= GiB;
			base += GiB;
		}
		else if (FITS(base, size, XM_SIZE))
		{
			rc = add_xm_region(entry, base, flags);
			if (rc)
				return rc;
			size -= XM_SIZE;
			base += XM_SIZE;
		}
		else
		{
			rc = add_mttl1_region(entry, base, flags);
			if (rc)
				return rc;
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

		if (!sbi_hart_has_smmtt_mode(scratch, dom->smmtt_mode))
			return SBI_EINVAL;

		rc = get_mtt_level(dom->smmtt_mode, &level);
		if (rc)
			return rc;

		if (level == 3)
		{
			dom->mtt = sbi_aligned_alloc_from(smmtt_hpctrl, MTTL3_SIZE, MTTL3_SIZE);
			memset(dom->mtt, 0, MTTL3_SIZE);
		}
		else
		{
			dom->mtt = sbi_aligned_alloc_from(smmtt_hpctrl, MTTL2_SIZE, MTTL2_SIZE);
			memset(dom->mtt, 0, MTTL2_SIZE);
		}
	
		if (!dom->mtt)
			return SBI_ENOMEM;

		sbi_domain_for_each_memregion(dom, reg)
		{
			if (!(reg->flags & SBI_DOMAIN_MEMREGION_SU_RWX))
				continue;

#if __riscv_xlen == 64
			if (level == 3)
				add_mttl3_region(dom->mtt, reg->base, reg->size, reg->flags);
#endif

			if (level == 2)
				add_mttl2_region(dom->mtt, reg->base, reg->size, reg->flags);
		}
	}

	return rc;
}

int sbi_hart_smmtt_configure(struct sbi_scratch *scratch)
{
	int rc;
	struct sbi_domain *dom = sbi_domain_thishart_ptr();
	unsigned int pmp_count = sbi_hart_pmp_count(scratch);

	/* initialize MTT table */
	rc = initialize_mtt(dom, scratch);
	if (rc)
		return rc;

	/* use PMP to protect MTT table */
	pmp_set(pmp_count - 1, PMP_R | PMP_W | PMP_X, 0, __riscv_xlen);
	pmp_set(0, 0, smmtt_table_base, log2roundup(smmtt_table_size));

	mttp_set(SMMTT_BARE, dom->index, ((uintptr_t)dom->mtt) >> PAGE_SHIFT);
	// mttp_set(dom->smmtt_mode, dom->index, ((uintptr_t)dom->mtt) >> PAGE_SHIFT);
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
				if(ret < 0) {
					return ret;
				}
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
	
		rc = create_regions_for_devices();
		if (rc < 0)
			return rc;
	}

	return rc;
}