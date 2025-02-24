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
    TYPE_2M_PAGE        = 0b101,
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

#define PA32_PN0      _ULL(0x000007000)
#define PA32_PN1      _ULL(0x001ff8000)
#define PA32_PN2      _ULL(0x3fe000000)
#define PA32_4M_OFFS  _ULL(0x001c00000)
#define PA64_PN0      _ULL(0x0000000000f000)
#define PA64_PN1      _ULL(0x00000001ff0000)
#define PA64_PN2      _ULL(0x003ffffe000000)
#define PA64_PN3      _ULL(0xffc00000000000)
#define PA64_2M_OFFS  _ULL(0x00000001e00000)

#if __riscv_xlen == 32
#define PA_PN0      PA32_PN0
#define PA_PN1      PA32_PN1
#define PA_PN2      PA32_PN2
#define PA_XM_OFFS  PA32_4M_OFFS
#else
#define PA_PN0      PA64_PN0
#define PA_PN1      PA64_PN1
#define PA_PN2      PA64_PN2
#define PA_PN3      PA64_PN3
#define PA_XM_OFFS  PA64_2M_OFFS
#endif

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

#if __riscv_xlen == 32
typedef struct
{
    uint32_t info: 22;
    uint32_t type: 3;
    uint32_t zero: 7;
} mttl2_entry_t;
#else
typedef struct
{
    uint64_t info: 44;
    uint64_t type: 3;
    uint64_t zero: 17;
} mttl2_entry_t;
#endif

typedef uint64_t mttl1_entry_t;

unsigned int mttp_get_sdidlen();

void mttp_set(mttp_mode_t mode, unsigned int sdid, physical_addr_t ppn);

void mttp_get(mttp_mode_t* mode, unsigned int* sdid, physical_addr_t* ppn);

int sbi_smmtt_init(struct sbi_scratch *scratch, bool cold_boot);

int sbi_hart_smmtt_configure(struct sbi_scratch *scratch);
#endif   // __SBI_SMMTT_H__