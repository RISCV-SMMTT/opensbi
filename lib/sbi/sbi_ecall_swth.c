/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 *   Atish Patra <atish.patra@wdc.com>
 */

#include <sbi/sbi_ecall.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_version.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_domain_context.h>

static int sbi_ecall_swth_handler(unsigned long extid, unsigned long funcid,
				  struct sbi_trap_regs *regs,
				  struct sbi_ecall_return *out)
{
	int ret = 0;
	unsigned long index;
	struct sbi_context *ctx = sbi_domain_context_thishart_ptr();
	struct sbi_domain *dom;
	u32 i, hartindex = sbi_hartid_to_hartindex(current_hartid());
	switch(funcid)
	{
		case SBI_EXT_SWTH_EXIT:
			/**
			 * Exit current domain, enter another domain.
			 * 1. if called by another domain, then return.
			 * 2. if root domain(has not caller, if domain is not root, can it have not caller?),
			 * 		loop through each domain to find next uninitialized user-defined domain.
			 * 3. if do not find uninitialized user-defined domain, then return to root domain.
			 */
			ret = sbi_domain_context_exit();
			break;
		case SBI_EXT_SWTH_ENTER:
			/**
			 * 1. ensure current hart is assigned to that domain.
			 * 2. enter specified domain.
			 */
			index = regs->a0;
			sbi_domain_for_each(i, dom) {
				if (dom->index == index)
					if (sbi_hartmask_test_hartindex(hartindex, dom->possible_harts))
						ret = sbi_domain_context_enter(dom);
			}
			if (!dom)	ret = SBI_ERR_FAILED;
			break;
		case SBI_EXT_SWTH_DUMP:
			/* dump all infomation of current domain */
			dom = ctx->dom;
			sbi_domain_dump(dom, "      ");
			break;
		case SBI_EXT_SWTH_ADDMEM:
			/**
			 * Add memory region for current domain, need limits as below:
			 * 1. Specify memory size(no more than ?), but not base(cannot specify where to allocate).
			 * 2. Request memory from the caller, as shown in SBI_EXT_SWTH_EXIT.
			 * 3. Whether this request is for confidential memory? if so, need extra limit.
			 */
			dom = ctx->dom;
			/* Need a new method to add dom_region after domain_finalize. */
			// sbi_domain_add_memrange(dom, 0xb0000000, 0x1000, 0x1000, 
			// 						SBI_DOMAIN_MEMREGION_M_READABLE |
			// 						SBI_DOMAIN_MEMREGION_M_WRITABLE);
			break;
		case SBI_EXT_SWTH_SUBMEM:
			break;
		default:
			return SBI_ENODEV;	
	}
	return ret;
}

struct sbi_ecall_extension ecall_swth;

static int sbi_ecall_swth_register_extensions(void)
{
	return sbi_ecall_register_extension(&ecall_swth);
}

struct sbi_ecall_extension ecall_swth = {
	.extid_start		= SBI_EXT_SWTH,
	.extid_end		= SBI_EXT_SWTH,
	.register_extensions	= sbi_ecall_swth_register_extensions,
	.handle			= sbi_ecall_swth_handler,
};
