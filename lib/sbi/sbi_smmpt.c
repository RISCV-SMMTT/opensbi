/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Xiao Xu <202337052@mail.sdu.edu.cn>
 */

#include <sbi/sbi_smmpt.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_bitops.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_heap.h>
#include <sbi/sbi_domain.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <libfdt.h>

#if __riscv_xlen == 32
#define PA_PN_OFFSET_LIST 15, 25
#define PA_PN_LEN       9, 10
#define PA_PN_MASK_LIST   (1ULL << 9) - 1, (1ULL << 10) - 1
#else
#define PA_PN_OFFSET_LIST 16, 25, 34, 43, 52
#define PA_PN_LEN       12, 9, 9, 9, 9
#define PA_PN_MASK_LIST   (1ULL << 12) - 1, (1ULL << 9) - 1, \
                          (1ULL << 9) - 1, (1ULL << 9) - 1, \
                          (1ULL << 9) - 1
#endif

extern const uint8_t pa_pn_offset[];
extern const uint8_t pa_pn_len[];
extern const uint64_t pa_pn_mask[];

#define PiB (1ULL << 50)
#define TiB (1ULL << 40)
#define GiB (1ULL << 30)
#define MiB (1ULL << 20)
#define KiB (1ULL << 10)

#if __riscv_xlen == 32
#define LEVEL_SIZE_LIST \
    4ULL * KiB, \
    4ULL * MiB
#else
#define LEVEL_SIZE_LIST \
    4ULL * KiB,  /* L0 */ \
    2ULL * MiB,  /* L1 */ \
    1ULL * GiB,  /* L2 */ \
    512ULL * GiB,/* L3 */ \
    256ULL * TiB,/* L4 */ \
    128ULL * PiB /* L5 */
#endif

const uint8_t pa_pn_offset[] = { PA_PN_OFFSET_LIST };
const uint8_t pa_pn_len[] = { PA_PN_LEN };
const uint64_t level_sizes[] = { LEVEL_SIZE_LIST };
const uint64_t pa_pn_mask[] = { PA_PN_MASK_LIST };

#define LEVEL_COUNT (sizeof(level_sizes) / sizeof(level_sizes[0]))
#define PA_PN_LEVELS (sizeof(pa_pn_offset) / sizeof(pa_pn_offset[0]))

#if __riscv_xlen == 32

#define SMMTT_DEFAULT_MODE (SMMTT_34)

#else

#define SMMTT_DEFAULT_MODE (SMMTT_43)

#endif

/* Globals */
static struct sbi_heap_control *smmpt_hpctrl = NULL;
static uint64_t smmpt_base, smmpt_size, smmpt_order;

/* MMPT handling */

void mmpt_set(mmpt_mode_t mode, unsigned int sdid, physical_addr_t ppn)
{
	uintptr_t mmpt = INSERT_FIELD(0, MMPT_PPN_MASK, ppn);
	mmpt	       = INSERT_FIELD(mmpt, MMPT_SDID_MASK, sdid);
	mmpt	       = INSERT_FIELD(mmpt, MMPT_MODE_MASK, mode);
	csr_write(CSR_MMPT, mmpt);
}

void mmpt_get(mmpt_mode_t *mode, unsigned int *sdid, physical_addr_t *ppn)
{
	uintptr_t mmpt = csr_read(CSR_MMPT);
	if (mode) {
		*mode = (mmpt & MMPT_MODE_MASK) >> MMPT_MODE_SHIFT;
	}
	if (sdid) {
		*sdid = (mmpt & MMPT_SDID_MASK) >> MMPT_SDID_SHIFT;
	}
	if (ppn) {
		*ppn = (mmpt & MMPT_PPN_MASK);
	}
}

static int get_mtt_level(mmpt_mode_t mode, int *level)
{
	int tmp = -1;

	switch (mode) {
	case SMMPT_BARE:
		tmp = -1;
		break;

#if __riscv_xlen == 32
	case SMMPT_34:
		tmp = 2;
		break;
#else
	case SMMPT_43:
#endif
		tmp = 3;
		break;

#if __riscv_xlen == 64
	case SMMPT_52:
		tmp = 4;
		break;
	case SMMPT_64:
		tmp = 5;
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

static inline uint64_t perms_from_flags(unsigned long flags)
{
	if (flags & SBI_DOMAIN_MEMREGION_SU_READABLE) {
		if (flags & SBI_DOMAIN_MEMREGION_SU_WRITABLE) {
			if (flags & SBI_DOMAIN_MEMREGION_SU_EXECUTABLE)
				return PERMS_RWX;
			else
				return PERMS_RW;
		} else {
			if (flags & SBI_DOMAIN_MEMREGION_SU_EXECUTABLE)
				return PERMS_RX;
			else
				return PERMS_RO;
		}
	} else
		return PERMS_NOACCESS;
}
#define FITS(base, size, level) \
	(((size) >= (level_sizes[level])) && (!((base) % (level_sizes[level]))))

static inline uint8_t get_perms_for_page(mpt_entry_t entry, uint8_t page_index)
{
	if (page_index > 7)
		return 0;

	return (entry.info >> (page_index * 3)) & 0x7;
}

static inline void set_perms_for_page(mpt_entry_t *entry, uint8_t page_index, smmpt_perms perms)
{
	entry->info = (entry->info & ~(0x7 << (page_index * 3))) |
		      ((perms & 0x7) << (page_index * 3));
}

static inline uint64_t mpt_get_ppn(mpt_entry_t entry)
{
#if __riscv_xlen == 32
	return (entry.info >> 2) & 0x3fffff;
#else
	return (entry.info >> 2) & 0xfffffffffff;
#endif
}

static inline void mpt_set_ppn(mpt_entry_t *mpt_entry, uintptr_t ppn)
{
#if __riscv_xlen == 32
	mpt_entry->info = ppn << 2;
#else
	mpt_entry->info = ppn << 2;
#endif
}

#define GET_INDEX(base, level)  \
    ((base >> pa_pn_offset[level - 1]) & pa_pn_mask[level - 1]);

static int add_mpt_region(mpt_entry_t *mpt, unsigned long base,
			  unsigned long order, unsigned long flags, int level)
{
	int rc;
	mpt_entry_t *mpt_entry;
	mpt_entry_t *new_mpt;
	smmpt_perms perms;
	int idx, offset;
	unsigned long size = (order == __riscv_xlen) ? -1UL : BIT(order);

	if (!mpt)
		return SBI_ENOMEM;

	while (size != 0) {
		idx	  = GET_INDEX(base, level);
		mpt_entry = &mpt[idx];

		if (mpt_entry->valid)
			return SBI_EINVAL;

		if (FITS(base, size, level)) {
			offset		    = (base & RANGE_MASK) >> 12;
			mpt_entry->valid    = 1;
			mpt_entry->leaf	    = 1;
			mpt_entry->napot    = 0;
			mpt_entry->reserved = 0;
			perms		    = perms_from_flags(flags);
			set_perms_for_page(mpt_entry, offset, perms);
			size -= level_sizes[level];
			base += level_sizes[level];
		} else {
			new_mpt = sbi_aligned_alloc_from(smmpt_hpctrl,
							 PAGE_SIZE, TABLE_SIZE);
			mpt_set_ppn(mpt_entry,
				    ((uintptr_t)new_mpt) >> PAGE_SHIFT);
			rc = add_mpt_region(new_mpt, base, order, flags,
					    level - 1);
			if (rc)
				return rc;
			mpt_entry->valid    = 1;
			mpt_entry->leaf	    = 0;
			mpt_entry->napot    = 0;
			mpt_entry->reserved = 0;
		}
	}

	return SBI_OK;
}

static int initialize_mpt(struct sbi_domain *dom, struct sbi_scratch *scratch)
{
	struct sbi_domain_memregion *reg;
	int level;
	get_mtt_level(dom->mmpt_mode, &level);

	// Allocate memory for MPT table
	if (!dom->mpt) {
		if (dom->mmpt_mode == SMMPT_BARE) {
			dom->mmpt_mode = SMMPT_DEFAULT_MODE;
		}

		if (level == 5) {
			dom->mpt = sbi_aligned_alloc_from(
				smmpt_hpctrl, PAGE_SIZE, ROOT_TABLE_SIZE);
		} else {
			dom->mpt = sbi_aligned_alloc_from(
				smmpt_hpctrl, PAGE_SIZE, TABLE_SIZE);
		}
		if (!dom->mpt)
			return SBI_ENOMEM;

		sbi_domain_for_each_memregion(dom, reg)
		{
			if (!(reg->flags & SBI_DOMAIN_MEMREGION_SU_RWX))
				continue;

			add_mpt_region(dom->mpt, reg->base, reg->order,
				       reg->flags, level);
		}
	}

	return SBI_OK;
}

int sbi_hart_smmpt_configure(struct sbi_scratch *scratch)
{
	int rc;
	struct sbi_domain *dom = sbi_domain_thishart_ptr();
	unsigned int pmp_count = sbi_hart_pmp_count(scratch);
	/* initialize MTT table */
	rc = initialize_mpt(dom, scratch);
	if (rc)
		return rc;

	mmpt_set(dom->mmpt_mode, dom->index,
		 ((uintptr_t)dom->mpt) >> PAGE_SHIFT);
	/* use PMP to protect MTT table */
	pmp_set(pmp_count - 1, PMP_R | PMP_W | PMP_X, 0, __riscv_xlen);
	pmp_set(0, 0, smmpt_base, smmpt_order);

	return SBI_OK;
}

static int setup_mpt_table()
{
	int len;
	const void *fdt;
	uint64_t chosen_offset;
	const int *order_prop;
	const int *base_prop;

	fdt = fdt_get_address();
	chosen_offset =
		fdt_path_offset(fdt, "/chosen/opensbi-domains/smmpt_table");
	if (chosen_offset < 0)
		return SBI_ENOMEM;

	order_prop = fdt_getprop(fdt, chosen_offset, "order", &len);
	base_prop  = fdt_getprop(fdt, chosen_offset, "base", &len);

	smmpt_size  = 1ULL << fdt32_to_cpu(*order_prop);
	smmpt_order = fdt32_to_cpu(order_prop[0]);
	smmpt_base  = ((uint64_t)fdt32_to_cpu(base_prop[0]) << 32) |
		     fdt32_to_cpu(base_prop[1]);

	if (smmpt_size == 0 || smmpt_base == 0)
		return SBI_ERR_FAILED;

	// Initialize the SMMTT table heap
	sbi_heap_alloc_new(&smmpt_hpctrl);
	sbi_heap_init_new(smmpt_hpctrl, smmpt_base, smmpt_size);

	return SBI_OK;
}

int sbi_smmpt_init(struct sbi_scratch *scratch, bool cold_boot)
{
	int rc;
	if (!sbi_hart_has_extension(scratch, SBI_HART_EXT_SMMPT))
		return SBI_OK;

	if (cold_boot) {
		rc = setup_mpt_table();
		if (rc < 0)
			return rc;
	}

	return rc;
}