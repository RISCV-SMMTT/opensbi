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

#define MiB (1UL << 20)
#define GiB (1ULL << 30)

#if __riscv_xlen == 32
#define XM_SIZE (4 * MiB)
#else
#define XM_SIZE (2 * MiB)
#endif

#define FITS(base, size, region) \
	(((size) >= (region)) && (!((base) % (region))))

static inline uint64_t mttl2_1g_type_from_flags(unsigned long flags) {
	if (flags & SBI_DOMAIN_MEMREGION_SU_READABLE) 
	{
		if (flags & SBI_DOMAIN_MEMREGION_SU_WRITABLE) 
		{
			if (flags & SBI_DOMAIN_MEMREGION_SU_EXECUTABLE) 
				return TYPE_1G_ALLOW_RWX;
			else
				return TYPE_1G_ALLOW_RW;
		}
		else
		{ 
			if (flags & SBI_DOMAIN_MEMREGION_SU_EXECUTABLE) 
				return TYPE_1G_ALLOW_RX;
			else 
				return TYPE_1G_DISALLOW;
		}
	}
	else 
		return TYPE_1G_DISALLOW;
}

static int add_1g_region(mttl2_entry_t *entry, unsigned long flags)
{
	smmtt_type type = mttl2_1g_type_from_flags(flags);
	if (entry->type == 0)
		entry->type = type;
	else 
		return SBI_ENODEV;
	
	entry->info = 0;
	entry->zero = 0;
	return SBI_OK;
}

static inline smmtt_xm_perms xm_perms_from_flags(unsigned long flags)
{
	if (flags & SBI_DOMAIN_MEMREGION_SU_READABLE)
	{
		if (flags & SBI_DOMAIN_MEMREGION_SU_WRITABLE) 
		{
			if (flags & SBI_DOMAIN_MEMREGION_SU_EXECUTABLE) 
				return PERMS_XM_ALLOW_RWX;
			else 
				return PERMS_XM_ALLOW_RW;
		}
		else 
		{
			if (flags & SBI_DOMAIN_MEMREGION_SU_EXECUTABLE) 
				return PERMS_XM_ALLOW_RX;
			else 
				return PERMS_XM_DISALLOW;
		}
	}
	else 
		return PERMS_XM_DISALLOW;
}

static int add_xm_region(mttl2_entry_t *entry, unsigned long base, unsigned long flags)
{

	unsigned long offset, info, perms, field;

#if __riscv_xlen == 32
	smmtt_type type = TYPE_4M_PAGE;
#else
	smmtt_type type = TYPE_2M_PAGE;
#endif

	if (entry->type == 0)
		entry->type = type;
	offset = EXTRACT_FIELD(base, PA_XM_OFFS);

	field = MTT_PERM_FIELD(offset);
	info = entry->info;
	if (EXTRACT_FIELD(entry->info, field) != 0)
		return SBI_EINVAL;	

	perms = xm_perms_from_flags(flags);
	info = INSERT_FIELD(info, field, perms);
	entry->info = info;

	entry->zero = 0;
	return SBI_OK;
}

static inline mttl1_entry_t *mttl1_from_mttl2(mttl2_entry_t *entry)
{
	unsigned long mttl1_ppn;
	mttl1_entry_t *mttl1 = NULL;

	// Make sure this entry is the correct type to have an mttl1
	if (entry->type != TYPE_MTTL1_DIR) {
		return NULL;
	}

	if (entry->info) {
		// mttl1 already allocated, extract from mttl2
		mttl1_ppn = entry->info;
		mttl1 = (mttl1_entry_t *)(mttl1_ppn << PAGE_SHIFT);
	} else {
		// Allocate new mttl1
		mttl1 = sbi_aligned_alloc_from(smmtt_hpctrl, PAGE_SIZE, PAGE_SIZE);
		if (!mttl1) {
			return NULL;
		}

		// Link to mttl2
		entry->info = ((uintptr_t) mttl1) >> PAGE_SHIFT;

		// Ensure zero field is zero
		entry->zero = 0;
	}

	return mttl1;
}

static inline perms_mttl1 mttl1_perms_from_flags(unsigned long flags)
{
	if (flags & SBI_DOMAIN_MEMREGION_SU_READABLE) 
	{
		if (flags & SBI_DOMAIN_MEMREGION_SU_WRITABLE) 
		{
			if (flags & SBI_DOMAIN_MEMREGION_SU_EXECUTABLE) 
				return PERMS_MTTL1_ALLOW_RWX; 
			else 
				return PERMS_MTTL1_ALLOW_RW;
		} 
		else 
		{
			if (flags & SBI_DOMAIN_MEMREGION_SU_EXECUTABLE) 
				return PERMS_MTTL1_ALLOW_RX;
			else
				return PERMS_MTTL1_DISALLOWED;
		}
	} else 
		return PERMS_MTTL1_DISALLOWED;
}

static int add_mttl1_region(mttl2_entry_t *entry, unsigned long base,
				  unsigned long flags)
{
	unsigned long index, offset, field;
	perms_mttl1 perms;
	mttl1_entry_t *mttl1;

	if (entry->type == 0)
		entry->type = TYPE_MTTL1_DIR;
	else
		return SBI_EINVAL;

	// Allocate or get an existing mttl1 table
	mttl1 = mttl1_from_mttl2(entry);
	if (!mttl1) {
		// Failed to allocate, reset entry
		entry->info = 0;
		entry->type = 0;
		entry->zero = 0;
		return SBI_ENOMEM;
	}

	// Determine index and offset in mttl1 that this address belongs to
	index = EXTRACT_FIELD(base, PA_PN1);
	offset = EXTRACT_FIELD(base, PA_PN0);

	// Generate the bitfield for the permissions and ensure it is not set
	field = MTT_PERM_FIELD(offset);
	if (EXTRACT_FIELD(mttl1[index], field) != 0)
		return SBI_EINVAL;

	// Set the new permissions
	perms = mttl1_perms_from_flags(flags);
	mttl1[index] = INSERT_FIELD(mttl1[index], field, perms);
	return SBI_OK;
}

static int add_mttl2_region(mttl2_entry_t *mttl2, unsigned long base, 
			unsigned long order, unsigned long flags)
{
	int i, rc;
	unsigned long index, size = (order == __riscv_xlen) ? -1UL : BIT(order);
	mttl2_entry_t *entry;

	while(size != 0)
	{
		index = EXTRACT_FIELD(base, PA_PN2);
		entry = &mttl2[index];
		entry->zero = 0;

		if (FITS(base, size, GiB))
		{
			for (i = 0; i < 32; i++)
			{
				rc = add_1g_region(&mttl2[index + i], flags);
				if (rc)
					return rc;
			}
			size -= GiB;
			base += GiB;
		}
		else if (FITS(base, size, XM_SIZE))
		{
			rc = add_xm_region(entry, base, flags);
			if (rc)
				return rc;
			size -= (XM_SIZE);
			base += (XM_SIZE);
		}
		else
		{
			rc = add_mttl1_region(entry, base, flags);
			if (rc)
				return rc;
			size -= PAGE_SIZE;
			base += PAGE_SIZE;
		}
	}

	return rc;
}

static int add_mttl3_region(mttl3_entry_t *mttl3, unsigned long base, 
			unsigned long order, unsigned long flags)
{
	unsigned long mttl2_ppn;
	mttl2_entry_t *mttl2;
	unsigned long index = EXTRACT_FIELD(base, PA_PN3);

	if (mttl3[index].mttl2_ppn == 0)
	{
		mttl2 = sbi_aligned_alloc_from(smmtt_hpctrl, MTTL2_SIZE, MTTL2_SIZE);
		
		if (!mttl2)
			return SBI_ENOMEM;

		mttl3[index].mttl2_ppn = ((uintptr_t)mttl2) >> PAGE_SHIFT;
		mttl3[index].zero = 0;
	}
	else
	{
		mttl2_ppn = mttl3[index].mttl2_ppn;
		mttl2 = (mttl2_entry_t *)(mttl2_ppn << PAGE_SHIFT);
	}
		// mttl2 = (mttl2_entry_t *)(mttl3[index].mttl2_ppn << PAGE_SHIFT);
	
	return add_mttl2_region(mttl2, base, order, flags);
}

static int initialize_mtt(struct sbi_domain *dom, struct sbi_scratch *scratch)
{
	int rc, level;
	struct sbi_domain_memregion *reg;

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

		sbi_domain_for_each_memregion(dom, reg)
		{
			if (!(reg->flags & SBI_DOMAIN_MEMREGION_SU_RWX))
				continue;

			if (level == 3)
				add_mttl3_region(dom->mtt, reg->base, reg->order, reg->flags);
			else
				add_mttl2_region(dom->mtt, reg->base, reg->order, reg->flags);
		}
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

	smmtt_order = fdt32_to_cpu(order_prop[0]);
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