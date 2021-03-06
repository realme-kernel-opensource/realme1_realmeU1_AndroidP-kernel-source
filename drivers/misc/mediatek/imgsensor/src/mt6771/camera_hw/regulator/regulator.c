/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "regulator.h"
#ifndef VENDOR_EDIT
#define VENDOR_EDIT
#endif

#ifdef VENDOR_EDIT
#include<soc/oppo/oppo_project.h>
#endif


static const int regulator_voltage[] = {
	REGULATOR_VOLTAGE_0,
	REGULATOR_VOLTAGE_1000,
	REGULATOR_VOLTAGE_1100,
	REGULATOR_VOLTAGE_1200,
	REGULATOR_VOLTAGE_1210,
	REGULATOR_VOLTAGE_1220,
	REGULATOR_VOLTAGE_1500,
	REGULATOR_VOLTAGE_1800,
	REGULATOR_VOLTAGE_2500,
	REGULATOR_VOLTAGE_2800,
	REGULATOR_VOLTAGE_2900,
};

#ifdef VENDOR_EDIT
/*Yijun.Tan @ Camera.Drv modify for main2 camera 20180109*/
struct REGULATOR_CTRL regulator_control[REGULATOR_TYPE_MAX_NUM] = {
	{"vcama"},
	{"vcamd"},
	{"vcamio"},
	{"vcama_sub"},
	{"vcamd_sub"},
	{"vcamio_sub"},
	{"vcama_sub"},
	{"vcamd"},
	{"vcamio"},
	{"vcama_sub2"},
	{"vcamd_sub2"},
	{"vcamio_sub2"},
	{"vcama_main3"},
	{"vcamd_main3"},
	{"vcamio_main3"}
};
/*Xiaoyang.Huang@RM.Camera add for 18611 board,20190304*/
struct REGULATOR_CTRL regulator_control_18611[REGULATOR_TYPE_MAX_NUM] = {
	{"vcama"},
	{"vcamd"},
	{"vcamio"},
	{"vcama_sub"},
	{"vcamd_sub"},
	{"vcamio_sub"},
	{"vcama_main2"},
	{"vcamd"},
	{"vcamio"},
	{"vcama_sub2"},
	{"vcamd_sub2"},
	{"vcamio_sub2"},
	{"vcama_main3"},
	{"vcamd_main3"},
	{"vcamio_main3"}
};
#else
struct REGULATOR_CTRL regulator_control[REGULATOR_TYPE_MAX_NUM] = {
	{"vcama"},
	{"vcamd"},
	{"vcamio"},
	{"vcama_sub"},
	{"vcamd_sub"},
	{"vcamio_sub"},
	{"vcama_main2"},
	{"vcamd_main2"},
	{"vcamio_main2"},
	{"vcama_sub2"},
	{"vcamd_sub2"},
	{"vcamio_sub2"},
	{"vcama_main3"},
	{"vcamd_main3"},
	{"vcamio_main3"}
};
#endif
static struct REGULATOR reg_instance;

#ifdef VENDOR_EDIT
/*Femg.Hu@Camera.Driver 20171120 add for flash&lens to use i2c individual*/
static struct regulator *gVCamIO;
static struct regulator *gVCamAF;
#endif

static enum IMGSENSOR_RETURN regulator_init(
	void *pinstance,
	struct IMGSENSOR_HW_DEVICE_COMMON *pcommon)
{
	struct REGULATOR      *preg            = (struct REGULATOR *)pinstance;
	struct REGULATOR_CTRL *pregulator_ctrl = regulator_control;
	int i;
	#ifdef VENDOR_EDIT
	/*Xiaoyang.Huang@RM.Camera add for 18611 board,20190304*/
	if (is_project(OPPO_18611)) {
		PK_PR_ERR("This is 18611 board regulator\n");
		pregulator_ctrl = regulator_control_18611;
	}

	#endif

	for (i = 0; i < REGULATOR_TYPE_MAX_NUM; i++, pregulator_ctrl++) {
		preg->pregulator[i] = regulator_get(
				&pcommon->pplatform_device->dev,
				pregulator_ctrl->pregulator_type);
		if (preg->pregulator[i] == NULL)
			PK_PR_ERR("regulator[%d]  %s fail!\n",
						i, pregulator_ctrl->pregulator_type);
		atomic_set(&preg->enable_cnt[i], 0);
	}
	#ifdef VENDOR_EDIT
	//*Femg.Hu@Camera.Driver 20171120 add for flash&lens to use i2c individual*/
	gVCamIO = regulator_get(&pcommon->pplatform_device->dev, "vcamio");
	gVCamAF = regulator_get(&pcommon->pplatform_device->dev, "vldo28");
	#endif
	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN regulator_release(void *pinstance)
{
	struct REGULATOR *preg = (struct REGULATOR *)pinstance;
	int i;

	for (i = 0; i < REGULATOR_TYPE_MAX_NUM; i++) {
		if (preg->pregulator[i] != NULL) {
			for (; atomic_read(&preg->enable_cnt[i]) > 0; ) {
				regulator_disable(preg->pregulator[i]);
				atomic_dec(&preg->enable_cnt[i]);
			}
		}
	}
	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN regulator_set(
	void *pinstance,
	enum IMGSENSOR_SENSOR_IDX   sensor_idx,
	enum IMGSENSOR_HW_PIN       pin,
	enum IMGSENSOR_HW_PIN_STATE pin_state)
{
	struct regulator     *pregulator;
	struct REGULATOR     *preg = (struct REGULATOR *)pinstance;
	enum   REGULATOR_TYPE reg_type_offset;
	atomic_t             *enable_cnt;


	if (pin > IMGSENSOR_HW_PIN_DOVDD   ||
	    pin < IMGSENSOR_HW_PIN_AVDD    ||
	    pin_state < IMGSENSOR_HW_PIN_STATE_LEVEL_0 ||
	    pin_state >= IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH)
		return IMGSENSOR_RETURN_ERROR;

	reg_type_offset =
		(sensor_idx == IMGSENSOR_SENSOR_IDX_MAIN)  ? REGULATOR_TYPE_MAIN_VCAMA :
		(sensor_idx == IMGSENSOR_SENSOR_IDX_SUB)   ? REGULATOR_TYPE_SUB_VCAMA :
		(sensor_idx == IMGSENSOR_SENSOR_IDX_MAIN2) ? REGULATOR_TYPE_MAIN2_VCAMA :
		(sensor_idx == IMGSENSOR_SENSOR_IDX_SUB2)   ? REGULATOR_TYPE_SUB2_VCAMA :
		REGULATOR_TYPE_MAIN3_VCAMA;

	pregulator = preg->pregulator[reg_type_offset + pin - IMGSENSOR_HW_PIN_AVDD];
	enable_cnt = preg->enable_cnt + (reg_type_offset + pin - IMGSENSOR_HW_PIN_AVDD);

	if (pregulator) {
		if (pin_state != IMGSENSOR_HW_PIN_STATE_LEVEL_0) {
			if (regulator_set_voltage(pregulator,
					regulator_voltage[pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0],
					regulator_voltage[pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0])) {

				PK_PR_ERR("[regulator]fail to regulator_set_voltage, powertype:%d powerId:%d\n",
						pin,
						regulator_voltage[pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0]);
			}
			if (regulator_enable(pregulator)) {
				PK_PR_ERR("[regulator]fail to regulator_enable, powertype:%d powerId:%d\n",
						pin,
						regulator_voltage[pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0]);
				return IMGSENSOR_RETURN_ERROR;
			}
			atomic_inc(enable_cnt);
			//PK_PR_ERR("regulator_set: powertype=%d, powerId=%d\n", pin, regulator_voltage[pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0]);
		} else {
			if (regulator_is_enabled(pregulator))
				PK_DBG("[regulator]%d is enabled\n", pin);

			if (regulator_disable(pregulator)) {
				PK_PR_ERR("[regulator]fail to regulator_disable, powertype: %d\n", pin);
				return IMGSENSOR_RETURN_ERROR;
			}
			atomic_dec(enable_cnt);
		}
	  	//PK_PR_ERR("regulator_set: powertype=%d, powerId=%d\n", pin, regulator_voltage[pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0]);
	} else {
		PK_PR_ERR("regulator == NULL %d %d %d\n",
				reg_type_offset,
				pin,
				IMGSENSOR_HW_PIN_AVDD);
	}

	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN regulator_dump(void *pinstance)
{
	struct REGULATOR *preg = (struct REGULATOR *)pinstance;
	int i;

	for (i = REGULATOR_TYPE_MAIN_VCAMA; i < REGULATOR_TYPE_MAX_NUM; i++) {
		if (regulator_is_enabled(preg->pregulator[i]) &&
				atomic_read(&preg->enable_cnt[i]) != 0)
			PK_DBG("%s = %d\n", regulator_control[i].pregulator_type,
				regulator_get_voltage(preg->pregulator[i]));
	}

	return IMGSENSOR_RETURN_SUCCESS;
}

static struct IMGSENSOR_HW_DEVICE device = {
	.id        = IMGSENSOR_HW_ID_REGULATOR,
	.pinstance = (void *)&reg_instance,
	.init      = regulator_init,
	.set       = regulator_set,
	.release   = regulator_release,
	.dump      = regulator_dump
};

#ifdef VENDOR_EDIT
/*Femg.Hu@Camera.Driver 20171120 add for flash&lens to use i2c individual*/
int kdVIOPowerOn(int On)
{
	if (On) {
		if (regulator_set_voltage(gVCamIO,
			regulator_voltage[IMGSENSOR_HW_PIN_STATE_LEVEL_1800- IMGSENSOR_HW_PIN_STATE_LEVEL_0],
			regulator_voltage[IMGSENSOR_HW_PIN_STATE_LEVEL_1800- IMGSENSOR_HW_PIN_STATE_LEVEL_0])) {
			PK_PR_ERR("[regulator]fail to regulator_set_voltage, powerId:%d\n",
				regulator_voltage[IMGSENSOR_HW_PIN_STATE_LEVEL_1800 - IMGSENSOR_HW_PIN_STATE_LEVEL_0]);
		}
		if (regulator_enable(gVCamIO)) {
			PK_PR_ERR("[regulator]fail to regulator_enable\n");
			return IMGSENSOR_RETURN_ERROR;
		}
	} else {
		if (regulator_set_voltage(gVCamIO,
			regulator_voltage[IMGSENSOR_HW_PIN_STATE_LEVEL_0],
			regulator_voltage[IMGSENSOR_HW_PIN_STATE_LEVEL_0])) {
			PK_PR_ERR("[regulator]fail to regulator_set_voltage, powerId:%d\n",
				regulator_voltage[IMGSENSOR_HW_PIN_STATE_LEVEL_0]);
		}
		if (regulator_disable(gVCamIO)) {
			PK_DBG("[regulator]fail to regulator_disable gVCamIO\n");
			return IMGSENSOR_RETURN_ERROR;
		}
	}
	return IMGSENSOR_RETURN_SUCCESS;
}

int kdVAFPowerOn(int On)
{
	if (On) {
		if (regulator_set_voltage(gVCamAF,
			regulator_voltage[IMGSENSOR_HW_PIN_STATE_LEVEL_2800- IMGSENSOR_HW_PIN_STATE_LEVEL_0],
			regulator_voltage[IMGSENSOR_HW_PIN_STATE_LEVEL_2800- IMGSENSOR_HW_PIN_STATE_LEVEL_0])) {
			PK_PR_ERR("[regulator]fail to regulator_set_voltage, powerId:%d\n",
				regulator_voltage[IMGSENSOR_HW_PIN_STATE_LEVEL_2800 - IMGSENSOR_HW_PIN_STATE_LEVEL_0]);
		}
		if (regulator_enable(gVCamAF)) {
			PK_PR_ERR("[regulator]fail to regulator_enable\n");
				return IMGSENSOR_RETURN_ERROR;
		}
	} else {
		if (regulator_set_voltage(gVCamAF,
			regulator_voltage[IMGSENSOR_HW_PIN_STATE_LEVEL_0],
			regulator_voltage[IMGSENSOR_HW_PIN_STATE_LEVEL_0])) {
			PK_PR_ERR("[regulator]fail to regulator_set_voltage, powerId:%d\n",
				regulator_voltage[IMGSENSOR_HW_PIN_STATE_LEVEL_0]);
		}

		if (regulator_disable(gVCamAF)) {
			PK_DBG("[regulator]fail to regulator_disable gVCamAF\n");
			return IMGSENSOR_RETURN_ERROR;
		}
	}
	return IMGSENSOR_RETURN_SUCCESS;
}
#endif

enum IMGSENSOR_RETURN imgsensor_hw_regulator_open(
	struct IMGSENSOR_HW_DEVICE **pdevice)
{
	*pdevice = &device;
	return IMGSENSOR_RETURN_SUCCESS;
}

