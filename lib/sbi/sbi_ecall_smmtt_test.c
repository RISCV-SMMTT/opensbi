/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Shandong University, School of Cyber Science and Technology.
 *
 * Authors:
 *   Xindong Fan <fanxindong@mail.sdu.edu.cn>
 * 	 Hao Wang 	 <202417044@mail.sdu.edu.cn>
 */

#include <sbi/sbi_ecall.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_version.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_smmtt_test.h>
#include <sbi/sbi_smmtt.h>
#include <sbi/sbi_console.h>

bool mttl3_entry_check(mttl3_entry_t *mttl3) {
	if (mttl3->mttl2_ppn != 0 && mttl3->zero == 0) {
		return true;
	}
	return false;
}

bool mttl2_entry_check(mttl2_entry_t *mttl2) {
	if (mttl2->type >= 0 && mttl2->type <= 6 && mttl2->zero == 0) {
		return true;
	}
	return false;
}

#if __riscv_xlen == 32
#define MTTL1_PERMS_ZERO_MASK _ULL(0xCCCCCCCC)
#define MTTL2_4M_PAGES_ZERO_MASK _ULL(0x3F0000)
#else
#define MTTL1_PERMS_ZERO_MASK _ULL(0xCCCCCCCCCCCCCCCC)
#define MTTL2_2M_PAGES_ZERO_MASK _ULL(0xFFF00000000)
#endif

#define TYPE_1G_XXX_MASK    0b100

bool mttl1_entry_check(mttl1_entry_t* mttl1)
{
	if ((*mttl1 & MTTL1_PERMS_ZERO_MASK) == 0) {
		return true;
	}
	return false;
}

void mtt_1g_info_check(mttl2_entry_t* mttl2, struct sbi_ecall_return* out)
{
	out->value = SBI_OK;
	for (int i = 0;i < 32;i++, mttl2++) {
		if (mttl2->info != 0) {
			out->value = SBI_EFAIL;
		}
	}
}


static int smmtt_test_mttp_rw(struct sbi_ecall_return* out)
{
	int ret = 0;
	smmtt_mode_t pre_mode = 0, mode = 1, test_mode = 0;
	unsigned int pre_sdid = 0, sdid = 1, test_sdid = 0;
	physical_addr_t pre_ppn = 0, ppn = 1, test_ppn = 0;

	// Save current values in mttp
	mttp_get(&pre_mode, &pre_sdid, &pre_ppn);

	// Write all ones to SDID and get values back
	mttp_set(mode, sdid, ppn);
	mttp_get(&test_mode, &test_sdid, &test_ppn);

	// Reset back old values
	mttp_set(pre_mode, pre_sdid, pre_ppn);

	if (test_mode == mode && test_sdid == sdid && test_ppn == ppn) {
		out->value = SBI_OK;
		sbi_printf("smmtt test mttp rw test passed\n");
		ret = SBI_OK;
	}
	else {
		out->value = SBI_EFAIL;
		sbi_printf("smmtt test mttp rw test failed\n");
		ret = SBI_EFAIL;
	}
	return ret;
}

static int mttl3_decode_valid(struct sbi_trap_regs* regs, struct sbi_ecall_return* out) {
	/*
	* MTTL3 decode valid test
	* TODO: Not yet implemented, return ok
	*/

	int ret = 0;
	out->value = SBI_OK;
	sbi_printf("smmtt test mttl3 decode valid test passed\n");
	ret = SBI_OK;
	return ret;
}

static int mttl3_decode_ppn(struct sbi_trap_regs* regs, struct sbi_ecall_return* out) {
	/*
	* MTTL3 decode PPN test
	*/

	int ret = 0;
	physical_addr_t mttp_ppn = 0;
	smmtt_mode_t mode = 0;

	// Get mttp ppn and mode
	mttp_get(&mode, NULL, &mttp_ppn);
	if (mode != SMMTT_56) {
		// If mode is not SMMTT_56, it means that mttl3 is not used, return ok.
		out->value = SBI_OK;
		sbi_printf("smmtt test mttl3 decode ppn test passed\n");
		ret = SBI_OK;
		return ret;
	}

	mttl3_entry_t* mttl3_entry = (mttl3_entry_t*)(mttp_ppn << PAGE_SHIFT);

	if (mttl3_entry->mttl2_ppn) {
		// ppn is not zero, decode success.
		out->value = SBI_OK;
		sbi_printf("smmtt test mttl3 decode ppn test passed\n");
		ret = SBI_OK;
	}
	else {
		out->value = SBI_EFAIL;
		sbi_printf("smmtt test mttl3 decode ppn test failed\n");
		ret = SBI_EFAIL;
	}

	return ret;
}

static int mttl2_decode_type(struct sbi_trap_regs* regs, struct sbi_ecall_return* out) {
	/*
	* MTTL2 decode type test
	*/
	int ret = 0;
	physical_addr_t mttp_ppn = 0;
	smmtt_mode_t mode = 0;
	mttl2_entry_t* mttl2_entry = NULL;

	// Get mttp ppn and mode
	mttp_get(&mode, NULL, &mttp_ppn);

	if (mode != SMMTT_56) {
		// If mode is not SMMTT_56, it means that mttl3 is not used, we can get mttl2 entry directly
		mttl2_entry = (mttl2_entry_t*)(mttp_ppn << PAGE_SHIFT);
	}
	else {
		mttl3_entry_t* mttl3_entry = (mttl3_entry_t*)(mttp_ppn << PAGE_SHIFT);
		mttl2_entry = (mttl2_entry_t*)(uintptr_t)(mttl3_entry->mttl2_ppn << PAGE_SHIFT);
	}

	// Check type
	if (mttl2_entry->type >= 0 && mttl2_entry->type <= 6) {
		out->value = SBI_OK;
		sbi_printf("smmtt test mttl2 decode type test passed\n");
		ret = SBI_OK;
	}
	else {
		out->value = SBI_EFAIL;
		sbi_printf("smmtt test mttl2 decode type test failed\n");
		ret = SBI_EFAIL;
	}
	return ret;
}

static int mttl2_decode_info(struct sbi_trap_regs* regs, struct sbi_ecall_return* out) {
	/*
	* MTTL2 decode info test
	*/
	int ret = 0;
	physical_addr_t mttp_ppn = 0;
	smmtt_mode_t mode = 0;
	mttl2_entry_t* mttl2_entry = NULL;

	// Get mttp ppn and mode
	mttp_get(&mode, NULL, &mttp_ppn);

	// Get mttl2 entry
	if (mode != SMMTT_56) {
		mttl2_entry = (mttl2_entry_t*)(mttp_ppn << PAGE_SHIFT);
	}
	else {
		mttl3_entry_t* mttl3_entry = (mttl3_entry_t*)(mttp_ppn << PAGE_SHIFT);
		mttl2_entry = (mttl2_entry_t*)(uintptr_t)(mttl3_entry->mttl2_ppn << PAGE_SHIFT);
	}

	// Save info
	uintptr_t pre_info = mttl2_entry->info;

	// Set info with test value
	mttl2_entry->info = 0x7;
	if (mttl2_entry->info == 7) {
		out->value = SBI_OK;
		sbi_printf("smmtt test mttl2 decode info test passed\n");
		ret = SBI_OK;
	}
	else {
		out->value = SBI_EFAIL;
		sbi_printf("smmtt test mttl2 decode info test failed\n");
		ret = SBI_EFAIL;
	}

	// Restore info
	mttl2_entry->info = pre_info;

	return ret;
}

static int mttl1_decode_perm(struct sbi_trap_regs* regs, struct sbi_ecall_return* out) {
	/*
	* MTTL1 decode perm test
	*/
	int ret = 0;
	physical_addr_t mttp_ppn = 0;
	smmtt_mode_t mode = 0;
	mttl2_entry_t* mttl2_entry = NULL;
	mttl1_entry_t* mttl1_entry = NULL;

	// Get mttp ppn and mode
	mttp_get(&mode, NULL, &mttp_ppn);

	// Get mttl2 entry
	if (mode != SMMTT_56) {
		mttl2_entry = (mttl2_entry_t*)(mttp_ppn << PAGE_SHIFT);
	} 
	else {
		mttl3_entry_t* mttl3_entry =
			(mttl3_entry_t*)(mttp_ppn << PAGE_SHIFT);
		mttl2_entry =
			(mttl2_entry_t*)(uintptr_t)(mttl3_entry->mttl2_ppn
				<< PAGE_SHIFT);
	}
	for (int i = 0; i <= MTTL2_ENTRIES; mttl2_entry++, i++) {
		if(mttl2_entry->type == TYPE_MTTL1_DIR) {
			mttl1_entry = (mttl1_entry_t*)(uintptr_t)(mttl2_entry->info << PAGE_SHIFT);
			break;
		}
	}
	// Save info
	uintptr_t pre_info = *mttl1_entry;
	*mttl1_entry = 0x7;

	if (*mttl1_entry == 7) {
		out->value = SBI_OK;
		sbi_printf("smmtt test mttl1 decode perm test passed\n");
		ret = SBI_OK;
	} 
	else {
		out->value = SBI_EFAIL;
		sbi_printf("smmtt test mttl1 decode perm test failed\n");
		ret = SBI_EFAIL;
	}

	// Restore info
	*mttl1_entry = pre_info;

	return ret;
}

static int mttp_bare(struct sbi_trap_regs* regs, struct sbi_ecall_return* out) {
	/*
	* MTTP bare test
	*/
	int ret = 0;
	physical_addr_t mttp_ppn = 0;
	smmtt_mode_t mode = 0;
	// Get mttp ppn and mode
	mttp_get(&mode, NULL, &mttp_ppn);

	if (mode == SMMTT_BARE && mttp_ppn == 0) {
		out->value = SBI_OK;
		sbi_printf("smmtt test mttp bare test passed\n");
		ret = SBI_OK;
	}
	else {
		out->value = SBI_EFAIL;
		sbi_printf("smmtt test mttp bare test failed\n");
		ret = SBI_EFAIL;
	}
	return ret;
}

static int smmtt34(struct sbi_trap_regs* regs, struct sbi_ecall_return* out) {
	/*
	* SMMTT34 test
	*/
	int ret = 0;
	physical_addr_t mttp_ppn = 0;
	smmtt_mode_t mode = 0;
	mttl2_entry_t* mttl2_entry = NULL;

	// Get mttp ppn and mode
	mttp_get(&mode, NULL, &mttp_ppn);
#if __riscv_xlen != 32
	out->value = SBI_EFAIL;
	sbi_printf("smmtt test smmtt34 test failed, mode is not SMMTT_34\n");
	ret = SBI_EFAIL;
	return ret;
#endif

	// Get mttl2 entry
	mttl2_entry = (mttl2_entry_t*)(mttp_ppn << PAGE_SHIFT);
	if(mttl2_entry_check(mttl2_entry)) {
		out->value = SBI_OK;
		sbi_printf("smmtt test smmtt34 test passed\n");
		ret = SBI_OK;
	}
	else {
		out->value = SBI_EFAIL;
		sbi_printf("smmtt test smmtt34 test failed\n");
		ret = SBI_EFAIL;
	}

	return ret;
}

static int smmtt46(struct sbi_trap_regs* regs, struct sbi_ecall_return* out) {
	/*
	* SMMTT46 test
	*/
	int ret = 0;
	physical_addr_t mttp_ppn = 0;
	smmtt_mode_t mode = 0;
	mttl2_entry_t* mttl2_entry = NULL;

	// Get mttp ppn and mode
	mttp_get(&mode, NULL, &mttp_ppn);
	if (mode != SMMTT_46) {
		out->value = SBI_EFAIL;
		sbi_printf("smmtt test smmtt46 test failed, mode is not SMMTT_46\n");
		ret = SBI_EFAIL;
		return ret;
	}

	// Get mttl2 entry
	mttl2_entry = (mttl2_entry_t*)(mttp_ppn << PAGE_SHIFT);
	if (mttl2_entry_check(mttl2_entry)) {
		out->value = SBI_OK;
		sbi_printf("smmtt test smmtt46 test passed\n");
		ret = SBI_OK;
	} 
	else {
		out->value = SBI_EFAIL;
		sbi_printf("smmtt test smmtt46 test failed\n");
		ret = SBI_EFAIL;
	}

	return ret;
}

static int smmtt56(struct sbi_trap_regs* regs, struct sbi_ecall_return* out) {
	/*
	* SMMTT56 test
	*/
	int ret = 0;
	physical_addr_t mttp_ppn = 0;
	smmtt_mode_t mode = 0;
	mttl3_entry_t* mttl3_entry = NULL;

	// Get mttp ppn and mode
	mttp_get(&mode, NULL, &mttp_ppn);
	if (mode != SMMTT_56) {
		out->value = SBI_EFAIL;
		sbi_printf("smmtt test smmtt56 test failed, mode is not SMMTT_56\n");
		ret = SBI_EFAIL;
		return ret;
	}

	// Get mttl2 entry
	mttl3_entry = (mttl3_entry_t*)(mttp_ppn << PAGE_SHIFT);
	if (mttl3_entry_check(mttl3_entry)) {
		out->value = SBI_OK;
		sbi_printf("smmtt test smmtt56 test passed\n");
		ret = SBI_OK;
	} 
	else {
		out->value = SBI_EFAIL;
		sbi_printf("smmtt test smmtt56 test failed\n");
		ret = SBI_EFAIL;
	}

	return ret;
}

static int mttp_ppn(struct sbi_trap_regs* regs, struct sbi_ecall_return* out) {
	/*
	* MTTP PPN test
	*/
	int ret = 0;
	physical_addr_t mttp_ppn = 0;

	smmtt_mode_t mode = 0;
	mttl3_entry_t* mttl3_entry = NULL;
	mttl2_entry_t* mttl2_entry = NULL;

	// Get mttp ppn and mode
	mttp_get(&mode, NULL, &mttp_ppn);
	if (mode == SMMTT_BARE || mttp_ppn == 0) {
		out->value = SBI_EFAIL;
		sbi_printf("smmtt test mttp ppn test failed, mode is SMMTT_BARE or mttp_ppn is zero\n");
		ret = SBI_EFAIL;
		return ret;
	}
	if (mode==SMMTT_56) {
		mttl3_entry = (mttl3_entry_t*)(mttp_ppn << PAGE_SHIFT);
		if (mttl3_entry_check(mttl3_entry)) {
			out->value = SBI_OK;
			sbi_printf("smmtt test mttp ppn test passed\n");
			ret = SBI_OK;
		} else {
			out->value = SBI_EFAIL;
			sbi_printf("smmtt test mttp ppn test failed\n");
			ret = SBI_EFAIL;
		}
	} else {
		mttl2_entry = (mttl2_entry_t*)(mttp_ppn << PAGE_SHIFT);
		if (mttl2_entry_check(mttl2_entry)) {
			out->value = SBI_OK;
			sbi_printf("smmtt test mttp ppn test passed\n");
			ret = SBI_OK;
		} else {
			out->value = SBI_EFAIL;
			sbi_printf("smmtt test mttp ppn test failed\n");
			ret = SBI_EFAIL;
		}
	}

	return ret;
}

static int mttl3_valid(struct sbi_trap_regs* regs, struct sbi_ecall_return* out) {
	/*
	* MTTP level3 pte.valid test
	* TODO: Not yet implemented, return ok
	*/
	int ret = 0;
	out->value = SBI_OK;
	ret = SBI_OK;
	return ret;
}

static int mttl3_ppn(struct sbi_trap_regs* regs, struct sbi_ecall_return* out) {
	/*
	* MTTP level3 pte.PPN test
	*/
	int ret = 0;
	physical_addr_t mttp_ppn = 0;

	smmtt_mode_t mode = 0;
	mttl3_entry_t* mttl3_entry = NULL;
	mttl2_entry_t* mttl2_entry = NULL;

	// Get mttp ppn and mode
	mttp_get(&mode, NULL, &mttp_ppn);
	if (mode != SMMTT_56 || mttp_ppn == 0) {
		out->value = SBI_EFAIL;
		sbi_printf("smmtt test mttl3 ppn test failed, mode is not SMMTT_56 or mttp_ppn is zero\n");
		ret = SBI_EFAIL;
		return ret;
	}
	mttl3_entry = (mttl3_entry_t*)(mttp_ppn << PAGE_SHIFT);
	mttl2_entry = (mttl2_entry_t*)((uintptr_t)mttl3_entry->mttl2_ppn << PAGE_SHIFT);
	if (mttl2_entry_check(mttl2_entry)) {
		out->value = SBI_OK;
		sbi_printf("smmtt test mttl3 ppn test passed\n");
		ret = SBI_OK;
	} 
	else {
		out->value = SBI_EFAIL;
		sbi_printf("smmtt test mttl3 ppn test failed\n");
		ret = SBI_EFAIL;
	}
	return ret;
}

static int handle_1g_xxx(struct sbi_trap_regs* regs, struct sbi_ecall_return* out) {
	/*
	* MTTP level2 pte.info test
	*/
	int ret = -1;
	physical_addr_t mttp_ppn = 0;

	smmtt_mode_t mode = 0;
	mttl3_entry_t* mttl3_entry = NULL;
	mttl2_entry_t* mttl2_entry = NULL;

	// Get mttp ppn and mode
	mttp_get(&mode, NULL, &mttp_ppn);
	if ((mode != SMMTT_56 && mode != SMMTT_46) || mttp_ppn == 0) {
		out->value = SBI_EFAIL;
		sbi_printf("smmtt test 1g xxx test failed, mode is wrong or mttp_ppn is zero\n");
		ret = SBI_EFAIL;
		return ret;
	}
	if (mode == SMMTT_56) {
		mttl3_entry = (mttl3_entry_t*)(mttp_ppn << PAGE_SHIFT);
		mttl2_entry = (mttl2_entry_t*)((uintptr_t)mttl3_entry->mttl2_ppn << PAGE_SHIFT);
	} 
	else {
		mttl2_entry = (mttl2_entry_t*)(mttp_ppn << PAGE_SHIFT);
	}
	for (int i = 0; i <= MTTL2_ENTRIES; mttl2_entry++, i++) {
		if ((mttl2_entry->type & TYPE_1G_XXX_MASK) == 0 && mttl2_entry->type!=0) {
			mtt_1g_info_check(mttl2_entry, out);
			ret = out->value;
			break;
		}
	}
	if (ret != SBI_OK) {
		sbi_printf("smmtt test 1g xxx test failed\n");
	}
	else if(ret == SBI_OK) {
		sbi_printf("smmtt test 1g xxx test passed\n");
	}
	return ret;
}

static int mtt_l1_dir(struct sbi_trap_regs* regs, struct sbi_ecall_return* out) {
	/*
	* MTTP level2 pte.info test
	*/
	int ret = 0;
	physical_addr_t mttp_ppn = 0;
	smmtt_mode_t mode = 0;
	mttl2_entry_t* mttl2_entry = NULL;
	mttl1_entry_t* mttl1_entry = NULL;

	// Get mttp ppn and mode
	mttp_get(&mode, NULL, &mttp_ppn);

	// Get mttl2 entry
	if (mode != SMMTT_56) {
		mttl2_entry = (mttl2_entry_t*)(mttp_ppn << PAGE_SHIFT);
	} 
	else {
		mttl3_entry_t* mttl3_entry =
			(mttl3_entry_t*)(mttp_ppn << PAGE_SHIFT);
		mttl2_entry =
			(mttl2_entry_t*)(uintptr_t)(mttl3_entry->mttl2_ppn
				<< PAGE_SHIFT);
	}
	for (int i = 0; i <= MTTL2_ENTRIES; mttl2_entry++, i++) {
		if(mttl2_entry->type == TYPE_MTTL1_DIR) {
			mttl1_entry = (mttl1_entry_t*)(uintptr_t)(mttl2_entry->info << PAGE_SHIFT);
			break;
		}
	}
	// Check mttl1 entry
	if (mttl1_entry_check(mttl1_entry)) {
		out->value = SBI_OK;
		ret = SBI_OK;
	}
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
	}
	if (ret == SBI_OK) {
		sbi_printf("smmtt test mtt l1 dir test passed\n");
	} 
	else {
		sbi_printf("smmtt test mtt l1 dir test failed\n");
	}
	return ret;
}

static int handle_4m_pages(struct sbi_trap_regs* regs, struct sbi_ecall_return* out) {
	/*
	* MTTP level2 pte.info test
	* TODO: Not yet implemented, return ok
	*/
	int ret = 0;
#if __riscv_xlen != 32
	out->value = SBI_EFAIL;
	sbi_printf("smmtt test 4M pages test failed, mode is not SMMTT_34\n");
	ret = SBI_EFAIL;
	return ret;
#elif __riscv_xlen == 32
	physical_addr_t mttp_ppn = 0;
	smmtt_mode_t mode = 0;
	mttl2_entry_t* mttl2_entry = NULL;
	// Get mttp ppn and mode
	mttp_get(&mode, NULL, &mttp_ppn);

	// Get mttl2 entry
	mttl2_entry = (mttl2_entry_t*)(mttp_ppn << PAGE_SHIFT);

	for (int i = 0; i <= MTTL2_ENTRIES; mttl2_entry++, i++) {
		if(mttl2_entry->type == TYPE_4M_PAGE) {
			if ((mttl2_entry->info & MTTL2_4M_PAGES_ZERO_MASK) == 0) {
				out->value = SBI_OK;
				sbi_printf("smmtt test 4M pages test passed\n");
				ret = SBI_OK;
			} 
			else {
				out->value = SBI_EFAIL;
				sbi_printf("smmtt test 4M pages test failed\n");
				ret = SBI_EFAIL;
			}
			break;
		}
	}
	return ret;
#endif
}

static int handle_2m_pages(struct sbi_trap_regs* regs, struct sbi_ecall_return* out) {
	/*
	* MTTP level2 pte.info test
	* TODO: Not yet implemented, return ok
	*/
	int ret = 0;
#if __riscv_xlen == 32
	out->value = SBI_EFAIL;
	sbi_printf("smmtt test 2M pages test failed, mode is not SMMTT_46/56\n");
	ret = SBI_EFAIL;
	return ret;
#elif __riscv_xlen == 64
	physical_addr_t mttp_ppn = 0;
	smmtt_mode_t mode = 0;
	mttl2_entry_t* mttl2_entry = NULL;
	// Get mttp ppn and mode
	mttp_get(&mode, NULL, &mttp_ppn);

	// Get mttl2 entry
	if (mode != SMMTT_56) {
		mttl2_entry = (mttl2_entry_t*)(mttp_ppn << PAGE_SHIFT);
	} 
	else {
		mttl3_entry_t* mttl3_entry =
			(mttl3_entry_t*)(mttp_ppn << PAGE_SHIFT);
		mttl2_entry =
			(mttl2_entry_t*)(uintptr_t)(mttl3_entry->mttl2_ppn
				<< PAGE_SHIFT);
	}
	for (int i = 0; i <= MTTL2_ENTRIES; mttl2_entry++, i++) {
		if(mttl2_entry->type == TYPE_2M_PAGE) {
			if ((mttl2_entry->info & MTTL2_2M_PAGES_ZERO_MASK) == 0) {
				out->value = SBI_OK;
				sbi_printf("smmtt test 2m pages test passed\n");
				ret = SBI_OK;
			} 
			else {
				out->value = SBI_EFAIL;
				sbi_printf("smmtt test 2m pages test failed\n");
				ret = SBI_EFAIL;
			}
			break;
		}
	}
	return ret;
#endif
}

static int mttp_warl_test(struct sbi_trap_regs* regs, struct sbi_ecall_return* out) {
	/*
	* MTTP WARL test
	*/
	int ret = 0;
	smmtt_mode_t pre_mode = 0, mode = 3, test_mode = 0;
	unsigned int pre_sdid = 0, sdid = 67, test_sdid = 0;
	physical_addr_t pre_ppn = 0, ppn = 9999, test_ppn = 0;

	// Save current values in mttp
	mttp_get(&pre_mode, &pre_sdid, &pre_ppn);

	// Write all illegal values to mttp and get values back
	mttp_set(mode, sdid, ppn);
	mttp_get(&test_mode, &test_sdid, &test_ppn);
	sbi_printf("smmtt test mttp warl test: pre_mode=%d, pre_sdid=%u, pre_ppn=%lu\n",
			pre_mode, pre_sdid, pre_ppn);
	sbi_printf("smmtt test mttp warl test: mode=%d, sdid=%u, ppn=%lu\n",
			test_mode, test_sdid, test_ppn);
	// Reset back old values
	mttp_set(pre_mode, pre_sdid, pre_ppn);

	/*
	 * Check the values .
	 * We expect the read-back value to differ from the written value due to WARL.
	 */
	if (test_mode == mode && test_sdid == sdid && test_ppn == ppn) {
		out->value = SBI_EFAIL;
		sbi_printf("smmtt test mttp rw test failed\n");
		ret = SBI_EFAIL;
	}
	else {
		out->value = SBI_OK;
		sbi_printf("smmtt test mttp rw test passed\n");
		ret = SBI_OK;
	}
	return ret;
}

static int sbi_ecall_smmtt_test_handler(unsigned long extid,
				unsigned long funcid,
				struct sbi_trap_regs* regs,
				struct sbi_ecall_return* out)
{
	int ret = 0;
	switch (funcid) {
	case SBI_EXT_MTTP_READ:
		ret = smmtt_test_mttp_rw(out);
		break;
	case SBI_EXT_MTTP_WRITE:
		ret = smmtt_test_mttp_rw(out);
		break;
	case SBI_EXT_MTTL3_DECODE_VALID:
		ret = mttl3_decode_valid(regs, out);
		break;
	case SBI_EXT_MTTL3_DECODE_PPN:
		ret = mttl3_decode_ppn(regs, out);
		break;
	case SBI_EXT_MTTL2_DECODE_TYPE:
		ret = mttl2_decode_type(regs, out);
		break;
	case SBI_EXT_MTTL2_DECODE_INFO:
		ret = mttl2_decode_info(regs, out);
		break;
	case SBI_EXT_MTTL1_DECODE_PERM:
		ret = mttl1_decode_perm(regs, out);
		break;
	case SBI_EXT_MTTP_BARE:
		ret = mttp_bare(regs, out);
		break;
	case SBI_EXT_MTTP_SMMTT34:
		ret = smmtt34(regs, out);
		break;
	case SBI_EXT_MTTP_SMMTT46:
		ret = smmtt46(regs, out);
		break;
	case SBI_EXT_MTTP_SMMTT56:
		ret = smmtt56(regs, out);
		break;
	case SBI_EXT_MTTP_PPN:
		ret = mttp_ppn(regs, out);
		break;
	case SBI_EXT_MTTL3_VALID:
		ret = mttl3_valid(regs, out);
		break;
	case SBI_EXT_MTTL3_PPN:
		ret = mttl3_ppn(regs, out);
		break;
	case SBI_EXT_1G_XXX:
		ret = handle_1g_xxx(regs, out);
		break;
	case SBI_EXT_MTT_L1_DIR:
		ret = mtt_l1_dir(regs, out);
		break;
	case SBI_EXT_4M_PAGES:
		ret = handle_4m_pages(regs, out);
		break;
	case SBI_EXT_2M_PAGES:
		ret = handle_2m_pages(regs, out);
		break;
	case SBI_EXT_MTTP_WARL:
		ret = mttp_warl_test(regs, out);
		break;
	default:
		ret = SBI_ENODEV;
		break;
	}

	return ret;
}

struct sbi_ecall_extension ecall_smmtt_test;

static int sbi_ecall_smmtt_test_register_extensions(void)
{
	return sbi_ecall_register_extension(&ecall_smmtt_test);
}

struct sbi_ecall_extension ecall_smmtt_test = {
	.extid_start = SBI_EXT_TEST,
	.extid_end = SBI_EXT_TEST,
	.register_extensions = sbi_ecall_smmtt_test_register_extensions,
	.handle = sbi_ecall_smmtt_test_handler,
};
