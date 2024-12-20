#ifndef __SBI_SMMTT_H__
#define __SBI_SMMTT_H__

#include <sbi/sbi_const.h>
#include <sbi/sbi_types.h>
#include <sbi/sbi_scratch.h>

typedef enum {
    SMMTT_BARE,
#if __riscv_xlen == 32
    SMMTT_34,
#else 
    SMMTT_46,
    SMMTT_56,
#endif
    SMMTT_MAX
} mttp_mode_t;

void mttp_set(mttp_mode_t mode, unsigned int sdid, physical_addr_t ppn);

void mttp_get(mttp_mode_t* mode, unsigned int* sdid, physical_addr_t* ppn);

int sbi_smmtt_init(struct sbi_scratch *scratch, bool cold_boot);

#endif   // __SBI_SMMTT_H__