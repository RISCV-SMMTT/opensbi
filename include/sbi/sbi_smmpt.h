#ifndef __SBI_SMMTT_H__
#define __SBI_SMMTT_H__

#include <sbi/sbi_const.h>
#include <sbi/sbi_types.h>
#include <sbi/sbi_scratch.h>

/** MMPT */

#define MMPT32_MODE_MASK      (unsigned long)0xC0000000
#define MMPT32_SDID_SHIFT		24
#define MMPT32_SDID_MASK      (unsigned long)0x0FC00000
#define MMPT32_MODE_SHIFT		30
#define MMPT32_PPN_MASK       (unsigned long)0x003FFFFF

#define MMPT64_MODE_MASK      (unsigned long long)0xF000000000000000
#define MMPT64_SDID_SHIFT		54
#define MMPT64_SDID_MASK      (unsigned long long)0x03F0000000000000
#define MMPT64_MODE_SHIFT		60
#define MMPT64_PPN_MASK       (unsigned long long)0x00000FFFFFFFFFFF

#if __riscv_xlen == 32

#define MMPT_MODE_MASK   MMPT32_MODE_MASK
#define MMPT_MODE_SHIFT    MMPT32_MODE_MASK_SHIFT
#define MMPT_SDID_MASK   MMPT32_SDID_MASK
#define MMPT_SDID_SHIFT   MMPT32_SDID_SHIFT
#define MMPT_PPN_MASK    MMPT32_PPN_MASK

#else // __riscv_xlen == 64

#define MMPT_MODE_MASK   MMPT64_MODE_MASK
#define MMPT_MODE_SHIFT    MMPT64_MODE_SHIFT
#define MMPT_SDID_MASK   MMPT64_SDID_MASK
#define MMPT_SDID_SHIFT   MMPT64_SDID_SHIFT
#define MMPT_PPN_MASK    MMPT64_PPN_MASK

#endif // __riscv_xlen

typedef enum {
    SMMPT_BARE,
#if __riscv_xlen == 32
    SMMPT_34,
#else 
    SMMPT_43,
    SMMPT_52,
    SMMPT_64,
#endif
    SMMTT_MAX
} mmpt_mode_t;

void mmpt_set(mmpt_mode_t mode, unsigned int sdid, physical_addr_t ppn);

void mmpt_get(mmpt_mode_t* mode, unsigned int* sdid, physical_addr_t* ppn);

int sbi_smmpt_init(struct sbi_scratch *scratch, bool cold_boot);

#endif   // __SBI_SMMTT_H__