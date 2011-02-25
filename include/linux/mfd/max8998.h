/*
 * max8698.h - Voltage regulator driver for the Maxim 8998
 *
 *  Copyright (C) 2009-2010 Samsung Electrnoics
 *  Kyungmin Park <kyungmin.park@samsung.com>
 *  Marek Szyprowski <m.szyprowski@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __LINUX_MFD_MAX8998_H
#define __LINUX_MFD_MAX8998_H

#include <linux/regulator/machine.h>

#define MAX8998_N_DVSARM_REGS	4
#define MAX8998_N_DVSINT_REGS	2

/* MAX 8998 regulator ids */
enum {
	MAX8998_LDO2 = 2,
	MAX8998_LDO3,
	MAX8998_LDO4,
	MAX8998_LDO5,
	MAX8998_LDO6,
	MAX8998_LDO7,
	MAX8998_LDO8,
	MAX8998_LDO9,
	MAX8998_LDO10,
	MAX8998_LDO11,
	MAX8998_LDO12,
	MAX8998_LDO13,
	MAX8998_LDO14,
	MAX8998_LDO15,
	MAX8998_LDO16,
	MAX8998_LDO17,
	MAX8998_BUCK1,
	MAX8998_BUCK2,
	MAX8998_BUCK3,
	MAX8998_BUCK4,
	MAX8998_EN32KHZ_AP,
	MAX8998_EN32KHZ_CP,
	MAX8998_ENVICHG,
	MAX8998_ESAFEOUT1,
	MAX8998_ESAFEOUT2,
};

/**
 * max8998_regulator_data - regulator data
 * @id: regulator id
 * @initdata: regulator init data (contraints, supplies, ...)
 */
struct max8998_regulator_data {
	int				id;
	struct regulator_init_data	*initdata;
};

enum cable_type_t {
	CABLE_TYPE_NONE = 0,
	CABLE_TYPE_USB,
	CABLE_TYPE_AC,
};

/**
 * max8998_adc_table_data
 * @adc_value : max8998 adc value
 * @temperature : temperature(C) * 10
 */
struct max8998_adc_table_data {
	int adc_value;
	int temperature;
};
struct max8998_charger_callbacks {
	void (*set_cable)(struct max8998_charger_callbacks *ptr,
		enum cable_type_t status);
};

/**
 * max8998_charger_data - charger data
 * @id: charger id
 * @initdata: charger init data (contraints, supplies, ...)
 * @adc_table: adc_table must be ascending adc value order
 */
struct max8998_charger_data {
	struct power_supply *psy_fuelgauge;
	void (*register_callbacks)(struct max8998_charger_callbacks *ptr);
	struct max8998_adc_table_data *adc_table;
	int adc_array_size;
};

/**
 * struct max8998_board - packages regulator init data
 * @num_regulators: number of regultors used
 * @regulators: array of defined regulators
 */

struct max8998_platform_data {
	int				num_regulators;
	struct max8998_regulator_data	*regulators;
	struct max8998_charger_data	*charger;
	int				buck1_preload[MAX8998_N_DVSARM_REGS];
	int				buck2_preload[MAX8998_N_DVSINT_REGS];
	int				set1_gpio;
	int				set2_gpio;
	int				set3_gpio;
};

#endif /*  __LINUX_MFD_MAX8998_H */
