/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Shandong University, School of Cyber Science and Technology.
 *
 * Authors:
 *   Xindong Fan <fanxindong@mail.sdu.edu.cn>
 *   Hao Wang <202417044@mail.sdu.edu.cn>
 */

#ifndef __SBI_SMMPT_TEST_H__
#define __SBI_SMMPT_TEST_H__

enum sbi_ext_test_fid {
    SBI_EXT_MMPT_READ = 0x0,
	SBI_EXT_MMPT_WRITE = 0x1,
    SBI_EXT_NON_LEAF_DECODE_BIT = 0x2,
    SBI_EXT_NON_LEAF_DECODE_PPN = 0x3,
    SBI_EXT_NON_NAPOT_LEAF_DECODE_BIT = 0x4,
    SBI_EXT_NON_NAPOT_LEAF_DECODE_PERM = 0x5,
    SBI_EXT_NAPOT_LEAF_DECODE_BIT = 0x6,
    SBI_EXT_NAPOT_LEAF_DECODE_G = 0x7,
    SBI_EXT_NAPOT_LEAF_DECODE_PERM = 0x8,
    SBI_EXT_MMPT_SMMPT34 = 0x9,
    SBI_EXT_MMPT_SMMPT43 = 0xA,
    SBI_EXT_MMPT_SMMPT52 = 0xB,
    SBI_EXT_MMPT_SMMPT64 = 0xC,
    SBI_EXT_MMPT_PPN = 0xD,
    SBI_EXT_NON_LEAF_BIT_L = 0xE,
    SBI_EXT_NON_LEAF_PPN = 0xF,
    SBI_EXT_NON_NAPOT_LEAF_BIT_L = 0x10,
    SBI_EXT_NON_NAPOT_LEAF_BIT_N = 0x11,
    SBI_EXT_NAPOT_LEAF_BIT_L = 0x12,
    SBI_EXT_NAPOT_LEAF_BIT_N = 0x13,
    SBI_EXT_NAPOT_LEAF_G = 0x14,
};

typedef union{
    mpt_entry_t mpt_entry_union;
    uint64_t value;
}mpt_entry_u;

#if __riscv_xlen == 32
#define entry_non_leaf_mask       0x3FCULL
#define entry_non_napot_leaf_mask 0xF8ULL
#define entry_napot_leaf_mask     0xFFFF08F8ULL
#else
#define entry_non_leaf_mask       0xFFC00000000003FCULL
#define entry_non_napot_leaf_mask 0xFF000000000000F8ULL
#define entry_napot_leaf_mask     0xFFFFFFFFFFFF08F8ULL
#endif

#endif  /* __SBI_SMMPT_TEST_H__ */