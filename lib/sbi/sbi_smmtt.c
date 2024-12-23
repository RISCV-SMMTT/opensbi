#include <sbi/sbi_smmtt.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_bitops.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_heap.h>
#include <sbi/sbi_domain.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <libfdt.h>

#if __riscv_xlen == 32

#define SMMTT_DEFAULT_MODE (SMMTT_34)
#define MTTL2_SIZE (0x4 * 0x400)

#else

#define SMMTT_DEFAULT_MODE (SMMTT_46)
#define MTTL3_SIZE (0x8 * 0x400)
#define MTTL2_SIZE (0x10 * 0x400 * 0x400)

#endif

/* Globals */
static struct sbi_heap_control *smmtt_hpctrl = NULL;
static uint64_t smmtt_base, smmtt_size, smmtt_order;

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

static int get_mtt_level(mttp_mode_t mode, int *level)
{
	int tmp = -1;

	switch (mode) {
	case SMMTT_BARE:
		tmp = -1;
		break;

#if __riscv_xlen == 32
	case SMMTT_34:
#else
	case SMMTT_46:
#endif
		tmp = 2;
		break;

#if __riscv_xlen == 64
	case SMMTT_56:
		tmp = 3;
		break;
#endif
	default:
		return SBI_EINVAL;
	}

	if (level) {
		*level = tmp;
	}

	return SBI_OK;
}

static int initialize_mtt(struct sbi_domain *dom, struct sbi_scratch *scratch)
{
	int rc, level;

	if (!dom->mtt)
	{
		if (dom->mttp_mode == SMMTT_BARE)
		{
			dom->mttp_mode = SMMTT_DEFAULT_MODE;
		}

		rc = get_mtt_level(dom->mttp_mode, &level);
		if (rc)
			return rc;

		if (level == 3)
			dom->mtt = sbi_aligned_alloc_from(smmtt_hpctrl, MTTL3_SIZE, MTTL3_SIZE);
		else
			dom->mtt = sbi_aligned_alloc_from(smmtt_hpctrl, MTTL2_SIZE, MTTL2_SIZE);
	
		if (!dom->mtt)
			return rc = SBI_ENOMEM;
	}

	return rc;
}

int sbi_hart_smmtt_configure(struct sbi_scratch *scratch)
{
	int rc;
	struct sbi_domain *dom = sbi_domain_thishart_ptr();
	unsigned int pmp_count = sbi_hart_pmp_count(scratch);
	/* initialize MTT table */
	rc = initialize_mtt(dom, scratch);
	if (rc)
		return rc;

	mttp_set(SMMTT_BARE, 3, ((uintptr_t)dom->mtt) >> PAGE_SHIFT);
	/* use PMP to protect MTT table */
	pmp_set(pmp_count - 1, PMP_R | PMP_W | PMP_X, 0, __riscv_xlen);
	pmp_set(0, 0, smmtt_base, smmtt_order);

	return SBI_OK;
}

static int setup_mtt_table()
{
	int len;
	void* fdt;
	uint64_t chosen_offset;
	const int* order_prop;
	const int* base_prop;

	fdt = fdt_get_address();
	chosen_offset = fdt_path_offset(fdt, "/chosen/opensbi-domains/smmtt_table");
	if (chosen_offset < 0)
		return SBI_ENOMEM;

	order_prop = fdt_getprop(fdt, chosen_offset, "order", &len);
	base_prop = fdt_getprop(fdt, chosen_offset, "base", &len);

	smmtt_order = fdt32_to_cpu(base_prop[0]);
	smmtt_size = 1ULL << fdt32_to_cpu(*order_prop);
	smmtt_base = ((uint64_t)fdt32_to_cpu(base_prop[0]) << 32) | fdt32_to_cpu(base_prop[1]);

	if (smmtt_size == 0 || smmtt_base == 0)
		return SBI_ERR_FAILED;

	// Initialize the SMMTT table heap
	sbi_heap_alloc_new(&smmtt_hpctrl);
	sbi_heap_init_new(smmtt_hpctrl, smmtt_base, smmtt_size);

	return SBI_OK;
}

int sbi_smmtt_init(struct sbi_scratch *scratch, bool cold_boot)
{
	int rc;
	if (!sbi_hart_has_extension(scratch, SBI_HART_EXT_SMMTT))	
		return SBI_OK;
	
	if (cold_boot)
	{
		rc = setup_mtt_table();
		if (rc < 0)
			return rc;
	}

	return rc;
}