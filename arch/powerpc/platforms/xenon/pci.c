// SPDX-License-Identifier: GPL-2.0+
/*
 * Xenon PCI support
 * Maintained by: Felix Domke <tmbinc@elitedvb.net>
 * Minor modification by: wolie <wolie@telia.com>
 * based on:
 * Copyright (C) 2004 Benjamin Herrenschmuidt (benh@kernel.crashing.org),
 *		      IBM Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/init.h>

#include <asm/sections.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/pci-bridge.h>
#include <asm/machdep.h>
#include <asm/iommu.h>
#include <asm/ppc-pci.h>

#define OFFSET(bus, slot, func) ((((bus) << 8) + PCI_DEVFN(slot, func)) << 12)

static int xenon_pci_read_config(struct pci_bus *bus, unsigned int devfn,
				 int offset, int len, u32 *val)
{
	struct pci_controller *hose;
	unsigned int slot = PCI_SLOT(devfn);
	unsigned int func = PCI_FUNC(devfn);
	void *addr;

	hose = pci_bus_to_host(bus);
	if (hose == NULL)
		return PCIBIOS_DEVICE_NOT_FOUND;

	pr_debug("%s, slot %d, func %d\n", __func__, slot, func);
	pr_debug("%s, %p, devfn=%d, offset=%d, len=%d\n", __func__, bus, devfn,
		 offset, len);

	addr = ((void *)hose->cfg_addr) + offset;

	/* map GPU to slot 0x0f */
	if (slot == 0x0f)
		addr += OFFSET(0, 0x02, func);
	else
		addr += OFFSET(1, slot, func);

	/*
	 * Note: the caller has already checked that offset is
	 * suitably aligned and that len is 1, 2 or 4.
	 */
	switch (len) {
	case 1:
		*val = in_8((u8 *)addr);
		break;
	case 2:
		*val = in_le16((u16 *)addr);
		break;
	default:
		*val = in_le32((u32 *)addr);
		break;
	}

	pr_debug("->%08x\n", (int)*val);
	return PCIBIOS_SUCCESSFUL;
}

static int xenon_pci_write_config(struct pci_bus *bus, unsigned int devfn,
				  int offset, int len, u32 val)
{
	struct pci_controller *hose;
	unsigned int slot = PCI_SLOT(devfn);
	unsigned int func = PCI_FUNC(devfn);
	void *addr;

	hose = pci_bus_to_host(bus);
	if (!hose)
		return PCIBIOS_DEVICE_NOT_FOUND;

	pr_debug("%s, slot %d, func %d\n", __func__, slot, func);

	if (PCI_SLOT(devfn) >= 32)
		return PCIBIOS_DEVICE_NOT_FOUND;

	pr_debug("%s, %p, devfn=%d, offset=%x, len=%d, val=%08x\n", __func__, bus,
		 devfn, offset, len, val);

	addr = ((void *)hose->cfg_addr) + offset;

	/* map GPU to slot 0x0f */
	if (slot == 0x0f)
		addr += OFFSET(0, 0x02, func);
	else
		addr += OFFSET(1, slot, func);

	if (len == 4)
		pr_debug("was: %08x\n", readl(addr));
	else if (len == 2)
		pr_debug("was: %04x\n", readw(addr));
	else if (len == 1)
		pr_debug("was: %02x\n", readb(addr));

	/*
	 * Note: the caller has already checked that offset is
	 * suitably aligned and that len is 1, 2 or 4.
	 */
	switch (len) {
	case 1:
		writeb(val, addr);
		break;
	case 2:
		writew(val, addr);
		break;
	default:
		writel(val, addr);
		break;
	}

	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops xenon_pci_ops = {
	.read = xenon_pci_read_config,
	.write = xenon_pci_write_config,
};

void __init xenon_pci_init(void)
{
	struct pci_controller *hose;
	struct device_node *dev;

	dev = of_find_node_by_name(NULL, "pci");
	if (!dev) {
		pr_err("couldn't find PCI node!\n");
		return;
	}

	hose = pcibios_alloc_controller(dev);
	if (!hose) {
		pr_err("pcibios_alloc_controller failed!\n");
		return;
	}

	hose->first_busno = 0;
	hose->last_busno = 1;

	hose->ops = &xenon_pci_ops;
	hose->cfg_addr = ioremap(0xd0000000, 0x1000000);

	pci_process_bridge_OF_ranges(hose, dev, 1);

	/* Tell pci.c to not change any resource allocations. */
	pci_set_flags(PCI_PROBE_ONLY);

	of_node_put(dev);
	pr_debug("PCI initialized\n");

	pci_io_base = 0;

	set_pci_dma_ops(&dma_iommu_ops);
}
