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


#if __riscv_xlen == 32

#define SMMTT_DEFAULT_MODE (SMMTT_34)

#else

#define SMMTT_DEFAULT_MODE (SMMTT_43)

#endif

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