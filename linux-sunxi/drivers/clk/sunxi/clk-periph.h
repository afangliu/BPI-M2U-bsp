/*
 * Copyright (C) 2013 Allwinnertech, kevin.z.m <kevin@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Adjustable periph-based clock implementation
 */
#ifndef __MACH_SUNXI_CLK_PERIPH_H
#define __MACH_SUNXI_CLK_PERIPH_H

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/io.h>
#include <asm/div64.h>

/**
 * struct sunxi_clk_periph_gate - peripheral gate clock
 *
 * @flags:      hardware-specific flags
 * @enable:     enable register
 * @reset:      reset register
 * @bus:        bus gating resiter
 * @dram:       dram gating register
 * @enb_shift:  enable gate bit shift
 * @rst_shift:  reset gate bit shift
 * @bus_shift:  bus gate bit shift
 * @ddr_shift:  dram gate bit shift
 *
 * Flags:
 * SUNXI_PERIPH_NO_GATE - this flag indicates that module gate is not allowed for this module.
 * SUNXI_PERIPH_NO_RESET - This flag indicates that reset is not allowed for this module.
 * SUNXI_PERIPH_NO_BUS_GATE - This flag indicates that bus gate is not allowed for this module.
 * SUNXI_PERIPH_NO_DDR_GATE - This flag indicates that dram gate is not allowed for this module.
 */
struct sunxi_clk_periph_gate {
	u32             flags;
	void __iomem    *enable;
	void __iomem    *reset;
	void __iomem    *bus;
	void __iomem    *dram;
	u8              enb_shift;
	u8              rst_shift;
	u8              bus_shift;
	u8              ddr_shift;
};

/**
 * struct sunxi_clk_periph_div - periph divider clock
 *
 * @reg:        register containing divider
 * @mshift:     shift to the divider-m bit field, div = (m+1)
 * @mwidth:     width of the divider-m bit field
 * @nshift:     shift to the divider-n bit field, div = (1<<n)
 * @nwidth:     width of the divider-n bit field
 * @lock:       register lock
 *
 * Flags:
 */
struct sunxi_clk_periph_div {
	void __iomem    *reg;
	u8              mshift;
	u8              mwidth;
	u8              nshift;
	u8              nwidth;
	spinlock_t      *lock;
};


/**
 * struct sunxi_clk_periph_mux - multiplexer clock
 *
 * @reg:        register controlling multiplexer
 * @shift:      shift to multiplexer bit field
 * @width:      width of mutliplexer bit field
 * @lock:       register lock
 *
 * Clock with multiple selectable parents.  Implements .get_parent, .set_parent
 * and .recalc_rate
 *
 */
struct sunxi_clk_periph_mux {
	void __iomem    *reg;
	u8              shift;
	u8              width;
	spinlock_t      *lock;
};

struct sunxi_clk_comgate {
	const u8        *name;
	u32              val;
	u32              mask;
	u32              share;
	u32              res;
};

#define BUS_GATE_SHARE  0x01
#define RST_GATE_SHARE  0x02
#define MBUS_GATE_SHARE 0x04
#define MOD_GATE_SHARE  0x08

#define IS_SHARE_BUS_GATE(x)  (x->com_gate?((x->com_gate->share & BUS_GATE_SHARE)?1:0):0)
#define IS_SHARE_RST_GATE(x)  (x->com_gate?((x->com_gate->share & RST_GATE_SHARE)?1:0):0)
#define IS_SHARE_MBUS_GATE(x) (x->com_gate?((x->com_gate->share & MBUS_GATE_SHARE)?1:0):0)
#define IS_SHARE_MOD_GATE(x)  (x->com_gate?((x->com_gate->share & MOD_GATE_SHARE)?1:0):0)

/**
 * struct sunxi-clk-periph - peripheral clock
 *
 * @hw:         handle between common and hardware-specific interfaces
 * @flags:      flags used across common struct clk, please take refference of the clk-provider.h
 * @lock:       lock for protecting the periph clock operations
 * @mux:        mux clock
 * @gate:       gate clock
 * @divider:    divider clock
 * @com_gate:       the shared clock
 * @com_gate_off:   bit shift to mark the flag in the com_gate
 * @priv_clkops:    divider clock ops
 * @priv_regops:    gate clock ops
 */
struct sunxi_clk_periph {
	struct clk_hw                   hw;
	unsigned long                   flags;
	spinlock_t                      *lock;

	struct sunxi_clk_periph_mux     mux;
	struct sunxi_clk_periph_gate    gate;
	struct sunxi_clk_periph_div     divider;
	struct sunxi_clk_comgate*       com_gate;
	u8                              com_gate_off;
	struct clk_ops*                 priv_clkops;
	struct sunxi_reg_ops*           priv_regops;
};

struct periph_init_data {
	const char              *name;
	unsigned long           flags;
	const char              **parent_names;
	int                     num_parents;
	struct sunxi_clk_periph *periph;
};

static inline u32 periph_readl(struct sunxi_clk_periph * periph, void __iomem * reg)
{
	return (((unsigned long)periph->priv_regops)?periph->priv_regops->reg_readl(reg):readl(reg));
}

static inline void periph_writel(struct sunxi_clk_periph * periph, unsigned int val, void __iomem * reg)
{
	(((unsigned long)periph->priv_regops) ? periph->priv_regops->reg_writel(val, reg) : writel(val, reg));
}

struct clk *sunxi_clk_register_periph(struct periph_init_data *pd,
					void __iomem  *base);

int sunxi_periph_reset_deassert(struct clk *c);
int sunxi_periph_reset_assert(struct clk *c);
extern void sunxi_clk_get_periph_ops(struct clk_ops* ops);

#define to_clk_periph(_hw) container_of(_hw, struct sunxi_clk_periph, hw)


#define SUNXI_CLK_PERIPH(name, _mux_reg, _mux_shift, _mux_width,  \
			_div_reg, _div_mshift, _div_mwidth, _div_nshift, _div_nwidth,   \
			_gate_flags, _enable_reg, _reset_reg, _bus_gate_reg, _drm_gate_reg, \
			_enable_shift, _reset_shift, _bus_gate_shift, _dram_gate_shift, _lock,_com_gate,_com_gate_off) \
static struct sunxi_clk_periph sunxi_clk_periph_##name ={       \
	.lock = _lock,                                          \
															\
	.mux = {                                                \
		.reg = (void __iomem  *)_mux_reg,                   \
		.shift = _mux_shift,                                \
		.width = _mux_width,                                \
	},                                                      \
															\
	.divider = {                                            \
		.reg = (void __iomem  *)_div_reg,                   \
		.mshift = _div_mshift,                              \
		.mwidth = _div_mwidth,                              \
		.nshift = _div_nshift,                              \
		.nwidth = _div_nwidth,                              \
	},                                                      \
	.gate = {                                               \
		.flags = _gate_flags,                               \
		.enable = (void __iomem  *)_enable_reg,             \
		.reset = (void __iomem  *)_reset_reg,               \
		.bus = (void __iomem  *)_bus_gate_reg,              \
		.dram = (void __iomem  *)_drm_gate_reg,             \
		.enb_shift = _enable_shift,                         \
		.rst_shift = _reset_shift,                          \
		.bus_shift = _bus_gate_shift,                       \
		.ddr_shift = _dram_gate_shift,                      \
	},                                                      \
	.com_gate = _com_gate,                                  \
	.com_gate_off = _com_gate_off,                          \
}

#endif /* __MACH_SUNXI_CLK_PERIPH_H */
