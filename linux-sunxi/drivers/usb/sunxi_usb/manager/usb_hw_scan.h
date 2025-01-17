/*
 * drivers/usb/sunxi_usb/manager/usb_hw_scan.h
 * (C) Copyright 2010-2015
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * javen, 2011-4-14, create this file
 *
 * usb detect module.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#ifndef  __USB_HW_SCAN_H__
#define  __USB_HW_SCAN_H__

#ifdef CONFIG_IO_EXPAND
extern __u32 g_usb_drv_det_pin;
extern __u32 g_usb_board_sel;
#endif

#define  USB_SCAN_INSMOD_DEVICE_DRIVER_DELAY	2
#define  USB_SCAN_INSMOD_HOST_DRIVER_DELAY	1

/* ubs id */
typedef enum usb_id_state{
	USB_HOST_MODE = 0,
	USB_DEVICE_MODE = 1,
}usb_id_state_t;

/* usb detect vbus */
typedef enum usb_det_vbus_state{
	USB_DET_VBUS_INVALID = 0,
	USB_DET_VBUS_VALID  = 1
}usb_det_vbus_state_t;

/* usb info */
typedef struct usb_scan_info{
	struct usb_cfg 		*cfg;

	usb_id_state_t          id_old_state;           /* last id state            */
	usb_det_vbus_state_t    det_vbus_old_state;     /* last detect vbus state   */

	u32                     device_insmod_delay;    /* debounce time            */
	u32                     host_insmod_delay;    	/* debounce time            */
}usb_scan_info_t;
extern int usb_hw_scan_debug;
extern int thread_stopped_flag;

void usb_hw_scan(struct usb_cfg *cfg);
__u32 set_vbus_id_state(u32 state);

__s32 usb_hw_scan_init(struct usb_cfg *cfg);
__s32 usb_hw_scan_exit(struct usb_cfg *cfg);
__s32 usbc0_platform_device_init(struct usb_port_info *port_info);
__s32 usbc0_platform_device_exit(struct usb_port_info *info);

#endif   //__USB_HW_SCAN_H__

