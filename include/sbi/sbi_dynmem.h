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

unsigned long start_1G_index[SBI_DOMAIN_MAX_INDEX] = { [0 ... (SBI_DOMAIN_MAX_INDEX - 1)] = USER_BASE_ADDR };
unsigned long start_XM_index[SBI_DOMAIN_MAX_INDEX] = { [0 ... (SBI_DOMAIN_MAX_INDEX - 1)] = USER_BASE_ADDR };
unsigned long start_4K_index[SBI_DOMAIN_MAX_INDEX] = { [0 ... (SBI_DOMAIN_MAX_INDEX - 1)] = USER_BASE_ADDR };

// void *add(unsigned long size, unsigned long flags);
unsigned long add(unsigned long size, unsigned long flags);

/* @return 0 on success and -1 on failure*/
int modify(unsigned long base, unsigned long size, unsigned long flags);




/**
 * Allocate memory from user space.
 * @param  size size need to allocate, must be multiple of PAGE_SIZE
 * @return base address on success and -1 on failure
 */

unsigned long allocate_user_memory(unsigned long size);

#endif