/*
 * Copyright (C) 2011 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef _LINUX_CT406_H__
#define _LINUX_CT406_H__

#include <linux/ioctl.h>

#ifdef __KERNEL__

#include <linux/types.h>

#define LD_CT406_NAME "ct406"

#define CT406_REGULATOR_NAME_LENGTH 10

struct ct406_platform_data {
	u16	irq;
	u8	regulator_name[CT406_REGULATOR_NAME_LENGTH];
	u8	prox_samples_for_noise_floor;
	u16	prox_saturation_threshold;
	u16	prox_covered_offset;
	u16	prox_uncovered_offset;
	u16	prox_recalibrate_offset;
	u8	als_lens_transmissivity;
} __attribute__ ((packed));

#endif	/* __KERNEL__ */

#define CT406_IOCTL_BASE		0xA4
#define CT406_IOCTL_GET_PROX_ENABLE	_IOR(CT406_IOCTL_BASE, 0x00, char)
#define CT406_IOCTL_SET_PROX_ENABLE	_IOW(CT406_IOCTL_BASE, 0x01, char)
#define CT406_IOCTL_GET_LIGHT_ENABLE	_IOR(CT406_IOCTL_BASE, 0x02, char)
#define CT406_IOCTL_SET_LIGHT_ENABLE	_IOW(CT406_IOCTL_BASE, 0x03, char)

#endif	/* _LINUX_CT406_H__ */