// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022-2023 DeepX, Inc. and/or its affiliates.
 * DeepX eDMA PCIe driver
 *
 * Author: Taegyun An <atg@deepx.ai>
 */

#include <linux/bitfield.h>
#include <linux/delay.h>

#include "dw-edma-core.h"
#include "dw-edma-v0-core.h"
#include "dw-edma-v0-regs.h"
#include "dw-edma-v0-debugfs.h"

#include "dx_lib.h"
#include "dx_util.h"

enum dw_edma_control {
	DW_EDMA_V0_CB					= BIT(0),
	DW_EDMA_V0_TCB					= BIT(1),
	DW_EDMA_V0_LLP					= BIT(2),
	DW_EDMA_V0_LIE					= BIT(3),
	DW_EDMA_V0_RIE					= BIT(4),
	DW_EDMA_V0_CCS					= BIT(8),
	DW_EDMA_V0_LLE					= BIT(9),
};

static inline struct dw_iatu_reg_t __iomem *__dw_iatu_regs(struct dw_edma *dw)
{
	return dw->iatu_region.vaddr;
}
#define SET_IATU_IN32(dw, idx, name, value)				\
	writel(value, &(__dw_iatu_regs(dw)->reg[idx].in.name))

#define GET_IATU_IN32(dw, idx, name)				\
	readl(&(__dw_iatu_regs(dw)->reg[idx].in.name))

static inline struct dw_edma_v0_regs __iomem *__dw_regs(struct dw_edma *dw)
{
	return dw->rg_region.vaddr;
}

#define SET_32(dw, name, value)				\
	writel(value, &(__dw_regs(dw)->name))

#define GET_32(dw, name)				\
	readl(&(__dw_regs(dw)->name))

#define SET_RW_32(dw, dir, name, value)			\
	do {						\
		if ((dir) == EDMA_DIR_WRITE)		\
			SET_32(dw, wr_##name, value);	\
		else					\
			SET_32(dw, rd_##name, value);	\
	} while (0)

#define GET_RW_32(dw, dir, name)			\
	((dir) == EDMA_DIR_WRITE			\
	  ? GET_32(dw, wr_##name)			\
	  : GET_32(dw, rd_##name))

#define SET_BOTH_32(dw, name, value)			\
	do {						\
		SET_32(dw, wr_##name, value);		\
		SET_32(dw, rd_##name, value);		\
	} while (0)

#ifdef CONFIG_64BIT

#define SET_64(dw, name, value)				\
	writeq(value, &(__dw_regs(dw)->name))

#define GET_64(dw, name)				\
	readq(&(__dw_regs(dw)->name))

#define SET_RW_64(dw, dir, name, value)			\
	do {						\
		if ((dir) == EDMA_DIR_WRITE)		\
			SET_64(dw, wr_##name, value);	\
		else					\
			SET_64(dw, rd_##name, value);	\
	} while (0)

#define GET_RW_64(dw, dir, name)			\
	((dir) == EDMA_DIR_WRITE			\
	  ? GET_64(dw, wr_##name)			\
	  : GET_64(dw, rd_##name))

#define SET_BOTH_64(dw, name, value)			\
	do {						\
		SET_64(dw, wr_##name, value);		\
		SET_64(dw, rd_##name, value);		\
	} while (0)

#endif /* CONFIG_64BIT */

#define SET_COMPAT(dw, name, value)			\
	writel(value, &(__dw_regs(dw)->type.unroll.name))

#define SET_RW_COMPAT(dw, dir, name, value)		\
	do {						\
		if ((dir) == EDMA_DIR_WRITE)		\
			SET_COMPAT(dw, wr_##name, value); \
		else					\
			SET_COMPAT(dw, rd_##name, value); \
	} while (0)

static inline struct dw_edma_v0_ch_regs __iomem *
__dw_ch_regs(struct dw_edma *dw, enum dw_edma_dir dir, u16 ch)
{
	if (dw->mf == DX_DMA_MF_EDMA_LEGACY)
		return &(__dw_regs(dw)->type.legacy.ch);

	if (dir == EDMA_DIR_WRITE)
		return &__dw_regs(dw)->type.unroll.ch[ch].wr;

	return &__dw_regs(dw)->type.unroll.ch[ch].rd;
}

static inline void writel_ch(struct dw_edma *dw, enum dw_edma_dir dir, u16 ch,
			     u32 value, void __iomem *addr)
{
	if (dw->mf == DX_DMA_MF_EDMA_LEGACY) {
		u32 viewport_sel;
		unsigned long flags;

		raw_spin_lock_irqsave(&dw->lock, flags);

		viewport_sel = FIELD_PREP(EDMA_V0_VIEWPORT_MASK, ch);
		if (dir == EDMA_DIR_READ)
			viewport_sel |= BIT(31);

		writel(viewport_sel,
		       &(__dw_regs(dw)->type.legacy.viewport_sel));
		writel(value, addr);

		raw_spin_unlock_irqrestore(&dw->lock, flags);
	} else {
		writel(value, addr);
	}
}

static inline u32 readl_ch(struct dw_edma *dw, enum dw_edma_dir dir, u16 ch,
			   const void __iomem *addr)
{
	u32 value;

	if (dw->mf == DX_DMA_MF_EDMA_LEGACY) {
		u32 viewport_sel;
		unsigned long flags;

		raw_spin_lock_irqsave(&dw->lock, flags);

		viewport_sel = FIELD_PREP(EDMA_V0_VIEWPORT_MASK, ch);
		if (dir == EDMA_DIR_READ)
			viewport_sel |= BIT(31);

		writel(viewport_sel,
		       &(__dw_regs(dw)->type.legacy.viewport_sel));
		value = readl(addr);

		raw_spin_unlock_irqrestore(&dw->lock, flags);
	} else {
		value = readl(addr);
	}

	return value;
}

#define SET_CH_32(dw, dir, ch, name, value) \
	writel_ch(dw, dir, ch, value, &(__dw_ch_regs(dw, dir, ch)->name))

#define GET_CH_32(dw, dir, ch, name) \
	readl_ch(dw, dir, ch, &(__dw_ch_regs(dw, dir, ch)->name))

#define SET_LL_32(ll, value) \
	writel(value, ll)

#ifdef CONFIG_64BIT

static inline void writeq_ch(struct dw_edma *dw, enum dw_edma_dir dir, u16 ch,
			     u64 value, void __iomem *addr)
{
	if (dw->mf == DX_DMA_MF_EDMA_LEGACY) {
		u32 viewport_sel;
		unsigned long flags;

		raw_spin_lock_irqsave(&dw->lock, flags);

		viewport_sel = FIELD_PREP(EDMA_V0_VIEWPORT_MASK, ch);
		if (dir == EDMA_DIR_READ)
			viewport_sel |= BIT(31);

		writel(viewport_sel,
		       &(__dw_regs(dw)->type.legacy.viewport_sel));
		writeq(value, addr);

		raw_spin_unlock_irqrestore(&dw->lock, flags);
	} else {
		writeq(value, addr);
	}
}

static inline u64 readq_ch(struct dw_edma *dw, enum dw_edma_dir dir, u16 ch,
			   const void __iomem *addr)
{
	u32 value;

	if (dw->mf == DX_DMA_MF_EDMA_LEGACY) {
		u32 viewport_sel;
		unsigned long flags;

		raw_spin_lock_irqsave(&dw->lock, flags);

		viewport_sel = FIELD_PREP(EDMA_V0_VIEWPORT_MASK, ch);
		if (dir == EDMA_DIR_READ)
			viewport_sel |= BIT(31);

		writel(viewport_sel,
		       &(__dw_regs(dw)->type.legacy.viewport_sel));
		value = readq(addr);

		raw_spin_unlock_irqrestore(&dw->lock, flags);
	} else {
		value = readq(addr);
	}

	return value;
}

#define SET_CH_64(dw, dir, ch, name, value) \
	writeq_ch(dw, dir, ch, value, &(__dw_ch_regs(dw, dir, ch)->name))

#define GET_CH_64(dw, dir, ch, name) \
	readq_ch(dw, dir, ch, &(__dw_ch_regs(dw, dir, ch)->name))

#define SET_LL_64(ll, value) \
	writeq(value, ll)

#endif /* CONFIG_64BIT */

/* eDMA management callbacks */
void dw_edma_v0_core_off(struct dw_edma *dw)
{
	SET_BOTH_32(dw, int_mask,
		    EDMA_V0_DONE_INT_MASK | EDMA_V0_ABORT_INT_MASK);
	SET_BOTH_32(dw, int_clear,
		    EDMA_V0_DONE_INT_MASK | EDMA_V0_ABORT_INT_MASK);
	SET_BOTH_32(dw, engine_en, 0);
}

u16 dw_edma_v0_core_ch_count(struct dw_edma *dw, enum dw_edma_dir dir)
{
	u32 num_ch;

	if (dir == EDMA_DIR_WRITE)
		num_ch = FIELD_GET(EDMA_V0_WRITE_CH_COUNT_MASK,
				   GET_32(dw, ctrl));
	else
		num_ch = FIELD_GET(EDMA_V0_READ_CH_COUNT_MASK,
				   GET_32(dw, ctrl));

	if (num_ch > EDMA_V0_MAX_NR_CH)
		num_ch = EDMA_V0_MAX_NR_CH;

	return (u16)num_ch;
}

enum dma_status dw_edma_v0_core_ch_status(struct dw_edma_chan *chan)
{
	struct dw_edma *dw = chan->chip->dw;
	u32 tmp;

	tmp = FIELD_GET(EDMA_V0_CH_STATUS_MASK,
			GET_CH_32(dw, chan->dir, chan->id, ch_control1));

	if (tmp == 1) {
		dbg_tfr("[%s] status is progress\n", dma_chan_name(&chan->vc.chan));
		return DMA_IN_PROGRESS;
	}
	else if (tmp == 3)
		return DMA_COMPLETE;
	else {
		pr_err("[%s] status is error\n", dma_chan_name(&chan->vc.chan));
		return DMA_ERROR;
	}
}

void dw_edma_v0_core_clear_done_int(struct dw_edma_chan *chan)
{
	struct dw_edma *dw = chan->chip->dw;

	SET_RW_32(dw, chan->dir, int_clear,
		  FIELD_PREP(EDMA_V0_DONE_INT_MASK, BIT(chan->id)));
}

void dw_edma_v0_core_clear_abort_int(struct dw_edma_chan *chan)
{
	struct dw_edma *dw = chan->chip->dw;

	SET_RW_32(dw, chan->dir, int_clear,
		  FIELD_PREP(EDMA_V0_ABORT_INT_MASK, BIT(chan->id)));
}

u32 dw_edma_v0_core_status_done_int(struct dw_edma *dw, enum dw_edma_dir dir)
{
	return FIELD_GET(EDMA_V0_DONE_INT_MASK,
			 GET_RW_32(dw, dir, int_status));
}

u32 dw_edma_v0_core_status_abort_int(struct dw_edma *dw, enum dw_edma_dir dir)
{
	return FIELD_GET(EDMA_V0_ABORT_INT_MASK,
			 GET_RW_32(dw, dir, int_status));
}

static void dw_edma_v0_core_write_chunk(struct dw_edma_chunk *chunk, int dev_n, int dma_n, int ch_n)
{
	struct dw_edma_burst *child;
	struct dw_edma_v0_lli __iomem *lli;
	struct dw_edma_v0_llp __iomem *llp;
	u32 control = 0, i = 0;
	int j;

	dx_pcie_start_profile(PCIE_DESC_SEND_T, 0, dev_n, dma_n, ch_n);

	lli = chunk->ll_region.vaddr;

	if (chunk->cb)
		control = DW_EDMA_V0_CB;

	j = chunk->bursts_alloc;
	list_for_each_entry(child, &chunk->burst->list, list) {
		j--;
		if (!j)
			control |= (DW_EDMA_V0_LIE | DW_EDMA_V0_RIE);

		/* Channel control */
		SET_LL_32(&lli[i].control, control);
		/* Transfer size */
		SET_LL_32(&lli[i].transfer_size, child->sz);
		/* SAR */
		#if defined(CONFIG_64BIT) && defined(CONFIG_X86_64)
			SET_LL_64(&lli[i].sar.reg, child->sar);
		#else /* CONFIG_64BIT */
			SET_LL_32(&lli[i].sar.lsb, lower_32_bits(child->sar));
			SET_LL_32(&lli[i].sar.msb, upper_32_bits(child->sar));
		#endif /* CONFIG_64BIT */
		/* DAR */
		#if defined(CONFIG_64BIT) && defined(CONFIG_X86_64)
			SET_LL_64(&lli[i].dar.reg, child->dar);
		#else /* CONFIG_64BIT */
			SET_LL_32(&lli[i].dar.lsb, lower_32_bits(child->dar));
			SET_LL_32(&lli[i].dar.msb, upper_32_bits(child->dar));
		#endif /* CONFIG_64BIT */
		#ifdef DUMP_DESC_TABLE
		dbg_tfr("[DESC_#%d CH:%d] CB:%d,TCB:%d,LLP:%d,LIE:%d,RIE:%d,CCS:%d,LLE:%d, size:0x%x, sar:0x%x%x, dar:0x%x%x\n",
			i, chunk->chan->id,
			(control & DW_EDMA_V0_CB)  ? 1:0,
			(control & DW_EDMA_V0_TCB) ? 1:0,
			(control & DW_EDMA_V0_LLP) ? 1:0,
			(control & DW_EDMA_V0_LIE) ? 1:0,
			(control & DW_EDMA_V0_RIE) ? 1:0,
			(control & DW_EDMA_V0_CCS) ? 1:0,
			(control & DW_EDMA_V0_LLE) ? 1:0,
			child->sz,
			upper_32_bits(child->sar), lower_32_bits(child->sar),
			upper_32_bits(child->dar), lower_32_bits(child->dar)
		);
		#endif /* DUMP_DESC_TABLE */
		i++;
	}

	llp = (void __iomem *)&lli[i];
	control = DW_EDMA_V0_LLP | DW_EDMA_V0_TCB;
	if (!chunk->cb)
		control |= DW_EDMA_V0_CB;

	/* Channel control */
	SET_LL_32(&llp->control, control);
	/* Linked list */
	#if defined(CONFIG_64BIT) && defined(CONFIG_X86_64)
		SET_LL_64(&llp->llp.reg, chunk->ll_region.paddr);
	#else /* CONFIG_64BIT */
		SET_LL_32(&llp->llp.lsb, lower_32_bits(chunk->ll_region.paddr));
		SET_LL_32(&llp->llp.msb, upper_32_bits(chunk->ll_region.paddr));
	#endif /* CONFIG_64BIT */

	dx_pcie_end_profile(PCIE_DESC_SEND_T, 0, dev_n, dma_n, ch_n);

#ifdef DUMP_DESC_TABLE
	dbg_tfr("[DESC_LLP] CB:%d,TCB:%d,LLP:%d,LIE:%d,RIE:%d,CCS:%d,LLE:%d, ll_region:0x%x%x\n",
		(control & DW_EDMA_V0_CB)  ? 1:0,
		(control & DW_EDMA_V0_TCB) ? 1:0,
		(control & DW_EDMA_V0_LLP) ? 1:0,
		(control & DW_EDMA_V0_LIE) ? 1:0,
		(control & DW_EDMA_V0_RIE) ? 1:0,
		(control & DW_EDMA_V0_CCS) ? 1:0,
		(control & DW_EDMA_V0_LLE) ? 1:0,
		upper_32_bits(chunk->ll_region.paddr), lower_32_bits(chunk->ll_region.paddr)
	);
#endif /* DUMP_DESC_TABLE */
}

void dw_edma_v0_core_start(struct dw_edma_chunk *chunk, bool first, bool set_desc)
{
	struct dw_edma_chan *chan = chunk->chan;
	struct dw_edma *dw = chan->chip->dw;
	u32 tmp;

	if (set_desc) {
		dbg_tfr("Description table settings are needed(%s), dir:%s\n",
			dma_chan_name(&chan->vc.chan), 
			(chan->dir == EDMA_DIR_WRITE) ? "DMA_W":"DMA_R");
		dw_edma_v0_core_write_chunk(chunk, dw->idx, chan->id, chan->dir);
	}

	if (first) {
		/* Enable engine */
		SET_RW_32(dw, chan->dir, engine_en, BIT(0));
		if (dw->mf == DX_DMA_MF_HDMA_COMPAT) {
			switch (chan->id) {
			case 0:
				SET_RW_COMPAT(dw, chan->dir, ch0_pwr_en,
					      BIT(0));
				break;
			case 1:
				SET_RW_COMPAT(dw, chan->dir, ch1_pwr_en,
					      BIT(0));
				break;
			case 2:
				SET_RW_COMPAT(dw, chan->dir, ch2_pwr_en,
					      BIT(0));
				break;
			case 3:
				SET_RW_COMPAT(dw, chan->dir, ch3_pwr_en,
					      BIT(0));
				break;
			case 4:
				SET_RW_COMPAT(dw, chan->dir, ch4_pwr_en,
					      BIT(0));
				break;
			case 5:
				SET_RW_COMPAT(dw, chan->dir, ch5_pwr_en,
					      BIT(0));
				break;
			case 6:
				SET_RW_COMPAT(dw, chan->dir, ch6_pwr_en,
					      BIT(0));
				break;
			case 7:
				SET_RW_COMPAT(dw, chan->dir, ch7_pwr_en,
					      BIT(0));
				break;
			}
		}
		/* Interrupt unmask - done, abort */
		tmp = GET_RW_32(dw, chan->dir, int_mask);
		tmp &= ~FIELD_PREP(EDMA_V0_DONE_INT_MASK, BIT(chan->id));
		tmp &= ~FIELD_PREP(EDMA_V0_ABORT_INT_MASK, BIT(chan->id));
		SET_RW_32(dw, chan->dir, int_mask, tmp);
		/* Linked list error */
		tmp = GET_RW_32(dw, chan->dir, linked_list_err_en);
		tmp |= FIELD_PREP(EDMA_V0_LINKED_LIST_ERR_MASK, BIT(chan->id));
		SET_RW_32(dw, chan->dir, linked_list_err_en, tmp);
		/* Channel control */
		SET_CH_32(dw, chan->dir, chan->id, ch_control1,
			  (DW_EDMA_V0_CCS | DW_EDMA_V0_LLE));
		/* Linked list */
		SET_CH_32(dw, chan->dir, chan->id, llp.lsb,
			  lower_32_bits(chunk->ll_region.paddr));
		SET_CH_32(dw, chan->dir, chan->id, llp.msb,
			  upper_32_bits(chunk->ll_region.paddr));
		dbg_tfr("[DMA_REG_SET] dir:%d, id:%d, llp:0x%x_%x, ll_region:0x%x_%x",
			chan->dir, chan->id,
			GET_CH_32(dw, chan->dir, chan->id, llp.msb),
			GET_CH_32(dw, chan->dir, chan->id, llp.lsb),
			upper_32_bits(chunk->ll_region.paddr),
			lower_32_bits(chunk->ll_region.paddr));
	}
	/* Doorbell */
	SET_RW_32(dw, chan->dir, doorbell,
		  FIELD_PREP(EDMA_V0_DOORBELL_CH_MASK, chan->id));
	dbg_tfr("Start Door Bell(%s)\n", dma_chan_name(&chan->vc.chan));
}

int dw_edma_v0_core_device_config(struct dw_edma_chan *chan)
{
	struct dw_edma *dw = chan->chip->dw;
	u32 tmp = 0;

	/* MSI done addr - low, high */
	SET_RW_32(dw, chan->dir, done_imwr.lsb, chan->msi.address_lo);
	SET_RW_32(dw, chan->dir, done_imwr.msb, chan->msi.address_hi);
	/* MSI abort addr - low, high */
	SET_RW_32(dw, chan->dir, abort_imwr.lsb, chan->msi.address_lo);
	SET_RW_32(dw, chan->dir, abort_imwr.msb, chan->msi.address_hi);
	/* MSI data - low, high */
	switch (chan->id) {
	case 0:
	case 1:
		tmp = GET_RW_32(dw, chan->dir, ch01_imwr_data);
		break;

	case 2:
	case 3:
		tmp = GET_RW_32(dw, chan->dir, ch23_imwr_data);
		break;

	case 4:
	case 5:
		tmp = GET_RW_32(dw, chan->dir, ch45_imwr_data);
		break;

	case 6:
	case 7:
		tmp = GET_RW_32(dw, chan->dir, ch67_imwr_data);
		break;
	}

	if (chan->id & BIT(0)) {
		/* Channel odd {1, 3, 5, 7} */
		tmp &= EDMA_V0_CH_EVEN_MSI_DATA_MASK;
		tmp |= FIELD_PREP(EDMA_V0_CH_ODD_MSI_DATA_MASK,
				  chan->msi.data);
	} else {
		/* Channel even {0, 2, 4, 6} */
		tmp &= EDMA_V0_CH_ODD_MSI_DATA_MASK;
		tmp |= FIELD_PREP(EDMA_V0_CH_EVEN_MSI_DATA_MASK,
				  chan->msi.data);
	}

	switch (chan->id) {
	case 0:
	case 1:
		SET_RW_32(dw, chan->dir, ch01_imwr_data, tmp);
		break;

	case 2:
	case 3:
		SET_RW_32(dw, chan->dir, ch23_imwr_data, tmp);
		break;

	case 4:
	case 5:
		SET_RW_32(dw, chan->dir, ch45_imwr_data, tmp);
		break;

	case 6:
	case 7:
		SET_RW_32(dw, chan->dir, ch67_imwr_data, tmp);
		break;
	}

	return 0;
}

/* eDMA debugfs callbacks */
void dw_edma_v0_core_debugfs_on(struct dw_edma_chip *chip)
{
	dw_edma_v0_debugfs_on(chip);
}

void dw_edma_v0_core_debugfs_off(struct dw_edma_chip *chip)
{
	dw_edma_v0_debugfs_off(chip);
}

/* iATU function */
static inline void dx_iatu_inbound_address(struct dw_edma *dw, u32 idx, u64 base_addr, u64 tgt_addr, u32 size)
{
	u32 retries, val;

	SET_IATU_IN32(dw, idx, base_lo_addr, lower_32_bits(base_addr));
	SET_IATU_IN32(dw, idx, base_hi_addr, upper_32_bits(base_addr));

	SET_IATU_IN32(dw, idx, limit_lo_addr, lower_32_bits(base_addr + size - 1));
	SET_IATU_IN32(dw, idx, limit_hi_addr, upper_32_bits(base_addr + size - 1));

	SET_IATU_IN32(dw, idx, targ_lo_addr, lower_32_bits(tgt_addr));
	SET_IATU_IN32(dw, idx, targ_hi_addr, upper_32_bits(tgt_addr));

	SET_IATU_IN32(dw, idx, region_ctl2, PCIE_ATU_ENABLE | PCIE_ATU_DMA_BYPASS);
	/*
	 * Make sure ATU enable takes effect before any subsequent config
	 * and I/O accesses.
	 */
	for (retries = 0; retries < LINK_WAIT_MAX_IATU_RETRIES; retries++) {
		val = GET_IATU_IN32(dw, idx, region_ctl2);
		if (val & PCIE_ATU_ENABLE)
			return;

		mdelay(LINK_WAIT_IATU);
	}
	printk("[ERR] Inbound iATU is not being enabled[ADDRESS_MODE]\n");
}

static inline void dx_iatu_inbound_bar(struct dw_edma *dw, u32 idx, u8 bar_no, u64 tgt_addr)
{
	u32 retries, val;

	SET_IATU_IN32(dw, idx, targ_lo_addr, lower_32_bits(tgt_addr));
	SET_IATU_IN32(dw, idx, targ_hi_addr, upper_32_bits(tgt_addr));

	SET_IATU_IN32(dw, idx, region_ctl2,
			PCIE_ATU_ENABLE |
			PCIE_ATU_BAR_MODE_ENABLE |
			(bar_no << 8));
	/*
	 * Make sure ATU enable takes effect before any subsequent config
	 * and I/O accesses.
	 */
	for (retries = 0; retries < LINK_WAIT_MAX_IATU_RETRIES; retries++) {
		val = GET_IATU_IN32(dw, idx, region_ctl2);
		if (val & PCIE_ATU_ENABLE)
			return;

		mdelay(LINK_WAIT_IATU);
	}
	printk("[ERR] Inbound iATU is not being enabled[BAR_MODE]\n");
}

void dw_iatu_config_inbound(struct dw_edma *dw, u8 mode, u64 base_addr, u64 tgt_addr, u32 size, u32 idx, u8 bar_no)
{
	dbg_init("iATU_IN#%d-mode:%d, b_addr:0x%llX, t_addr:0x%llX, size:0x%X, BAR:%d\n",
		idx, mode, base_addr, tgt_addr, size, bar_no);

	/* TODO - unroll check (CDM[0x900, 0xFFFFFFFF] -> Enable UNROLL) */
	if (idx >= IATU_BOUND_MAX) {
		printk("[ERROR] Exceed the number of iatu bound(%d/%d)\n", idx+1, IATU_BOUND_MAX);
		return;
	}

	switch (mode)
	{
		case DW_PCIE_IATU_BAR_MATCH:
			dx_iatu_inbound_bar(dw, idx, bar_no, tgt_addr);
			break;
		case DW_PCIE_IATU_ADDRESS_MATCH:
			dx_iatu_inbound_address(dw, idx, base_addr, tgt_addr, size);
			break;
		default:
			break;
	}
}

void dw_iatu_desc_region_check(struct dw_edma *dw, u64 addr, u64 size)
{
	struct dx_iatu_inbound *iatu_inb = dw->iatu_inb;
	u64 desc_addr = iatu_inb[IATU_INB_DMA_DESC].tgt_addr;
	u64 desc_size = iatu_inb[IATU_INB_DMA_DESC].size;
	u64 desc_mv_offs = desc_size/2;
	u32 mv_cnt = 0;
	int i;

	/* check descriptor table position into local memory */
	do {
		if ((addr + size < desc_addr) || (addr > desc_addr + desc_size)) {
			break;
		} else {
			/* move descriptor table */
			desc_addr += desc_mv_offs;
			mv_cnt++;
		}
	} while(true);

	if (desc_addr != iatu_inb[IATU_INB_DMA_DESC].tgt_addr) {
		pr_err("DMA Descriptor [A:%llx, S:%llx] -> [A:%llx, S:%llx]\n",
			iatu_inb[IATU_INB_DMA_DESC].tgt_addr, iatu_inb[IATU_INB_DMA_DESC].size,
			desc_addr, desc_size);
		/* Control iATU */
		iatu_inb[IATU_INB_DMA_DESC].tgt_addr = desc_addr;
		dw_iatu_config_inbound(dw,
			dw->iatu_inb[IATU_INB_DMA_DESC].mode,
			dw->iatu_inb[IATU_INB_DMA_DESC].base_addr,
			dw->iatu_inb[IATU_INB_DMA_DESC].tgt_addr,
			dw->iatu_inb[IATU_INB_DMA_DESC].size,
			dw->iatu_inb[IATU_INB_DMA_DESC].idx,
			dw->iatu_inb[IATU_INB_DMA_DESC].bar_no);
		/* Control descriptor point of DMA ch */
		/* TODO - Consider for multi DMA channel */
		for (i = 0; i < ARRAY_SIZE(dw->ll_region_wr); i++) {
			dw->ll_region_wr[i].paddr += (mv_cnt * desc_mv_offs);
			pr_info("[WR] Descriptor Region : 0x%llx\n", dw->ll_region_wr[i].paddr);
		}
		for (i = 0; i < ARRAY_SIZE(dw->ll_region_rd); i++) {
			dw->ll_region_rd[i].paddr += (mv_cnt * desc_mv_offs);
			pr_info("[RD] Descriptor Region : 0x%llx\n", dw->ll_region_rd[i].paddr);
		}
	}
}

inline void dw_iatu_set_datas(struct dx_iatu_inbound *iatu_inb, u8 mode, u64 base_addr, u64 tgt_addr, u64 size, u32 idx, u8 bar_no)
{
	iatu_inb->mode		= mode;
	iatu_inb->base_addr	= base_addr;
	iatu_inb->tgt_addr	= tgt_addr;
	iatu_inb->size		= size;
	iatu_inb->idx		= idx;
	iatu_inb->bar_no	= bar_no;
}

/* Set Inbound Region as default */
void dw_iatu_default_config_set(struct dw_edma *dw)
{
	int i;

	/* configure for dma descriptor region */
	// dw_iatu_set_datas(&dw->iatu_inb[IATU_INB_DMA_DESC],
	// 	DW_PCIE_IATU_ADDRESS_MATCH,
	// 	dw->dma_desc_base_bar,	/* Base address */
	// 	dw->dma_desc_base_addr,	/* Target Base address */
	// 	dw->dma_desc_size,
	// 	IATU_INB_DMA_DESC,
	// 	0		/*Dummy*/);

	dw_iatu_set_datas(&dw->iatu_inb[IATU_INB_DMA_DESC],
		DW_PCIE_IATU_BAR_MATCH,
		0x00,              /*Dummy*/
		dw->dma_desc_base_addr,	/* Target Base address */
		0x00,              /*Dummy*/
		IATU_INB_DMA_DESC, /*Inbound-0*/
		dw->dma_desc_bar_num);

	/* configure for NPU 0 / 1 region */
	dw_iatu_set_datas(&dw->iatu_inb[IATU_INB_NPU0],
		DW_PCIE_IATU_BAR_MATCH,
		0x00,           /*Dummy*/
		dw->npu_region[0].ep_addr,
		0x00,           /*Dummy*/
		IATU_INB_NPU0,	/*Inbound-1*/
		dw->npu_region[0].bar_num);
	dw_iatu_set_datas(&dw->iatu_inb[IATU_INB_NPU1],
		DW_PCIE_IATU_BAR_MATCH,
		0x00,           /*Dummy*/
		dw->npu_region[1].ep_addr,
		0x00,           /*Dummy*/
		IATU_INB_NPU1,  /*Inbound-2*/
		dw->npu_region[1].bar_num);

	/* Write config iATU Register */
	for (i = 0; i < IATU_INB_MAX; i++) {
		if (dw->iatu_inb[i].mode)
			dw_iatu_config_inbound(dw,
				dw->iatu_inb[i].mode,
				dw->iatu_inb[i].base_addr,
				dw->iatu_inb[i].tgt_addr,
				dw->iatu_inb[i].size,
				dw->iatu_inb[i].idx,
				dw->iatu_inb[i].bar_no);
	}
}