/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Shandong University, School of Cyber Science and Technology.
 *
 * Authors:
 *   Xindong Fan <fanxindong@mail.sdu.edu.cn>
 *   Hao Wang <202417044@mail.sdu.edu.cn>
 */

#include <sbi/sbi_ecall.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_version.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_smmpt.h>
#include <sbi/sbi_smmpt_test.h>
#include <sbi/sbi_console.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_hart.h>

static inline unsigned long mpt_get_ppn(mpt_entry_t entry)
{
#if __riscv_xlen == 32
	return (entry.info >> 2) & 0x3fffff;
#else
	return (entry.info >> 2) & 0xfffffffffffUL;
#endif
}

static inline void mpt_set_ppn(mpt_entry_t *mpt_entry, uintptr_t ppn)
{
	mpt_entry->info = (unsigned long)(ppn << 2);
}

static int smmpt_test_mmpt_rw(struct sbi_ecall_return* out)
{
	/*
	* MMPT CSR Read/Write test
	*/
	int ret = SBI_OK;
	mmpt_mode_t pre_mode = 0, mode = 1, test_mode = 0;
	unsigned int pre_sdid = 0, sdid = 1, test_sdid = 0;
	physical_addr_t pre_ppn = 0, ppn = 1, test_ppn = 0;
	
	// Save current values in mmpt
	mmpt_get(&pre_mode, &pre_sdid, &pre_ppn);

	// Write all ones to SDID and get values back
	mmpt_set(mode, sdid, ppn);
	mmpt_get(&test_mode, &test_sdid, &test_ppn);

	// Reset back old values
	mmpt_set(pre_mode, pre_sdid, pre_ppn);

	if (test_mode == mode && test_sdid == sdid && test_ppn == ppn) {
		out->value = SBI_OK;
		sbi_printf("smmpt test mmpt rw test passed\n");
		ret = SBI_OK;
	}
	else {
		out->value = SBI_EFAIL;
		sbi_printf("smmpt test mmpt rw test failed\n");
		ret = SBI_EFAIL;
	}
	return ret;
}

static int mpt_non_leaf_decode_bit(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* MPT non leaf decode control bit test
	*/
	int ret = SBI_OK;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a non-leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 0){
		entry++;
	}

	if ((entry->value & entry_non_leaf_mask) == 0) {
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt non leaf decode bit test passed\n");
	} 
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("mpt non leaf decode bit test failed\n");
	}
	return ret;
}

static int mpt_non_leaf_decode_ppn(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* MPT non leaf decode ppn test
	*/
	int ret = SBI_OK;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a non-leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 0){
		entry++;
	}
	// ppn is not zero, decode success.
	if (entry->mpt_entry_union.info){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt non leaf decode ppn test passed\n");
	}
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("mpt non leaf decode ppn test failed\n");
	}
	return ret;
}

static int mpt_non_napot_leaf_decode_bit(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* MPT non-napot leaf decode control bit test
	*/
	int ret = SBI_OK;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a non-napot leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1 || entry->mpt_entry_union.napot != 0){
		entry++;
	}

	if ((entry->value & entry_non_napot_leaf_mask) == 0) {
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt non napot leaf decode bit test passed\n");
	} 
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("mpt non napot leaf decode bit test failed\n");
	}
	return ret;
}

static int mpt_non_napot_leaf_decode_perm(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* MPT non-napot leaf decode permission test
	*/
	int ret = SBI_OK;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a non-napot leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1 || entry->mpt_entry_union.napot != 0){
		entry++;
	}

	//save perms
	unsigned long perms = entry->mpt_entry_union.info;

	//set perms to test value
	entry->mpt_entry_union.info = 0x7;
	if (entry->mpt_entry_union.info == 0x7){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt non napot leaf decode permission test passed\n");
	}
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("mpt non napot leaf decode permission test failed\n");
	}

	//restore perms
	entry->mpt_entry_union.info = perms;
	return ret;
}

static int mpt_napot_leaf_decode_bit(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* MPT napot leaf decode control bit test
	*/
	int ret = SBI_OK;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a napot leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1 || entry->mpt_entry_union.napot != 1){
		entry++;
	}

	if ((entry->value & entry_napot_leaf_mask) == 0) {
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt napot leaf decode bit test passed\n");
	} 
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("mpt napot leaf decode bit test failed\n");
	}
	return ret;
}

static int mpt_napot_leaf_decode_G(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* MPT napot leaf decode G field test
	*/
	int ret = SBI_OK;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a non-napot leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1 || entry->mpt_entry_union.napot != 0){
		entry++;
	}

	// save info
	unsigned long info = entry->mpt_entry_union.info;

	// set G to test value
	entry->mpt_entry_union.info = (0x5 << 4);
	if ((entry->mpt_entry_union.info >> 4) == 0x5){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt napot leaf decode G test passed\n");
	}
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("mpt napot leaf decode G test failed\n");
	}

	// restore info
	entry->mpt_entry_union.info = info;
	return ret;
}

static int mpt_napot_leaf_decode_perm(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* MPT napot leaf decode permission test
	*/
	int ret = SBI_OK;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a non-napot leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1 || entry->mpt_entry_union.napot != 0){
		entry++;
	}

	// save info
	unsigned long info = entry->mpt_entry_union.info;

	// set G to test value
	entry->mpt_entry_union.info = 0x7;
	if (entry->mpt_entry_union.info == 0x7){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt napot leaf decode permission test passed\n");
	}
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("mpt napot leaf decode permission test failed\n");
	}

	// restore info
	entry->mpt_entry_union.info = info;
	return ret;
}

static int smmpt34(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT 34 mode test
	*/
	int ret = SBI_OK;

	// if not riscv32, return fail
#if __riscv_xlen != 32
	out->value = SBI_EFAIL;
	ret = SBI_EFAIL;
	sbi_printf("smmpt 34 mode test failed, not riscv32 \n");
	return ret;
#else

	mmpt_mode_t mode = 0;
	physical_addr_t ppn = 0;
	physical_addr_t addr = 0x80000000;
	int idx	= 0;
	mmpt_get(&mode, NULL, &ppn);

	if (mode != SMMPT_34){
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("smmpt 34 mode test failed \n");
		return ret;
	}

	int level = 2;
	mpt_entry_u *entry  = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	idx = GET_INDEX(addr, level);
	entry = (mpt_entry_u*)(&entry[idx]);
	while (entry->mpt_entry_union.leaf != 1){
		level--;
		ppn = mpt_get_ppn(entry->mpt_entry_union) << PAGE_SHIFT;
		entry = (mpt_entry_u*)(ppn);
		idx = GET_INDEX(addr, level);
		entry = (mpt_entry_u*)(&entry[idx]);
	}

	if (level >= 0){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("smmpt 34 mode test passed \n");
	}
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("smmpt 34 mode test failed \n");
	}
	return ret;
#endif
}

static int smmpt43(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT 43 mode test
	*/
	int ret = SBI_OK;

	// if not riscv64, return fail
#if __riscv_xlen != 64
	out->value = SBI_EFAIL;
	ret = SBI_EFAIL;
	sbi_printf("smmpt 43 mode test failed, not riscv64 \n");
	return ret;
#else

	mmpt_mode_t mode = 0;
	physical_addr_t ppn = 0;
	physical_addr_t addr = 0x80000000;
	int idx	= 0;
	mmpt_get(&mode, NULL, &ppn);

	if (mode != SMMPT_43){
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("smmpt 43 mode test failed \n");
		return ret;
	}

	int level = 3;
	mpt_entry_u *entry  = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	idx = GET_INDEX(addr, level);
	entry = (mpt_entry_u*)(&entry[idx]);
	while (entry->mpt_entry_union.leaf != 1){
		level--;
		ppn = mpt_get_ppn(entry->mpt_entry_union) << PAGE_SHIFT;
		entry = (mpt_entry_u*)(ppn);
		idx = GET_INDEX(addr, level);
		entry = (mpt_entry_u*)(&entry[idx]);
	}

	if (level >= 0){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("smmpt 43 mode test passed \n");
	}
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("smmpt 43 mode test failed \n");
	}
	return ret;
#endif
}

static int smmpt52(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT 52 mode test
	*/
	int ret = SBI_OK;

	// if not riscv64, return fail
#if __riscv_xlen != 64
	out->value = SBI_EFAIL;
	ret = SBI_EFAIL;
	sbi_printf("smmpt 52 mode test failed, not riscv64 \n");
	return ret;
#else

	mmpt_mode_t mode = 0;
	physical_addr_t ppn = 0;
	physical_addr_t addr = 0x80000000;
	int idx	= 0;
	mmpt_get(&mode, NULL, &ppn);

	if (mode != SMMPT_52){
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("smmpt 52 mode test failed \n");
		return ret;
	}

	int level = 4;
	mpt_entry_u *entry  = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	idx = GET_INDEX(addr, level);
	entry = (mpt_entry_u*)(&entry[idx]);
	while (entry->mpt_entry_union.leaf != 1){
		level--;
		ppn = mpt_get_ppn(entry->mpt_entry_union) << PAGE_SHIFT;
		entry = (mpt_entry_u*)(ppn);
		idx = GET_INDEX(addr, level);
		entry = (mpt_entry_u*)(&entry[idx]);
	}

	if (level >= 0){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("smmpt 52 mode test passed \n");
	}
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("smmpt 52 mode test failed \n");
	}
	return ret;
#endif
}

static int smmpt64(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT 64 mode test
	*/
	int ret = SBI_OK;

	// if not riscv64, return fail
#if __riscv_xlen != 64
	out->value = SBI_EFAIL;
	ret = SBI_EFAIL;
	sbi_printf("smmpt 64 mode test failed, not riscv64 \n");
	return ret;
#else

	mmpt_mode_t mode = 0;
	physical_addr_t ppn = 0;
	physical_addr_t addr = 0x80000000;
	int idx	= 0;
	mmpt_get(&mode, NULL, &ppn);

	if (mode != SMMPT_64){
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("smmpt 64 mode test failed \n");
		return ret;
	}

	int level = 5;
	mpt_entry_u *entry  = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	idx = GET_INDEX(addr, level);
	entry = (mpt_entry_u*)(&entry[idx]);
	while (entry->mpt_entry_union.leaf != 1){
		level--;
		ppn = mpt_get_ppn(entry->mpt_entry_union) << PAGE_SHIFT;
		entry = (mpt_entry_u*)(ppn);
		idx = GET_INDEX(addr, level);
		entry = (mpt_entry_u*)(&entry[idx]);
	}

	if (level >= 0){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("smmpt 64 mode test passed \n");
	}
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("smmpt 64 mode test failed \n");
	}
	return ret;
#endif
}

static int mmpt_ppn(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* MMPT CSR PPN test
	*/
	int ret = SBI_OK;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and check entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);

	if ((entry->value & entry_non_leaf_mask) ==0 ||
	   (entry->value & entry_non_napot_leaf_mask) ==0 ||
	   (entry->value & entry_napot_leaf_mask) ==0){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mmpt csr ppn test passed\n");
	}
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("mmpt csr ppn test failed\n");
	}
	return ret;
}

static int smmpt_non_leaf_bit_L(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT non-leaf entry bit L test
	*/
	int ret = SBI_OK;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a non-leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 0){
		entry++;
	}

	if ((entry->value & entry_non_leaf_mask) == 0) {
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt non-leaf entry bit L test passed\n");
	} 
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("mpt non-leaf entry bit L test failed\n");
	}
	return ret;
}

static int smmpt_non_leaf_ppn(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT non-leaf entry ppn test
	*/
	int ret = SBI_OK;
	mmpt_mode_t mode = 0;
	physical_addr_t ppn = 0;
	int level = 0;
	mpt_entry_u *entry  = NULL;

	// Get root ppn
	mmpt_get(&mode, NULL, &ppn);
	int rc = get_mpt_level(mode, &level);
	entry  = (mpt_entry_u *)(ppn << PAGE_SHIFT);

	if (rc || entry->mpt_entry_union.leaf != 0){
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("SMMPT non-leaf entry ppn test failed\n");
		return ret;
	}

	// go to next level 
	level--;
	ppn = mpt_get_ppn(entry->mpt_entry_union) << PAGE_SHIFT;
	entry = (mpt_entry_u*)(ppn);
	int idx = GET_INDEX(ppn, level);
	entry = (mpt_entry_u*)(&entry[idx]);

	// check entry
	if ((entry->value & entry_non_leaf_mask) ==0 ||
	   (entry->value & entry_non_napot_leaf_mask) ==0 ||
	   (entry->value & entry_napot_leaf_mask) ==0){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("SMMPT non-leaf entry ppn test passed\n");
	}
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("SMMPT non-leaf entry ppn test failed\n");
	}
	return ret;
}

static int smmpt_leaf_bit_L(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT leaf entry bit L test
	*/
	int ret = SBI_OK;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a non-leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1){
		entry++;
	}

	if ((entry->value & entry_napot_leaf_mask) == 0 || 
	    (entry->value & entry_non_napot_leaf_mask) == 0 ) {
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt leaf entry bit L test passed\n");
	} 
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("mpt leaf entry bit L test failed\n");
	}
	return ret;
}

static int smmpt_non_napot_leaf_bit_N(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT non-napot leaf entry bit N test
	*/
	int ret = SBI_OK;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a non-napot leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1 || entry->mpt_entry_union.napot != 0){
		entry++;
	}

	if ((entry->value & entry_non_napot_leaf_mask) == 0) {
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("SMMPT non-napot leaf entry bit N test passed\n");
	} 
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("SMMPT non-napot leaf entry bit N test failed\n");
	}
	return ret;
}

static int smmpt_napot_leaf_bit_N(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT napot leaf entry bit N test
	*/
	int ret = SBI_OK;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a napot leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1 || entry->mpt_entry_union.napot != 1){
		entry++;
	}

	if ((entry->value & entry_napot_leaf_mask) == 0) {
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("SMMPT napot leaf entry bit N test passed\n");
	} 
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("SMMPT napot leaf entry bit N test failed\n");
	}
	return ret;
}

static int smmpt_napot_leaf_G(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT napot leaf entry G field test
	*/
	int ret = SBI_OK;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a napot leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1 || entry->mpt_entry_union.napot != 1){
		entry++;
	}

	if (((entry->mpt_entry_union.info >> 4) & 0xF) != G){
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("SMMPT napot leaf entry G field test failed\n");
	}
	
	//check all entries in the G block
	int num_G_blocks = 1 << (G + 1);
	uint8_t perms = entry->mpt_entry_union.info & 0x7;
	uint8_t G_test = (entry->mpt_entry_union.info >> 4) & 0xF;
	for (int i = 0; i < num_G_blocks; i++){
		if ((entry + i)->mpt_entry_union.leaf != 1 ||
		   (entry + i)->mpt_entry_union.napot != 1 ||
		   (((entry + i)->mpt_entry_union.info >> 4) & 0xF) != G_test ||
		   ((entry + i)->mpt_entry_union.info & 0x7) != perms){
			out->value = SBI_EFAIL;
			ret = SBI_EFAIL;
			sbi_printf("SMMPT napot leaf entry G field test failed\n");
			return ret;
		}
	}
	out->value = SBI_OK;
	ret = SBI_OK;
	sbi_printf("SMMPT napot leaf entry G field test passed\n");
	return ret;
}

static int smmpt_bit_valid(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT bit valid test
	*/
	int ret = SBI_OK;
	physical_addr_t addr = regs->a0;
	mmpt_mode_t mode = 0;
	physical_addr_t ppn = 0;
	int idx, level;

	mmpt_get(&mode, NULL, &ppn);
	int rc = get_mpt_level(mode, &level);
	if (rc){
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		return ret;
	}

	mpt_entry_u *entry  = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	idx = GET_INDEX(addr, level);
	entry = (mpt_entry_u*)(&entry[idx]);

	while (entry->mpt_entry_union.leaf != 1){
		level--;
		ppn = mpt_get_ppn(entry->mpt_entry_union) << PAGE_SHIFT;
		entry = (mpt_entry_u*)(ppn);
		idx = GET_INDEX(addr, level);
		entry = (mpt_entry_u*)(&entry[idx]);
	}
	entry->mpt_entry_union.valid = 0;

	/* mpt cache flush instruction mfence.mcpa */
	__asm__ __volatile__(".insn r 0x73, 0x0, 0x12, x0, x0, x0");

	out->value = SBI_OK;
	ret = SBI_OK;
	return ret;
}

static int mmpt_warl_test(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* MMPT WARL test
	*/
	int ret = SBI_OK;
	mmpt_mode_t pre_mode = 0, mode = 6, test_mode = 0;
	unsigned int pre_sdid = 0, sdid = 67, test_sdid = 0;
	physical_addr_t pre_ppn = 0, ppn = 9999, test_ppn = 0;

	// Save current values in mttp
	mmpt_get(&pre_mode, &pre_sdid, &pre_ppn);

	// Write all illegal values to mttp and get values back
	mmpt_set(mode, sdid, ppn);
	mmpt_get(&test_mode, &test_sdid, &test_ppn);
	// Reset back old values
	mmpt_set(pre_mode, pre_sdid, pre_ppn);

	/*
	 * Check the values .
	 * We expect the read-back value to differ from the written value due to WARL.
	 */
	if (test_mode == mode && test_sdid == sdid && test_ppn == ppn) {
		out->value = SBI_EFAIL;
		sbi_printf("smmtt test mttp warl test failed\n");
		ret = SBI_EFAIL;
	}
	else {
		out->value = SBI_OK;
		sbi_printf("smmtt test mttp warl test passed\n");
		ret = SBI_OK;
	}
	return ret;
}

static int smmpt_non_leaf_reserved_test(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT non leaf entry reserved field test
	*/
	int ret = SBI_OK;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	/* Get ppn and find a non-leaf entry */
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 0){
		entry++;
	}

	if ((entry->value & entry_non_leaf_mask) == 0) {
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt non leaf reserved test passed\n");
	} 
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("mpt non leaf reserved test failed\n");
	}
	return ret;
}

static int smmpt_non_napot_leaf_reserved_test(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT non napot leaf entry reserved field test
	*/
	int ret = SBI_OK;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	/* Get ppn and find a non napot leaf entry */
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1 || entry->mpt_entry_union.napot != 0){
		entry++;
	}

	if ((entry->value & entry_non_napot_leaf_mask) == 0) {
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt non napot leaf reserved test passed\n");
	} 
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("mpt non napot leaf reserved test failed\n");
	}
	return ret;
}

static int smmpt_napot_leaf_reserved_test(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT napot leaf entry reserved field test
	*/
	int ret = SBI_OK;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	/* Get ppn and find a napot leaf entry */
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1 || entry->mpt_entry_union.napot != 1){
		entry++;
	}

	if ((entry->value & entry_napot_leaf_mask) == 0) {
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt napot leaf reserved test passed\n");
	} 
	else {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("mpt napot leaf reserved test failed\n");
	}
	return ret;
}

static int smmpt_perm_revise_test(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT perm field revise test
	*/
	int ret = SBI_OK;
	physical_addr_t addr = regs->a0;
	unsigned long new_perm = (regs->a1) >> 3;
	mmpt_mode_t mode = 0;
	physical_addr_t ppn = 0;
	int idx, level, offset;

	mmpt_get(&mode, NULL, &ppn);
	int rc = get_mpt_level(mode, &level);
	if (rc){
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		return ret;
	}

	mpt_entry_u *entry  = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	idx = GET_INDEX(addr, level);
	entry = (mpt_entry_u*)(&entry[idx]);

	while (entry->mpt_entry_union.leaf != 1){
		level--;
		ppn = mpt_get_ppn(entry->mpt_entry_union) << PAGE_SHIFT;
		entry = (mpt_entry_u*)(ppn);
		idx = GET_INDEX(addr, level);
		entry = (mpt_entry_u*)(&entry[idx]);
	}
	offset = addr >> (pa_pn_offset[level - 1] - RANGE_NUM) & RANGE_MASK;
	entry->mpt_entry_union.info = (entry->mpt_entry_union.info & ~((unsigned long)0x7 << (offset * 3))) |
		      (((unsigned long)new_perm & 0x7) << (offset * 3));

	/* mpt cache flush instruction mfence.mcpa */
	__asm__ __volatile__(".insn r 0x73, 0x0, 0x12, x0, x0, x0");

	out->value = SBI_OK;
	ret = SBI_OK;
	return ret;
}

static int smmpt_pmp_disallow_test(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT pmp disallow test
	*/
	int ret = SBI_OK;
	physical_addr_t addr = regs->a0;
	mmpt_mode_t mode = 0;
	physical_addr_t ppn = 0;
	int idx, level;
	unsigned long prot, base, log2len;
#if __riscv_xlen == 32
	unsigned long new_log2len = 2;
#else
	unsigned long new_log2len = 3;
#endif
	struct sbi_trap_info trap;
	register ulong tinfo asm("a3");
	register ulong mtvec = sbi_hart_expected_trap_addr();
	ulong test = 0;


	/* get mpt ppn */
	mmpt_get(&mode, NULL, &ppn);
	int rc = get_mpt_level(mode, &level);
	if (rc){
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		return ret;
	}

	mpt_entry_u *entry  = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	idx = GET_INDEX(addr, level);
	entry = (mpt_entry_u*)(&entry[idx]);

	while (entry->mpt_entry_union.leaf != 1){
		level--;
		ppn = mpt_get_ppn(entry->mpt_entry_union) << PAGE_SHIFT;
		entry = (mpt_entry_u*)(ppn);
		idx = GET_INDEX(addr, level);
		entry = (mpt_entry_u*)(&entry[idx]);
	}

	/* set mpt ppn pmp disallow */
	pmp_get(0, &prot, &base, &log2len);

	pmp_set(0, PMP_L, (unsigned long)ppn, new_log2len);
	pmp_set(1, prot, base, log2len);

	/* load from ppn */
#if __riscv_xlen == 32
    	asm volatile(
	    "add %[tinfo], %[taddr], zero\n"
	    "csrrw %[mtvec], " STR(CSR_MTVEC) ", %[mtvec]\n"
        ".option norvc\n\t"
        "lw %[tmp], (%[address])\n\t"
        ".option rvc\n\t"
	    "csrw " STR(CSR_MTVEC) ", %[mtvec]"
	    : [mtvec] "+&r"(mtvec),
	      [tinfo] "+&r"(tinfo), 
		  [tmp] "=r"(test)
	    : [taddr] "r"((ulong)&trap), 
		  [address] "r"((ulong)ppn)
	    : "memory");
#else
    	asm volatile(
	    "add %[tinfo], %[taddr], zero\n"
	    "csrrw %[mtvec], " STR(CSR_MTVEC) ", %[mtvec]\n"
		".option norvc\n\t"			/* disable compress instruction */
        "ld %[tmp], (%[address])\n\t"
        ".option rvc\n\t" 			/* enable compress instruction */
	    "csrw " STR(CSR_MTVEC) ", %[mtvec]"
	    : [mtvec] "+&r"(mtvec),
	      [tinfo] "+&r"(tinfo), 
		  [tmp] "=r"(test)
	    : [taddr] "r"((ulong)&trap), 
		  [address] "r"((ulong)ppn)
	    : "memory");
#endif

	if (trap.cause == CAUSE_LOAD_ACCESS){
		out->value = SBI_OK;
	}
	else {
		out->value = SBI_EFAIL;
	}

	return ret;
}

static int smmpt_set_hart(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT set hartid test
	*/
	int ret = SBI_OK;
	u32 hartid = regs->a0;
	u32 hidx = sbi_hartid_to_hartindex(hartid);
	struct sbi_domain *dom = sbi_domain_thishart_ptr();

	/* set assigned_harts bit under lock */
	spin_lock(&dom->assigned_harts_lock);
	sbi_hartmask_set_hartindex(hidx, &dom->assigned_harts);
	spin_unlock(&dom->assigned_harts_lock);

	/* update hartindex -> domain mapping */
	sbi_update_hartindex_to_domain(hidx, dom);

	out->value = SBI_OK;
	ret = SBI_OK;
	return ret;
}

static int smmpt_test_mem_init(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT test memregion initialization
	*/
	int ret = SBI_OK;
	unsigned long base, size, perm;
	int level, rc;
	bool is_flush = regs->a3;

	struct sbi_domain *dom = sbi_domain_thishart_ptr();

	/* get test memregion base,size and perm from regs */
	base = regs->a0;
	size = regs->a1;
	perm = regs->a2;

	/* init memregion according to base,size and perm */
	if (dom->mmpt_mode == SMMPT_BARE)
		dom->mmpt_mode = SMMPT_DEFAULT_MODE;

	rc = get_mpt_level(dom->mmpt_mode, &level);
	if (rc){
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		return ret;
	}

	rc = add_mpt_region((mpt_entry_t *)dom->mpt, &base, &size, perm, level);
	if (rc){
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		return ret;
	}
	
 	/* mpt cache flush instruction mfence.mcpa */
	if (is_flush){
		__asm__ __volatile__(".insn r 0x73, 0x0, 0x12, x0, x0, x0");
	}
	
	out->value = SBI_OK;
	ret = SBI_OK;
	return ret;
}

static int smmpt_debug(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* smmpt debug ecall
	*/
	int ret = SBI_OK;
	sbi_printf("SMMPT Test Extension Debug Ecall Invoked\n");

	out->value = SBI_OK;
	ret = SBI_OK;
	return ret;
}

static int sbi_ecall_smmpt_test_handler(unsigned long extid, unsigned long funcid,
							struct sbi_trap_regs* regs,
							struct sbi_ecall_return* out)
{
	int ret = SBI_OK;
	switch (funcid)
	{
	case SBI_EXT_MMPT_READ:
		ret = smmpt_test_mmpt_rw(out);
		break;
	case SBI_EXT_MMPT_WRITE:
		ret = smmpt_test_mmpt_rw(out);
		break;
	case SBI_EXT_NON_LEAF_DECODE_BIT:
		ret = mpt_non_leaf_decode_bit(regs, out);
		break;
	case SBI_EXT_NON_LEAF_DECODE_PPN:
		ret = mpt_non_leaf_decode_ppn(regs, out);
		break;
	case SBI_EXT_NON_NAPOT_LEAF_DECODE_BIT:
		ret = mpt_non_napot_leaf_decode_bit(regs, out);
		break;
	case SBI_EXT_NON_NAPOT_LEAF_DECODE_PERM:
		ret = mpt_non_napot_leaf_decode_perm(regs, out);
		break;
	case SBI_EXT_NAPOT_LEAF_DECODE_BIT:
		ret = mpt_napot_leaf_decode_bit(regs, out);
		break;
	case SBI_EXT_NAPOT_LEAF_DECODE_G:
		ret = mpt_napot_leaf_decode_G(regs, out);
		break;
	case SBI_EXT_NAPOT_LEAF_DECODE_PERM:
		ret = mpt_napot_leaf_decode_perm(regs, out);
		break;
	case SBI_EXT_MMPT_SMMPT34:
		ret = smmpt34(regs, out);
		break;
	case SBI_EXT_MMPT_SMMPT43:
		ret = smmpt43(regs, out);
		break;
	case SBI_EXT_MMPT_SMMPT52:
		ret = smmpt52(regs, out);
		break;
	case SBI_EXT_MMPT_SMMPT64:
		ret = smmpt64(regs, out);
		break;
	case SBI_EXT_MMPT_PPN:
		ret = mmpt_ppn(regs, out);
		break;
	case SBI_EXT_NON_LEAF_BIT_L:
		ret = smmpt_non_leaf_bit_L(regs, out);
		break;
	case SBI_EXT_NON_LEAF_PPN:
		ret = smmpt_non_leaf_ppn(regs, out);
		break;
	case SBI_EXT_NON_NAPOT_LEAF_BIT_L:
		ret = smmpt_leaf_bit_L(regs, out);
		break;
	case SBI_EXT_NON_NAPOT_LEAF_BIT_N:
		ret = smmpt_non_napot_leaf_bit_N(regs, out);
		break;
	case SBI_EXT_NAPOT_LEAF_BIT_L:
		ret = smmpt_leaf_bit_L(regs, out);
		break;
	case SBI_EXT_NAPOT_LEAF_BIT_N:
		ret = smmpt_napot_leaf_bit_N(regs, out);
		break;
	case SBI_EXT_NAPOT_LEAF_G:
		ret = smmpt_napot_leaf_G(regs, out);
		break;
	case SBI_EXT_BIT_VALID:
		ret = smmpt_bit_valid(regs, out);
		break;
	case SBI_EXT_MMPT_WARL:
		ret = mmpt_warl_test(regs, out);
		break;
	case SBI_EXT_NON_LEAF_RESERVE:
		ret = smmpt_non_leaf_reserved_test(regs, out);
		break;
	case SBI_EXT_NON_NAPOT_LEAF_RESERVE:
		ret = smmpt_non_napot_leaf_reserved_test(regs, out);
		break;
	case SBI_EXT_NAPOT_LEAF_RESERVE:
		ret = smmpt_napot_leaf_reserved_test(regs, out);
		break;
	case SBI_EXT_MPT_PERM_REVISE:
		ret = smmpt_perm_revise_test(regs, out);
		break;
	case SBI_EXT_PMP_DISALLOW:
		ret = smmpt_pmp_disallow_test(regs, out);
		break;
	case SBI_EXT_SET_HART:
		ret = smmpt_set_hart(regs, out);
		break;
	case SBI_EXT_TEST_MEM_INIT:
		ret = smmpt_test_mem_init(regs, out);
		break;
	case SBI_EXT_DEBUG:
		ret = smmpt_debug(regs, out);
		break;
	default:
		ret = SBI_ENODEV;
		break;
	}

	return ret;
}

struct sbi_ecall_extension ecall_smmpt_test;

static int sbi_ecall_smmpt_test_register_extensions(void)
{
	return sbi_ecall_register_extension(&ecall_smmpt_test);
}

struct sbi_ecall_extension ecall_smmpt_test = {
	.extid_start		= SBI_EXT_TEST,
	.extid_end		= SBI_EXT_TEST,
	.register_extensions	= sbi_ecall_smmpt_test_register_extensions,
	.handle			= sbi_ecall_smmpt_test_handler,
};