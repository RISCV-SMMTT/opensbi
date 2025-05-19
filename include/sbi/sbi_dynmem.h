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
unsigned long allocate(unsigned long size, unsigned long flags);

void memory_region(struct sbi_scratch *scratch, void * mtt);

/**
 * Allocate memory.
 */
bool check_mem(struct sbi_domain_memregion *reg);

#endif