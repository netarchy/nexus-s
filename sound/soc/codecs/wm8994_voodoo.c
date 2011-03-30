/*
 * voodoo_sound.c  --  WM8994 ALSA Soc Audio driver related
 *
 *  Copyright (C) 2010/11 François SIMOND / twitter & XDA-developers @supercurio
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <sound/soc.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include "wm8994_voodoo.h"

#ifndef MODULE
#ifdef NEXUS_S
#include "wm8994_samsung.h"
#else
#include "wm8994.h"
#endif
#else
#ifdef NEXUS_S
#include "../wm8994_samsung.h"
#else
#include "../wm8994.h"
#endif
#endif

#define SUBJECT "wm8994_voodoo.c"
#define VOODOO_SOUND_VERSION 7

#ifdef MODULE
#include "tegrak_voodoo_sound.h"

// wm8994_write -> tegrak_wm8994_write for dynamic link
#ifdef wm8994_write
#undef wm8994_write
#endif

// wm8994_read -> tegrak_wm8994_read for dynamic link
#ifdef wm8994_read
#undef wm8994_read
#endif

#define wm8994_write(codec, reg, value) tegrak_wm8994_write(codec, reg, value)
#define wm8994_read(codec, reg) tegrak_wm8994_read(codec, reg)
#endif

bool bypass_write_hook = false;

#ifdef CONFIG_SND_VOODOO_HP_LEVEL_CONTROL
unsigned short hplvol = CONFIG_SND_VOODOO_HP_LEVEL;
unsigned short hprvol = CONFIG_SND_VOODOO_HP_LEVEL;
#endif

#ifdef CONFIG_SND_VOODOO_FM
bool fm_radio_headset_restore_bass = true;
bool fm_radio_headset_restore_highs = true;
bool fm_radio_headset_normalize_gain = true;
#endif

#ifdef CONFIG_SND_VOODOO_RECORD_PRESETS
unsigned short recording_preset = 1;
unsigned short origin_recgain;
unsigned short origin_recgain_mixer;
#endif

#ifdef NEXUS_S
bool speaker_tuning = false;
#endif

// global active or kill switch
bool enable = false;

bool dac_osr128 = true;
bool adc_osr128 = false;
bool fll_tuning = true;
bool dac_direct = true;
bool mono_downmix = false;

// keep here a pointer to the codec structure
struct snd_soc_codec *codec;

#define DECLARE_BOOL_SHOW(name) \
static ssize_t name##_show(struct device *dev, \
struct device_attribute *attr, char *buf) \
{ \
	return sprintf(buf,"%u\n",(name ? 1 : 0)); \
}

#define DECLARE_BOOL_STORE_UPDATE_WITH_MUTE(name, updater, with_mute) \
static ssize_t name##_store(struct device *dev, struct device_attribute *attr, \
	const char *buf, size_t size) \
{ \
	unsigned short state; \
	if (sscanf(buf, "%hu", &state) == 1) { \
		name = state == 0 ? false : true; \
		updater(with_mute); \
	} \
	return size; \
}

#ifdef NEXUS_S
#define DECLARE_WM8994(codec) struct wm8994_priv *wm8994 = codec->drvdata;
#else
#define DECLARE_WM8994(codec) struct wm8994_priv *wm8994 = codec->private_data;
#endif

#ifdef CONFIG_SND_VOODOO_HP_LEVEL_CONTROL
void update_hpvol()
{
	unsigned short val;

	bypass_write_hook = true;
	// hard limit to 62 because 63 introduces distortions
	if (hplvol > 62)
		hplvol = 62;
	if (hprvol > 62)
		hprvol = 62;

	// we don't need the Volume Update flag when sending the first volume
	val = (WM8994_HPOUT1L_MUTE_N | hplvol);
	val |= WM8994_HPOUT1L_ZC;
	wm8994_write(codec, WM8994_LEFT_OUTPUT_VOLUME, val);

	// this time we write the right volume plus the Volume Update flag.
	//This way, both volume are set at the same time
	val = (WM8994_HPOUT1_VU | WM8994_HPOUT1R_MUTE_N | hprvol);
	val |= WM8994_HPOUT1L_ZC;
	wm8994_write(codec, WM8994_RIGHT_OUTPUT_VOLUME, val);
	bypass_write_hook = false;
}
#endif

#ifdef CONFIG_SND_VOODOO_FM
void update_fm_radio_headset_restore_freqs(bool with_mute)
{
	unsigned short val;
	DECLARE_WM8994(codec);

	bypass_write_hook = true;
	// apply only when FM radio is active
	if (wm8994->fmradio_path == FMR_OFF)
		return;

	if (with_mute) {
		wm8994_write(codec, WM8994_AIF2_DAC_FILTERS_1, 0x236);
		msleep(180);
	}

	if (fm_radio_headset_restore_bass) {
		// disable Sidetone high-pass filter
		// was designed for voice and not FM radio
		wm8994_write(codec, WM8994_SIDETONE, 0x0000);
		// disable 4FS ultrasonic mode and
		// restore the hi-fi <4Hz hi pass filter
		wm8994_write(codec, WM8994_AIF2_ADC_FILTERS, 0x1800);
	} else {
		// default settings in GT-I9000 Froyo XXJPX kernel sources
		wm8994_write(codec, WM8994_SIDETONE, 0x01c0);
		wm8994_write(codec, WM8994_AIF2_ADC_FILTERS, 0xF800);
	}

	if (fm_radio_headset_restore_highs) {
		val = wm8994_read(codec, WM8994_AIF2_DAC_FILTERS_1);
		val &= ~(WM8994_AIF2DAC_DEEMP_MASK);
		wm8994_write(codec, WM8994_AIF2_DAC_FILTERS_1, val);
	} else {
		wm8994_write(codec, WM8994_AIF2_DAC_FILTERS_1, 0x0036);
	}

	// un-mute
	if (with_mute) {
		val = wm8994_read(codec, WM8994_AIF2_DAC_FILTERS_1);
		val &= ~(WM8994_AIF2DAC_MUTE_MASK);
		wm8994_write(codec, WM8994_AIF2_DAC_FILTERS_1, val);
	}
	bypass_write_hook = false;
}

void update_fm_radio_headset_normalize_gain(bool with_mute)
{
	DECLARE_WM8994(codec);

	bypass_write_hook = true;
	// apply only when FM radio is active
	if (wm8994->fmradio_path == FMR_OFF)
		return;

	if (fm_radio_headset_normalize_gain) {
		// Bumped volume, change with Zero Cross
		wm8994_write(codec, WM8994_LEFT_LINE_INPUT_3_4_VOLUME, 0x52);
		wm8994_write(codec, WM8994_RIGHT_LINE_INPUT_3_4_VOLUME, 0x152);
		wm8994_write(codec, WM8994_AIF2_DRC_2, 0x0840);
		wm8994_write(codec, WM8994_AIF2_DRC_3, 0x2408);
		wm8994_write(codec, WM8994_AIF2_DRC_4, 0x0082);
		wm8994_write(codec, WM8994_AIF2_DRC_5, 0x0100);
		wm8994_write(codec, WM8994_AIF2_DRC_1, 0x019C);
	} else {
		// Original volume, change with Zero Cross
		wm8994_write(codec, WM8994_LEFT_LINE_INPUT_3_4_VOLUME, 0x4B);
		wm8994_write(codec, WM8994_RIGHT_LINE_INPUT_3_4_VOLUME, 0x14B);
		wm8994_write(codec, WM8994_AIF2_DRC_2, 0x0840);
		wm8994_write(codec, WM8994_AIF2_DRC_3, 0x2400);
		wm8994_write(codec, WM8994_AIF2_DRC_4, 0x0000);
		wm8994_write(codec, WM8994_AIF2_DRC_5, 0x0000);
		wm8994_write(codec, WM8994_AIF2_DRC_1, 0x019C);
	}
	bypass_write_hook = false;
}
#endif

#ifdef CONFIG_SND_VOODOO_RECORD_PRESETS
void update_recording_preset(bool with_mute)
{
	if (!is_path(MAIN_MICROPHONE))
		return;

	switch (recording_preset) {
	case 0:
		// Original:
		// On Galaxy S: IN1L_VOL1=11000 (+19.5 dB)
		// On Nexus S: variable value
		wm8994_write(codec, WM8994_LEFT_LINE_INPUT_1_2_VOLUME,
			     WM8994_IN1L_VU | origin_recgain);
		wm8994_write(codec, WM8994_INPUT_MIXER_3, origin_recgain_mixer);
		// DRC disabled
		wm8994_write(codec, WM8994_AIF1_DRC1_1, 0x0080);
		break;
	case 2:
		// High sensitivy:
		// Original - 4.5 dB, IN1L_VOL1=10101 (+15 dB)
		wm8994_write(codec, WM8994_LEFT_LINE_INPUT_1_2_VOLUME, 0x0115);
		wm8994_write(codec, WM8994_INPUT_MIXER_3, 0x30);
		// DRC Input: -6dB, Ouptut -3.75dB
		//     Above knee 1/8, Below knee 1/2
		//     Max gain 24 / Min gain -12
		wm8994_write(codec, WM8994_AIF1_DRC1_1, 0x009A);
		wm8994_write(codec, WM8994_AIF1_DRC1_2, 0x0426);
		wm8994_write(codec, WM8994_AIF1_DRC1_3, 0x0019);
		wm8994_write(codec, WM8994_AIF1_DRC1_4, 0x0105);
		break;
	case 3:
		// Concert new: IN1L_VOL1=10110 (+4.5 dB)
		// +30dB input mixer gain deactivated
		wm8994_write(codec, WM8994_LEFT_LINE_INPUT_1_2_VOLUME, 0x010F);
		wm8994_write(codec, WM8994_INPUT_MIXER_3, 0x20);
		// DRC Input: -4.5dB, Ouptut -6.75dB
		//     Above knee 1/4, Below knee 1/2
		//     Max gain 24 / Min gain -12
		wm8994_write(codec, WM8994_AIF1_DRC1_1, 0x009A);
		wm8994_write(codec, WM8994_AIF1_DRC1_2, 0x0846);
		wm8994_write(codec, WM8994_AIF1_DRC1_3, 0x0011);
		wm8994_write(codec, WM8994_AIF1_DRC1_4, 0x00C9);
		break;
	case 4:
		// ULTRA LOUD:
		// Original - 36 dB - 30 dB IN1L_VOL1=00000 (-16.5 dB)
		// +30dB input mixer gain deactivated
		wm8994_write(codec, WM8994_LEFT_LINE_INPUT_1_2_VOLUME, 0x0100);
		wm8994_write(codec, WM8994_INPUT_MIXER_3, 0x20);
		// DRC Input: -7.5dB, Ouptut -6dB
		//     Above knee 1/8, Below knee 1/4
		//     Max gain 36 / Min gain -12
		wm8994_write(codec, WM8994_AIF1_DRC1_1, 0x009A);
		wm8994_write(codec, WM8994_AIF1_DRC1_2, 0x0847);
		wm8994_write(codec, WM8994_AIF1_DRC1_3, 0x001A);
		wm8994_write(codec, WM8994_AIF1_DRC1_4, 0x00C9);
		break;
	default:
		// make sure recording_preset is the default
		recording_preset = 1;
		// New Balanced: Original - 16.5 dB
		// IN1L_VOL1=01101 (+27 dB)
		// +30dB input mixer gain deactivated
		wm8994_write(codec, WM8994_LEFT_LINE_INPUT_1_2_VOLUME, 0x055D);
		wm8994_write(codec, WM8994_INPUT_MIXER_3, 0x20);
		// DRC Input: -18.5dB, Ouptut -9dB
		//     Above knee 1/8, Below knee 1/2
		//     Max gain 18 / Min gain -12
		wm8994_write(codec, WM8994_AIF1_DRC1_1, 0x009A);
		wm8994_write(codec, WM8994_AIF1_DRC1_2, 0x0845);
		wm8994_write(codec, WM8994_AIF1_DRC1_3, 0x0019);
		wm8994_write(codec, WM8994_AIF1_DRC1_4, 0x030C);
		break;
	}
}
#endif

bool is_path(int unified_path)
{
	DECLARE_WM8994(codec);

	switch (unified_path) {
		// speaker
	case SPEAKER:
#ifdef GALAXY_TAB
		return (wm8994->cur_path == SPK
			|| wm8994->cur_path == RING_SPK
			|| wm8994->fmradio_path == FMR_SPK
			|| wm8994->fmradio_path == FMR_SPK_MIX);
#else
		return (wm8994->cur_path == SPK
			|| wm8994->cur_path == RING_SPK);
#endif

		// headphones
		// FIXME: be sure dac_direct doesn't break phone calls on TAB
		// with these spath detection settings (HP4P)
	case HEADPHONES:
#ifdef NEXUS_S
		return (wm8994->cur_path == HP
			|| wm8994->cur_path == HP_NO_MIC);
#else
#ifdef GALAXY_TAB
		return (wm8994->cur_path == HP3P
			|| wm8994->cur_path == HP4P
			|| wm8994->fmradio_path == FMR_HP);
#else
#ifdef M110S
		return (wm8994->cur_path == HP);
#else
		return (wm8994->cur_path == HP
			|| wm8994->fmradio_path == FMR_HP);
#endif
#endif
#endif

		// FM Radio on headphones
	case RADIO_HEADPHONES:
#ifdef NEXUS_S
		return false;
#else
#ifdef M110S
		return false;
#else
#ifdef GALAXY_TAB
		return (wm8994->codec_state & FMRADIO_ACTIVE)
		    && (wm8994->fmradio_path == FMR_HP);
#else
		return (wm8994->codec_state & FMRADIO_ACTIVE)
		    && (wm8994->fmradio_path == FMR_HP);
#endif
#endif
#endif

		// headphones
		// FIXME: be sure dac_direct doesn't break phone calls on TAB
		// with these spath detection settings (HP4P)
	case MAIN_MICROPHONE:
		return (wm8994->codec_state & CAPTURE_ACTIVE)
		    && (wm8994->rec_path == MAIN);

	}
	return false;
}

#ifdef NEXUS_S
void update_speaker_tuning(bool with_mute)
{
	DECLARE_WM8994(codec);

	if (!(is_path(SPEAKER) || (wm8994->codec_state & CALL_ACTIVE)))
		return;

	printk("We are on speaker!\n");
	if (speaker_tuning) {
		// DRC settings
		wm8994_write(codec, WM8994_AIF1_DRC1_3, 0x0010);
		wm8994_write(codec, WM8994_AIF1_DRC1_4, 0x00EB);

		// hardware EQ
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_GAINS_1,   0x041D);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_GAINS_2,   0x4C00);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_1_A,  0x0FE3);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_1_B,  0x0403);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_1_PG, 0x0074);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_2_A,  0x1F03);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_2_B,  0xF0F9);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_2_C,  0x040A);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_2_PG, 0x03DA);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_3_A,  0x1ED2);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_3_B,  0xF11A);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_3_C,  0x040A);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_3_PG, 0x045D);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_4_A,  0x0E76);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_4_B,  0xFCE4);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_4_C,  0x040A);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_4_PG, 0x330D);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_5_A,  0xFC8F);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_5_B,  0x0400);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_5_PG, 0x323C);

		// Speaker Boost tuning
		wm8994_write(codec, WM8994_CLASSD,                 0x0170);
	} else {
		// DRC settings
		wm8994_write(codec, WM8994_AIF1_DRC1_3, 0x0028);
		wm8994_write(codec, WM8994_AIF1_DRC1_4, 0x0186);

		// hardware EQ
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_GAINS_1,   0x0019);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_GAINS_2,   0x6280);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_1_A,  0x0FC3);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_1_B,  0x03FD);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_1_PG, 0x00F4);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_2_A,  0x1F30);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_2_B,  0xF0CD);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_2_C,  0x040A);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_2_PG, 0x032C);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_3_A,  0x1C52);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_3_B,  0xF379);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_3_C,  0x040A);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_3_PG, 0x0DC1);
		wm8994_write(codec, WM8994_CLASSD,                 0x0170);

		// Speaker Boost tuning
		wm8994_write(codec, WM8994_CLASSD,                 0x0168);
	}
}
#endif

unsigned short osr128_get_value(unsigned short val)
{
	if (dac_osr128 == 1)
		val |= WM8994_DAC_OSR128;
	else
		val &= ~WM8994_DAC_OSR128;

	if (adc_osr128 == 1)
		val |= WM8994_ADC_OSR128;
	else
		val &= ~WM8994_ADC_OSR128;

	return val;
}

void update_osr128(bool with_mute)
{
	unsigned short val;
	val = osr128_get_value(wm8994_read(codec, WM8994_OVERSAMPLING));
	bypass_write_hook = true;
	wm8994_write(codec, WM8994_OVERSAMPLING, val);
	bypass_write_hook = false;
}

unsigned short fll_tuning_get_value(unsigned short val)
{
	val = (val >> WM8994_FLL1_GAIN_WIDTH << WM8994_FLL1_GAIN_WIDTH);
	if (fll_tuning == 1)
		val |= 5;

	return val;
}

void update_fll_tuning(bool with_mute)
{
	unsigned short val;
	val = fll_tuning_get_value(wm8994_read(codec, WM8994_FLL1_CONTROL_4));
	bypass_write_hook = true;
	wm8994_write(codec, WM8994_FLL1_CONTROL_4, val);
	bypass_write_hook = false;
}

unsigned short mono_downmix_get_value(unsigned short val)
{
	// depends on the output path in order to preserve mono downmixing
	// on speaker
	if (!is_path(SPEAKER)) {
		if (mono_downmix)
			val |= WM8994_AIF1DAC1_MONO;
		else
			val &= ~WM8994_AIF1DAC1_MONO;
	}

	return val;
}

void update_mono_downmix(bool with_mute)
{
	unsigned short val1, val2, val3;
	val1 = mono_downmix_get_value(wm8994_read
				      (codec, WM8994_AIF1_DAC1_FILTERS_1));
	val2 = mono_downmix_get_value(wm8994_read
				      (codec, WM8994_AIF1_DAC2_FILTERS_1));
	val3 = mono_downmix_get_value(wm8994_read
				      (codec, WM8994_AIF2_DAC_FILTERS_1));

	bypass_write_hook = true;
	wm8994_write(codec, WM8994_AIF1_DAC1_FILTERS_1, val1);
	wm8994_write(codec, WM8994_AIF1_DAC2_FILTERS_1, val2);
	wm8994_write(codec, WM8994_AIF2_DAC_FILTERS_1, val3);
	bypass_write_hook = false;
}

unsigned short dac_direct_get_value(unsigned short val)
{
	DECLARE_WM8994(codec);

	if (is_path(HEADPHONES)
	    && (wm8994->codec_state & PLAYBACK_ACTIVE)
	    && !(wm8994->codec_state & CALL_ACTIVE)
	    && !(wm8994->stream_state & PCM_STREAM_PLAYBACK)) {

		if (dac_direct) {
			if (val == WM8994_DAC1L_TO_MIXOUTL)
				return WM8994_DAC1L_TO_HPOUT1L;
			else if (val == WM8994_DAC1L_TO_HPOUT1L)
				return WM8994_DAC1L_TO_MIXOUTL;
		}
	}

	return val;
}

void update_dac_direct(bool with_mute)
{
	unsigned short val1, val2;
	val1 = dac_direct_get_value(wm8994_read(codec, WM8994_OUTPUT_MIXER_1));
	val2 = dac_direct_get_value(wm8994_read(codec, WM8994_OUTPUT_MIXER_2));

	bypass_write_hook = true;
	wm8994_write(codec, WM8994_OUTPUT_MIXER_1, val1);
	wm8994_write(codec, WM8994_OUTPUT_MIXER_2, val2);
	bypass_write_hook = false;
}

/*
 *
 * Declaring the controling misc devices
 *
 */
#ifdef CONFIG_SND_VOODOO_HP_LEVEL_CONTROL
static ssize_t headphone_amplifier_level_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	// output median of left and right headphone amplifier volumes
	return sprintf(buf, "%u\n", (hplvol + hprvol) / 2);
}

static ssize_t headphone_amplifier_level_store(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t size)
{
	unsigned short vol;
	if (sscanf(buf, "%hu", &vol) == 1) {
		// left and right are set to the same volumes
		hplvol = hprvol = vol;
		update_hpvol();
	}
	return size;
}
#endif

#ifdef NEXUS_S
DECLARE_BOOL_SHOW(speaker_tuning);
DECLARE_BOOL_STORE_UPDATE_WITH_MUTE(speaker_tuning,
				    update_speaker_tuning,
				    false);
#endif

#ifdef CONFIG_SND_VOODOO_FM
DECLARE_BOOL_SHOW(fm_radio_headset_restore_bass);
DECLARE_BOOL_STORE_UPDATE_WITH_MUTE(fm_radio_headset_restore_bass,
				    update_fm_radio_headset_restore_freqs,
				    true);

DECLARE_BOOL_SHOW(fm_radio_headset_restore_highs);
DECLARE_BOOL_STORE_UPDATE_WITH_MUTE(fm_radio_headset_restore_highs,
				    update_fm_radio_headset_restore_freqs,
				    true);

DECLARE_BOOL_SHOW(fm_radio_headset_normalize_gain);
DECLARE_BOOL_STORE_UPDATE_WITH_MUTE(fm_radio_headset_normalize_gain,
				    update_fm_radio_headset_normalize_gain,
				    false);
#endif

#ifdef CONFIG_SND_VOODOO_RECORD_PRESETS
static ssize_t recording_preset_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", recording_preset);
}

static ssize_t recording_preset_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	unsigned short preset_number;
	if (sscanf(buf, "%hu", &preset_number) == 1) {
		recording_preset = preset_number;
		update_recording_preset(false);
	}
	return size;
}
#endif

DECLARE_BOOL_SHOW(dac_osr128);
DECLARE_BOOL_STORE_UPDATE_WITH_MUTE(dac_osr128,
				    update_osr128,
				    false);

DECLARE_BOOL_SHOW(adc_osr128);
DECLARE_BOOL_STORE_UPDATE_WITH_MUTE(adc_osr128,
				    update_osr128,
				    false);

DECLARE_BOOL_SHOW(fll_tuning);
DECLARE_BOOL_STORE_UPDATE_WITH_MUTE(fll_tuning,
				    update_fll_tuning,
				    false);

DECLARE_BOOL_SHOW(mono_downmix);
DECLARE_BOOL_STORE_UPDATE_WITH_MUTE(mono_downmix,
				    update_mono_downmix,
				    false);

DECLARE_BOOL_SHOW(dac_direct);
DECLARE_BOOL_STORE_UPDATE_WITH_MUTE(dac_direct,
				    update_dac_direct,
				    false);

#ifdef CONFIG_SND_VOODOO_DEBUG
static ssize_t show_wm8994_register_dump(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	// modified version of register_dump from wm8994_aries.c
	// r = wm8994 register
	int r;

	for (r = 0; r <= 0x6; r++)
		sprintf(buf, "0x%X 0x%X\n", r, wm8994_read(codec, r));

	sprintf(buf, "%s0x%X 0x%X\n", buf, 0x15, wm8994_read(codec, 0x15));

	for (r = 0x18; r <= 0x3C; r++)
		sprintf(buf, "%s0x%X 0x%X\n", buf, r, wm8994_read(codec, r));

	sprintf(buf, "%s0x%X 0x%X\n", buf, 0x4C, wm8994_read(codec, 0x4C));

	for (r = 0x51; r <= 0x5C; r++)
		sprintf(buf, "%s0x%X 0x%X\n", buf, r, wm8994_read(codec, r));

	sprintf(buf, "%s0x%X 0x%X\n", buf, 0x60, wm8994_read(codec, 0x60));
	sprintf(buf, "%s0x%X 0x%X\n", buf, 0x101, wm8994_read(codec, 0x101));
	sprintf(buf, "%s0x%X 0x%X\n", buf, 0x110, wm8994_read(codec, 0x110));
	sprintf(buf, "%s0x%X 0x%X\n", buf, 0x111, wm8994_read(codec, 0x111));

	for (r = 0x200; r <= 0x212; r++)
		sprintf(buf, "%s0x%X 0x%X\n", buf, r, wm8994_read(codec, r));

	for (r = 0x220; r <= 0x224; r++)
		sprintf(buf, "%s0x%X 0x%X\n", buf, r, wm8994_read(codec, r));

	for (r = 0x240; r <= 0x244; r++)
		sprintf(buf, "%s0x%X 0x%X\n", buf, r, wm8994_read(codec, r));

	for (r = 0x300; r <= 0x317; r++)
		sprintf(buf, "%s0x%X 0x%X\n", buf, r, wm8994_read(codec, r));

	for (r = 0x400; r <= 0x411; r++)
		sprintf(buf, "%s0x%X 0x%X\n", buf, r, wm8994_read(codec, r));

	for (r = 0x420; r <= 0x423; r++)
		sprintf(buf, "%s0x%X 0x%X\n", buf, r, wm8994_read(codec, r));

	for (r = 0x440; r <= 0x444; r++)
		sprintf(buf, "%s0x%X 0x%X\n", buf, r, wm8994_read(codec, r));

	for (r = 0x450; r <= 0x454; r++)
		sprintf(buf, "%s0x%X 0x%X\n", buf, r, wm8994_read(codec, r));

	for (r = 0x480; r <= 0x493; r++)
		sprintf(buf, "%s0x%X 0x%X\n", buf, r, wm8994_read(codec, r));

	for (r = 0x4A0; r <= 0x4B3; r++)
		sprintf(buf, "%s0x%X 0x%X\n", buf, r, wm8994_read(codec, r));

	for (r = 0x500; r <= 0x503; r++)
		sprintf(buf, "%s0x%X 0x%X\n", buf, r, wm8994_read(codec, r));

	sprintf(buf, "%s0x%X 0x%X\n", buf, 0x510, wm8994_read(codec, 0x510));
	sprintf(buf, "%s0x%X 0x%X\n", buf, 0x520, wm8994_read(codec, 0x520));
	sprintf(buf, "%s0x%X 0x%X\n", buf, 0x521, wm8994_read(codec, 0x521));

	for (r = 0x540; r <= 0x544; r++)
		sprintf(buf, "%s0x%X 0x%X\n", buf, r, wm8994_read(codec, r));

	for (r = 0x580; r <= 0x593; r++)
		sprintf(buf, "%s0x%X 0x%X\n", buf, r, wm8994_read(codec, r));

	for (r = 0x600; r <= 0x614; r++)
		sprintf(buf, "%s0x%X 0x%X\n", buf, r, wm8994_read(codec, r));

	sprintf(buf, "%s0x%X 0x%X\n", buf, 0x620, wm8994_read(codec, 0x620));
	sprintf(buf, "%s0x%X 0x%X\n", buf, 0x621, wm8994_read(codec, 0x621));

	for (r = 0x700; r <= 0x70A; r++)
		sprintf(buf, "%s0x%X 0x%X\n", buf, r, wm8994_read(codec, r));

	return sprintf(buf, "%s", buf);
}

static ssize_t store_wm8994_write(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	short unsigned int reg = 0;
	short unsigned int val = 0;
	int unsigned bytes_read = 0;

	while (sscanf(buf, "%hx %hx%n", &reg, &val, &bytes_read) == 2) {
		buf += bytes_read;
		wm8994_write(codec, reg, val);
	}
	return size;
}
#endif

static ssize_t voodoo_sound_version(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", VOODOO_SOUND_VERSION);
}

#ifndef MODULE
DECLARE_BOOL_SHOW(enable);
static ssize_t enable_store(struct device *dev,
			    struct device_attribute *attr, const char *buf,
			    size_t size)
{
	unsigned short state;
	bool bool_state;
	if (sscanf(buf, "%hu", &state) == 1) {
		bool_state = state == 0 ? false : true;
		if (state != enable) {
			enable = bool_state;
			update_enable();
		}
	}
	return size;
}
#endif

#ifdef CONFIG_SND_VOODOO_HP_LEVEL_CONTROL
static DEVICE_ATTR(headphone_amplifier_level, S_IRUGO | S_IWUGO,
		   headphone_amplifier_level_show,
		   headphone_amplifier_level_store);
#endif

#ifdef NEXUS_S
static DEVICE_ATTR(speaker_tuning, S_IRUGO | S_IWUGO,
		   speaker_tuning_show,
		   speaker_tuning_store);
#endif

#ifdef CONFIG_SND_VOODOO_FM
static DEVICE_ATTR(fm_radio_headset_restore_bass, S_IRUGO | S_IWUGO,
		   fm_radio_headset_restore_bass_show,
		   fm_radio_headset_restore_bass_store);

static DEVICE_ATTR(fm_radio_headset_restore_highs, S_IRUGO | S_IWUGO,
		   fm_radio_headset_restore_highs_show,
		   fm_radio_headset_restore_highs_store);

static DEVICE_ATTR(fm_radio_headset_normalize_gain, S_IRUGO | S_IWUGO,
		   fm_radio_headset_normalize_gain_show,
		   fm_radio_headset_normalize_gain_store);
#endif

#ifdef CONFIG_SND_VOODOO_RECORD_PRESETS
static DEVICE_ATTR(recording_preset, S_IRUGO | S_IWUGO,
		   recording_preset_show,
		   recording_preset_store);
#endif

static DEVICE_ATTR(dac_osr128, S_IRUGO | S_IWUGO,
		   dac_osr128_show,
		   dac_osr128_store);

static DEVICE_ATTR(adc_osr128, S_IRUGO | S_IWUGO,
		   adc_osr128_show,
		   adc_osr128_store);

static DEVICE_ATTR(fll_tuning, S_IRUGO | S_IWUGO,
		   fll_tuning_show,
		   fll_tuning_store);

static DEVICE_ATTR(dac_direct, S_IRUGO | S_IWUGO,
		   dac_direct_show,
		   dac_direct_store);

static DEVICE_ATTR(mono_downmix, S_IRUGO | S_IWUGO,
		   mono_downmix_show,
		   mono_downmix_store);

#ifdef CONFIG_SND_VOODOO_DEBUG
static DEVICE_ATTR(wm8994_register_dump, S_IRUGO,
		   show_wm8994_register_dump,
		   NULL);

static DEVICE_ATTR(wm8994_write, S_IWUSR,
		   NULL,
		   store_wm8994_write);
#endif

static DEVICE_ATTR(version, S_IRUGO,
		   voodoo_sound_version,
		   NULL);

#ifndef MODULE
static DEVICE_ATTR(enable, S_IRUGO | S_IWUGO,
		   enable_show,
		   enable_store);
#endif

static struct attribute *voodoo_sound_attributes[] = {
#ifdef CONFIG_SND_VOODOO_HP_LEVEL_CONTROL
	&dev_attr_headphone_amplifier_level.attr,
#endif
#ifdef NEXUS_S
	&dev_attr_speaker_tuning.attr,
#endif
#ifdef CONFIG_SND_VOODOO_FM
	&dev_attr_fm_radio_headset_restore_bass.attr,
	&dev_attr_fm_radio_headset_restore_highs.attr,
	&dev_attr_fm_radio_headset_normalize_gain.attr,
#endif
#ifdef CONFIG_SND_VOODOO_RECORD_PRESETS
	&dev_attr_recording_preset.attr,
#endif
	&dev_attr_dac_osr128.attr,
	&dev_attr_adc_osr128.attr,
	&dev_attr_fll_tuning.attr,
	&dev_attr_dac_direct.attr,
	&dev_attr_mono_downmix.attr,
#ifdef CONFIG_SND_VOODOO_DEBUG
	&dev_attr_wm8994_register_dump.attr,
	&dev_attr_wm8994_write.attr,
#endif
	&dev_attr_version.attr,
	NULL
};

#ifndef MODULE
static struct attribute *voodoo_sound_control_attributes[] = {
	&dev_attr_enable.attr,
	NULL
};
#endif

static struct attribute_group voodoo_sound_group = {
	.attrs = voodoo_sound_attributes,
};

#ifndef MODULE
static struct attribute_group voodoo_sound_control_group = {
	.attrs = voodoo_sound_control_attributes,
};
#endif

static struct miscdevice voodoo_sound_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "voodoo_sound",
};

#ifndef MODULE
static struct miscdevice voodoo_sound_control_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "voodoo_sound_control",
};
#endif

void voodoo_hook_wm8994_pcm_remove()
{
	printk("Voodoo sound: removing driver v%d\n", VOODOO_SOUND_VERSION);
	sysfs_remove_group(&voodoo_sound_device.this_device->kobj,
			   &voodoo_sound_group);
	misc_deregister(&voodoo_sound_device);
}

void update_enable()
{
	if (enable) {
		printk("Voodoo sound: initializing driver v%d\n",
		       VOODOO_SOUND_VERSION);

		misc_register(&voodoo_sound_device);
		if (sysfs_create_group(&voodoo_sound_device.this_device->kobj,
				       &voodoo_sound_group) < 0) {
			printk("%s sysfs_create_group fail\n", __FUNCTION__);
			pr_err("Failed to create sysfs group for (%s)!\n",
			       voodoo_sound_device.name);
		}
	} else
		voodoo_hook_wm8994_pcm_remove();
}

/*
 *
 * Driver Hooks
 *
 */

#ifdef CONFIG_SND_VOODOO_FM
void voodoo_hook_fmradio_headset()
{
	// global kill switch
	if (!enable)
		return;

	if (!fm_radio_headset_restore_bass
	    && !fm_radio_headset_restore_highs
	    && !fm_radio_headset_normalize_gain)
		return;

	update_fm_radio_headset_restore_freqs(false);
	update_fm_radio_headset_normalize_gain(false);
}
#endif

#ifdef CONFIG_SND_VOODOO_RECORD_PRESETS
void voodoo_hook_record_main_mic()
{
	// global kill switch
	if (!enable)
		return;

	if (recording_preset == 0)
		return;

	origin_recgain = wm8994_read(codec, WM8994_LEFT_LINE_INPUT_1_2_VOLUME);
	origin_recgain_mixer = wm8994_read(codec, WM8994_INPUT_MIXER_3);
	update_recording_preset(false);
}
#endif

void voodoo_hook_playback_speaker()
{
	// global kill switch
	if (!enable)
		return;
#ifdef NEXUS_S
	if (!speaker_tuning)
		return;

	update_speaker_tuning(false);
#endif
}

unsigned int voodoo_hook_wm8994_write(struct snd_soc_codec *codec_,
				      unsigned int reg, unsigned int value)
{
	DECLARE_WM8994(codec_);

	// global kill switch
	if (!enable)
		return value;

	// modify some registers before those being written to the codec
	// be sure our pointer to codec is up to date
	codec = codec_;

	if (!bypass_write_hook) {

#ifdef CONFIG_SND_VOODOO_HP_LEVEL_CONTROL
		if (is_path(HEADPHONES)
		    && !(wm8994->codec_state & CALL_ACTIVE)) {

			if (reg == WM8994_LEFT_OUTPUT_VOLUME)
				value =
				    (WM8994_HPOUT1_VU |
				     WM8994_HPOUT1L_MUTE_N |
				     hplvol);

			if (reg == WM8994_RIGHT_OUTPUT_VOLUME)
				value =
				    (WM8994_HPOUT1_VU |
				     WM8994_HPOUT1R_MUTE_N |
				     hprvol);
		}
#endif

#ifdef CONFIG_SND_VOODOO_FM
		if (is_path(RADIO_HEADPHONES)) {
			if (reg == WM8994_INPUT_MIXER_2
			    || reg == WM8994_AIF2_DRC_1
			    || reg == WM8994_ANALOGUE_HP_1)
				voodoo_hook_fmradio_headset();
		}
#endif

		if (reg == WM8994_OVERSAMPLING)
			value = osr128_get_value(value);

		if (reg == WM8994_FLL1_CONTROL_4)
			value = fll_tuning_get_value(value);

		if (reg == WM8994_AIF1_DAC1_FILTERS_1
		    || reg == WM8994_AIF1_DAC2_FILTERS_1
		    || reg == WM8994_AIF2_DAC_FILTERS_1)
			value = mono_downmix_get_value(value);

		if (reg == WM8994_OUTPUT_MIXER_1
		    || reg == WM8994_OUTPUT_MIXER_2)
			value = dac_direct_get_value(value);
	}
#ifdef CONFIG_SND_VOODOO_DEBUG_LOG
	// log every write to dmesg
	printk("Voodoo sound: wm8994_write register= [%X] value= [%X]\n",
	       reg, value);
#ifdef NEXUS_S
	printk("Voodoo sound: codec_state=%u, stream_state=%u, "
	       "cur_path=%i, rec_path=%i, "
	       "power_state=%i\n",
	       wm8994->codec_state, wm8994->stream_state,
	       wm8994->cur_path, wm8994->rec_path, wm8994->power_state);
#else
	printk("Voodoo sound: codec_state=%u, stream_state=%u, "
	       "cur_path=%i, rec_path=%i, "
	       "fmradio_path=%i, fmr_mix_path=%i, "
	       "power_state=%i, "
	       "recognition_active=%i, ringtone_active=%i\n",
	       wm8994->codec_state, wm8994->stream_state,
	       wm8994->cur_path, wm8994->rec_path,
	       wm8994->fmradio_path, wm8994->fmr_mix_path,
	       wm8994->power_state,
	       wm8994->recognition_active, wm8994->ringtone_active);
#endif
#endif
	return value;
}

void voodoo_hook_wm8994_pcm_probe(struct snd_soc_codec *codec_)
{
	enable = true;
	update_enable();

#ifndef MODULE
	misc_register(&voodoo_sound_control_device);
	if (sysfs_create_group(&voodoo_sound_control_device.this_device->kobj,
			       &voodoo_sound_control_group) < 0) {
		printk("%s sysfs_create_group fail\n", __FUNCTION__);
		pr_err("Failed to create sysfs group for device (%s)!\n",
		       voodoo_sound_control_device.name);
	}
#endif

	// make a copy of the codec pointer
	codec = codec_;
}
