/*
* SPDX-License-Identifier: BSD-2-Clause
*
* Copyright (c) 2020 Western Digital Corporation or its affiliates.
*
* Authors:
*   Xiao Xu <xiao_xu@mail.sdu.edu.cn>
*/

#include <sbi/sbi_ecall.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_version.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_domain_context.h>
#include <sbi/sbi_smmtt.h>
#include <sbi/sbi_dynmem.h>

static int sbi_ecall_dynm_handler(unsigned long extid, unsigned long funcid,
                struct sbi_trap_regs *regs,
                struct sbi_ecall_return *out)
{
    int ret = 0, flags;
    unsigned long page_size, addr;
    page_size = regs->a0;
    flags = regs->a1;
    addr = regs->a2;
    switch(funcid)
    {
        case SBI_EXT_DYNM_ALOC:
        // allocate memory for current domain.
            addr = allocate(page_size, flags);
            if (addr == 0)
                return SBI_ENOMEM;
            ret = SBI_OK;
            out->value = addr;
            break;
        case SBI_EXT_DYNM_MODIFY:
        // modify memory for current domain.
            ret = modify(addr, page_size, flags);
            break;
        case SBI_EXT_DYNM_RECLAIM:
        // reclaim memory for current domain.
            ret = remove(addr, page_size);
            break;
        default:
            return SBI_ENODEV;	
    }
    return ret;
}

struct sbi_ecall_extension ecall_dynm;

static int sbi_ecall_dynm_register_extensions(void)
{
    return sbi_ecall_register_extension(&ecall_dynm);
}

struct sbi_ecall_extension ecall_dynm = {
    .extid_start		= SBI_EXT_DYNM,
    .extid_end		= SBI_EXT_DYNM,
    .register_extensions	= sbi_ecall_dynm_register_extensions,
    .handle			= sbi_ecall_dynm_handler,
};
