/*
 * drivers/usb/sunxi_usb/udc/sunxi_udc_board.c
 * (C) Copyright 2010-2015
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * javen, 2010-12-20, create this file
 *
 * usb board config.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include  "sunxi_udc_config.h"
#include  "sunxi_udc_board.h"

#define res_size(_r) (((_r)->end - (_r)->start) + 1)

u32  open_usb_clock(sunxi_udc_io_t *sunxi_udc_io)
{
	DMSG_INFO_UDC("open_usb_clock\n");

	/* To fix hardware design issue. */
#if defined(CONFIG_ARCH_SUN50IW3) || defined(CONFIG_ARCH_SUN50IW6)
	usb_otg_phy_txtune(sunxi_udc_io->usb_vbase);
#endif

	if (sunxi_udc_io->ahb_otg && sunxi_udc_io->mod_usbphy && !sunxi_udc_io->clk_is_open) {
		if (clk_prepare_enable(sunxi_udc_io->ahb_otg)) {
			DMSG_PANIC("ERR:try to prepare_enable sunxi_udc_io->mod_usbphy failed!\n");
		}

		udelay(10);

		if (clk_prepare_enable(sunxi_udc_io->mod_usbphy)) {
			DMSG_PANIC("ERR:try to prepare_enable sunxi_udc_io->mod_usbphy failed!\n");
		}

		udelay(10);

		sunxi_udc_io->clk_is_open = 1;
	} else {
		DMSG_PANIC("ERR: clock handle is null, ahb_otg(0x%p), mod_usbotg(0x%p), mod_usbphy(0x%p), open(%d)\n",
			sunxi_udc_io->ahb_otg, sunxi_udc_io->mod_usbotg, sunxi_udc_io->mod_usbphy, sunxi_udc_io->clk_is_open);
	}

#if defined(CONFIG_ARCH_SUN8IW6) || defined(CONFIG_ARCH_SUN50IW6) \
		|| defined(CONFIG_ARCH_SUN50IW3)
	USBC_PHY_Set_Ctl(sunxi_udc_io->usb_vbase, USBC_PHY_CTL_VBUSVLDEXT);
	USBC_PHY_Clear_Ctl(sunxi_udc_io->usb_vbase, USBC_PHY_CTL_SIDDQ);
#else
	UsbPhyInit(0);
#endif

#if defined(CONFIG_ARCH_SUN50I) || defined(CONFIG_ARCH_SUN8IW10) \
	|| defined(CONFIG_ARCH_SUN8IW11) || defined(CONFIG_ARCH_SUN8IW17)
	/*otg and hci0 Controller Shared phy in SUN50I and SUN8IW10*/
	USBC_SelectPhyToDevice(sunxi_udc_io->usb_vbase);
#endif

	return 0;
}

u32 close_usb_clock(sunxi_udc_io_t *sunxi_udc_io)
{
	DMSG_INFO_UDC("close_usb_clock\n");

	if (sunxi_udc_io->ahb_otg && sunxi_udc_io->mod_usbphy && sunxi_udc_io->clk_is_open) {
		sunxi_udc_io->clk_is_open = 0;

		clk_disable_unprepare(sunxi_udc_io->mod_usbphy);

		clk_disable_unprepare(sunxi_udc_io->ahb_otg);
		udelay(10);

	} else {
		DMSG_PANIC("ERR: clock handle is null, ahb_otg(0x%p), mod_usbotg(0x%p), mod_usbphy(0x%p), open(%d)\n",
			sunxi_udc_io->ahb_otg, sunxi_udc_io->mod_usbotg, sunxi_udc_io->mod_usbphy, sunxi_udc_io->clk_is_open);
	}

#if defined(CONFIG_ARCH_SUN8IW6) || defined(CONFIG_ARCH_SUN50IW6) \
		|| defined(CONFIG_ARCH_SUN50IW3)
	USBC_PHY_Set_Ctl(sunxi_udc_io->usb_vbase, USBC_PHY_CTL_SIDDQ);
#else
	UsbPhyInit(0);
#endif

	return 0;
}

__s32 sunxi_udc_bsp_init(sunxi_udc_io_t *sunxi_udc_io)
{
	spinlock_t lock;
	unsigned long flags = 0;

	/* open usb lock */
	open_usb_clock(sunxi_udc_io);

#ifdef  SUNXI_USB_FPGA
	clear_usb_reg(sunxi_udc_io->usb_vbase);
	fpga_config_use_otg(sunxi_udc_io->usbc.sram_base);
#endif

	USBC_EnhanceSignal(sunxi_udc_io->usb_bsp_hdle);

	USBC_EnableDpDmPullUp(sunxi_udc_io->usb_bsp_hdle);
	USBC_EnableIdPullUp(sunxi_udc_io->usb_bsp_hdle);
	USBC_ForceId(sunxi_udc_io->usb_bsp_hdle, USBC_ID_TYPE_DEVICE);
	USBC_ForceVbusValid(sunxi_udc_io->usb_bsp_hdle, USBC_VBUS_TYPE_HIGH);

	USBC_SelectBus(sunxi_udc_io->usb_bsp_hdle, USBC_IO_TYPE_PIO, 0, 0);

	USBC_PHY_Clear_Ctl(sunxi_udc_io->usb_vbase, 1);

	/* config usb fifo */
	spin_lock_init(&lock);
	spin_lock_irqsave(&lock, flags);
	USBC_ConfigFIFO_Base(sunxi_udc_io->usb_bsp_hdle, USBC_FIFO_MODE_8K);
	spin_unlock_irqrestore(&lock, flags);

	return 0;
}

__s32 sunxi_udc_bsp_exit(sunxi_udc_io_t *sunxi_udc_io)
{
	USBC_DisableDpDmPullUp(sunxi_udc_io->usb_bsp_hdle);
	USBC_DisableIdPullUp(sunxi_udc_io->usb_bsp_hdle);
	USBC_ForceId(sunxi_udc_io->usb_bsp_hdle, USBC_ID_TYPE_DISABLE);
	USBC_ForceVbusValid(sunxi_udc_io->usb_bsp_hdle, USBC_VBUS_TYPE_DISABLE);

	close_usb_clock(sunxi_udc_io);
	return 0;
}

__s32 sunxi_udc_io_init(__u32 usbc_no, sunxi_udc_io_t *sunxi_udc_io)
{

	//DMSG_INFO_UDC("sram_vbase = 0x%p\n", sunxi_udc_io->sram_vbase);
	//DMSG_INFO_UDC("usb_vbase  = 0x%p\n", sunxi_udc_io->usb_vbase);

	sunxi_udc_io->usbc.usbc_info.num = usbc_no;
	sunxi_udc_io->usbc.usbc_info.base = sunxi_udc_io->usb_vbase;
	sunxi_udc_io->usbc.sram_base = sunxi_udc_io->sram_vbase;

	USBC_init(&sunxi_udc_io->usbc);
	sunxi_udc_io->usb_bsp_hdle = USBC_open_otg(usbc_no);
	if (sunxi_udc_io->usb_bsp_hdle == 0) {
		DMSG_PANIC("ERR: sunxi_udc_init: USBC_open_otg failed\n");
		return -1;
	}

	return 0;
}

__s32 sunxi_udc_io_exit(sunxi_udc_io_t *sunxi_udc_io)
{
	USBC_close_otg(sunxi_udc_io->usb_bsp_hdle);
	sunxi_udc_io->usb_bsp_hdle = 0;
	USBC_exit(&sunxi_udc_io->usbc);
	sunxi_udc_io->usb_vbase  = NULL;
	sunxi_udc_io->sram_vbase = NULL;

	return 0;
}

