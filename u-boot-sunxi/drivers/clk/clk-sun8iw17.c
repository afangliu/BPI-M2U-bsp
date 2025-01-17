/*
 * Copyright (C) 2013 Allwinnertech, kevin.z.m <kevin@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "clk-sun8iw17.h"
#include "clk-sun8iw17_tbl.c"
#include<clk/clk_plat.h>
#include"clk_factor.h"
#include"clk_periph.h"
#include<div64.h>

#define FACTOR_SIZEOF(name) (sizeof(factor_pll##name##_tbl)/ \
			     sizeof(struct sunxi_clk_factor_freq))

#define FACTOR_SEARCH(name) (sunxi_clk_com_ftr_sr( \
		&sunxi_clk_factor_pll_##name, factor, \
		factor_pll##name##_tbl, index, \
		FACTOR_SIZEOF(name)))

#ifndef CONFIG_EVB_PLATFORM
	#define LOCKBIT(x) 31
#else
	#define LOCKBIT(x) x
#endif

static DEFINE_SPINLOCK(clk_lock);
void __iomem *sunxi_clk_base;
void __iomem *sunxi_clk_cpus_base;
int sunxi_clk_maxreg = SUNXI_CLK_MAX_REG;
int cpus_clk_maxreg = CPUS_CLK_MAX_REG;

/*                                ns  nw  ks  kw  ms  mw  ps  pw  d1s d1w d2s d2w {frac   out mode}   en-s    sdmss   sdmsw   sdmpat         sdmval*/
SUNXI_CLK_FACTORS(pll_cpu0,       8,  5,  4,  2,  0,  2,  16, 2,  0,  0,  0,  0,    0,    0,  0,      31,     24,     0,      PLL_CPU0PAT,   0xd1303333);
SUNXI_CLK_FACTORS(pll_cpu1,       8,  5,  4,  2,  0,  2,  16, 2,  0,  0,  0,  0,    0,    0,  0,      31,     24,     0,      PLL_CPU1PAT,   0xd1303333);
SUNXI_CLK_FACTORS(pll_ddr0,       8,  7,  0,  0,  0,  2,  0,  0,  0,  0,  0,  0,    0,    0,  0,      31,     0,      0,      PLL_DDR0PAT,   0xf1303333);
SUNXI_CLK_FACTORS(pll_ddr1,       8,  7,  0,  0,  0,  2,  0,  0,  0,  0,  0,  0,    0,    0,  0,      31,     0,      0,      PLL_DDR1PAT,   0xf1303333);
SUNXI_CLK_FACTORS(pll_periph0,    8,  5,  4,  2,  0,  0,  0,  0,  0,  0,  0,  0,    0,    0,  0,      31,     0,      0,      0,             0);
SUNXI_CLK_FACTORS(pll_periph1,    8,  5,  4,  2,  0,  0,  0,  0,  0,  0,  0,  0,    0,    0,  0,      31,     24,     0,      PLL_PERI1PAT,  0xd1303333);
SUNXI_CLK_FACTORS(pll_gpu,        8,  7,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,    1,    25, 26,     31,     24,     0,      PLL_GPUPAT,    0xd1303333);
SUNXI_CLK_FACTORS(pll_video0,     8,  7,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,    1,    25, 26,     31,     24,     0,      PLL_VIDEO0PAT, 0xd1303333);
SUNXI_CLK_FACTORS(pll_video1,     8,  7,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,    1,    25, 26,     31,     24,     0,      PLL_VIDEO1PAT, 0xd1303333);
SUNXI_CLK_FACTORS(pll_ve,         8,  7,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,    1,    25, 26,     31,     24,     0,      PLL_VEPAT,     0xd1303333);
SUNXI_CLK_FACTORS(pll_de,         8,  7,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,    1,    25, 26,     31,     24,     0,      PLL_DEPAT   ,  0xd1303333);
SUNXI_CLK_FACTORS(pll_isp,        8,  7,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,    1,    25, 26,     31,     24,     0,      PLL_ISPPAT ,   0xd1303333);
SUNXI_CLK_FACTORS(pll_hsic,       8,  7,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,    1,    25, 26,     31,     24,     0,      PLL_HSICPAT ,  0xd1303333);
SUNXI_CLK_FACTORS(pll_audio,      8,  7,  0,  0,  0,  5,  16, 4,  0,  0,  0,  0,    0,    0,  0,      31,     24,     1,      PLL_AUDIOPAT,  0xc0010d84);
SUNXI_CLK_FACTORS(pll_mipi,       8,  4,  4,  2,  0,  4,  0,  0,  0,  0,  0,  0,    0,    0,  0,      31,     24,     0,      PLL_MIPIPAT,   0xd1303333);
SUNXI_CLK_FACTORS(pll_eve,        8,  7,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,    1,    26, 25,     31,     24,     0,      PLL_EVEPAT ,   0xd1303333);

static int get_factors_pll_cpu0(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{

	int index;
	u64 tmp_rate;

	if (!factor)
		return -1;
	tmp_rate = rate > pllcpu0_max ? pllcpu0_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(cpu0))
		return -1;

	return 0;
}

static int get_factors_pll_cpu1(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{

	int index;
	u64 tmp_rate;

	if (!factor)
		return -1;
	tmp_rate = rate > pllcpu1_max ? pllcpu1_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(cpu1))
		return -1;

	return 0;
}

static int get_factors_pll_ddr0(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{
	int index;
	u64 tmp_rate;

	if (!factor)
		return -1;

	tmp_rate = rate > pllddr0_max ? pllddr0_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(ddr0))
		return -1;

	return 0;
}

static int get_factors_pll_ddr1(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{
	int index;
	u64 tmp_rate;

	if (!factor)
		return -1;

	tmp_rate = rate > pllddr1_max ? pllddr1_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(ddr1))
		return -1;

	return 0;
}

static int get_factors_pll_periph0(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{
	int index;
	u64 tmp_rate;

	if (!factor)
		return -1;

	tmp_rate = rate > pllperiph0_max ? pllperiph0_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(periph0))
		return -1;

	return 0;
}

static int get_factors_pll_periph1(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{
	int index;
	u64 tmp_rate;

	if (!factor)
		return -1;

	tmp_rate = rate > pllperiph1_max ? pllperiph1_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(periph1))
		return -1;

	return 0;
}

static int get_factors_pll_gpu(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{
	u64 tmp_rate;
	int index;

	if (!factor)
		return -1;

	tmp_rate = rate > pllgpu_max ? pllgpu_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(gpu))
		return -1;
	if (rate == 297000000) {
		factor->frac_mode = 0;
		factor->frac_freq = 1;
		factor->factorm = 0;
	} else if (rate == 270000000) {
		factor->frac_mode = 0;
		factor->frac_freq = 0;
		factor->factorm = 0;
	} else {
		factor->frac_mode = 1;
		factor->frac_freq = 0;
	}

	return 0;
}

static int get_factors_pll_video0(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{
	u64 tmp_rate;
	int index;

	if (!factor)
		return -1;

	tmp_rate = rate > pllvideo0_max ? pllvideo0_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(video0))
		return -1;
	if (rate == 297000000) {
		factor->frac_mode = 0;
		factor->frac_freq = 1;
		factor->factorm = 0;
	} else if (rate == 270000000) {
		factor->frac_mode = 0;
		factor->frac_freq = 0;
		factor->factorm = 0;
	} else {
		factor->frac_mode = 1;
		factor->frac_freq = 0;
	}

	return 0;
}

static int get_factors_pll_video1(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{
	u64 tmp_rate;
	int index;

	if (!factor)
		return -1;

	tmp_rate = rate > pllvideo1_max ? pllvideo1_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(video1))
		return -1;
	if (rate == 297000000) {
		factor->frac_mode = 0;
		factor->frac_freq = 1;
		factor->factorm = 0;
	} else if (rate == 270000000) {
		factor->frac_mode = 0;
		factor->frac_freq = 0;
		factor->factorm = 0;
	} else {
		factor->frac_mode = 1;
		factor->frac_freq = 0;
	}

	return 0;
}

static int get_factors_pll_ve(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{
	u64 tmp_rate;
	int index;

	if (!factor)
		return -1;

	tmp_rate = rate > pllve_max ? pllve_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(ve))
		return -1;
	if (rate == 297000000) {
		factor->frac_mode = 0;
		factor->frac_freq = 1;
		factor->factorm = 0;
	} else if (rate == 270000000) {
		factor->frac_mode = 0;
		factor->frac_freq = 0;
		factor->factorm = 0;
	} else {
		factor->frac_mode = 1;
		factor->frac_freq = 0;
	}

	return 0;
}

static int get_factors_pll_de(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{
	u64 tmp_rate;
	int index;

	if (!factor)
		return -1;

	tmp_rate = rate > pllde_max ? pllde_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(de))
		return -1;
	if (rate == 297000000) {
		factor->frac_mode = 0;
		factor->frac_freq = 1;
		factor->factorm = 0;
	} else if (rate == 270000000) {
		factor->frac_mode = 0;
		factor->frac_freq = 0;
		factor->factorm = 0;
	} else {
		factor->frac_mode = 1;
		factor->frac_freq = 0;
	}

	return 0;
}

static int get_factors_pll_isp(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{
	u64 tmp_rate;
	int index;

	if (!factor)
		return -1;

	tmp_rate = rate > pllisp_max ? pllisp_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(isp))
		return -1;
	if (rate == 297000000) {
		factor->frac_mode = 0;
		factor->frac_freq = 1;
		factor->factorm = 0;
	} else if (rate == 270000000) {
		factor->frac_mode = 0;
		factor->frac_freq = 0;
		factor->factorm = 0;
	} else {
		factor->frac_mode = 1;
		factor->frac_freq = 0;
	}

	return 0;
}

static int get_factors_pll_hsic(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{
	u64 tmp_rate;
	int index;

	if (!factor)
		return -1;

	tmp_rate = rate > pllhsic_max ? pllhsic_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(hsic))
		return -1;
	if (rate == 297000000) {
		factor->frac_mode = 0;
		factor->frac_freq = 1;
		factor->factorm = 0;
	} else if (rate == 270000000) {
		factor->frac_mode = 0;
		factor->frac_freq = 0;
		factor->factorm = 0;
	} else {
		factor->frac_mode = 1;
		factor->frac_freq = 0;
	}

	return 0;
}

static int get_factors_pll_audio(u32 rate, u32 parent_rate, struct clk_factors_value *factor)
{
	if (rate == 22579200) {
		factor->factorn = 6;
		factor->factorm = 0;
		factor->factorp = 7;
		sunxi_clk_factor_pll_audio.sdmval = 0xc0010d84;
	} else if (rate == 24576000) {
		factor->factorn = 13;
		factor->factorm = 0;
		factor->factorp = 13;
		sunxi_clk_factor_pll_audio.sdmval = 0xc000ac02;
	} else if (rate == 45158400) {
		factor->factorn = 6;
		factor->factorm = 0;
		factor->factorp = 3;
		sunxi_clk_factor_pll_audio.sdmval = 0xc0010d84;
	} else if (rate == 49152000) {
		factor->factorn = 13;
		factor->factorm = 0;
		factor->factorp = 6;
		sunxi_clk_factor_pll_audio.sdmval = 0xc000ac02;
	} else
		return -1;

	return 0;
}

static int get_factors_pll_mipi(u32 rate, u32 parent_rate, struct clk_factors_value *factor)
{

	u64 tmp_rate;
	u32 delta1, delta2, want_rate, new_rate, save_rate = 0;
	int n, k, m;

	if (!factor)
		return -1;

	tmp_rate = (rate > 1440000000) ? 1440000000 : rate;
	do_div(tmp_rate, 1000000);
	want_rate = tmp_rate;

	for (m = 1; m <= 16; m++) {
		for (k = 2; k <= 4; k++) {
			for (n = 1; n <= 16; n++) {
				new_rate = (parent_rate / 1000000) * k * n / m;

				delta1 = (new_rate > want_rate)
					? (new_rate - want_rate)
					: (want_rate - new_rate);

				delta2 = (save_rate > want_rate)
					? (save_rate - want_rate)
					: (want_rate - save_rate);

				if (delta1 < delta2) {
					factor->factorn = n - 1;
					factor->factork = k - 1;
					factor->factorm = m - 1;
					save_rate = new_rate;
				}
			}
		}
	}

	return 0;
}

static int get_factors_pll_eve(u32 rate, u32 parent_rate,
		struct clk_factors_value *factor)
{
	u64 tmp_rate;
	int index;

	if (!factor)
		return -1;

	tmp_rate = rate > plleve_max ? plleve_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;

	if (FACTOR_SEARCH(eve))
		return -1;
	if (rate == 297000000) {
		factor->frac_mode = 0;
		factor->frac_freq = 1;
		factor->factorm = 0;
	} else if (rate == 270000000) {
		factor->frac_mode = 0;
		factor->frac_freq = 0;
		factor->factorm = 0;
	} else {
		factor->frac_mode = 1;
		factor->frac_freq = 0;
	}

	return 0;
}

/*    pll_cpux: 24*N*K/(M*P)    */
static unsigned long calc_rate_pll_cpu(u32 parent_rate, struct clk_factors_value *factor)
{
	u64 tmp_rate = (parent_rate ? parent_rate : 24000000);
	tmp_rate = tmp_rate * (factor->factorn + 1) * (factor->factork + 1);
	do_div(tmp_rate, (factor->factorm + 1) * (1 << factor->factorp));
	return (unsigned long)tmp_rate;
}

/*    pll_ddr1: 24*N/M    */
static unsigned long calc_rate_pll_ddr(u32 parent_rate, struct clk_factors_value *factor)
{
	u64 tmp_rate = (parent_rate ? parent_rate : 24000000);
	tmp_rate = tmp_rate * (factor->factorn + 1) ;
	do_div(tmp_rate, factor->factorm + 1);
	return (unsigned long)tmp_rate;
}

/*    pll_periph0:24*N*K/2    */
static unsigned long calc_rate_pll_periph(u32 parent_rate, struct clk_factors_value *factor)
{
	return (unsigned long)(parent_rate ? (parent_rate / 2) : 12000000) * (factor->factorn + 1) * (factor->factork + 1);
}

/*pll_video0/pll_video1/pll_gpu/pll_ve/pll_de/pll_isp/pll_hsic/pll_eve:24*N/M */
static unsigned long calc_rate_pll_media(u32 parent_rate, struct clk_factors_value *factor)
{
	u64 tmp_rate = (parent_rate ? parent_rate : 24000000);
	if (factor->frac_mode == 0) {
		if (factor->frac_freq == 1)
			return 297000000;
		else
			return 270000000;
	} else {
		tmp_rate = tmp_rate * (factor->factorn + 1);
		do_div(tmp_rate, factor->factorm + 1);
		return (unsigned long)tmp_rate;
	}
}

/*    pll_mipi: pll_video0*N*K/M    */
static unsigned long calc_rate_pll_mipi(u32 parent_rate, struct clk_factors_value *factor)
{
	u64 tmp_rate = (parent_rate ? parent_rate : 24000000);
	tmp_rate = tmp_rate * (factor->factorn+1) * (factor->factork+1);
	do_div(tmp_rate, factor->factorm+1);
	return (unsigned long)tmp_rate;
}

/*    pll_audio:24*N/(M*P)    */
static unsigned long calc_rate_pll_audio(u32 parent_rate, struct clk_factors_value *factor)
{
	u64 tmp_rate = (parent_rate ? parent_rate : 24000000);
	if ((factor->factorn == 6) && (factor->factorm == 0) && (factor->factorp == 7))
		return 22579200;
	else if ((factor->factorn == 6) && (factor->factorm == 0) && (factor->factorp == 3))
		return 45158400;
	else if ((factor->factorn == 13) && (factor->factorm == 0) && (factor->factorp == 13))
		return 24576000;
	else if ((factor->factorn == 13) && (factor->factorm == 0) && (factor->factorp == 6))
		return 49152000;
	else {
		tmp_rate = tmp_rate * (factor->factorn+1);
		do_div(tmp_rate, (factor->factorm+1) * (factor->factorp+1));
		return (unsigned long)tmp_rate;
	}
}

static const char *mipi_parents[] = {"pll_video0", ""};
static const char *hosc_parents[] = {"hosc"};
struct clk_ops pll_mipi_ops;
struct factor_init_data sunxi_factos[] = {
	/* name         parent        parent_num, flags                 reg          lock_reg     lock_bit     pll_lock_ctrl_reg lock_en_bit lock_mode       config                         get_factors               calc_rate              priv_ops*/
	{"pll_cpu0",    hosc_parents, 1,          CLK_GET_RATE_NOCACHE, PLL_CPU0,    PLL_CPU0,    LOCKBIT(28), PLL_CPU0,    29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_cpu0,    &get_factors_pll_cpu0,    &calc_rate_pll_cpu,    (struct clk_ops *)NULL},
	{"pll_cpu1",    hosc_parents, 1,          CLK_GET_RATE_NOCACHE, PLL_CPU1,    PLL_CPU1,    LOCKBIT(28), PLL_CPU1,    29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_cpu1,    &get_factors_pll_cpu1,    &calc_rate_pll_cpu,    (struct clk_ops *)NULL},
	{"pll_ddr0",    hosc_parents, 1,          CLK_GET_RATE_NOCACHE, PLL_DDR0,    PLL_DDR0,    LOCKBIT(28), PLL_DDR0,    29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_ddr0,    &get_factors_pll_ddr0,    &calc_rate_pll_ddr,    (struct clk_ops *)NULL},
	{"pll_ddr1",    hosc_parents, 1,          CLK_GET_RATE_NOCACHE, PLL_DDR1,    PLL_DDR1,    LOCKBIT(28), PLL_DDR1,    29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_ddr1,    &get_factors_pll_ddr1,    &calc_rate_pll_ddr,    (struct clk_ops *)NULL},
	{"pll_periph0", hosc_parents, 1,          0,                    PLL_PERIPH0, PLL_PERIPH0, LOCKBIT(28), PLL_PERIPH0, 29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_periph0, &get_factors_pll_periph0, &calc_rate_pll_periph, (struct clk_ops *)NULL},
	{"pll_periph1", hosc_parents, 1,          0,                    PLL_PERIPH1, PLL_PERIPH1, LOCKBIT(28), PLL_PERIPH1, 29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_periph1, &get_factors_pll_periph1, &calc_rate_pll_periph, (struct clk_ops *)NULL},
	{"pll_gpu",     hosc_parents, 1,          0,                    PLL_GPU,     PLL_GPU,     LOCKBIT(28), PLL_GPU,     29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_gpu,     &get_factors_pll_gpu,     &calc_rate_pll_media,  (struct clk_ops *)NULL},
	{"pll_video0",  hosc_parents, 1,          0,                    PLL_VIDEO0,  PLL_VIDEO0,  LOCKBIT(28), PLL_VIDEO0,  29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_video0,  &get_factors_pll_video0,  &calc_rate_pll_media,  (struct clk_ops *)NULL},
	{"pll_video1",  hosc_parents, 1,          0,                    PLL_VIDEO1,  PLL_VIDEO1,  LOCKBIT(28), PLL_VIDEO1,  29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_video1,  &get_factors_pll_video1,  &calc_rate_pll_media,  (struct clk_ops *)NULL},
	{"pll_ve",      hosc_parents, 1,          0,                    PLL_VE,      PLL_VE,      LOCKBIT(28), PLL_VE,      29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_ve,      &get_factors_pll_ve,      &calc_rate_pll_media,  (struct clk_ops *)NULL},
	{"pll_de",      hosc_parents, 1,          0,                    PLL_DE,      PLL_DE,      LOCKBIT(28), PLL_DE,      29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_de,      &get_factors_pll_de,      &calc_rate_pll_media,  (struct clk_ops *)NULL},
	{"pll_isp",     hosc_parents, 1,          0,                    PLL_ISP,     PLL_ISP,     LOCKBIT(28), PLL_ISP,     29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_isp,     &get_factors_pll_isp,     &calc_rate_pll_media,  (struct clk_ops *)NULL},
	{"pll_hsic",    hosc_parents, 1,          0,                    PLL_HSIC,    PLL_HSIC,    LOCKBIT(28), PLL_HSIC,    29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_hsic,    &get_factors_pll_hsic,    &calc_rate_pll_media,  (struct clk_ops *)NULL},
	{"pll_audio",   hosc_parents, 1,          0,                    PLL_AUDIO,   PLL_AUDIO,   LOCKBIT(28), PLL_AUDIO,   29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_audio,   &get_factors_pll_audio,   &calc_rate_pll_audio,  (struct clk_ops *)NULL},
	{"pll_mipi",    mipi_parents, 1,          0,                    PLL_MIPI,    PLL_MIPI,    LOCKBIT(28), PLL_MIPI,    29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_mipi,    &get_factors_pll_mipi,    &calc_rate_pll_mipi,   &pll_mipi_ops         },
	{"pll_eve",     hosc_parents, 1,          0,                    PLL_EVE,     PLL_EVE,     LOCKBIT(28), PLL_EVE,     29,          PLL_LOCK_NEW_MODE, &sunxi_clk_factor_pll_eve,     &get_factors_pll_eve,     &calc_rate_pll_media,  (struct clk_ops *)NULL},
};

static const char *cluster0_parents[] = {"hosc", "losc", "iosc", "pll_cpu0"};
static const char *cluster1_parents[] = {"hosc", "losc", "iosc", "pll_cpu1"};
static const char *axi0_parents[] = {"cluster0"};
static const char *axi1_parents[] = {"cluster1"};
static const char *cpuapb0_parents[] = {"cluster0"};
static const char *cpuapb1_parents[] = {"cluster1"};
static const char *psi_parents[] = {"hosc", "losc", "iosc", "pll_periph0"};
static const char *ahb1_parents[] = {"psi"};
static const char *ahb2_parents[] = {"psi"};
static const char *ahb3_parents[] = {"hosc", "losc", "psi", "pll_periph0"};
static const char *apb1_parents[] = {"hosc", "losc", "psi", "pll_periph0"};
static const char *apb2_parents[] = {"hosc", "losc", "psi", "pll_periph0"};
static const char *cci400_parents[] = {"hosc", "losc", "pll_periph0x2", "pll_periph1x2", "pll_ddr1", "", "", ""};
static const char *mbus_parents[] = {"hosc", "pll_periph0x2", "pll_ddr0", "pll_ddr1"};
static const char *de_parents[] = {"pll_de", "pll_periph0x2"};
static const char *g2d_parents[] = {"pll_de", "pll_periph0x2"};
static const char *di_parents[] = {"pll_periph0", "pll_periph1"};
static const char *eve_parents[] = {"pll_eve", ""};
static const char *gpu_parents[] = {"pll_gpu", ""};
static const char *ce_parents[] = {"hosc", "pll_periph0x2"};
static const char *ve_parents[] = {"pll_ve", ""};
static const char *ahb1mod_parents[] = {"ahb1"};
static const char *ahb3mod_parents[] = {"ahb3"};
static const char *apb1mod_parents[] = {"apb1"};
static const char *apb2mod_parents[] = {"apb2"};
static const char *sdram_parents[] = {"pll_ddr0", "pll_ddr1", "", ""};
static const char *nand_parents[] = {"hosc", "pll_periph0", "pll_periph1", "pll_periph0x2", "pll_periph1x2", "", "", ""};
static const char *smhc_parents[] = {"hosc", "pll_periph0x2", "pll_periph1x2", ""};
static const char *spi_parents[] = {"hosc", "pll_periph0", "pll_periph1", "pll_periph0x2", "pll_periph1x2", "", "", ""};
static const char *ts_parents[] = {"hosc", "pll_periph0"};
static const char *audio_parents[] = {"pll_audio", "pll_audiox2", "pll_audiox4", "pll_audiox8"};
static const char *usbohci_12m_parents[] = {"osc48md4", "hoscd2", "losc", ""};
static const char *mipi_host_parents[] = {"pll_periph0", "pll_periph0x2"};
static const char *tcon_lcd_parents[] = {"pll_video0", "pll_video0x2", "pll_video1", "pll_video1x2", "pll_mipi", "", "", ""};
static const char *lvds_parents[] = {"hosc"};
static const char *tve_parents[] = {"pll_video0", "pll_video0x2", "pll_video1", "pll_video1x2", "", "", "pll_mipi", ""};
static const char *tvd_parents[] = {"pll_video0", "pll_video0x2", "pll_video1", "pll_video1x2", "", "", "", ""};
static const char *csi_top_parents[] = {"pll_video0", "pll_isp", "pll_ve", "pll_periph0", "", "", "", ""};
static const char *csi_master_parents[] = {"hosc", "pll_video0", "pll_periph0x2", "pll_periph1", "pll_isp", "", "", ""};
static const char *mipi_rx_parents[] = {"pll_video0", "pll_periph0", "pll_isp", ""};
static const char *cpurcpus_pll_parents[] = {"pll_periph0"};
static const char *cpurcpus_parents[] = {"hosc", "losc", "iosc", "cpurcpus_pll"};
static const char *cpurahbs_parents[] = {"cpurcpus"};
static const char *cpurapbs1_parents[] = {"cpurahbs"};
static const char *cpurapbs2_pll_parents[] = {"pll_periph0"};
static const char *cpurapbs2_parents[] = {"hosc", "losc", "iosc", "cpurapbs2_pll"};
static const char *apbs1mod_parents[] = {"cpurapbs1"};
static const char *apbs2mod_parents[] = {"cpurapbs2"};
static const char *cpurvm_ths_parents[] = {"losc", "hosc"};
static const char *vm_ths_parents[] = {"cpur_vm_ths"};
static const char *cpurcir_parents[] = {"losc", "hosc"};
static const char *cpurspi_parents[] = {"hosc", "pll_periph0", "pll_periph1", "pll_periph0x2", "pll_periph1x2", "iosc", "losc", ""};
static const char *losc_parents[] = {"losc"};

struct sunxi_clk_comgate com_gates[] = {
{"nand",    0, 0x3,   BUS_GATE_SHARE|RST_GATE_SHARE|MBUS_GATE_SHARE, 0},
{"codec",   0, 0x3,   BUS_GATE_SHARE|RST_GATE_SHARE,                 0},
{"csi",     0, 0xff, BUS_GATE_SHARE|RST_GATE_SHARE|MBUS_GATE_SHARE, 0},
{"tvd",     0, 0xf,   MBUS_GATE_SHARE,                               0},
};

/*
SUNXI_CLK_PERIPH(name,           mux_reg,         mux_sft, mux_wid,      div_reg,            div_msft,  div_mwid,   div_nsft,   div_nwid,   gate_flag,  en_reg,          rst_reg,         bus_gate_reg,  drm_gate_reg,  en_sft,     rst_sft,    bus_gate_sft,   dram_gate_sft, lock,      com_gate,         com_gate_off)
*/
SUNXI_CLK_PERIPH(cluster0,       CPU0_CFG,        24,      2,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cluster1,       CPU1_CFG,        24,      2,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(axi0,           0,               0,       0,            CPU0_CFG,           0,         2,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(axi1,           0,               0,       0,            CPU1_CFG,           0,         2,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpuapb0,        0,               0,       0,            CPU0_CFG,           8,         2,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpuapb1,        0,               0,       0,            CPU1_CFG,           8,         2,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(psi,            PSI_CFG,         24,      2,            PSI_CFG,            0,         2,          8,          2,          0,          0,               PSI_GATE,        PSI_GATE,      0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(ahb1,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(ahb2,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(ahb3,           AHB3_CFG,        24,      2,            AHB3_CFG,           0,         2,          8,          2,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(apb1,           APB1_CFG,        24,      2,            APB1_CFG,           0,         2,          8,          2,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(apb2,           APB2_CFG,        24,      2,            APB2_CFG,           0,         2,          8,          2,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cci400,         CCI400_CFG,      24,      3,            CCI400_CFG,         0,         2,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(mbus,           MBUS_CFG,        24,      2,            MBUS_CFG,           0,         3,          0,          0,          0,          MBUS_CFG,        MBUS_CFG,        0,             0,             31,         30,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(de,             DE_CFG,          24,      1,            DE_CFG,             0,         4,          0,          0,          0,          DE_CFG,          DE_GATE,         DE_GATE,       0,             31,         16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(di,             DI_CFG,          24,      1,            DI_CFG,             0,         4,          0,          0,          0,          DI_CFG,          DI_GATE,         DI_GATE,       MBUS_GATE,     31,         16,         0,              11,            &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(g2d,            G2D_CFG,         24,      1,            G2D_CFG,            0,         4,          0,          0,          0,          G2D_CFG,         G2D_GATE,        G2D_GATE,      MBUS_GATE,     31,         16,         0,              10,            &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(eve,            EVE_CFG,         24,      1,            EVE_CFG,            0,         4,          8,          2,          0,          EVE_CFG,         EVE_GATE,        EVE_GATE,      MBUS_GATE,     31,         16,         0,             21,            &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(gpu,            GPU_CFG,         24,      1,            GPU_CFG,            0,         3,          0,          0,          0,          GPU_CFG,         GPU_GATE,        GPU_GATE,      0,             31,         16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(ce,             CE_CFG,          24,      1,            CE_CFG,             0,         4,          8,          2,          0,          CE_CFG,          CE_GATE,         CE_GATE,       MBUS_GATE,     31,         16,         0,              2,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(ve,             VE_CFG,          24,      1,            VE_CFG,             0,         3,          0,          0,          0,          VE_CFG,          VE_GATE,         VE_GATE,       MBUS_GATE,     31,         16,         0,              1,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(dma,            0,                0,      0,            0,                  0,         0,          0,          0,          0,          0,               DMA_GATE,        DMA_GATE,      MBUS_GATE,     0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(msgbox,         0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               MSGBOX_GATE,     MSGBOX_GATE,   0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(hwspinlock_rst, 0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               SPINLOCK_GATE,   0,             0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(hwspinlock_bus, 0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               SPINLOCK_GATE, 0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(hstimer,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               HSTIMER_GATE,    HSTIMER_GATE,  0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(avs,            0,               0,       0,            0,                  0,         0,          0,          0,          0,          AVS_CFG,         0,               0,             0,             31,         0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(dbgsys,         0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               DBGSYS_GATE,     DBGSYS_GATE,   0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(pwm,            0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               PWM_GATE,        PWM_GATE,      0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdram,          DRAM_CFG,        24,      2,            DRAM_CFG,           0,         2,          0,          0,          0,          0,               DRAM_GATE,       DRAM_GATE,     0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(nand0,          NAND0_CFG,       24,      3,            NAND0_CFG,          0,         4,          8,          2,          0,          NAND0_CFG,       NAND_GATE,      NAND_GATE,      MBUS_GATE,     31,         16,         0,              5,             &clk_lock, &com_gates[0],    0);
SUNXI_CLK_PERIPH(nand1,          NAND1_CFG,       24,      3,            NAND1_CFG,          0,         4,          8,          2,          0,          NAND1_CFG,       NAND_GATE,      NAND_GATE,      MBUS_GATE,     31,         16,         0,              5,             &clk_lock, &com_gates[0],    1);
SUNXI_CLK_PERIPH(sdmmc0_mod,     SMHC0_CFG,       24,      2,            SMHC0_CFG,          0,         4,          8,          2,          0,          SMHC0_CFG,       0,              0,              0,             31,         0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc0_rst,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               SMHC_GATE,      0,              0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc0_bus,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,              SMHC_GATE,      0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc1_mod,     SMHC1_CFG,       24,      2,            SMHC1_CFG,          0,         4,          8,          2,          0,          SMHC1_CFG,       0,              0,              0,             31,         0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc1_rst,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               SMHC_GATE,      0,              0,             0,          17,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc1_bus,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,              SMHC_GATE,      0,             0,          0,          1,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc2_mod,     SMHC2_CFG,       24,      2,            SMHC2_CFG,          0,         4,          8,          2,          0,          SMHC2_CFG,       0,              0,              0,             31,         0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc2_rst,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               SMHC_GATE,      0,              0,             0,          18,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc2_bus,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,              SMHC_GATE,      0,             0,          0,          2,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc3_mod,     SMHC3_CFG,       24,      2,            SMHC3_CFG,          0,         4,          8,          2,          0,          SMHC3_CFG,       0,              0,              0,             31,         0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc3_rst,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               SMHC_GATE,      0,              0,             0,          19,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(sdmmc3_bus,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,              SMHC_GATE,      0,             0,          0,          3,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(uart0,          0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               UART_GATE,      UART_GATE,      0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(uart1,          0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               UART_GATE,      UART_GATE,      0,             0,          17,         1,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(uart2,          0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               UART_GATE,      UART_GATE,      0,             0,          18,         2,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(uart3,          0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               UART_GATE,      UART_GATE,      0,             0,          19,         3,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(uart4,          0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               UART_GATE,      UART_GATE,      0,             0,          20,         4,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(twi0,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               TWI_GATE,       TWI_GATE,       0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(twi1,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               TWI_GATE,       TWI_GATE,       0,             0,          17,         1,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(twi2,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               TWI_GATE,       TWI_GATE,       0,             0,          18,         2,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(twi3,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               TWI_GATE,       TWI_GATE,       0,             0,          19,         3,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(twi4,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               TWI_GATE,       TWI_GATE,       0,             0,          20,         4,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(twi5,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               TWI_GATE,       TWI_GATE,       0,             0,          21,         5,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(twi6,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               TWI_GATE,       TWI_GATE,       0,             0,          22,         6,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(can,            0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CAN_GATE,       CAN_GATE,       0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(scr,            0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               SCR_GATE,       SCR_GATE,       0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(spi0,           SPI0_CFG,        24,      3,            SPI0_CFG,           0,         4,          8,          2,          0,          SPI0_CFG,        SPI_GATE,       SPI_GATE,       0,             31,         16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(spi1,           SPI1_CFG,        24,      3,            SPI1_CFG,           0,         4,          8,          2,          0,          SPI1_CFG,        SPI_GATE,       SPI_GATE,       0,             31,         17,         1,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(gmac,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               GMAC_GATE,      GMAC_GATE,      0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(ts,             TS_CFG,          24,      1,            TS_CFG,             0,         4,          8,          2,          0,          TS_CFG,          TS_GATE,         TS_GATE,       MBUS_GATE,     31,         16,         0,              3,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(gpadc,          0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               GPADC_GATE,     GPADC_GATE,     0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(i2s0,           I2S0_CFG,        24,      2,            I2S0_CFG,           0,         0,          8,          2,          0,          I2S0_CFG,        I2S_GATE,        I2S_GATE,      0,             31,         16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(i2s1,           I2S1_CFG,        24,      2,            I2S1_CFG,           0,         0,          8,          2,          0,          I2S1_CFG,        I2S_GATE,        I2S_GATE,      0,             31,         17,         1,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(i2s2,           I2S2_CFG,        24,      2,            I2S2_CFG,           0,         0,          8,          2,          0,          I2S2_CFG,        I2S_GATE,        I2S_GATE,      0,             31,         18,         2,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(spdif,          SPDIF_CFG,       24,      2,            SPDIF_CFG,          0,         0,          8,          2,          0,          SPDIF_CFG,       SPDIF_GATE,      SPDIF_GATE,    0,             31,         16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(dmic,           DMIC_CFG,        24,      2,            DMIC_CFG,           0,         0,          8,          2,          0,          DMIC_CFG,        DMIC_GATE,       DMIC_GATE,     0,             31,         16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(codec_1x,       CODEC_1X_CFG,    24,      2,            CODEC_1X_CFG,       0,         4,          0,          0,          0,          CODEC_1X_CFG,    CODEC_GATE,      CODEC_GATE,    0,             31,         16,         0,              0,             &clk_lock, &com_gates[1],    0);
SUNXI_CLK_PERIPH(codec_4x,       CODEC_4X_CFG,    24,      2,            CODEC_4X_CFG,       0,         4,          0,          0,          0,          CODEC_4X_CFG,    CODEC_GATE,      CODEC_GATE,    0,             31,         16,         0,              0,             &clk_lock, &com_gates[1],    1);
SUNXI_CLK_PERIPH(usbphy0,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          USB0_CFG,        USB0_CFG,        0,             0,             29,         30,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbphy1,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          USB1_CFG,        USB1_CFG,        0,             0,             29,         30,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbphy2,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          USB2_CFG,        USB2_CFG,        0,             0,             29,         30,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbphy3,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          USB3_CFG,        USB3_CFG,        0,             0,             29,         30,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbohci0,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          USB0_CFG,        USB_GATE,        USB_GATE,      0,             31,         16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbohci1,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          USB1_CFG,        USB_GATE,        USB_GATE,      0,             31,         17,         1,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbohci2,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          USB2_CFG,        USB_GATE,        USB_GATE,      0,             31,         18,         2,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbohci3,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          USB3_CFG,        USB_GATE,        USB_GATE,      0,             31,         19,         3,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbehci0,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               USB_GATE,        USB_GATE,      0,             0,          20,         4,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbehci1,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               USB_GATE,        USB_GATE,      0,             0,          21,         5,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbehci2,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               USB_GATE,        USB_GATE,      0,             0,          22,         6,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbehci3,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               USB_GATE,        USB_GATE,      0,             0,          23,         7,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbotg,         0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               USB_GATE,        USB_GATE,      0,             0,          24,         8,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbohci0_12m,   USB0_CFG,        24,      2,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbohci1_12m,   USB1_CFG,        24,      2,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbohci2_12m,   USB2_CFG,        24,      2,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(usbohci3_12m,   USB3_CFG,        24,      2,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(mipi_host,      MIPI_DSI_HOST_CFG, 24,    1,            MIPI_DSI_HOST_CFG,  0,         4,          0,          0,          0,          MIPI_DSI_HOST_CFG, MIPI_GATE,     MIPI_GATE,     0,             31,         16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(display_top,    0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               DISPLAY_TOP_GATE, DISPLAY_TOP_GATE, 0,         0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(tcon_lcd0,      TCON_LCD0_CFG,   24,      3,            0,                  0,         0,          0,          0,          0,          TCON_LCD0_CFG,   TCON_LCD_GATE,   TCON_LCD_GATE, 0,             31,         16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(tcon_lcd1,      TCON_LCD1_CFG,   24,      3,            0,                  0,         0,          0,          0,          0,          TCON_LCD1_CFG,   TCON_LCD_GATE,   TCON_LCD_GATE, 0,             31,         17,         1,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(tcon_tv,                    0,    0,      0,            0,                  0,         0,          0,          0,          0,                      0,   TCON_TV_GATE,    TCON_TV_GATE,  0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(lvds,                       0,    0,      0,            0,                  0,         0,          0,          0,          0,                      0,   LVDS_GATE,       0,             0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(tve_top, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, TVE_GATE, TVE_GATE, 0, 0, 16, 0, 0, &clk_lock, NULL, 0);
SUNXI_CLK_PERIPH(tve0, TVE_CFG, 24, 3, TVE_CFG, 0, 4, 8, 2, 0, TVE_CFG, TVE_GATE, TVE_GATE, 0, 31, 17, 1, 0, &clk_lock, NULL, 0);
SUNXI_CLK_PERIPH(tvd_top, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, TVD_GATE, TVD_GATE,
			MBUS_GATE, 0, 16, 0, 7, &clk_lock, NULL, 0);
SUNXI_CLK_PERIPH(tvd0,           TVD0_CFG,        24,      3,            TVD0_CFG,           0,         4,          8,          2,          0,          TVD0_CFG,        TVD_GATE,        TVD_GATE,      MBUS_GATE,     31,         17,         1,              7,             &clk_lock, &com_gates[3],    0);
SUNXI_CLK_PERIPH(tvd1,           TVD1_CFG,        24,      3,            TVD1_CFG,           0,         4,          8,          2,          0,          TVD1_CFG,        TVD_GATE,        TVD_GATE,      MBUS_GATE,     31,         18,         2,              7,             &clk_lock, &com_gates[3],    1);
SUNXI_CLK_PERIPH(tvd2,           TVD2_CFG,        24,      3,            TVD2_CFG,           0,         4,          8,          2,          0,          TVD2_CFG,        TVD_GATE,        TVD_GATE,      MBUS_GATE,     31,         19,         3,              7,             &clk_lock, &com_gates[3],    2);
SUNXI_CLK_PERIPH(tvd3,           TVD3_CFG,        24,      3,            TVD3_CFG,           0,         4,          8,          2,          0,          TVD3_CFG,        TVD_GATE,        TVD_GATE,      MBUS_GATE,     31,         20,         4,              7,             &clk_lock, &com_gates[3],    3);
SUNXI_CLK_PERIPH(csi_cci0,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          CSI_MISC_CFG,    CSI_GATE,        CSI_GATE,      MBUS_GATE,     0,          16,         0,              8,             &clk_lock, &com_gates[2],    0);
SUNXI_CLK_PERIPH(csi_cci1,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          CSI_MISC_CFG,    CSI_GATE,        CSI_GATE,      MBUS_GATE,     1,          16,         0,              8,             &clk_lock, &com_gates[2],    1);
SUNXI_CLK_PERIPH(csi_cci2,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          CSI_MISC_CFG,    CSI_GATE,        CSI_GATE,      MBUS_GATE,     2,          16,         0,              8,             &clk_lock, &com_gates[2],    2);
SUNXI_CLK_PERIPH(csi_cci3,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          CSI_MISC_CFG,    CSI_GATE,        CSI_GATE,      MBUS_GATE,     3,          16,         0,              8,             &clk_lock, &com_gates[2],    3);
SUNXI_CLK_PERIPH(csi_top,        CSI_TOP_CFG,     24,      3,            CSI_TOP_CFG,        0,         4,          0,          0,          0,          CSI_TOP_CFG,     CSI_GATE,        CSI_GATE,      MBUS_GATE,     31,         16,         0,              8,             &clk_lock, &com_gates[2],    4);
SUNXI_CLK_PERIPH(csi_master0,    CSI_MASTER0_CFG, 24,      3,            CSI_MASTER0_CFG,    0,         5,          0,          0,          0,          CSI_MASTER0_CFG, CSI_GATE,        CSI_GATE,      MBUS_GATE,     31,         16,         0,              8,             &clk_lock, &com_gates[2],    5);
SUNXI_CLK_PERIPH(csi_master1,    CSI_MASTER1_CFG, 24,      3,            CSI_MASTER1_CFG,    0,         5,          0,          0,          0,          CSI_MASTER1_CFG, CSI_GATE,        CSI_GATE,      MBUS_GATE,     31,         16,         0,              8,             &clk_lock, &com_gates[2],    6);
SUNXI_CLK_PERIPH(csi_master2,    CSI_MASTER2_CFG, 24,      3,            CSI_MASTER2_CFG,    0,         5,          0,          0,          0,          CSI_MASTER2_CFG, CSI_GATE,        CSI_GATE,      MBUS_GATE,     31,         16,         0,              8,             &clk_lock, &com_gates[2],    7);
SUNXI_CLK_PERIPH(csi_master3,    CSI_MASTER3_CFG, 24,      3,            CSI_MASTER3_CFG,    0,         5,          0,          0,          0,          CSI_MASTER3_CFG, CSI_GATE,        CSI_GATE,      MBUS_GATE,     31,         16,         0,              8,             &clk_lock, &com_gates[2],    8);
SUNXI_CLK_PERIPH(isp,            0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             MBUS_GATE,     0,          0,          0,              9,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(mipi_rx,        MIPI_RX_CFG,     24,      2,            MIPI_RX_CFG,        0,         4,          0,          0,          0,          MIPI_RX_CFG,     0,               0,             0,             31,         0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(pio,            0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurcpus_pll,   0,               0,       0,            CPUS_CFG,           0,         5,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurcpus,       CPUS_CFG,        24,      2,            CPUS_CFG,           0,         0,          8,          2,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurahbs,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurapbs1,      0,               0,       0,            CPUS_APBS1_CFG,     0,         2,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurapbs2_pll,  0,               0,       0,            CPUS_APBS2_CFG,     0,         5,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurapbs2,      CPUS_APBS2_CFG,  24,      2,            CPUS_APBS2_CFG,     0,         0,          8,          2,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurtimer,      0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CPUS_TIMER_GATE, CPUS_TIMER_GATE, 0,           0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurtwdog,      0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CPUS_TWDOG_GATE, CPUS_TWDOG_GATE, 0,           0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurpwm,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CPUS_PWM_GATE,   CPUS_PWM_GATE, 0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpur_vm_ths,    CPUS_VM_CFG,     24,      1,            CPUS_VM_CFG,        0,         5,          8,          2,          0,          CPUS_VM_CFG,     0,               0,             0,             31,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurvm,         0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CPUS_VM_GATE,    CPUS_VM_GATE,  0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurths,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CPUS_THS_GATE,   CPUS_THS_GATE, 0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurlradc,      0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CPUS_LRADC_GATE, CPUS_LRADC_GATE, 0,           0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpuruart0,      0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CPUS_UART_GATE,  CPUS_UART_GATE, 0,            0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpuruart1,      0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CPUS_UART_GATE,  CPUS_UART_GATE, 0,            0,          17,         1,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpuruart2,      0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CPUS_UART_GATE,  CPUS_UART_GATE, 0,            0,          18,         2,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpuruart3,      0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CPUS_UART_GATE,  CPUS_UART_GATE, 0,            0,          19,         3,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpuruart4,      0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CPUS_UART_GATE,  CPUS_UART_GATE, 0,            0,          20,         4,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurtwi0,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CPUS_TWI_GATE,   CPUS_TWI_GATE, 0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurtwi1,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CPUS_TWI_GATE,   CPUS_TWI_GATE, 0,             0,          17,         1,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurtwi2,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CPUS_TWI_GATE,   CPUS_TWI_GATE, 0,             0,          18,         2,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurcan,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CPUS_CAN_GATE,   CPUS_CAN_GATE, 0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurrsb,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CPUS_RSB_GATE,   CPUS_RSB_GATE, 0,             0,          16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurcir,        CPUS_CIR_CFG,    24,      1,            CPUS_CIR_CFG,       0,         5,          8,          2,          0,          CPUS_CIR_CFG,    CPUS_CIR_GATE,   CPUS_CIR_GATE, 0,             31,         16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurspi,        CPUS_SPI_CFG,    24,      1,            CPUS_SPI_CFG,       0,         4,          8,          2,          0,          CPUS_SPI_CFG,    CPUS_SPI_GATE,   CPUS_SPI_GATE, 0,             31,         16,         0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurrtc,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               CPUS_RSB_GATE, 0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(cpurpio,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,             &clk_lock, NULL,             0);
SUNXI_CLK_PERIPH(losc_out,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               LOSC_OUT_GATE, 0,             0,          0,          0,              0,             &clk_lock, NULL,             0);

struct periph_init_data sunxi_periphs_init[] = {
	{"cluster0",       CLK_GET_RATE_NOCACHE, cluster0_parents,       ARRAY_SIZE(cluster0_parents),       &sunxi_clk_periph_cluster0         },
	{"cluster1",       CLK_GET_RATE_NOCACHE, cluster1_parents,       ARRAY_SIZE(cluster1_parents),       &sunxi_clk_periph_cluster1         },
	{"axi0",           0,                    axi0_parents,            ARRAY_SIZE(axi0_parents),            &sunxi_clk_periph_axi0             },
	{"axi1",           0,                    axi1_parents,            ARRAY_SIZE(axi1_parents),            &sunxi_clk_periph_axi1             },
	{"cpuapb0",        0,                    cpuapb0_parents,         ARRAY_SIZE(cpuapb0_parents),         &sunxi_clk_periph_cpuapb0          },
	{"cpuapb1",        0,                    cpuapb1_parents,         ARRAY_SIZE(cpuapb1_parents),         &sunxi_clk_periph_cpuapb1          },
	{"psi",            0,                    psi_parents,            ARRAY_SIZE(psi_parents),            &sunxi_clk_periph_psi              },
	{"ahb1",           0,                    ahb1_parents,           ARRAY_SIZE(ahb1_parents),           &sunxi_clk_periph_ahb1             },
	{"ahb2",           0,                    ahb2_parents,           ARRAY_SIZE(ahb2_parents),           &sunxi_clk_periph_ahb2             },
	{"ahb3",           0,                    ahb3_parents,           ARRAY_SIZE(ahb3_parents),           &sunxi_clk_periph_ahb3             },
	{"apb1",           0,                    apb1_parents,           ARRAY_SIZE(apb1_parents),           &sunxi_clk_periph_apb1             },
	{"apb2",           0,                    apb2_parents,           ARRAY_SIZE(apb2_parents),           &sunxi_clk_periph_apb2             },
	{"cci400",         0,                    cci400_parents,         ARRAY_SIZE(cci400_parents),         &sunxi_clk_periph_cci400           },
	{"mbus",           0,                    mbus_parents,           ARRAY_SIZE(mbus_parents),           &sunxi_clk_periph_mbus             },
	{"de",             0,                    de_parents,             ARRAY_SIZE(de_parents),             &sunxi_clk_periph_de               },
	{"di",             0,                    di_parents,             ARRAY_SIZE(di_parents),             &sunxi_clk_periph_di               },
	{"g2d",            0,                    g2d_parents,            ARRAY_SIZE(g2d_parents),            &sunxi_clk_periph_g2d              },
	{"eve",            0,                    eve_parents,            ARRAY_SIZE(eve_parents),            &sunxi_clk_periph_eve              },
	{"gpu",            0,                    gpu_parents,            ARRAY_SIZE(gpu_parents),            &sunxi_clk_periph_gpu              },
	{"ce",             0,                    ce_parents,             ARRAY_SIZE(ce_parents),             &sunxi_clk_periph_ce               },
	{"ve",             0,                    ve_parents,             ARRAY_SIZE(ve_parents),             &sunxi_clk_periph_ve               },
	{"dma",            0,                    ahb1mod_parents,        ARRAY_SIZE(ahb1mod_parents),        &sunxi_clk_periph_dma              },
	{"msgbox",         0,                    ahb1mod_parents,        ARRAY_SIZE(ahb1mod_parents),        &sunxi_clk_periph_msgbox           },
	{"hwspinlock_rst", 0,                    ahb1mod_parents,        ARRAY_SIZE(ahb1mod_parents),        &sunxi_clk_periph_hwspinlock_rst   },
	{"hwspinlock_bus", 0,                    ahb1mod_parents,        ARRAY_SIZE(ahb1mod_parents),        &sunxi_clk_periph_hwspinlock_bus   },
	{"hstimer",        0,                    ahb1mod_parents,        ARRAY_SIZE(ahb1mod_parents),        &sunxi_clk_periph_hstimer          },
	{"avs",            0,                    hosc_parents,           ARRAY_SIZE(hosc_parents),           &sunxi_clk_periph_avs              },
	{"dbgsys",         0,                    ahb1mod_parents,        ARRAY_SIZE(ahb1mod_parents),        &sunxi_clk_periph_dbgsys           },
	{"pwm",            0,                    apb1mod_parents,        ARRAY_SIZE(apb1mod_parents),        &sunxi_clk_periph_pwm              },
	{"sdram",          0,                    sdram_parents,          ARRAY_SIZE(sdram_parents),          &sunxi_clk_periph_sdram            },
	{"nand0",          0,                    nand_parents,           ARRAY_SIZE(nand_parents),           &sunxi_clk_periph_nand0            },
	{"nand1",          0,                    nand_parents,           ARRAY_SIZE(nand_parents),           &sunxi_clk_periph_nand1            },
	{"sdmmc0_mod",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc0_mod       },
	{"sdmmc0_rst",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc0_rst       },
	{"sdmmc0_bus",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc0_bus       },
	{"sdmmc1_mod",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc1_mod       },
	{"sdmmc1_rst",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc1_rst       },
	{"sdmmc1_bus",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc1_bus       },
	{"sdmmc2_mod",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc2_mod       },
	{"sdmmc2_rst",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc2_rst       },
	{"sdmmc2_bus",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc2_bus       },
	{"sdmmc3_mod",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc3_mod       },
	{"sdmmc3_rst",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc3_rst       },
	{"sdmmc3_bus",     0,                    smhc_parents,           ARRAY_SIZE(smhc_parents),           &sunxi_clk_periph_sdmmc3_bus       },
	{"uart0",          0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_uart0            },
	{"uart1",          0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_uart1            },
	{"uart2",          0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_uart2            },
	{"uart3",          0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_uart3            },
	{"uart4",          0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_uart4            },
	{"twi0",           0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_twi0             },
	{"twi1",           0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_twi1             },
	{"twi2",           0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_twi2             },
	{"twi3",           0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_twi3             },
	{"twi4",           0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_twi4             },
	{"twi5",           0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_twi5             },
	{"twi6",           0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_twi6             },
	{"can",            0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_can              },
	{"scr",            0,                    apb2mod_parents,        ARRAY_SIZE(apb2mod_parents),        &sunxi_clk_periph_scr              },
	{"spi0",           0,                    spi_parents,            ARRAY_SIZE(spi_parents),            &sunxi_clk_periph_spi0             },
	{"spi1",           0,                    spi_parents,            ARRAY_SIZE(spi_parents),            &sunxi_clk_periph_spi1             },
	{"gmac",           0,                    ahb3mod_parents,        ARRAY_SIZE(ahb3mod_parents),        &sunxi_clk_periph_gmac             },
	{"ts",             0,                    ts_parents,             ARRAY_SIZE(ts_parents),             &sunxi_clk_periph_ts               },
	{"gpadc",          0,                    apb1mod_parents,        ARRAY_SIZE(apb1mod_parents),        &sunxi_clk_periph_gpadc            },
	{"i2s0",           0,                    audio_parents,          ARRAY_SIZE(audio_parents),          &sunxi_clk_periph_i2s0             },
	{"i2s1",           0,                    audio_parents,          ARRAY_SIZE(audio_parents),          &sunxi_clk_periph_i2s1             },
	{"i2s2",           0,                    audio_parents,          ARRAY_SIZE(audio_parents),          &sunxi_clk_periph_i2s2             },
	{"spdif",          0,                    audio_parents,          ARRAY_SIZE(audio_parents),          &sunxi_clk_periph_spdif            },
	{"dmic",           0,                    audio_parents,          ARRAY_SIZE(audio_parents),          &sunxi_clk_periph_dmic             },
	{"codec_1x",       0,                    audio_parents,          ARRAY_SIZE(audio_parents),          &sunxi_clk_periph_codec_1x         },
	{"codec_4x",       0,                    audio_parents,          ARRAY_SIZE(audio_parents),          &sunxi_clk_periph_codec_4x         },
	{"usbphy0",        0,                    hosc_parents,           ARRAY_SIZE(hosc_parents),           &sunxi_clk_periph_usbphy0          },
	{"usbphy1",        0,                    hosc_parents,           ARRAY_SIZE(hosc_parents),           &sunxi_clk_periph_usbphy1          },
	{"usbphy2",        0,                    hosc_parents,           ARRAY_SIZE(hosc_parents),           &sunxi_clk_periph_usbphy2          },
	{"usbphy3",        0,                    hosc_parents,           ARRAY_SIZE(hosc_parents),           &sunxi_clk_periph_usbphy3          },
	{"usbohci0",       0,                    ahb3mod_parents,        ARRAY_SIZE(ahb3mod_parents),        &sunxi_clk_periph_usbohci0         },
	{"usbohci1",       0,                    ahb3mod_parents,        ARRAY_SIZE(ahb3mod_parents),        &sunxi_clk_periph_usbohci1         },
	{"usbohci2",       0,                    ahb3mod_parents,        ARRAY_SIZE(ahb3mod_parents),        &sunxi_clk_periph_usbohci2         },
	{"usbohci3",       0,                    ahb3mod_parents,        ARRAY_SIZE(ahb3mod_parents),        &sunxi_clk_periph_usbohci3         },
	{"usbehci0",       0,                    ahb3mod_parents,        ARRAY_SIZE(ahb3mod_parents),        &sunxi_clk_periph_usbehci0         },
	{"usbehci1",       0,                    ahb3mod_parents,        ARRAY_SIZE(ahb3mod_parents),        &sunxi_clk_periph_usbehci1         },
	{"usbehci2",       0,                    ahb3mod_parents,        ARRAY_SIZE(ahb3mod_parents),        &sunxi_clk_periph_usbehci2         },
	{"usbehci3",       0,                    ahb3mod_parents,        ARRAY_SIZE(ahb3mod_parents),        &sunxi_clk_periph_usbehci3         },
	{"usbohci0_12m",   0,                    usbohci_12m_parents,    ARRAY_SIZE(usbohci_12m_parents),    &sunxi_clk_periph_usbohci0_12m     },
	{"usbohci1_12m",   0,                    usbohci_12m_parents,    ARRAY_SIZE(usbohci_12m_parents),    &sunxi_clk_periph_usbohci1_12m     },
	{"usbohci2_12m",   0,                    usbohci_12m_parents,    ARRAY_SIZE(usbohci_12m_parents),    &sunxi_clk_periph_usbohci2_12m     },
	{"usbohci3_12m",   0,                    usbohci_12m_parents,    ARRAY_SIZE(usbohci_12m_parents),    &sunxi_clk_periph_usbohci3_12m     },
	{"usbotg",         0,                    ahb3mod_parents,        ARRAY_SIZE(ahb3mod_parents),        &sunxi_clk_periph_usbotg           },
	{"mipi_host",      0,                    mipi_host_parents,      ARRAY_SIZE(mipi_host_parents),      &sunxi_clk_periph_mipi_host        },
	{"display_top",    0,                    ahb3mod_parents,        ARRAY_SIZE(ahb3mod_parents),        &sunxi_clk_periph_display_top      },
	{"tcon_lcd0",      0,                    tcon_lcd_parents,       ARRAY_SIZE(tcon_lcd_parents),       &sunxi_clk_periph_tcon_lcd0        },
	{"tcon_lcd1",      0,                    tcon_lcd_parents,       ARRAY_SIZE(tcon_lcd_parents),       &sunxi_clk_periph_tcon_lcd1        },
	{"tcon_tv",        0,                    tve_parents,            ARRAY_SIZE(tve_parents),            &sunxi_clk_periph_tcon_tv          },
	{"lvds",           0,                    lvds_parents,           ARRAY_SIZE(lvds_parents),           &sunxi_clk_periph_lvds             },
	{"tve_top",        0,                    tve_parents,            ARRAY_SIZE(tve_parents),            &sunxi_clk_periph_tve_top          },
	{"tve0",           0,                    tve_parents,            ARRAY_SIZE(tve_parents),            &sunxi_clk_periph_tve0            },
	{"tvd_top", 0, lvds_parents, ARRAY_SIZE(lvds_parents), &sunxi_clk_periph_tvd_top},
	{"tvd0",           0,                    tvd_parents,            ARRAY_SIZE(tvd_parents),            &sunxi_clk_periph_tvd0             },
	{"tvd1",           0,                    tvd_parents,            ARRAY_SIZE(tvd_parents),            &sunxi_clk_periph_tvd1             },
	{"tvd2",           0,                    tvd_parents,            ARRAY_SIZE(tvd_parents),            &sunxi_clk_periph_tvd2             },
	{"tvd3",           0,                    tvd_parents,            ARRAY_SIZE(tvd_parents),            &sunxi_clk_periph_tvd3             },
	{"csi_cci0",       0,                    hosc_parents,           ARRAY_SIZE(hosc_parents),           &sunxi_clk_periph_csi_cci0         },
	{"csi_cci1",       0,                    hosc_parents,           ARRAY_SIZE(hosc_parents),           &sunxi_clk_periph_csi_cci1         },
	{"csi_cci2",       0,                    hosc_parents,           ARRAY_SIZE(hosc_parents),           &sunxi_clk_periph_csi_cci2         },
	{"csi_cci3",       0,                    hosc_parents,           ARRAY_SIZE(hosc_parents),           &sunxi_clk_periph_csi_cci3         },
	{"csi_top",        0,                    csi_top_parents,        ARRAY_SIZE(csi_top_parents),        &sunxi_clk_periph_csi_top          },
	{"csi_master0",    0,                    csi_master_parents,     ARRAY_SIZE(csi_master_parents),     &sunxi_clk_periph_csi_master0      },
	{"csi_master1",    0,                    csi_master_parents,     ARRAY_SIZE(csi_master_parents),     &sunxi_clk_periph_csi_master1      },
	{"csi_master2",    0,                    csi_master_parents,     ARRAY_SIZE(csi_master_parents),     &sunxi_clk_periph_csi_master2      },
	{"csi_master3",    0,                    csi_master_parents,     ARRAY_SIZE(csi_master_parents),     &sunxi_clk_periph_csi_master3      },
	{"isp",            0,                    csi_top_parents,        ARRAY_SIZE(csi_top_parents),        &sunxi_clk_periph_isp              },
	{"mipi_rx",        0,                    mipi_rx_parents,        ARRAY_SIZE(mipi_rx_parents),        &sunxi_clk_periph_mipi_rx          },
	{"pio",            0,                    apb1mod_parents,        ARRAY_SIZE(apb1mod_parents),        &sunxi_clk_periph_pio              },
};

struct periph_init_data sunxi_periphs_cpus_init[] = {
	{"cpurcpus_pll",    CLK_GET_RATE_NOCACHE|CLK_READONLY,  cpurcpus_pll_parents,   ARRAY_SIZE(cpurcpus_pll_parents),   &sunxi_clk_periph_cpurcpus_pll  },
	{"cpurcpus",        CLK_GET_RATE_NOCACHE|CLK_READONLY,  cpurcpus_parents,       ARRAY_SIZE(cpurcpus_parents),       &sunxi_clk_periph_cpurcpus      },
	{"cpurahbs",        CLK_GET_RATE_NOCACHE|CLK_READONLY,  cpurahbs_parents,       ARRAY_SIZE(cpurahbs_parents),       &sunxi_clk_periph_cpurahbs      },
	{"cpurapbs1",       CLK_GET_RATE_NOCACHE|CLK_READONLY,  cpurapbs1_parents,      ARRAY_SIZE(cpurapbs1_parents),      &sunxi_clk_periph_cpurapbs1     },
	{"cpurapbs2_pll",   CLK_GET_RATE_NOCACHE|CLK_READONLY,  cpurapbs2_pll_parents,  ARRAY_SIZE(cpurapbs2_pll_parents),  &sunxi_clk_periph_cpurapbs2_pll },
	{"cpurapbs2",       CLK_GET_RATE_NOCACHE|CLK_READONLY,  cpurapbs2_parents,      ARRAY_SIZE(cpurapbs2_parents),      &sunxi_clk_periph_cpurapbs2     },
	{"losc_out",        0,                                  losc_parents,           ARRAY_SIZE(losc_parents),           &sunxi_clk_periph_losc_out      },
	{"cpurpio",         CLK_GET_RATE_NOCACHE|CLK_READONLY,  apbs1mod_parents,       ARRAY_SIZE(apbs1mod_parents),        &sunxi_clk_periph_cpurpio      },
	{"cpurtimer",       0,                                  apbs1mod_parents,       ARRAY_SIZE(apbs1mod_parents),       &sunxi_clk_periph_cpurtimer     },
	{"cpurtwdog",       0,                                  apbs1mod_parents,       ARRAY_SIZE(apbs1mod_parents),       &sunxi_clk_periph_cpurtwdog     },
	{"cpurpwm",         0,                                  apbs1mod_parents,       ARRAY_SIZE(apbs1mod_parents),       &sunxi_clk_periph_cpurpwm       },
	{"cpur_vm_ths",     0,                                  cpurvm_ths_parents,     ARRAY_SIZE(cpurvm_ths_parents),     &sunxi_clk_periph_cpur_vm_ths   },
	{"cpurvm",          0,                                  vm_ths_parents,         ARRAY_SIZE(vm_ths_parents),         &sunxi_clk_periph_cpurvm        },
	{"cpurths",         0,                                  vm_ths_parents,         ARRAY_SIZE(vm_ths_parents),         &sunxi_clk_periph_cpurths       },
	{"cpurlradc",       0,                                  apbs1mod_parents,       ARRAY_SIZE(apbs1mod_parents),       &sunxi_clk_periph_cpurlradc     },
	{"cpuruart0",       0,                                  apbs2mod_parents,       ARRAY_SIZE(apbs2mod_parents),       &sunxi_clk_periph_cpuruart0     },
	{"cpuruart1",       0,                                  apbs2mod_parents,       ARRAY_SIZE(apbs2mod_parents),       &sunxi_clk_periph_cpuruart1     },
	{"cpuruart2",       0,                                  apbs2mod_parents,       ARRAY_SIZE(apbs2mod_parents),       &sunxi_clk_periph_cpuruart2     },
	{"cpuruart3",       0,                                  apbs2mod_parents,       ARRAY_SIZE(apbs2mod_parents),       &sunxi_clk_periph_cpuruart3     },
	{"cpuruart4",       0,                                  apbs2mod_parents,       ARRAY_SIZE(apbs2mod_parents),       &sunxi_clk_periph_cpuruart4     },
	{"cpurtwi0",        0,                                  apbs2mod_parents,       ARRAY_SIZE(apbs2mod_parents),       &sunxi_clk_periph_cpurtwi0      },
	{"cpurtwi1",        0,                                  apbs2mod_parents,       ARRAY_SIZE(apbs2mod_parents),       &sunxi_clk_periph_cpurtwi1      },
	{"cpurtwi2",        0,                                  apbs2mod_parents,       ARRAY_SIZE(apbs2mod_parents),       &sunxi_clk_periph_cpurtwi2      },
	{"cpurcan",         0,                                  apbs2mod_parents,       ARRAY_SIZE(apbs2mod_parents),       &sunxi_clk_periph_cpurcan       },
	{"cpurrsb",         0,                                  apbs2mod_parents,       ARRAY_SIZE(apbs2mod_parents),       &sunxi_clk_periph_cpurrsb       },
	{"cpurcir",         0,                                  cpurcir_parents,        ARRAY_SIZE(cpurcir_parents),        &sunxi_clk_periph_cpurcir       },
	{"cpurspi",         0,                                  cpurspi_parents,        ARRAY_SIZE(cpurspi_parents),        &sunxi_clk_periph_cpurspi       },
	{"cpurrtc",         0,                                  apbs1mod_parents,       ARRAY_SIZE(apbs1mod_parents),       &sunxi_clk_periph_cpurrtc       },
};

static int pll_clk_is_lock(unsigned long clk_cfg_addr)
{
	u32 reg;
	u32 loop = 5000;

	reg = readl(sunxi_clk_base + clk_cfg_addr);
	reg = SET_BITS(29, 1, reg, 1);
	writel(reg , sunxi_clk_base + clk_cfg_addr);

	while (--loop) {
		reg = readl(sunxi_clk_base + clk_cfg_addr);
		if (GET_BITS(28, 1, reg)) {
			udelay(20);
			break;
		} else {
			udelay(1);
		}
	}

	reg = readl(sunxi_clk_base + clk_cfg_addr);
	reg = SET_BITS(29, 1, reg, 0);
	writel(reg , sunxi_clk_base + clk_cfg_addr);

	if (!loop)
		return -1;
	return 0;
}

u8 get_parent_pll_mipi(struct clk_hw *hw)
{
	u8 parent;
	unsigned long reg;
	struct sunxi_clk_factors *factor = to_clk_factor(hw);

	if (!factor->reg)
		return 0;
	reg = readl(factor->reg);
	parent = GET_BITS(21, 1, reg);

	return parent;
}
int set_parent_pll_mipi(struct clk_hw *hw, u8 index)
{
	unsigned long reg;
	struct sunxi_clk_factors *factor = to_clk_factor(hw);

	if (!factor->reg)
		return 0;
	reg = readl(factor->reg);
	reg = SET_BITS(21, 1, reg, index);
	writel(reg, factor->reg);
	return 0;
}
static int clk_enable_pll_mipi(struct clk_hw *hw)
{
	struct sunxi_clk_factors *factor = to_clk_factor(hw);
	struct sunxi_clk_factors_config *config = factor->config;
	unsigned long reg = readl(factor->reg);

	if (config->sdmwidth) {
		writel(config->sdmval, (void __iomem *)config->sdmpat);
		reg = SET_BITS(config->sdmshift, config->sdmwidth, reg, 1);
	}

	reg |= 0x3 << 22;
	writel(reg, factor->reg);
	udelay(100);

	reg = SET_BITS(config->enshift, 1, reg, 1);
	writel(reg, factor->reg);
	udelay(100);

	if (pll_clk_is_lock(PLL_MIPI)) {
		printf("pll_mipi enable failed!\n");
		return -1;
	}

	return 0;
}

static void clk_disable_pll_mipi(struct clk_hw *hw)
{
	struct sunxi_clk_factors *factor = to_clk_factor(hw);
	struct sunxi_clk_factors_config *config = factor->config;
	unsigned long reg = readl(factor->reg);

	if (config->sdmwidth)
		reg = SET_BITS(config->sdmshift, config->sdmwidth, reg, 0);
	reg = SET_BITS(config->enshift, 1, reg, 0);
	reg &= ~(0x3 << 22);
	writel(reg, factor->reg);
}

int mipi_rx_priv_enable(struct clk_hw *hw)
{
	unsigned long reg;
	struct sunxi_clk_periph *periph = to_clk_periph(hw);
	struct sunxi_clk_periph_gate *gate = &periph->gate;

	if (!(GET_BITS(31, 1, readl(sunxi_clk_base + PLL_VIDEO0)))) {
		reg = readl(sunxi_clk_base + PLL_VIDEO0);
		reg = SET_BITS(31, 1, reg, 1);
		writel(reg, sunxi_clk_base + PLL_VIDEO0);
		if (pll_clk_is_lock(PLL_VIDEO0)) {
			printf("clk video enable failed\n");
			return -1;
		}
	}

	/* de-assert module */
	if (gate->reset && !(periph->flags & CLK_IGNORE_AUTORESET)
					&& !IS_SHARE_RST_GATE(periph)) {
		reg = periph_readl(periph, gate->reset);
		reg = SET_BITS(gate->rst_shift, 1, reg, 1);
		periph_writel(periph, reg, gate->reset);
	}

	/* enable bus gating */
	if (gate->bus && !IS_SHARE_BUS_GATE(periph)) {
		reg = periph_readl(periph, gate->bus);
		reg = SET_BITS(gate->bus_shift, 1, reg, 1);
		periph_writel(periph, reg, gate->bus);
	}

	/* enable module gating */
	if (gate->enable && !IS_SHARE_MOD_GATE(periph)) {
		reg = periph_readl(periph, gate->enable);
		if (periph->flags & CLK_REVERT_ENABLE)
			reg = SET_BITS(gate->enb_shift, 1, reg, 0);
		else
			reg = SET_BITS(gate->enb_shift, 1, reg, 1);
		periph_writel(periph, reg, gate->enable);
	}

	/* enable dram gating */
	if (gate->dram && !IS_SHARE_MBUS_GATE(periph)) {
		reg = periph_readl(periph, gate->dram);
		reg = SET_BITS(gate->ddr_shift, 1, reg, 1);
		periph_writel(periph, reg, gate->dram);
	}

	return 0;
}

void set_mipi_rx_priv_ops(struct clk_ops *priv_ops)
{
	priv_ops->enable = mipi_rx_priv_enable;
}

void sunxi_set_clk_priv_ops(char *clk_name,
			void (*set_priv_ops)(struct clk_ops *priv_ops))
{
	int i = 0;
	struct clk_ops *clk_priv_ops;

	clk_priv_ops = malloc(sizeof(*clk_priv_ops));
	sunxi_clk_get_periph_ops(clk_priv_ops);
	set_priv_ops(clk_priv_ops);
	for (i = 0; i < (ARRAY_SIZE(sunxi_periphs_init)); i++) {
		if (!strcmp(sunxi_periphs_init[i].name, clk_name)) {
			sunxi_periphs_init[i].periph->priv_clkops =
								clk_priv_ops;
			return;
		}
	}
	for (i = 0; i < (ARRAY_SIZE(sunxi_periphs_cpus_init)); i++) {
		if (!strcmp(sunxi_periphs_cpus_init[i].name, clk_name)) {
			sunxi_periphs_cpus_init[i].periph->priv_clkops =
								clk_priv_ops;
			return;
		}
	}
}

void init_clocks(void)
{
	int i;
	/* struct clk *clk; */
	struct factor_init_data *factor;
	struct periph_init_data *periph;

	/* get clk register base address */
	sunxi_clk_base = (void *)0x03001000; /* fixed base address. */
	/*sunxi_clk_cpus_base = of_iomap(node, 1);*/
	/*sunxi_clk_periph_losc_out.gate.bus = of_iomap(node, 2);*/
	sunxi_clk_factor_initlimits();

	sunxi_clk_get_factors_ops(&pll_mipi_ops);
	pll_mipi_ops.get_parent = get_parent_pll_mipi;
	pll_mipi_ops.set_parent = set_parent_pll_mipi;
	pll_mipi_ops.enable = clk_enable_pll_mipi;
	pll_mipi_ops.disable = clk_disable_pll_mipi;
	sunxi_set_clk_priv_ops("mipi_rx", set_mipi_rx_priv_ops);

	clk_register_fixed_rate(NULL, "hosc", NULL, CLK_IS_ROOT, 24000000);
	/* clk_register_fixed_rate(NULL, "pll_24m", NULL, CLK_IS_ROOT, */
	/* 24000000); */

	/* register normal factors, based on sunxi factor framework */
	for (i = 0; i < ARRAY_SIZE(sunxi_factos); i++) {
		factor = &sunxi_factos[i];
		factor->priv_regops = NULL;
		sunxi_clk_register_factors(NULL, (void *)sunxi_clk_base,
					   (struct factor_init_data *)factor);
	}

	/* register periph clock */
	for (i = 0; i < ARRAY_SIZE(sunxi_periphs_init); i++) {
		periph = &sunxi_periphs_init[i];
		periph->periph->priv_regops = NULL;
		sunxi_clk_register_periph(periph, sunxi_clk_base);
	}
	printf("%s: finish init_clocks.\n", __func__);
}
