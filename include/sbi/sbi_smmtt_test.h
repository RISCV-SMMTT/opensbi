/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Shandong University, School of Cyber Science and Technology.
 *
 * Authors:
 *   Xindong Fan <fanxindong@mail.sdu.edu.cn>
 */

#ifndef __SBI_SMMTT_TEST_H__
#define __SBI_SMMTT_TEST_H__

enum sbi_ext_test_fid {
	SBI_EXT_MTTP_READ = 0x0,
	SBI_EXT_MTTP_WRITE = 0x1,
	SBI_EXT_MTTL3_DECODE_VALID = 0x2,
	SBI_EXT_MTTL3_DECODE_PPN = 0x3,
	SBI_EXT_MTTL2_DECODE_TYPE = 0x4,
	SBI_EXT_MTTL2_DECODE_INFO = 0x5,
	SBI_EXT_MTTL1_DECODE_PERM = 0x6,
	SBI_EXT_MTTP_BARE = 0x7,
	SBI_EXT_MTTP_SMMTT34 = 0x8,
	SBI_EXT_MTTP_SMMTT46 = 0x9,
	SBI_EXT_MTTP_SMMTT56 = 0xA,
	SBI_EXT_MTTP_PPN = 0xB,
	SBI_EXT_MTTL3_VALID = 0xC,
	SBI_EXT_MTTL3_PPN = 0xD,
	SBI_EXT_1G_XXX = 0xE,
	SBI_EXT_MTT_L1_DIR = 0xF,
	SBI_EXT_4M_PAGES = 0x10,
	SBI_EXT_2M_PAGES = 0x11,
	SBI_EXT_MTTP_WARL = 0x12,
};

#endif