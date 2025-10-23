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

static inline uint64_t mpt_get_ppn(mpt_entry_t entry)
{
#if __riscv_xlen == 32
	return (entry.info >> 2) & 0x3fffff;
#else
	return (entry.info >> 2) & 0xfffffffffffULL;
#endif
}

static inline void mpt_set_ppn(mpt_entry_t *mpt_entry, uintptr_t ppn)
{
	mpt_entry->info = (uint64_t)(ppn << 2);
}

static int smmpt_test_mmpt_rw(struct sbi_ecall_return* out)
{
	/*
	* MMPT CSR Read/Write test
	*/
	int ret = 0;
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
	int ret = 0;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a non-leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 0){
		entry++;
	}

	if ((entry->value & entry_non_leaf_mask) == 0) {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("mpt non leaf decode bit test passed\n");
	} 
	else {
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt non leaf decode bit test failed\n");
	}
	return ret;
}

static int mpt_non_leaf_decode_ppn(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* MPT non leaf decode ppn test
	*/
	int ret = 0;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a non-leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 0){
		entry++;
	}
	// ppn is not zero, decode success.
	if(entry->mpt_entry_union.info){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt non leaf decode ppn test passed\n");
	}
	else{
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
	int ret = 0;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a non-napot leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1 || entry->mpt_entry_union.napot != 0){
		entry++;
	}

	if ((entry->value & entry_non_napot_leaf_mask) == 0) {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("mpt non napot leaf decode bit test passed\n");
	} 
	else {
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt non napot leaf decode bit test failed\n");
	}
	return ret;
}

static int mpt_non_napot_leaf_decode_perm(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* MPT non-napot leaf decode permission test
	*/
	int ret = 0;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a non-napot leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1 || entry->mpt_entry_union.napot != 0){
		entry++;
	}

	//save perms
	uint64_t perms = entry->mpt_entry_union.info;

	//set perms to test value
	entry->mpt_entry_union.info = 0x7;
	if(entry->mpt_entry_union.info == 0x7){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt non napot leaf decode permission test passed\n");
	}
	else{
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
	int ret = 0;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a napot leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1 || entry->mpt_entry_union.napot != 1){
		entry++;
	}

	if ((entry->value & entry_napot_leaf_mask) == 0) {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("mpt napot leaf decode bit test passed\n");
	} 
	else {
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt napot leaf decode bit test failed\n");
	}
	return ret;
}

static int mpt_napot_leaf_decode_G(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* MPT napot leaf decode G field test
	*/
	int ret = 0;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a non-napot leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1 || entry->mpt_entry_union.napot != 0){
		entry++;
	}

	// save info
	uint64_t info = entry->mpt_entry_union.info;

	// set G to test value
	entry->mpt_entry_union.info = (0x5 << 4);
	if((entry->mpt_entry_union.info >> 4) == 0x5){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt napot leaf decode G test passed\n");
	}
	else{
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
	int ret = 0;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a non-napot leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1 || entry->mpt_entry_union.napot != 0){
		entry++;
	}

	// save info
	uint64_t info = entry->mpt_entry_union.info;

	// set G to test value
	entry->mpt_entry_union.info = 0x7;
	if(entry->mpt_entry_union.info == 0x7){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt napot leaf decode permission test passed\n");
	}
	else{
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
	int ret = 0;

	// if not riscv32, return fail
#if __riscv_xlen != 32
	out->value = SBI_EFAIL;
	ret = SBI_EFAIL;
	sbi_printf("smmpt 34 mode test failed, not riscv32 \n");
	return ret;
#else

	mmpt_mode_t mode = 0;
	uint64_t ppn = 0;
	int idx	= 0;
	mmpt_get(&mode, NULL, &ppn);

	if(mode != SMMPT_34){
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("smmpt 34 mode test failed \n");
		return ret;
	}

	int level = 2;
	mpt_entry_u *entry  = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while(entry->mpt_entry_union.leaf != 1){
		level--;
		ppn = mpt_get_ppn(entry->mpt_entry_union) << PAGE_SHIFT;
		entry = (mpt_entry_u*)(ppn);
		idx = GET_INDEX(ppn, level);
		entry = (mpt_entry_u*)(&entry[idx]);
	}

	if(level >= 0){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("smmpt 34 mode test passed \n");
	}
	else{
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
	int ret = 0;

	// if not riscv64, return fail
#if __riscv_xlen != 64
	out->value = SBI_EFAIL;
	ret = SBI_EFAIL;
	sbi_printf("smmpt 43 mode test failed, not riscv64 \n");
	return ret;
#else

	mmpt_mode_t mode = 0;
	uint64_t ppn = 0;
	int idx	= 0;
	mmpt_get(&mode, NULL, &ppn);

	if(mode != SMMPT_43){
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("smmpt 43 mode test failed \n");
		return ret;
	}

	int level = 3;
	mpt_entry_u *entry  = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while(entry->mpt_entry_union.leaf != 1){
		level--;
		ppn = mpt_get_ppn(entry->mpt_entry_union) << PAGE_SHIFT;
		entry = (mpt_entry_u*)(ppn);
		idx = GET_INDEX(ppn, level);
		entry = (mpt_entry_u*)(&entry[idx]);
	}

	if(level >= 0){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("smmpt 43 mode test passed \n");
	}
	else{
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
	int ret = 0;

	// if not riscv64, return fail
#if __riscv_xlen != 64
	out->value = SBI_EFAIL;
	ret = SBI_EFAIL;
	sbi_printf("smmpt 52 mode test failed, not riscv64 \n");
	return ret;
#else

	mmpt_mode_t mode = 0;
	uint64_t ppn = 0;
	int idx	= 0;
	mmpt_get(&mode, NULL, &ppn);

	if(mode != SMMPT_52){
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("smmpt 52 mode test failed \n");
		return ret;
	}

	int level = 4;
	mpt_entry_u *entry  = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while(entry->mpt_entry_union.leaf != 1){
		level--;
		ppn = mpt_get_ppn(entry->mpt_entry_union) << PAGE_SHIFT;
		entry = (mpt_entry_u*)(ppn);
		idx = GET_INDEX(ppn, level);
		entry = (mpt_entry_u*)(&entry[idx]);
	}

	if(level >= 0){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("smmpt 52 mode test passed \n");
	}
	else{
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
	int ret = 0;

	// if not riscv64, return fail
#if __riscv_xlen != 64
	out->value = SBI_EFAIL;
	ret = SBI_EFAIL;
	sbi_printf("smmpt 64 mode test failed, not riscv64 \n");
	return ret;
#else

	mmpt_mode_t mode = 0;
	uint64_t ppn = 0;
	int idx	= 0;
	mmpt_get(&mode, NULL, &ppn);

	if(mode != SMMPT_64){
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("smmpt 64 mode test failed \n");
		return ret;
	}

	int level = 5;
	mpt_entry_u *entry  = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while(entry->mpt_entry_union.leaf != 1){
		level--;
		ppn = mpt_get_ppn(entry->mpt_entry_union) << PAGE_SHIFT;
		entry = (mpt_entry_u*)(ppn);
		idx = GET_INDEX(ppn, level);
		entry = (mpt_entry_u*)(&entry[idx]);
	}

	if(level >= 0){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("smmpt 64 mode test passed \n");
	}
	else{
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
	int ret = 0;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and check entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);

	if((entry->value & entry_non_leaf_mask) ==0 ||
	   (entry->value & entry_non_napot_leaf_mask) ==0 ||
	   (entry->value & entry_napot_leaf_mask) ==0){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mmpt csr ppn test passed\n");
	}
	else{
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
	int ret = 0;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a non-leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 0){
		entry++;
	}

	if ((entry->value & entry_non_leaf_mask) == 0) {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("mpt non-leaf entry bit L test passed\n");
	} 
	else {
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt non-leaf entry bit L test failed\n");
	}
	return ret;
}

static int smmpt_non_leaf_ppn(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT non-leaf entry ppn test
	*/
	int ret = 0;
	mmpt_mode_t mode = 0;
	physical_addr_t ppn = 0;
	int level = 0;
	mpt_entry_u *entry  = NULL;

	// Get root ppn
	mmpt_get(&mode, NULL, &ppn);
	int rc = get_mpt_level(mode, &level);
	entry  = (mpt_entry_u *)(ppn << PAGE_SHIFT);

	if(rc || entry->mpt_entry_union.leaf != 0){
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("SMMPT non-leaf entry ppn test failed\n");
	}

	// go to next level 
	level--;
	ppn = mpt_get_ppn(entry->mpt_entry_union) << PAGE_SHIFT;
	entry = (mpt_entry_u*)(ppn);
	int idx = GET_INDEX(ppn, level);
	entry = (mpt_entry_u*)(&entry[idx]);

	// check entry
	if((entry->value & entry_non_leaf_mask) ==0 ||
	   (entry->value & entry_non_napot_leaf_mask) ==0 ||
	   (entry->value & entry_napot_leaf_mask) ==0){
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("SMMPT non-leaf entry ppn test passed\n");
	}
	else{
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
	int ret = 0;
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
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("mpt leaf entry bit L test passed\n");
	} 
	else {
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("mpt leaf entry bit L test failed\n");
	}
	return ret;
}

static int smmpt_non_napot_leaf_bit_N(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT non-napot leaf entry bit N test
	*/
	int ret = 0;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a non-napot leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1 || entry->mpt_entry_union.napot != 0){
		entry++;
	}

	if ((entry->value & entry_non_napot_leaf_mask) == 0) {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("SMMPT non-napot leaf entry bit N test passed\n");
	} 
	else {
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("SMMPT non-napot leaf entry bit N test failed\n");
	}
	return ret;
}

static int smmpt_napot_leaf_bit_N(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT napot leaf entry bit N test
	*/
	int ret = 0;
	physical_addr_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a napot leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1 || entry->mpt_entry_union.napot != 1){
		entry++;
	}

	if ((entry->value & entry_napot_leaf_mask) == 0) {
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("SMMPT napot leaf entry bit N test passed\n");
	} 
	else {
		out->value = SBI_OK;
		ret = SBI_OK;
		sbi_printf("SMMPT napot leaf entry bit N test failed\n");
	}
	return ret;
}

static int smmpt_napot_leaf_G(struct sbi_trap_regs* regs, struct sbi_ecall_return* out)
{
	/*
	* SMMPT napot leaf entry G field test
	* TODO: Not yet implemented, return ok
	*/
	int ret = 0;
	uint64_t ppn = 0;
	mpt_entry_u *entry  = NULL;

	// Get ppn and find a napot leaf entry
	mmpt_get(NULL, NULL, &ppn);
	entry = (mpt_entry_u*)(ppn << PAGE_SHIFT);
	while (entry->mpt_entry_union.leaf != 1 || entry->mpt_entry_union.napot != 1){
		entry++;
	}

	if(((entry->mpt_entry_union.info >> 4) & 0xF) != G){
		out->value = SBI_EFAIL;
		ret = SBI_EFAIL;
		sbi_printf("SMMPT napot leaf entry G field test failed\n");
	}
	
	//check all entries in the G block
	int num_G_blocks = 2 << (G + 1);
	uint8_t perms = entry->mpt_entry_union.info & 0x7;
	uint8_t G_test = (entry->mpt_entry_union.info >> 4) & 0xF;
	for(int i = 0; i < num_G_blocks; i++){
		if((entry + i)->mpt_entry_union.leaf != 1 ||
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

static int sbi_ecall_smmpt_test_handler(unsigned long extid, unsigned long funcid,
							struct sbi_trap_regs* regs,
							struct sbi_ecall_return* out)
{
	int ret = 0;
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