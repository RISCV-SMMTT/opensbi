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

#include <sbi/sbi_domain.h>
#include <sbi/sbi_smmtt.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_error.h>


#if __riscv_xlen == 32
#define USER_BASE_ADDR  0x89000000ULL
#define USER_MAX_ADDR   0xBFFFFFFFULL
#else 
#define USER_BASE_ADDR  0x4000000000ULL
#define USER_MAX_ADDR   0x7FFFFFFFFFULL
#endif

unsigned long start_1G_index[SBI_DOMAIN_MAX_INDEX];
unsigned long start_XM_index[SBI_DOMAIN_MAX_INDEX];
unsigned long start_4K_index[SBI_DOMAIN_MAX_INDEX];
unsigned long dram_max;

/**
 * Modify privilege of memory.
 * @param  base base address to modify;
 * @param  size size need to modify, must be multiple of PAGE_SIZE
 * @param  flags new flags setted to the base address.
 * @return 0 on success and SBI_EINVAL on failure
 */
int modify(unsigned long base, unsigned long size, unsigned long flags);

/**
 * remove memory.
 * @param  base base address to reclaim;
 * @param  size size need to reclaim, must be multiple of PAGE_SIZE
 * @return 0 on success and SBI_EINVAL on failure
 */
int remove(unsigned long base, unsigned long size);

/**
 * Allocate memory from user space.
 * @param  size size need to allocate, must be multiple of PAGE_SIZE
 * @param  flags flags of new memory.
 * @return base address on success and 0 on failure
 */
unsigned long allocate_user_memory(unsigned long size, unsigned long flags);

#endif