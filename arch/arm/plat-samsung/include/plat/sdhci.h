/* linux/arch/arm/plat-s3c/include/plat/sdhci.h
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C Platform - SDHCI (HSMMC) platform data definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __PLAT_S3C_SDHCI_H
#define __PLAT_S3C_SDHCI_H __FILE__

struct platform_device;
struct mmc_host;
struct mmc_card;
struct mmc_ios;

/**
 * struct s3c_sdhci_platdata() - Platform device data for Samsung SDHCI
 * @max_width: The maximum number of data bits supported.
 * @host_caps: Standard MMC host capabilities bit field.
 * @cfg_gpio: Configure the GPIO for a specific card bit-width
 * @cfg_card: Configure the interface for a specific card and speed. This
 *            is necessary the controllers and/or GPIO blocks require the
 *	      changing of driver-strength and other controls dependant on
 *	      the card and speed of operation.
 *
 * Initialisation data specific to either the machine or the platform
 * for the device driver to use or call-back when configuring gpio or
 * card speed information.
*/
struct s3c_sdhci_platdata {
	unsigned int	max_width;
	unsigned int	host_caps;

	char		**clocks;	/* set of clock sources */

	void	(*cfg_gpio)(struct platform_device *dev, int width);
	void	(*cfg_card)(struct platform_device *dev,
			    void __iomem *regbase,
			    struct mmc_ios *ios,
			    struct mmc_card *card);
	void	(*adjust_cfg_card)(struct s3c_sdhci_platdata *pdata, void __iomem *regbase, int rw);
	int		rx_cfg;
	int		tx_cfg;

	/* add to deal with EXT_IRQ as a card detect pin */
	void            (*cfg_ext_cd) (void);
	unsigned int    (*detect_ext_cd) (void);
	unsigned int    ext_cd;

	/* add to deal with GPIO as a card write protection pin */
	void            (*cfg_wp) (void);
	int             (*get_ro) (struct mmc_host *mmc);

        /* add to deal with non-removable device */
        int     built_in;

	int must_maintain_clock;
	int enable_intr_on_resume;
};

/**
 * s3c_sdhci0_set_platdata - Set platform data for S3C SDHCI device.
 * @pd: Platform data to register to device.
 *
 * Register the given platform data for use withe S3C SDHCI device.
 * The call will copy the platform data, so the board definitions can
 * make the structure itself __initdata.
 */
extern void s3c_sdhci_set_platdata(void);
extern void s3c_sdhci0_set_platdata(struct s3c_sdhci_platdata *pd);
extern void s3c_sdhci1_set_platdata(struct s3c_sdhci_platdata *pd);
extern void s3c_sdhci2_set_platdata(struct s3c_sdhci_platdata *pd);
extern void s3c_sdhci3_set_platdata(struct s3c_sdhci_platdata *pd);

/* Default platform data, exported so that per-cpu initialisation can
 * set the correct one when there are more than one cpu type selected.
*/

extern struct s3c_sdhci_platdata s3c_hsmmc0_def_platdata;
extern struct s3c_sdhci_platdata s3c_hsmmc1_def_platdata;
extern struct s3c_sdhci_platdata s3c_hsmmc2_def_platdata;
extern struct s3c_sdhci_platdata s3c_hsmmc3_def_platdata;

/* Helper function availablity */

extern void s3c64xx_setup_sdhci0_cfg_gpio(struct platform_device *, int w);
extern void s3c64xx_setup_sdhci1_cfg_gpio(struct platform_device *, int w);
extern void s5pc100_setup_sdhci0_cfg_gpio(struct platform_device *, int w);
extern void s5pc100_setup_sdhci1_cfg_gpio(struct platform_device *, int w);
extern void s5pc100_setup_sdhci2_cfg_gpio(struct platform_device *, int w);
extern void s3c64xx_setup_sdhci2_cfg_gpio(struct platform_device *, int w);
extern void s5pv210_setup_sdhci0_cfg_gpio(struct platform_device *, int w);
extern void s5pv210_setup_sdhci1_cfg_gpio(struct platform_device *, int w);
extern void s5pv210_setup_sdhci2_cfg_gpio(struct platform_device *, int w);
extern void s5pv210_setup_sdhci3_cfg_gpio(struct platform_device *, int w);

/* S3C6400 SDHCI setup */

#ifdef CONFIG_S3C64XX_SETUP_SDHCI
extern char *s3c64xx_hsmmc_clksrcs[4];

#ifdef CONFIG_S3C_DEV_HSMMC
extern void s3c6400_setup_sdhci_cfg_card(struct platform_device *dev,
					 void __iomem *r,
					 struct mmc_ios *ios,
					 struct mmc_card *card);

static inline void s3c6400_default_sdhci0(void)
{
	s3c_hsmmc0_def_platdata.clocks = s3c64xx_hsmmc_clksrcs;
	s3c_hsmmc0_def_platdata.cfg_gpio = s3c64xx_setup_sdhci0_cfg_gpio;
	s3c_hsmmc0_def_platdata.cfg_card = s3c6400_setup_sdhci_cfg_card;
}

#else
static inline void s3c6400_default_sdhci0(void) { }
#endif  /* CONFIG_S3C_DEV_HSMMC */

#ifdef CONFIG_S3C_DEV_HSMMC1
static inline void s3c6400_default_sdhci1(void)
{
	s3c_hsmmc1_def_platdata.clocks = s3c64xx_hsmmc_clksrcs;
	s3c_hsmmc1_def_platdata.cfg_gpio = s3c64xx_setup_sdhci1_cfg_gpio;
	s3c_hsmmc1_def_platdata.cfg_card = s3c6400_setup_sdhci_cfg_card;
}
#else
static inline void s3c6400_default_sdhci1(void) { }
#endif /* CONFIG_S3C_DEV_HSMMC1 */

#ifdef CONFIG_S3C_DEV_HSMMC2
static inline void s3c6400_default_sdhci2(void)
{
	s3c_hsmmc2_def_platdata.clocks = s3c64xx_hsmmc_clksrcs;
	s3c_hsmmc2_def_platdata.cfg_gpio = s3c64xx_setup_sdhci2_cfg_gpio;
	s3c_hsmmc2_def_platdata.cfg_card = s3c6400_setup_sdhci_cfg_card;
}
#else
static inline void s3c6400_default_sdhci2(void) { }
#endif /* CONFIG_S3C_DEV_HSMMC2 */

/* S3C6410 SDHCI setup */

extern void s3c6410_setup_sdhci_cfg_card(struct platform_device *dev,
					 void __iomem *r,
					 struct mmc_ios *ios,
					 struct mmc_card *card);

#ifdef CONFIG_S3C_DEV_HSMMC
static inline void s3c6410_default_sdhci0(void)
{
	s3c_hsmmc0_def_platdata.clocks = s3c64xx_hsmmc_clksrcs;
	s3c_hsmmc0_def_platdata.cfg_gpio = s3c64xx_setup_sdhci0_cfg_gpio;
	s3c_hsmmc0_def_platdata.cfg_card = s3c6410_setup_sdhci_cfg_card;
}
#else
static inline void s3c6410_default_sdhci0(void) { }
#endif /* CONFIG_S3C_DEV_HSMMC */

#ifdef CONFIG_S3C_DEV_HSMMC1
static inline void s3c6410_default_sdhci1(void)
{
	s3c_hsmmc1_def_platdata.clocks = s3c64xx_hsmmc_clksrcs;
	s3c_hsmmc1_def_platdata.cfg_gpio = s3c64xx_setup_sdhci1_cfg_gpio;
	s3c_hsmmc1_def_platdata.cfg_card = s3c6410_setup_sdhci_cfg_card;
}
#else
static inline void s3c6410_default_sdhci1(void) { }
#endif /* CONFIG_S3C_DEV_HSMMC1 */

#ifdef CONFIG_S3C_DEV_HSMMC2
static inline void s3c6410_default_sdhci2(void)
{
	s3c_hsmmc2_def_platdata.clocks = s3c64xx_hsmmc_clksrcs;
	s3c_hsmmc2_def_platdata.cfg_gpio = s3c64xx_setup_sdhci2_cfg_gpio;
	s3c_hsmmc2_def_platdata.cfg_card = s3c6410_setup_sdhci_cfg_card;
}
#else
static inline void s3c6410_default_sdhci2(void) { }
#endif /* CONFIG_S3C_DEV_HSMMC2 */

#else
static inline void s3c6410_default_sdhci0(void) { }
static inline void s3c6410_default_sdhci1(void) { }
static inline void s3c6410_default_sdhci2(void) { }
static inline void s3c6400_default_sdhci0(void) { }
static inline void s3c6400_default_sdhci1(void) { }
static inline void s3c6400_default_sdhci2(void) { }

#endif /* CONFIG_S3C64XX_SETUP_SDHCI */

/* S5PC100 SDHCI setup */

#ifdef CONFIG_S5PC100_SETUP_SDHCI
extern char *s5pc100_hsmmc_clksrcs[4];

extern void s5pc100_setup_sdhci0_cfg_card(struct platform_device *dev,
					   void __iomem *r,
					   struct mmc_ios *ios,
					   struct mmc_card *card);

#ifdef CONFIG_S3C_DEV_HSMMC
static inline void s5pc100_default_sdhci0(void)
{
	s3c_hsmmc0_def_platdata.clocks = s5pc100_hsmmc_clksrcs;
	s3c_hsmmc0_def_platdata.cfg_gpio = s5pc100_setup_sdhci0_cfg_gpio;
	s3c_hsmmc0_def_platdata.cfg_card = s5pc100_setup_sdhci0_cfg_card;
}
#else
static inline void s5pc100_default_sdhci0(void) { }
#endif /* CONFIG_S3C_DEV_HSMMC */

#ifdef CONFIG_S3C_DEV_HSMMC1
static inline void s5pc100_default_sdhci1(void)
{
	s3c_hsmmc1_def_platdata.clocks = s5pc100_hsmmc_clksrcs;
	s3c_hsmmc1_def_platdata.cfg_gpio = s5pc100_setup_sdhci1_cfg_gpio;
	s3c_hsmmc1_def_platdata.cfg_card = s5pc100_setup_sdhci0_cfg_card;
}
#else
static inline void s5pc100_default_sdhci1(void) { }
#endif /* CONFIG_S3C_DEV_HSMMC1 */

#ifdef CONFIG_S3C_DEV_HSMMC2
static inline void s5pc100_default_sdhci2(void)
{
	s3c_hsmmc2_def_platdata.clocks = s5pc100_hsmmc_clksrcs;
	s3c_hsmmc2_def_platdata.cfg_gpio = s5pc100_setup_sdhci2_cfg_gpio;
	s3c_hsmmc2_def_platdata.cfg_card = s5pc100_setup_sdhci0_cfg_card;
}
#else
static inline void s5pc100_default_sdhci2(void) { }
#endif /* CONFIG_S3C_DEV_HSMMC1 */


#else
static inline void s5pc100_default_sdhci0(void) { }
static inline void s5pc100_default_sdhci1(void) { }
static inline void s5pc100_default_sdhci2(void) { }
#endif /* CONFIG_S5PC100_SETUP_SDHCI */


/* S5PC110 SDHCI setup */
#ifdef CONFIG_S5PV210_SETUP_SDHCI
extern char *s5pv210_hsmmc_clksrcs[4];

extern void s5pv210_setup_sdhci_cfg_card(struct platform_device *dev,
					 void __iomem *r,
					 struct mmc_ios *ios,
					 struct mmc_card *card);
extern void s5pv210_adjust_sdhci_cfg_card(struct s3c_sdhci_platdata *pdata, void __iomem *r, int rw);

#ifdef CONFIG_S3C_DEV_HSMMC
static inline void s5pv210_default_sdhci0(void)
{
	s3c_hsmmc0_def_platdata.clocks = s5pv210_hsmmc_clksrcs;
	s3c_hsmmc0_def_platdata.cfg_gpio = s5pv210_setup_sdhci0_cfg_gpio;
	s3c_hsmmc0_def_platdata.cfg_card = s5pv210_setup_sdhci_cfg_card;
	s3c_hsmmc0_def_platdata.adjust_cfg_card = s5pv210_adjust_sdhci_cfg_card;
}
#else
static inline void s5pv210_default_sdhci0(void) { }
#endif /* CONFIG_S3C_DEV_HSMMC */

#ifdef CONFIG_S3C_DEV_HSMMC1
static inline void s5pv210_default_sdhci1(void)
{
	s3c_hsmmc1_def_platdata.clocks = s5pv210_hsmmc_clksrcs;
	s3c_hsmmc1_def_platdata.cfg_gpio = s5pv210_setup_sdhci1_cfg_gpio;
	s3c_hsmmc1_def_platdata.cfg_card = s5pv210_setup_sdhci_cfg_card;
	s3c_hsmmc1_def_platdata.adjust_cfg_card = s5pv210_adjust_sdhci_cfg_card;
}
#else
static inline void s5pv210_default_sdhci1(void) { }
#endif /* CONFIG_S3C_DEV_HSMMC1 */

#ifdef CONFIG_S3C_DEV_HSMMC2
static inline void s5pv210_default_sdhci2(void)
{
	s3c_hsmmc2_def_platdata.clocks = s5pv210_hsmmc_clksrcs;
	s3c_hsmmc2_def_platdata.cfg_gpio = s5pv210_setup_sdhci2_cfg_gpio;
	s3c_hsmmc2_def_platdata.cfg_card = s5pv210_setup_sdhci_cfg_card;
	s3c_hsmmc2_def_platdata.adjust_cfg_card = s5pv210_adjust_sdhci_cfg_card;
}
#else
static inline void s5pv210_default_sdhci2(void) { }
#endif /* CONFIG_S3C_DEV_HSMMC2 */

#ifdef CONFIG_S3C_DEV_HSMMC3
static inline void s5pv210_default_sdhci3(void)
{
	s3c_hsmmc3_def_platdata.clocks = s5pv210_hsmmc_clksrcs;
	s3c_hsmmc3_def_platdata.cfg_gpio = s5pv210_setup_sdhci3_cfg_gpio;
	s3c_hsmmc3_def_platdata.cfg_card = s5pv210_setup_sdhci_cfg_card;
	s3c_hsmmc3_def_platdata.adjust_cfg_card = s5pv210_adjust_sdhci_cfg_card;
}
#else
static inline void s5pv210_default_sdhci3(void) { }
#endif /* CONFIG_S3C_DEV_HSMMC2 */

#else
static inline void s5pv210_default_sdhci0(void) { }
static inline void s5pv210_default_sdhci1(void) { }
static inline void s5pv210_default_sdhci2(void) { }
static inline void s5pv210_default_sdhci3(void) { }
#endif /* CONFIG_S5PV210_SETUP_SDHCI */

extern void sdhci_s3c_force_presence_change(struct platform_device *pdev);

#endif /* __PLAT_S3C_SDHCI_H */
