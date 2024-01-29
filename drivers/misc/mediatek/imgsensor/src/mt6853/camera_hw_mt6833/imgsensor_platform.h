/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/
#ifndef __IMGSENSOR_PLATFORM_H__
#define __IMGSENSOR_PLATFORM_H__

//bug767771,liangyiyi.wt,DEL,2022/07/28,the w2 project don't need to configure MIPI_SWITCH
//#define MIPI_SWITCH

enum IMGSENSOR_HW_ID {
	IMGSENSOR_HW_ID_MCLK,
	IMGSENSOR_HW_ID_REGULATOR,
	IMGSENSOR_HW_ID_GPIO,
	IMGSENSOR_HW_ID_MAX_NUM,
	IMGSENSOR_HW_ID_NONE = -1
};
#endif
