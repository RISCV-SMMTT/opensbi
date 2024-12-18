#include <sbi/sbi_smmtt.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_bitops.h>
#include <sbi/riscv_asm.h>


#if __riscv_xlen == 32

#define SMMTT_DEFAULT_MODE (SMMTT_34)

#else

#define SMMTT_DEFAULT_MODE (SMMTT_46)

#endif

/* MTTP handling */

void mttp_set(mttp_mode_t mode, unsigned int sdid, physical_addr_t ppn)
{
	uintptr_t mttp = INSERT_FIELD(0, MTTP_PPN_MASK, ppn);
	mttp	       = INSERT_FIELD(mttp, MTTP_SDID_MASK, sdid);
	mttp	       = INSERT_FIELD(mttp, MTTP_MODE_MASK, mode);
	csr_write(CSR_MTTP, mttp);
}

void mttp_get(mttp_mode_t* mode, unsigned int* sdid, physical_addr_t* ppn)
{
	uintptr_t mttp = csr_read(CSR_MTTP);
	if(mode) {
		*mode = (mttp & MTTP_MODE_MASK) >> MTTP_MODE_SHIFT;
	}
	if(sdid) {
		*sdid = (mttp & MTTP_SDID_MASK) >> MTTP_SDID_SHIFT;
	}
	if(ppn) {
		*ppn = (mttp & MTTP_PPN_MASK);
	}
}