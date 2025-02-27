/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Xiao Xu <xiao_xu@mail.sdu.edu.cn>
 */

#ifndef __SBI_DOMAIN_DYNMEM_H__
#define __SBI_DOMAIN_DYNMEM_H__

#if __riscv_xlen == 32
#define USER_BASE_ADDR  0x4000000000ULL
#define USER_MAX_ADDR   0x7FFFFFFFFFULL
#define PAGE_SIZE       0x1000
#else 
#define USER_BASE_ADDR  0x4000000000ULL
#define USER_MAX_ADDR   0x7FFFFFFFFFULL
#define PAGE_SIZE       0x1000
#endif


#include <sbi/sbi_domain.h>
#include <sbi/sbi_smmtt.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_error.h>

bool sbi_domain_modify_page(const struct sbi_domain *dom, 
    physical_addr_t base, unsigned long size,
    uint64_t flags);

#endif