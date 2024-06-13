// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022-2023 DeepX, Inc. and/or its affiliates.
 * DeepX eDMA PCIe driver
 *
 * Author: Taegyun An <atg@deepx.ai>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/pci-epf.h>
#include <linux/msi.h>
#include <linux/bitfield.h>
#include <linux/aer.h>

#include "dx_edma.h"

#include "dw-edma-v0-core.h"
#include "dw-edma-core.h"
#include "dx_cdev.h"
#include "dx_util.h"
#include "dx_lib.h"
#include "dw-edma-thread.h"

#ifdef DX_DEBUG_ENABLE /*DEEPX MODIFIED*/
	#ifdef pci_dbg
		#undef pci_dbg
		#define pci_dbg		pci_err
	#endif
#endif

#define NPU_NUM_CNT						2

#define DW_PCIE_VSEC_DMA_ID				0x6
#define DW_PCIE_VSEC_DMA_BAR			GENMASK(10, 8)
#define DW_PCIE_VSEC_DMA_MAP			GENMASK(2, 0)
#define DW_PCIE_VSEC_DMA_WR_CH			GENMASK(9, 0)
#define DW_PCIE_VSEC_DMA_RD_CH			GENMASK(25, 16)

#define DW_BLOCK(a, b, c) \
	{ \
		.bar = a, \
		.off = b, \
		.sz = c, \
	},

#define DW_NPU_BLOCK(a, b, c, d) \
	{ \
		.bar = a, \
		.off = b, \
		.sz = c, \
		.ep_addr = d, \
	},

struct dx_edma_block {
	enum pci_barno			bar;
	off_t				off;
	size_t				sz;
};

struct dx_ep_npu_block {
	enum pci_barno		bar;
	off_t				off;
	size_t				sz;
	u64					ep_addr;
};

struct dw_edma_pcie_data {
	/* eDMA registers location */
	struct dx_edma_block		rg;
	/* eDMA memory linked list location */
	struct dx_edma_block		ll_wr[EDMA_MAX_WR_CH];
	struct dx_edma_block		ll_rd[EDMA_MAX_RD_CH];
	/* eDMA memory data location */
	// struct dx_edma_block		dt_wr[EDMA_MAX_WR_CH];
	// struct dx_edma_block		dt_rd[EDMA_MAX_RD_CH];
	/* Other */
	enum dx_edma_map_format		mf;
	u8				irqs;		/* Total irqs (included user irqs) */
	u8				dma_irqs;	/* dma irqs */
	u16				wr_ch_cnt;
	u16				rd_ch_cnt;
	/* iATU registers */
	struct dx_edma_block		iatu;
	/* NPU registers */
	u16							npu_cnt;
	struct dx_ep_npu_block		npu[NPU_NUM_CNT];
};

/*  Total Size : BAR0_MEM_SIZE
-------------------------------------------------- << DESC_WR_BASE_OFFS
     DMA Write Descriptor  (Size : DESC_WR_RD_SIZE)
-------------------------------------------------- << DESC_RD_BASE_OFFS
     DMA Read Descriptor   (Size : DESC_WR_RD_SIZE)
-------------------------------------------------- */

#ifdef DMA_DESC_SRAM
	#define DMA_DESC_START_ADDR	(0xD8100000)
	#define BAR0_MEM_SIZE		(1*1024*1024) /* 1MB */
	#define DMA_CH_TOTAL_COUNT	(4)
	#define DESC_WR_RD_SIZE		(BAR0_MEM_SIZE/DMA_CH_TOTAL_COUNT) /* to be aligned for 0x10000 as inbound region mask value */

	#define DESC_WR_BASE_OFFS	(0x00)
	#define DESC_RD_BASE_OFFS	(BAR0_MEM_SIZE/DMA_CH_TOTAL_COUNT*2)
#else
	#define DMA_DESC_START_ADDR	(0x40000000)
	#define BAR0_MEM_SIZE		(4*1024*1024) /* 4MB */
	#define DMA_CH_TOTAL_COUNT	(4)
	#define DESC_WR_RD_SIZE		(BAR0_MEM_SIZE/DMA_CH_TOTAL_COUNT) /* to be aligned for 0x10000 as inbound region mask value */

	#define DESC_WR_BASE_OFFS	(0x00)
	#define DESC_RD_BASE_OFFS	(BAR0_MEM_SIZE/DMA_CH_TOTAL_COUNT*2)
#endif /*DMA_DESC_SRAM*/

static const struct dw_edma_pcie_data dx_pcie_data = {
	/* eDMA registers location */
	.rg.bar				= BAR_2,
	.rg.off				= 0x00000000,	/*  0   Kbytes */ 
	.rg.sz				= 0x00000A00,	/*  2.5 Kbytes */
	/* eDMA memory linked list location */
	.ll_wr = {
		DW_BLOCK(BAR_0, DESC_WR_BASE_OFFS,					DESC_WR_RD_SIZE)	/* Channel 0 */
		DW_BLOCK(BAR_0, DESC_WR_BASE_OFFS+DESC_WR_RD_SIZE,	DESC_WR_RD_SIZE)	/* Channel 1 */
	},
	.ll_rd = {
		DW_BLOCK(BAR_0, DESC_RD_BASE_OFFS,					DESC_WR_RD_SIZE)	/* Channel 0 */
		DW_BLOCK(BAR_0, DESC_RD_BASE_OFFS+DESC_WR_RD_SIZE,	DESC_WR_RD_SIZE)	/* Channel 1 */
	},
	/* Other */
	.mf					= DX_DMA_MF_HDMA_COMPAT,
	.dma_irqs			= 1,
	.wr_ch_cnt			= 2,
	.rd_ch_cnt			= 2,
	/* iATU registers */
	.iatu.bar			= BAR_2,
	.iatu.off			= 0x00000A00,
	.iatu.sz			= 0x00001000,
	/* NPU registers */
	.npu_cnt			= NPU_NUM_CNT,
	.npu = {
		/* NPU0 */
		DW_NPU_BLOCK(BAR_3, 0x00000000, 0x00100000, 0xCD000000)
		/* NPU1 */
		DW_NPU_BLOCK(BAR_4, 0x00000000, 0x00100000, 0xCD800000)
	},
};

static int dw_edma_pcie_irq_vector(struct device *dev, unsigned int nr)
{
	pci_dbg(to_pci_dev(dev), "[%s] nr:%d\n", __func__, nr);
	return pci_irq_vector(to_pci_dev(dev), nr);
}

static const struct dx_edma_core_ops dw_edma_pcie_core_ops = {
	.irq_vector = dw_edma_pcie_irq_vector,
};

static void dw_edma_pcie_get_vsec_dma_data(struct pci_dev *pdev,
					   struct dw_edma_pcie_data *pdata)
{
	u32 val, map;
	u16 vsec;
	u64 off;

	vsec = dx_pci_find_vsec_capability(pdev, PCI_VENDOR_ID_SYNOPSYS,
					DW_PCIE_VSEC_DMA_ID);
	if (!vsec)
		return;

	pci_read_config_dword(pdev, vsec + PCI_VNDR_HEADER, &val);
	if (PCI_VNDR_HEADER_REV(val) != 0x00 ||
	    PCI_VNDR_HEADER_LEN(val) != 0x18)
		return;

	pci_dbg(pdev, "Detected PCIe Vendor-Specific Extended Capability DMA\n");
	pci_read_config_dword(pdev, vsec + 0x8, &val);
	map = FIELD_GET(DW_PCIE_VSEC_DMA_MAP, val);
	if (map != DX_DMA_MF_EDMA_LEGACY &&
	    map != DX_DMA_MF_EDMA_UNROLL &&
	    map != DX_DMA_MF_HDMA_COMPAT)
		return;

	pdata->mf = map;
	pdata->rg.bar = FIELD_GET(DW_PCIE_VSEC_DMA_BAR, val);

	pci_read_config_dword(pdev, vsec + 0xc, &val);
	pdata->wr_ch_cnt = min_t(u16, pdata->wr_ch_cnt,
				 FIELD_GET(DW_PCIE_VSEC_DMA_WR_CH, val));
	pdata->rd_ch_cnt = min_t(u16, pdata->rd_ch_cnt,
				 FIELD_GET(DW_PCIE_VSEC_DMA_RD_CH, val));

	pci_read_config_dword(pdev, vsec + 0x14, &val);
	off = val;
	pci_read_config_dword(pdev, vsec + 0x10, &val);
	off <<= 32;
	off |= val;
	pdata->rg.off = off;

	/* Debug */
	pci_dbg(pdev, "pdata->mf        : 0x%x\n", pdata->mf);
	pci_dbg(pdev, "pdata->rg.bar    : 0x%x\n", pdata->rg.bar);
	pci_dbg(pdev, "pdata->wr_ch_cnt : 0x%x\n", pdata->wr_ch_cnt);
	pci_dbg(pdev, "pdata->rd_ch_cnt : 0x%x\n", pdata->rd_ch_cnt);
	pci_dbg(pdev, "pdata->rg.off    : 0x%lx\n", pdata->rg.off);
	pci_dbg(pdev, "Header Type      : 0x%x\n", pdev->hdr_type);
}

static int dx_dma_pcie_probe(struct pci_dev *pdev,
			      const struct pci_device_id *pid)
{
	struct dw_edma_pcie_data *pdata = (void *)pid->driver_data;
	struct dw_edma_pcie_data vsec_data;
	struct device *dev = &pdev->dev;
	struct dw_edma_chip *chip;
	struct dw_edma *dw;
	int err, nr_irqs;
	int i, mask, bar_size;
	int total_irqs;

	dbg_init("pdev : %p name[%s].\n", pdev, pci_name(pdev));

	/* Enable PCI device */
	err = pcim_enable_device(pdev);
	if (err) {
		pci_err(pdev, "enabling device failed\n");
		return err;
	}

	memcpy(&vsec_data, pdata, sizeof(struct dw_edma_pcie_data));

	/*
	 * Tries to find if exists a PCIe Vendor-Specific Extended Capability
	 * for the DMA, if one exists, then reconfigures it.
	 */
	dw_edma_pcie_get_vsec_dma_data(pdev, &vsec_data);

	/* Mapping PCI BAR regions */
	mask = BIT(vsec_data.rg.bar);
	for (i = 0; i < vsec_data.npu_cnt; i++) {
		mask |= BIT(vsec_data.npu[i].bar);
	}
	for (i = 0; i < vsec_data.wr_ch_cnt; i++) {
		mask |= BIT(vsec_data.ll_wr[i].bar);
	}
	for (i = 0; i < vsec_data.rd_ch_cnt; i++) {
		mask |= BIT(vsec_data.ll_rd[i].bar);
	}
	err = pcim_iomap_regions(pdev, mask, pci_name(pdev));
	if (err) {
		pci_err(pdev, "eDMA BAR I/O remapping failed\n");
		return err;
	}

	pci_set_master(pdev);
	/* AER (Advanced Error Reporting) hooks */
	//pci_enable_pcie_error_reporting(pdev);

	/* DMA configuration */
	err = dma_set_mask(&pdev->dev, DMA_BIT_MASK(64));
	if (!err) {
		err = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(64));
		if (err) {
			pci_err(pdev, "consistent DMA mask 64 set failed\n");
			return err;
		}
	} else {
		pci_err(pdev, "DMA mask 64 set failed\n");

		err = dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));
		if (err) {
			pci_err(pdev, "DMA mask 32 set failed\n");
			return err;
		}

		err = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
		if (err) {
			pci_err(pdev, "consistent DMA mask 32 set failed\n");
			return err;
		}
	}

	/* Data structure allocation */
	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	dw = devm_kzalloc(dev, sizeof(*dw), GFP_KERNEL);
	if (!dw)
		return -ENOMEM;

	/* Set number of IRQS */
	total_irqs = vsec_data.dma_irqs + get_nr_user_irqs();

	/* IRQs allocation */
	pci_dbg(pdev, "Total IRQ number with including npu handler: %d\n", total_irqs);
	nr_irqs = pci_alloc_irq_vectors(pdev, 1, total_irqs,
					PCI_IRQ_MSI | PCI_IRQ_MSIX);
	if (nr_irqs < 1) {
		pci_err(pdev, "fail to alloc IRQ vector (number of IRQs=%u)\n",
			nr_irqs);
		return -EPERM;
	}

#ifndef DMA_DESC_SRAM
	/* Check BAR0 size */
	bar_size = dx_pci_rebar_get_current_size(pdev, 0);
	if (bar_size != -ENOTSUPP) { /* skip check logic */
		bar_size = dx_pci_rebar_size_to_bytes(bar_size);
		if (bar_size !=  BAR0_MEM_SIZE) {
			pci_err(pdev, "size of a BAR is not matched(%d)\n", bar_size);
			return -ENOMEM;
		}
	}
#endif /* DMA_DESC_SRAM */

	/* Data structure initialization */
	chip->dw = dw;
	chip->dev = dev;
//	chip->id = pdev->devfn;
	// chip->irq = pdev->irq;

	dw->mf = vsec_data.mf;
	dw->nr_irqs = nr_irqs;
	dw->dma_irqs = vsec_data.dma_irqs;
	dw->ops = &dw_edma_pcie_core_ops;
	dw->wr_ch_cnt = vsec_data.wr_ch_cnt;
	dw->rd_ch_cnt = vsec_data.rd_ch_cnt;

	dw->rg_region.vaddr = pcim_iomap_table(pdev)[vsec_data.rg.bar];
	if (!dw->rg_region.vaddr)
		return -ENOMEM;

	dw->rg_region.vaddr += vsec_data.rg.off;
	dw->rg_region.paddr = pdev->resource[vsec_data.rg.bar].start;
	dw->rg_region.paddr += vsec_data.rg.off;
	dw->rg_region.sz = vsec_data.rg.sz;

	/* iATU */
	dw->dma_desc_base_addr = DMA_DESC_START_ADDR;
	dw->dma_desc_base_bar  = pdev->resource[vsec_data.ll_wr[0].bar].start;
	dw->dma_desc_size      = BAR0_MEM_SIZE;
	dw->dma_desc_bar_num   = vsec_data.ll_wr[0].bar;
	{
		struct dx_edma_region *iatu = &dw->iatu_region;
		iatu->vaddr = pcim_iomap_table(pdev)[vsec_data.iatu.bar];
		if (!iatu->vaddr) {
			pci_err(pdev, "iATU BAR#%d mapping fail!\n", vsec_data.iatu.bar);
			return -ENOMEM;
		}
		iatu->vaddr += vsec_data.iatu.off;
		iatu->paddr = pdev->resource[vsec_data.iatu.bar].start;
		iatu->paddr += vsec_data.iatu.off;
		iatu->sz = vsec_data.iatu.sz;
	}

	/* NPU */
	dw->npu_cnt = vsec_data.npu_cnt;
	for (i = 0; i < dw->npu_cnt; i++) {
		struct dx_npu_region *npu = &dw->npu_region[i];
		npu->vaddr = pcim_iomap_table(pdev)[vsec_data.npu[i].bar];
		if (!npu->vaddr) {
			pci_err(pdev, "NPU BAR#%d mapping Fail!\n", vsec_data.npu[i].bar);
			return -ENOMEM;
		}
		npu->vaddr	+=	vsec_data.npu[i].off;
		npu->paddr	=	pdev->resource[vsec_data.npu[i].bar].start;
		npu->paddr	+=	vsec_data.npu[i].off;
		npu->sz		=	vsec_data.npu[i].sz;
		npu->bar_num=	vsec_data.npu[i].bar;
		npu->ep_addr=	vsec_data.npu[i].ep_addr;
	}

	for (i = 0; i < dw->wr_ch_cnt; i++) {
		struct dx_edma_region *ll_region = &dw->ll_region_wr[i];
		struct dx_edma_block *ll_block = &vsec_data.ll_wr[i];

		ll_region->vaddr = pcim_iomap_table(pdev)[ll_block->bar];
		if (!ll_region->vaddr)
			return -ENOMEM;

		ll_region->vaddr += ll_block->off;
		ll_region->paddr = dw->dma_desc_base_addr;
		ll_region->paddr += ll_block->off;
		ll_region->sz = ll_block->sz;

	}

	for (i = 0; i < dw->rd_ch_cnt; i++) {
		struct dx_edma_region *ll_region = &dw->ll_region_rd[i];
		struct dx_edma_block *ll_block = &vsec_data.ll_rd[i];

		ll_region->vaddr = pcim_iomap_table(pdev)[ll_block->bar];
		if (!ll_region->vaddr)
			return -ENOMEM;

		ll_region->vaddr += ll_block->off;
		ll_region->paddr = dw->dma_desc_base_addr;
		ll_region->paddr += ll_block->off;
		ll_region->sz = ll_block->sz;
	}

	/* Debug info */
	pci_dbg(pdev, "Probe pdev:%p\n", pdev);
	if (dw->mf == DX_DMA_MF_EDMA_LEGACY)
		pci_dbg(pdev, "Version:\teDMA Port Logic (0x%x)\n", dw->mf);
	else if (dw->mf == DX_DMA_MF_EDMA_UNROLL)
		pci_dbg(pdev, "Version:\teDMA Unroll (0x%x)\n", dw->mf);
	else if (dw->mf == DX_DMA_MF_HDMA_COMPAT)
		pci_dbg(pdev, "Version:\tHDMA Compatible (0x%x)\n", dw->mf);
	else
		pci_dbg(pdev, "Version:\tUnknown (0x%x)\n", dw->mf);

	pci_dbg(pdev, "Descriptor Table:\tBAR=%d, addr=0x%llx, sz=0x%llx bytes, target_addr=0x%llx\n",
		dw->dma_desc_bar_num, dw->dma_desc_base_bar,
		dw->dma_desc_size, dw->dma_desc_base_addr);

	pci_dbg(pdev, "Registers:\tBAR=%u, off=0x%.8lx, sz=0x%zx bytes, addr(v=%p, p=%pa)\n",
		vsec_data.rg.bar, vsec_data.rg.off, vsec_data.rg.sz,
		dw->rg_region.vaddr, &dw->rg_region.paddr);

	for (i = 0; i < dw->wr_ch_cnt; i++) {
		pci_dbg(pdev, "L. List:\tWRITE CH%.2u, BAR=%u, off=0x%.8lx, sz=0x%zx bytes, addr(v=%p, p=%pa)\n",
			i, vsec_data.ll_wr[i].bar,
			vsec_data.ll_wr[i].off, dw->ll_region_wr[i].sz,
			dw->ll_region_wr[i].vaddr, &dw->ll_region_wr[i].paddr);
	}

	for (i = 0; i < dw->rd_ch_cnt; i++) {
		pci_dbg(pdev, "L. List:\tREAD CH%.2u, BAR=%u, off=0x%.8lx, sz=0x%zx bytes, addr(v=%p, p=%pa)\n",
			i, vsec_data.ll_rd[i].bar,
			vsec_data.ll_rd[i].off, dw->ll_region_rd[i].sz,
			dw->ll_region_rd[i].vaddr, &dw->ll_region_rd[i].paddr);
	}

	pci_dbg(pdev, "iATU:\t BAR=%u, off=0x%.8lx, sz=0x%zx bytes, addr(v=%p, p=%pa)\n",
		vsec_data.iatu.bar, vsec_data.iatu.off, vsec_data.iatu.sz,
		dw->iatu_region.vaddr, &dw->iatu_region.paddr);

	for (i = 0; i < dw->npu_cnt; i++) {
		pci_dbg(pdev, "NPU%d:\t BAR=%u, off=0x%.8lx, sz=0x%zx bytes, addr(v=%p, p=%pa)\n",
			i, vsec_data.npu[i].bar, vsec_data.npu[i].off, vsec_data.npu[i].sz,
			dw->npu_region[i].vaddr, &dw->npu_region[i].paddr);
	}

	pci_dbg(pdev, "Nr. IRQs:\t%u\n", dw->nr_irqs);

	/* Validating if PCI interrupts were enabled */
	if (!pci_dev_msi_enabled(pdev)) {
		pci_err(pdev, "enable interrupt failed\n");
		return -EPERM;
	}

	dw->irq = devm_kcalloc(dev, nr_irqs, sizeof(*dw->irq), GFP_KERNEL);
	if (!dw->irq)
		return -ENOMEM;

	/* Detect device number */
	dx_dev_list_add(chip->dw);

	/* Starting eDMA driver */
	err = dx_dma_probe(chip);
	if (err) {
		pci_err(pdev, "eDMA probe failed(%d)\n", err);
		return err;
	}

	/* Saving data structure reference */
	pci_set_drvdata(pdev, chip);

	/* Create Cdev */
	err = xpdev_create_interfaces(chip);
	if (err)
		return err;

	dw_edma_thread_init(chip->dw->idx);

	pci_err(pdev, "[%s] Probe Done!!\n", __func__);

	return 0;
}

static void dx_dma_pcie_remove(struct pci_dev *pdev)
{
	struct dw_edma_chip *chip = pci_get_drvdata(pdev);
	int err;

	pci_dbg(pdev, "[%s]\n", __func__);

	/* Stopping eDMA driver */
	err = dx_dma_remove(chip);
	if (err)
		pci_warn(pdev, "can't remove device properly: %d\n", err);

	/* AER (Advanced Error Reporting) hooks */
	//pci_disable_pcie_error_reporting(pdev);

	/* Remove Cdev */
	xpdev_release_interfaces(chip->dw->xpdev);

	/* Remove device list */
	dx_dev_list_remove(chip->dw);

	/* Freeing IRQs */
	pci_free_irq_vectors(pdev);

	dw_edma_thread_exit(chip->dw->idx);
}

#ifdef CONFIG_PM_SLEEP
static int dx_pcie_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	pci_dbg(pdev, "Power Managerment to enter suspend\n");

	/* Suspend logic using PCI-PM */

	return 0;
}

static int dx_pcie_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	pci_dbg(pdev, "Power Managerment to enter resume\n");

	/* Wake-up logic using PCI-PM */

	return 0;
}

static SIMPLE_DEV_PM_OPS(dx_pcie_pm_ops, dx_pcie_suspend, dx_pcie_resume);
#endif /* CONFIG_PM_SLEEP */

static pci_ers_result_t dx_dma_pcie_error_detected(struct pci_dev *pdev,
						 pci_channel_state_t error)
{
	pci_dbg(pdev, "dx_dma_pcie_error_detected\n");
	/* TODO */
	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t dx_dma_pcie_error_slot_reset(struct pci_dev *pdev)
{
	pci_ers_result_t result = PCI_ERS_RESULT_RECOVERED;
	pci_dbg(pdev, "dx_dma_pcie_error_slot_reset\n");
	/* TODO */
	return result;
}

static void dx_dma_pcie_error_resume(struct pci_dev *pdev)
{
	pci_dbg(pdev, "dx_dma_pcie_error_resume\n");
	/* TODO */
}

static const struct pci_device_id dx_dma_pcie_id_table[] = {
	// { PCI_DEVICE_DATA(SYNOPSYS, EDDA, &dx_pcie_data) },
	// { PCI_DEVICE_DATA(SYNOPSYS, HAPSUSB3, &dx_pcie_data) },
	{ PCI_DEVICE(0x16c3, 0x1004), .driver_data = (kernel_ulong_t)(&dx_pcie_data) },
	{ }
};
MODULE_DEVICE_TABLE(pci, dx_dma_pcie_id_table);

static const struct pci_error_handlers dx_dma_err_handler = {
	.error_detected = dx_dma_pcie_error_detected,
	.slot_reset = dx_dma_pcie_error_slot_reset,
	.resume = dx_dma_pcie_error_resume,
};

static struct pci_driver dx_dma_pcie_driver = {
	.name		= "dx_dma_pcie",
	.id_table	= dx_dma_pcie_id_table,
	.probe		= dx_dma_pcie_probe,
	.remove		= dx_dma_pcie_remove,
	.err_handler	= &dx_dma_err_handler,
#ifdef CONFIG_PM_SLEEP
	.driver		= {
		pm: &dx_pcie_pm_ops,
	},
#endif /* CONFIG_PM_SLEEP */
};

static int dx_dma_mod_init(void)
{
	int rv;

	dbg_init("pci_register_driver.\n");
	rv = dx_cdev_init();
	if (rv < 0)
		return rv;
	dw_edma_thread_probe();

	return pci_register_driver(&dx_dma_pcie_driver);
}

static void dx_dma_mod_exit(void)
{
	/* unregister this driver from the PCI bus driver */
	dbg_init("pci_unregister_driver.\n");
	pci_unregister_driver(&dx_dma_pcie_driver);
	dx_cdev_cleanup();
}

module_init(dx_dma_mod_init);
module_exit(dx_dma_mod_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DeepX eDMA PCIe driver");
MODULE_AUTHOR("Taegyun An <atg@deepx.ai>");
