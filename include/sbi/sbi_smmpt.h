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
#define SMMPT32_DEFAULT_MODE (SMMPT_34)

#define MMPT64_MODE_MASK      (unsigned long long)0xF000000000000000
#define MMPT64_SDID_SHIFT		54
#define MMPT64_SDID_MASK      (unsigned long long)0x03F0000000000000
#define MMPT64_MODE_SHIFT		60
#define MMPT64_PPN_MASK       (unsigned long long)0x00000FFFFFFFFFFF
#define SMMPT64_DEFAULT_MODE (SMMPT_43)

#if __riscv_xlen == 32

#define MMPT_MODE_MASK   MMPT32_MODE_MASK
#define MMPT_MODE_SHIFT    MMPT32_MODE_MASK_SHIFT
#define MMPT_SDID_MASK   MMPT32_SDID_MASK
#define MMPT_SDID_SHIFT   MMPT32_SDID_SHIFT
#define MMPT_PPN_MASK    MMPT32_PPN_MASK
#define SMMPT_DEFAULT_MODE (SMMPT32_DEFAULT_MODE)
#define LEAF_TABLE_SIZE (2 << 13)       // BYTE
#define RANGE_OFFSET 15
#define RANGE_MASK 0x7fff
#define PA_PN0 10
#define PA_PN1 9

#else // __riscv_xlen == 64

#define MMPT_MODE_MASK   MMPT64_MODE_MASK
#define MMPT_MODE_SHIFT    MMPT64_MODE_SHIFT
#define MMPT_SDID_MASK   MMPT64_SDID_MASK
#define MMPT_SDID_SHIFT   MMPT64_SDID_SHIFT
#define MMPT_PPN_MASK    MMPT64_PPN_MASK
#define SMMPT_DEFAULT_MODE (SMMPT64_DEFAULT_MODE)
#define ROOT_TABLE_SIZE (2 << 15)       // BYTE
#define RANGE_OFFSET 16
#define RANGE_MASK 0xffff
#define PA_PN 9
#define PA_PN4 12

#endif // __riscv_xlen

extern const uint64_t level_sizes[];

#if __riscv_xlen == 32
#define MUST_PERMS_COUNT 7
#else
#define MUST_PERMS_COUNT 15
#endif

#define TABLE_SIZE (2 << 12)       // BYTE

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

/* MPTE decode */

typedef enum
{
    PERMS_NOACCESS    = 0b000,
    PERMS_RO    = 0b001,
    PERMS_RW    = 0b011,
    PERMS_EO    = 0b100,
    PERMS_RX    = 0b101,
    PERMS_RWX   = 0b111,
} smmpt_perms;

#if __riscv_xlen == 32
typedef struct
{
    uint32_t valid: 1;
    uint32_t leaf: 1;
    uint32_t napot: 1;
    uint32_t reserved: 5;
    uint32_t info: 24;
} mpt_entry_t;
#else
typedef struct
{
    uint64_t valid: 1;
    uint64_t leaf: 1;
    uint64_t napot: 1;
    uint64_t reserved: 5;
    uint64_t info: 56;
} mpt_entry_t;
#endif

void mmpt_set(mmpt_mode_t mode, unsigned int sdid, physical_addr_t ppn);

void mmpt_get(mmpt_mode_t* mode, unsigned int* sdid, physical_addr_t* ppn);

int sbi_smmpt_init(struct sbi_scratch *scratch, bool cold_boot);

int sbi_hart_smmpt_configure(struct sbi_scratch *scratch);

#endif   // __SBI_SMMTT_H__