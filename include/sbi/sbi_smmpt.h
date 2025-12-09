#ifndef __SBI_SMMPT_H__
#define __SBI_SMMPT_H__

#include <sbi/sbi_const.h>
#include <sbi/sbi_types.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_scratch.h>

/** MMPT */

#define MMPT32_MODE_MASK      (0x3ULL << 30)
#define MMPT32_SDID_MASK      (0x3FULL << 22)
#define MMPT32_PPN_MASK       (0x3FFFFFULL)
#define MMPT32_MODE_SHIFT	  30
#define MMPT32_SDID_SHIFT	  22
#define SMMPT32_DEFAULT_MODE (SMMPT_34)

#define MMPT64_MODE_MASK      (0xFULL << 60)
#define MMPT64_SDID_MASK      (0x3FULL << 52)
#define MMPT64_PPN_MASK       (0x00000FFFFFFFFFFFULL)
#define MMPT64_MODE_SHIFT	  60
#define MMPT64_SDID_SHIFT	  52
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
#define RANGE_MASK 0x7
#define PA_PN0 10
#define PA_PN1 9
#define RANGE_NUM 3
#define G 6

#else // __riscv_xlen == 64

#define MMPT_MODE_MASK   MMPT64_MODE_MASK
#define MMPT_MODE_SHIFT    MMPT64_MODE_SHIFT
#define MMPT_SDID_MASK   MMPT64_SDID_MASK
#define MMPT_SDID_SHIFT   MMPT64_SDID_SHIFT
#define MMPT_PPN_MASK    MMPT64_PPN_MASK
#define SMMPT_DEFAULT_MODE (SMMPT64_DEFAULT_MODE)
#define ROOT_TABLE_SIZE (2 << 15)       // BYTE
#define RANGE_OFFSET 16
#define RANGE_MASK 0xf
#define PA_PN 9
#define PA_PN4 12
#define RANGE_NUM 4
#define G 4

#endif // __riscv_xlen

extern const uint64_t level_sizes[];
extern const uint8_t  pa_pn_offset[];
extern const uint8_t  pa_pn_len[];
extern const uint64_t pa_pn_mask[];
extern const bool     enable_napot_leaf[];

#if __riscv_xlen == 32
#define MUST_PERMS_COUNT 7
#else
#define MUST_PERMS_COUNT 15
#endif

#define TABLE_SIZE (1 << 12)

typedef enum {
    SMMPT_BARE,
#if __riscv_xlen == 32
    SMMPT_34,
#else 
    SMMPT_43,
    SMMPT_52,
    SMMPT_64,
#endif
    SMMPT_MAX
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

/*
 * When a MPT entry is a non-leaf entry, the reserved field is 2 bits longer than that of a leaf entry. 
 * Therefore, decoding the info field of a non-leaf entry requires a corresponding bit offset.
 */
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

#if __riscv_xlen == 32
#define PA_PN_OFFSET_LIST 15, 25
#define PA_PN_LEN         9, 10
#define PA_PN_MASK_LIST   ((1ULL << 9) - 1), ((1ULL << 10) - 1)
#else
/* FIX: pn[0..3]=9-bit, pn[4]=12-bit (top/root) */
#define PA_PN_OFFSET_LIST 16, 25, 34, 43, 52
#define PA_PN_LEN         9,  9,  9,  9,  12
#define PA_PN_MASK_LIST   ((1ULL << 9)  - 1), ((1ULL << 9)  - 1), \
                          ((1ULL << 9)  - 1), ((1ULL << 9)  - 1), \
                          ((1ULL << 12) - 1)
#endif

#define PiB (1ULL << 50)
#define TiB (1ULL << 40)
#define GiB (1ULL << 30)
#define MiB (1ULL << 20)
#define KiB (1ULL << 10)

#if __riscv_xlen == 32
#define LEVEL_SIZE_LIST \
	(4ULL * KiB), \
	(4ULL * MiB)
#define ENABLE_NAPOT_LEAF_LIST \
    (0),  /* L0: 4K page */ \
    (1)    /* L1: 4M page */
#else
#define LEVEL_SIZE_LIST \
	(4ULL * KiB),   /* L0: per 4KiB subpage tuple in leaf */ \
	(2ULL * MiB),   /* L1 */ \
	(1ULL * GiB),   /* L2 */ \
	(512ULL * GiB), /* L3 */ \
	(256ULL * TiB), /* L4 */ \
	(128ULL * PiB)  /* L5 */
#define ENABLE_NAPOT_LEAF_LIST \
    (0),   /* L0: 4K page */ \
    (1),   /* L1: 2M page */ \
    (1),   /* L2: 1G page */ \
    (0),   /* L3: 512G page */ \
    (0),   /* L4: 256T page */ \
    (0)    /* L5: 128P page */
#endif

#define LEVEL_COUNT    (sizeof(level_sizes) / sizeof(level_sizes[0]))
#define PA_PN_LEVELS   (sizeof(pa_pn_offset) / sizeof(pa_pn_offset[0]))

#if __riscv_xlen == 32
#define NUMPGINRANGE   3
#else
#define NUMPGINRANGE   4
#endif
#define OFFSET_ENTRIES (1U << NUMPGINRANGE)

#ifndef PAGE_SHIFT
#define PAGE_SHIFT 12
#endif

#ifndef TABLE_SIZE
#define TABLE_SIZE      (4 * KiB)
#endif

#define SMMPT64_ROOT_ALIGN (32 * KiB)
#define SMMPT64_ROOT_SIZE  (32 * KiB)

#define FITS(base, size, level) \
    (((size) >= level_sizes[(level) - 1]) && \
     ((base) % level_sizes[(level) - 1]) == 0)

#define GET_INDEX(base, level) \
	(((base) >> pa_pn_offset[(level) - 1]) & pa_pn_mask[(level) - 1])

#define ENABLE_NAPOT_LEAF(level) \
    (enable_napot_leaf[(level) - 1])

void mmpt_set(mmpt_mode_t mode, unsigned int sdid, physical_addr_t ppn);

void mmpt_get(mmpt_mode_t* mode, unsigned int* sdid, physical_addr_t* ppn);

int get_mpt_level(mmpt_mode_t mode, int *level);

int add_mpt_region(mpt_entry_t *mpt, unsigned long long *base,
		   unsigned long long *size, unsigned long flags, int level);

int sbi_smmpt_init(struct sbi_scratch *scratch, bool cold_boot);

int sbi_hart_smmpt_configure(struct sbi_scratch *scratch);

void sbi_smmpt_print_table(struct sbi_domain *dom);

#endif   // __SBI_SMMPT_H__