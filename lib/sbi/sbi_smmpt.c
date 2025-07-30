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
static uint64_t smmpt_base, smmpt_size;

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

static int setup_mpt_table()
{
	int len;
	void* fdt;
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
	smmpt_base = ((uint64_t)fdt32_to_cpu(base_prop[0]) << 32) | fdt32_to_cpu(base_prop[1]);

	if (smmpt_size == 0 || smmpt_base == 0)
		return SBI_ERR_FAILED;

	// Initialize the SMMTT table heap
	sbi_heap_alloc_new(&smmpt_hpctrl);
	sbi_heap_init_new(smmpt_hpctrl, smmpt_base, smmpt_size);

	return SBI_OK;
}

int sbi_smmtt_init(struct sbi_scratch *scratch, bool cold_boot)
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