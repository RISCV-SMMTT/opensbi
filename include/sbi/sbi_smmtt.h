#ifndef __SBI_SMMTT_H__
#define __SBI_SMMTT_H__

#include <sbi/sbi_const.h>
#include <sbi/sbi_types.h>
#include <sbi/sbi_scratch.h>

typedef enum
{
    TYPE_1G_DISALLOW    = 0b000,
    TYPE_1G_ALLOW_RX    = 0b001,
    TYPE_1G_ALLOW_RW    = 0b010,
    TYPE_1G_ALLOW_RWX   = 0b011,
    TYPE_MTTL1_DIR      = 0b100,
#if __riscv_xlen == 32
    TYPE_4M_PAGE        = 0b101,
#else
    TYPE_2M_PAGE        = 0b110,
#endif
} smmtt_type;

typedef enum
{
    PERMS_XM_DISALLOW   = 0b00,
    PERMS_XM_ALLOW_RX   = 0b01,
    PERMS_XM_ALLOW_RW   = 0b10,
    PERMS_XM_ALLOW_RWX  = 0b11,
} smmtt_xm_perms;

typedef enum {
    PERMS_MTTL1_DISALLOWED   = 0b00,
    PERMS_MTTL1_ALLOW_RX     = 0b01,
    PERMS_MTTL1_ALLOW_RW     = 0b10,
    PERMS_MTTL1_ALLOW_RWX    = 0b11,
} perms_mttl1;

#define PA_PN0      _ULL(0x0000000000f000)
#define PA_PN1      _ULL(0x00000001ff0000)
#define PA_PN2      _ULL(0x003ffffe000000)
#define PA_PN3      _ULL(0xffc00000000000)
#define PA_XM_OFFS  _ULL(0x00000001e00000)

#define MTT_PERMS_MASK  _ULL(0b11)
#define MTT_PERMS_BITS  (2)

#define MTT_PERM_FIELD(idx) \
    MTT_PERMS_MASK << (MTT_PERMS_BITS * (idx))

typedef int mttp_mode_t;

enum {
    SMMTT_BARE,
#if __riscv_xlen == 32
    SMMTT_34,
#else
    SMMTT_46,
    SMMTT_56,
#endif
    SMMTT_MAX
};

typedef struct
{
    uint64_t mttl2_ppn: 44;
    uint64_t zero: 20;
} mttl3_entry_t;

typedef struct
{
    uint64_t info: 44;
    uint64_t type: 3;
    uint64_t zero: 17;
} mttl2_entry_t;

typedef uint64_t mttl1_entry_t;

void mttp_set(mttp_mode_t mode, unsigned int sdid, physical_addr_t ppn);

void mttp_get(mttp_mode_t* mode, unsigned int* sdid, physical_addr_t* ppn);

int sbi_smmtt_init(struct sbi_scratch *scratch, bool cold_boot);

int sbi_hart_smmtt_configure(struct sbi_scratch *scratch);
#endif   // __SBI_SMMTT_H__