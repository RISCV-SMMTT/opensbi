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
    u32 i;
	unsigned long index;
	struct sbi_domain *dom;
	switch(funcid)
	{
		case SBI_EXT_SWTH_EXIT:
			ret = sbi_domain_context_exit();
			break;
		case SBI_EXT_SWTH_ENTER:
			index = regs->a0;
			sbi_domain_for_each(i, dom) {
				if (dom->index == index)
					if (sbi_hartmask_test_hartindex(sbi_hartid_to_hartindex(current_hartid()), dom->possible_harts))
						ret = sbi_domain_context_enter(dom);
			}
			if (!dom)	ret = SBI_ERR_FAILED;
			break;
		case 123456789:
			/* not quite correct */
			// ret = get_domain_index(index);
			// regs->a0 = (unsigned long)index;
			// break;
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
