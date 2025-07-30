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

void mmpt_get(mmpt_mode_t* mode, unsigned int* sdid, physical_addr_t* ppn)
{
	uintptr_t mmpt = csr_read(CSR_MMPT);
	if(mode) {
		*mode = (mmpt & MMPT_MODE_MASK) >> MMPT_MODE_SHIFT;
	}
	if(sdid) {
		*sdid = (mmpt & MMPT_SDID_MASK) >> MMPT_SDID_SHIFT;
	}
	if(ppn) {
		*ppn = (mmpt & MMPT_PPN_MASK);
	}
}

// static int get_mtt_level(mmpt_mode_t mode, int *level)
// {
// 	int tmp = -1;

// 	switch (mode) {
// 	case SMMPT_BARE:
// 		tmp = -1;
// 		break;

// #if __riscv_xlen == 32
// 	case SMMPT_34:
//         tmp = 2;
//         break;
// #else
// 	case SMMPT_43:
// #endif
// 		tmp = 3;
// 		break;

// #if __riscv_xlen == 64
// 	case SMMTT_52:
// 		tmp = 4;
// 		break;
//     case SMMPT_64:
//         tmp = 5;
//         break;
// #endif
// 	default:
// 		return SBI_EINVAL;
// 	}

// 	if (level) {
// 		*level = tmp;
// 	}

// 	return SBI_OK;
// }

static int initialize_mpt(struct sbi_domain *dom, struct sbi_scratch *scratch)
{
	if (!dom->mpt)
	{
		if (dom->mmpt_mode == SMMPT_BARE)
		{
			dom->mmpt_mode = SMMPT_DEFAULT_MODE;
		}

        dom->mpt = sbi_aligned_alloc_from(smmpt_hpctrl, PAGE_SIZE, TABLE_SIZE);
	}

    if (!dom->mpt)
        return SBI_ENOMEM;
    else 
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

	mmpt_set(dom->mmpt_mode, dom->index, ((uintptr_t)dom->mpt) >> PAGE_SHIFT);
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
	const int* order_prop;
	const int* base_prop;

	fdt = fdt_get_address();
	chosen_offset = fdt_path_offset(fdt, "/chosen/opensbi-domains/smmpt_table");
	if (chosen_offset < 0)
		return SBI_ENOMEM;

	order_prop = fdt_getprop(fdt, chosen_offset, "order", &len);
	base_prop = fdt_getprop(fdt, chosen_offset, "base", &len);

	smmpt_size = 1ULL << fdt32_to_cpu(*order_prop);
    smmpt_order = fdt32_to_cpu(base_prop[0]);
	smmpt_base = ((uint64_t)fdt32_to_cpu(base_prop[0]) << 32) | fdt32_to_cpu(base_prop[1]);

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
	
	if (cold_boot)
	{
		rc = setup_mpt_table();
		if (rc < 0)
			return rc;
	}

	return rc;
}