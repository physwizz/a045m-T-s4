
/* Copyright Statement:
*
* This software/firmware and related documentation ("MediaTek Software") are
* protected under relevant copyright laws. The information contained herein
* is confidential and proprietary to MediaTek Inc. and/or its licensors.
* Without the prior written permission of MediaTek inc. and/or its licensors,
* any reproduction, modification, use or disclosure of MediaTek Software,
* and information contained herein, in whole or in part, shall be strictly prohibited.
*/
/* MediaTek Inc. (C) 2015. All rights reserved.
*
* BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
* THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
* RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
* AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
* NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
* SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
* SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
* THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
* THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
* CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
* SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
* CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
* AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
* OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
* MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*/
#define LOG_TAG "LCM"
#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include "disp_dts_gpio.h"
#endif

#include "panel_notifier.h"  //tp suspend before lcd suspend

#ifndef MACH_FPGA
#include <lcm_pmic.h>
#endif
#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

static struct LCM_UTIL_FUNCS lcm_util;
#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))
#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif
#define LCM_DSI_CMD_MODE									0
#define FRAME_WIDTH										(720)
#define FRAME_HEIGHT									(1600)
#define LCM_PHYSICAL_WIDTH								(67932)
#define LCM_PHYSICAL_HEIGHT								(150960)
#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW		0xFFFE
#define REGFLAG_RESET_HIGH		0xFFFF
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

extern void tddi_lcm_tp_reset_output(int level);

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	// SLEEP IN + DISPLAY OFF
	{0x28, 0,{0x00}},
	{REGFLAG_DELAY, 20,{}},
	{0x10, 0,{0x00}},
	{REGFLAG_DELAY, 150,{}},
	{0x17, 1,{0x5A}},
	{0x18, 1,{0x5A}},
	{REGFLAG_DELAY, 1,{}},
};

static struct LCM_setting_table init_setting_vdo[] = {

	{0x41, 1, {0x5A}},

	{0x41, 2, {0x5A, 0x24}},
	{0x90, 1, {0x5A}},

	{0x41, 2, {0x5A, 0x08}},
	{0x80, 3, {0xC8, 0x2C, 0x01}},

	{0x41, 2, {0x5A, 0x09}},
	{0x80, 16, {0x5A,0x51,0xB5,0x2A,0x6C,0xE5,0x4A,0x01,0x40,0x62,0x0F,0x82,0x20,0x08,0xF0,0xB7}},
	{0x90, 16, {0x00,0x24,0x42,0x0A,0xE3,0x91,0xA4,0xF0,0xC3,0xC3,0x6B,0x20,0x2D,0xA1,0x26,0x00}},
	{0xA0, 16, {0x51,0x55,0x55,0x00,0xA0,0x4C,0x06,0x11,0x0D,0x60,0x5A,0xFF,0xFF,0x03,0xA5,0xE6}},
	{0xB0, 16, {0x08,0xC9,0x16,0x64,0x0B,0x00,0x00,0x11,0x07,0x60,0x00,0xFF,0xFF,0x03,0xFF,0x34}},
	{0xC0, 8, {0x0C,0x3F,0x1F,0x9F,0x0F,0x00,0x08,0x00}},

	{0x41, 2, {0x5A, 0x0A}},
	{0x80, 16, {0xFF,0x77,0xEF,0x02,0x00,0x0E,0x19,0x28,0x39,0x4B,0x51,0x85,0x5E,0x9A,0x9A,0x62}},
	{0x90, 16, {0x8A,0x5B,0x56,0x46,0x35,0x24,0x16,0x00,0x00,0x0E,0x19,0x28,0x39,0x4B,0x51,0x85}},
	{0xA0, 13, {0x5E,0x9A,0x9A,0x62,0x8A,0x5B,0x56,0x46,0x35,0x24,0x16,0x16,0x00}},

	{0x41, 2, {0x5A, 0x0B}},
	{0x80, 16, {0x00,0x00,0x20,0x44,0x08,0x00,0x60,0x47,0x00,0x00,0x10,0x22,0x04,0x00,0xB0,0x23}},
	{0x90, 2, {0x15, 0x00}},

	{0x41, 2, {0x5A, 0x0C}},
	{0x80, 16, {0xBA,0x68,0x68,0x01,0xB2,0x75,0xD0,0x07,0x00,0x60,0x15,0x00,0x50,0x15,0x56,0x51}},
	{0x90, 16, {0x15,0x55,0x61,0x15,0x00,0x60,0x15,0x00,0x50,0x15,0x56,0x51,0x15,0x55,0x61,0x95}},
	{0xA0, 16, {0xAB,0x18,0x00,0x05,0x00,0x05,0x00,0x05,0x00,0x49,0x29,0x84,0x52,0x01,0x09,0x00}},
	{0xB0, 2, {0x00, 0x00}},

	{0x41, 2, {0x5A, 0x0D}},
	{0x80, 7, {0xF0,0xB1,0x71,0xEF,0x4B,0xC0,0x80}},

	{0x41, 2, {0x5A, 0x0E}},
	{0x80, 8, {0xFF,0x01,0x55,0x55,0x23,0x88,0x88,0x1C}},

	{0x41, 2, {0x5A, 0x0F}},
	{0x80, 16, {0xD1,0x07,0x70,0x40,0x13,0x38,0x64,0x08,0x52,0x51,0x58,0x49,0x03,0x52,0x4C,0x4C}},
	{0x90, 16, {0x68,0x68,0x68,0x4C,0x4C,0x7C,0x14,0x00,0x20,0x06,0xC2,0x00,0x04,0x06,0x0C,0x00}},
	{0xA0, 4, {0x00, 0x92, 0x00, 0x00}},

	{0x41, 2, {0x5A, 0x10}},
	{0x80, 16, {0x00,0x00,0x03,0xE7,0x1F,0x17,0x10,0x48,0x80,0xAA,0xD0,0x18,0x30,0x88,0x41,0x8A}},
	{0x90, 15, {0x39,0x28,0xA9,0xC5,0x9A,0x7B,0xF0,0x07,0x7E,0xE0,0x07,0x7E,0x20,0x10,0x00}},

	{0x41, 2, {0x5A, 0x11}},
	{0x80, 16, {0x49,0x77,0x03,0x40,0xCA,0xF3,0xFF,0xA3,0x20,0x08,0xC4,0x06,0xA1,0xD8,0x24,0x18}},
	{0x90, 16, {0x30,0xC6,0x66,0xC1,0x80,0x1D,0x15,0xCB,0xE5,0xE2,0x70,0x6C,0x36,0x1D,0x04,0xC8}},
	{0xA0, 16, {0xB0,0xD9,0x88,0x60,0xB0,0x81,0x40,0x1C,0x1B,0x88,0x63,0x03,0xB9,0x00,0x1C,0x80}},
	{0xB0, 16, {0x50,0x30,0x00,0xE0,0xE1,0x01,0x00,0x28,0x0E,0x06,0x43,0x55,0x55,0x55,0x55,0x55}},
	{0xC0, 16, {0x95,0x88,0x88,0x88,0x88,0x88,0xC8,0x08,0x84,0xC6,0xE3,0x81,0x00,0x20,0x00,0x21}},
	{0xD0, 16, {0x42,0x88,0x00,0x00,0x00,0x00,0x40,0x00,0x20,0x31,0x04,0x41,0x06,0x00,0x00,0x00}},
	{0xE0, 16, {0x00,0x92,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x92,0x04,0x00,0x85,0x11,0x0C}},
	{0xF0, 15, {0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x5E,0x4A,0x01,0x10,0x22,0x00,0x00,0x00}},

	{0x41, 2, {0x5A, 0x12}},
	{0x80, 16, {0x00,0x00,0x00,0x00,0x00,0x02,0x03,0x00,0x00,0x00,0x00,0x02,0x03,0x01,0x41,0x37}},
	{0x90, 16, {0xF1,0xE7,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2D,0x23,0x05}},
	{0xA0, 7, {0xFB,0x08,0x2D,0x23,0x05,0xFB,0x0C}},

	{0x41, 2, {0x5A, 0x13}},
	{0x80, 16, {0xFD,0x0F,0x00,0x0C,0x00,0x00,0x00,0x00,0x01,0x08,0x01,0x1C,0x44,0x0C,0xCE,0xE7}},
	{0x90, 14, {0x62,0x0E,0x24,0x98,0xAC,0x21,0x01,0x00,0xD0,0x93,0x24,0x49,0x06,0x20}},

	{0x41, 2, {0x5A, 0x14}},
	{0x80, 16, {0x01,0x02,0x41,0x36,0xE9,0xEF,0xF7,0xFB,0xFD,0x7E,0x01,0x00,0x00,0x90,0xC5,0x4C}},
	{0x90, 16, {0x3A,0x6D,0x20,0x56,0xD2,0x69,0x03,0x5B,0x91,0x4A,0x9F,0xD0,0x84,0x54,0x5A,0x00}},
	{0xA0, 16, {0xAC,0x88,0xDE,0x63,0x4C,0x00,0xDF,0x0F,0x00,0x22,0x8B,0x62,0x33,0x95,0xA6,0x20}},
	{0xB0, 16, {0xC3,0x41,0x99,0x6C,0x00,0x00,0x00,0x00,0x00,0x1C,0xB2,0x21,0x2B,0x00,0x40,0xA1}},
	{0xC0, 16, {0x50,0x78,0x17,0xE9,0xF4,0x80,0x51,0x1C,0x6F,0x22,0x9D,0x1E,0x30,0x8A,0xE3,0x3D}},
	{0xD0, 16, {0xA4,0xD3,0x03,0x46,0x71,0xBC,0x85,0x74,0x7A,0xC0,0x28,0x8E,0x77,0x90,0x4E,0x0F}},
	{0xE0, 16, {0x18,0xC5,0xF1,0x06,0xD2,0xE9,0x01,0xA3,0x38,0x5E,0x40,0x3A,0x3D,0x60,0x14,0xC7}},
	{0xF0, 16, {0x2B,0x48,0xA7,0x07,0x8C,0xE2,0x18,0x01,0xBF,0xDF,0x08,0x00,0x00,0x00,0x00,0x00}},

	{0x41, 2, {0x5A, 0x15}},
	{0x80, 16, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0xC0,0xFF,0xF7}},
	{0x90, 16, {0x93,0x01,0x20,0x4B,0x87,0x99,0x4E,0x01,0x00,0x00,0xD6,0x61,0xA6,0x53,0x00,0x00}},
	{0xA0, 16, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x90}},
	{0xB0, 16, {0x59,0xA1,0x28,0x14,0x00,0x00,0x00,0x00,0x13,0x00,0xF0,0x00,0x14,0xB3,0x6D,0x5B}},
	{0xC0, 16, {0xB3,0x05,0x43,0xAA,0x05,0x08,0x84,0x69,0x21,0xA9,0x35,0x4D,0xB3,0x35,0xDB,0xB6}},
	{0xD0, 16, {0x05,0x5B,0x33,0xA4,0x5A,0x47,0x31,0x84,0x76,0x8A,0x5A,0xD3,0x34,0x5B,0xB3,0x6D}},
	{0xE0, 16, {0x5B,0xB3,0x05,0x43,0xAA,0x15,0x0C,0xC5,0x69,0xA3,0xA9,0x35,0x4D,0xB3,0x35,0xDB}},
	{0xF0, 16, {0xB6,0x05,0x5B,0x33,0xA4,0x5A,0x06,0x21,0x80,0x56,0x82,0x5A,0xD3,0x34,0x5B,0x00}},

	{0x41, 2, {0x5A, 0x16}},
	{0x80, 16, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0x90, 6, {0x00,0x00,0x00,0x00,0xF0,0x20}},

	{0x41, 2, {0x5A, 0x18}},
	{0x80, 16, {0xEF,0xBD,0xF7,0xDE,0x7B,0xEF,0xBD,0x07,0x08,0x08,0x0A,0x0C,0x0C,0x0C,0x0C,0x0C}},
	{0x90, 16, {0x0C,0x0C,0x0C,0x5C,0x09,0xA8,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0x5A}},
	{0xA0, 16, {0x59,0x59,0x59,0x59,0x59,0x59,0x59,0x59,0x59,0x59,0x59,0x09,0x04,0xFF,0x00,0x80}},
	{0xB0, 15, {0x80,0x00,0x04,0x20,0x00,0x01,0x08,0x40,0x00,0x02,0x10,0x80,0x00,0x04,0x00}},

	{0x41, 2, {0x5A, 0x19}},
	{0x80, 16, {0xC0,0xAF,0xA3,0x9B,0x92,0x8D,0x8A,0x86,0x84,0x83,0x82,0x81,0x00,0x50,0xF6,0xCF}},
	{0x90, 16, {0xFC,0x2F,0xF3,0xEF,0xCF,0xBF,0x0F,0xFF,0xAF,0xB5,0x71,0x0E,0x6C,0x4A,0x69,0x08}},
	{0xA0, 4, {0x00,0x00,0x06,0x00}},

	{0x41, 2, {0x5A, 0x1A}},
	{0x80, 16, {0x00,0x04,0x08,0x0C,0x00,0x10,0x14,0x18,0x1C,0x00,0x20,0x28,0x30,0x38,0x00,0x40}},
	{0x90, 16, {0x48,0x50,0x58,0x00,0x60,0x68,0x70,0x78,0x00,0x80,0x88,0x90,0x98,0x00,0xA0,0xA8}},
	{0xA0, 16, {0xB0,0xB8,0x00,0xC0,0xC8,0xD0,0xD8,0x00,0xE0,0xE8,0xF0,0xF8,0x00,0xFC,0xFE,0xFF}},
	{0xB0, 16, {0x00,0x00,0x04,0x08,0x0C,0x00,0x10,0x14,0x18,0x1C,0x00,0x20,0x28,0x30,0x38,0x00}},
	{0xC0, 16, {0x40,0x48,0x50,0x58,0x00,0x60,0x68,0x70,0x78,0x00,0x80,0x88,0x90,0x98,0x00,0xA0}},
	{0xD0, 16, {0xA8,0xB0,0xB8,0x00,0xC0,0xC8,0xD0,0xD8,0x00,0xE0,0xE8,0xF0,0xF8,0x00,0xFC,0xFE}},
	{0xE0, 16, {0xFF,0x00,0x00,0x04,0x08,0x0C,0x00,0x10,0x14,0x18,0x1C,0x00,0x20,0x28,0x30,0x38}},
	{0xF0, 16, {0x00,0x40,0x48,0x50,0x58,0x00,0x60,0x68,0x70,0x78,0x00,0x80,0x88,0x90,0x98,0x00}},

	{0x41, 2, {0x5A, 0x1B}},
	{0x80, 16, {0xA0,0xA8,0xB0,0xB8,0x00,0xC0,0xC8,0xD0,0xD8,0x00,0xE0,0xE8,0xF0,0xF8,0x00,0xFC}},
	{0x90, 4, {0xFE,0xFF,0x00,0x00}},

	{0x41, 2, {0x5A, 0x20}},
	{0x80, 7, {0x87,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x41, 2, {0x5A, 0x22}},
	{0x80, 13, {0x2D,0xD3,0x00,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x9F,0x00}},

	{0x41, 2, {0x5A, 0x23}},
	{0x80, 16, {0x01,0x05,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0x90, 12, {0xFF,0x0F,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0xFF,0x07,0x35}},

	{0x41, 2, {0x5A, 0x24}},
	{0x80, 16, {0x00,0x03,0x00,0xFF,0xFF,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xC2,0x63}},
	{0x90, 16, {0x5A,0x5A,0x5A,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x42, 1, {0x24}},
	{0x90, 1, {0x00}},

	{0x41, 2, {0x5A, 0x2F}},
	{0x19, 1, {0x00}},
	{0x4C, 1, {0x03}},
	{0x11, 0x01, {0x00} },
	{REGFLAG_DELAY, 100, {} },
	{0x29, 0x01, {0x00} },
	{0x51, 0x02, {0x00,0x00}},
	{0x53, 0x01, {0x2C}},
	{0x55, 0x01, {0x00}},
	{ REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table bl_level[] = {

	{ 0x51, 0x02, {0xFF, 0x0E} },
	{ REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void push_table(void *cmdq, struct LCM_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned cmd;
	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;
		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));
	params->type = LCM_TYPE_DSI;
	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
#endif
	params->dsi.switch_mode_enable = 0;
	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;
	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.vertical_sync_active = 8;
	params->dsi.vertical_backporch = 106;
	params->dsi.vertical_frontporch = 140;
	//params->dsi.vertical_frontporch_for_low_power = 540;/*disable dynamic frame rate*/
	params->dsi.vertical_active_line = FRAME_HEIGHT;
	params->dsi.horizontal_sync_active = 10;
	params->dsi.horizontal_backporch = 52;
	params->dsi.horizontal_frontporch = 52;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_range = 4;
	params->dsi.ssc_disable = 1;
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 282;	/* this value must be in MTK suggested table */
#else
	params->dsi.PLL_CLOCK = 295;	/* this value must be in MTK suggested table */
	params->dsi.data_rate = 590;
#endif
	//params->dsi.PLL_CK_CMD = 220;
	//params->dsi.PLL_CK_VDO = 255;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	/* mipi hopping setting params */
	params->dsi.dynamic_switch_mipi = 1;
	params->dsi.data_rate_dyn = 610;
	params->dsi.PLL_CLOCK_dyn = 305;
	params->dsi.horizontal_sync_active_dyn = 10;
	params->dsi.horizontal_backporch_dyn = 64;
	params->dsi.horizontal_frontporch_dyn  = 60;

	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 0;
	params->corner_pattern_width = 720;
	params->corner_pattern_height = 32;
#endif
}

enum Color {
	LOW,
	HIGH
};

static void lcm_init_power(void)
{
	int ret = 0;
	pr_debug("[LCM]ft8006s skyworth lcm_init_power !\n");
	tddi_lcm_tp_reset_output(LOW);
	lcm_reset_pin(LOW);
	/*lcm_vio_enable();
	MDELAY(10);*/
	ret = lcm_power_enable();
	MDELAY(5);
}

extern bool fts_gestrue_status;
static void lcm_suspend_power(void)
{
	if (fts_gestrue_status == 1) {
		pr_debug("[LCM] ft8006s skyworth lcm_suspend_power fts_gestrue_status = 1!\n");
	} else {
		int ret  = 0;
		pr_debug("[LCM] ft8006s skyworth lcm_suspend_power fts_gestrue_status = 0!\n");
		//lcm_reset_pin(LOW);
		MDELAY(2);
		ret = lcm_power_disable();
		MDELAY(10);
	}
}

static void lcm_resume_power(void)
{
	pr_debug("[LCM]ft8006s skyworth lcm_resume_power !\n");
	lcm_init_power();
}

static void lcm_init(void)
{
	pr_debug("[LCM] ft8006s skyworth lcm_init\n");
	lcm_reset_pin(HIGH);
	MDELAY(5);
	lcm_reset_pin(LOW);
	MDELAY(6);
	tddi_lcm_tp_reset_output(HIGH);
	MDELAY(1);
	lcm_reset_pin(HIGH);
	MDELAY(36);
	push_table(NULL, init_setting_vdo, sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table), 1);
	pr_debug("[LCM] ft8006s skyworth----init success ----\n");
}

static void lcm_suspend(void)
{
	pr_debug("[LCM] lcm_suspend\n");
	panel_notifier_call_chain(PANEL_UNPREPARE, NULL);
	pr_debug("[FTS]tpd_ilitek_notifier_callback in lcm_suspend\n ");
	push_table(NULL, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	MDELAY(10);
	/* lcm_set_gpio_output(GPIO_LCM_RST,0); */
}

static void lcm_resume(void)
{
	pr_debug("[LCM] lcm_resume\n");
	lcm_init();
	//add for TP resume
	panel_notifier_call_chain(PANEL_PREPARE, NULL);
	pr_debug("[FTS]tpd_ilitek_notifier_callback in lcm_resume\n ");
}

static unsigned int lcm_compare_id(void)
{
	return 1;
}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;
	unsigned int x1 = FRAME_WIDTH * 3 / 4;
	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned int data_array[3];
	unsigned char read_buf[4];
	pr_debug("ATA check size = 0x%x,0x%x,0x%x,0x%x\n", x0_MSB, x0_LSB, x1_MSB, x1_LSB);
	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	data_array[0] = 0x00043700;	/* read id return two byte,version and id */
	dsi_set_cmdq(data_array, 1, 1);
	read_reg_v2(0x2A, read_buf, 4);
	if ((read_buf[0] == x0_MSB) && (read_buf[1] == x0_LSB)
	    && (read_buf[2] == x1_MSB) && (read_buf[3] == x1_LSB))
		ret = 1;
	else
		ret = 0;
	x0 = 0;
	x1 = FRAME_WIDTH - 1;
	x0_MSB = ((x0 >> 8) & 0xFF);
	x0_LSB = (x0 & 0xFF);
	x1_MSB = ((x1 >> 8) & 0xFF);
	x1_LSB = (x1 & 0xFF);
	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	return ret;
#else
	return 0;
#endif
}

static void lcm_setbacklight_cmdq(void* handle,unsigned int level)
{
	unsigned int bl_lvl;
	bl_lvl = wingtech_bright_to_bl(level,255,10,2047,24);
	pr_debug("%s,ft8006s skyworth backlight: level = %d,bl_lvl=%d\n", __func__, level,bl_lvl);
	// set 11bit
	bl_level[0].para_list[0] = (bl_lvl&0x7F8)>>3;
	bl_level[0].para_list[1] = (bl_lvl&0x07)<<1;
	pr_debug("ft8006s skyworth backlight: para_list[0]=%x,para_list[1]=%x\n",bl_level[0].para_list[0],bl_level[0].para_list[1]);
	push_table(handle, bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

struct LCM_DRIVER ft8006s_dsi_vdo_hdp_skyworth_shenchao_drv = {
	.name = "ft8006s_dsi_vdo_hdp_skyworth_shenchao",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
};