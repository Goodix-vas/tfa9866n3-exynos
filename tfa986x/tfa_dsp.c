/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "inc/dbgprint.h"
#include "inc/tfa_device.h"
#include "inc/tfa_container.h"
#include "inc/tfa.h"
#include "inc/tfa98xx_tfafieldnames.h"
#include "inc/tfa_internal.h"

#ifndef MIN
#define MIN(A, B) ((A < B) ? A : B)
#endif

/* retry values */
#define CFSTABLE_TRIES		10
#define AMPOFFWAIT_TRIES	50
#define MTPBWAIT_TRIES		50
#define MTPEX_WAIT_NTRIES	50
#define ACS_RESET_WAIT_NTRIES 10

/* set intervals */
#define BUSLOAD_INTERVAL	10
#define RAMPING_INTERVAL	1

#define STAT_LEN	80

static TFA9866_IRQ_NAMETABLE;
static TFA9866_IRQ_INFO;

/*
 * static variables
 */
static DEFINE_MUTEX(dev_lock);
static DEFINE_MUTEX(dsp_msg_lock);
static int dsp_cal_value[MAX_CHANNELS] = {-1, -1};

static enum tfa98xx_error tfa_calibration_range_check(struct tfa_device *tfa,
	unsigned int channel, int mohm);
static enum tfa98xx_error tfa_process_re25(struct tfa_device *tfa);
static enum tfa98xx_error _dsp_msg(struct tfa_device *tfa, int lastmessage);

void tfa_handle_damaged_speakers(struct tfa_device *tfa)
{
	int cur_spkg = 0;

	tfa->is_configured = -1;

	if (!tfa_is_active_device(tfa))
		return;

	if (tfa->spkr_damaged) {
		pr_info("[%d] stop damaged device!\n", tfa->dev_idx);
		tfa_dev_stop(tfa);
		return;
	}
	cur_spkg = TFAxx_GET_BF(tfa, TDMSPKG);

	if (cur_spkg < TFA_TDMSPKG_IN_BYPASS) {
		pr_info("[%d] reduce TDMSPKG (%d to %d) in bypass!\n",
			tfa->dev_idx, cur_spkg, TFA_TDMSPKG_IN_BYPASS);

		/* force TDMSPKG as 7 dB */
		TFAxx_SET_BF(tfa, TDMSPKG, TFA_TDMSPKG_IN_BYPASS);

		/* reload for next time */
		tfa->first_after_boot = 1;
	}
	pr_info("%s: UNMUTE dev %d\n", __func__, tfa->dev_idx);
	tfa_dev_set_state(tfa, TFA_STATE_UNMUTE, 1);
}

/*
 * interrupt bit function to clear
 */
int tfa_irq_clear(struct tfa_device *tfa, int bit)
{
	unsigned char reg;

	/* make bitfield enum */
	if (bit == tfa->irq_all) {
		/* operate on all bits */
		for (reg = TFA98XX_INTERRUPT_IN_REG;
			reg < TFA98XX_INTERRUPT_IN_REG + 3; reg++)
			reg_write(tfa, reg, 0xffff); /* all bits */
	} else if (bit < tfa->irq_max) {
		reg = (unsigned char)
			(TFA98XX_INTERRUPT_IN_REG + (bit >> 4));
		reg_write(tfa, reg, 1 << (bit & 0x0f));
		/* only this bit */
	} else {
		return TFA_ERROR;
	}

	return 0;
}

/*
 * return state of irq or -1 if illegal bit
 */
int tfa_irq_get(struct tfa_device *tfa, int bit)
{
	uint16_t value;
	int reg, mask;

	if (bit < tfa->irq_max) {
		/* only this bit */
		reg = TFA98XX_INTERRUPT_OUT_REG + (bit >> 4);
		mask = 1 << (bit & 0x0f);
		reg_read(tfa, (unsigned char)reg, &value);
	} else {
		return TFA_ERROR;
	}

	return (value & mask) != 0;
}

/*
 * interrupt bit function that operates on the shadow regs in the handle
 */
int tfa_irq_ena(struct tfa_device *tfa, int bit, int state)
{
	uint16_t value, new_value;
	int reg = 0, mask;

	/* */
	if (bit == tfa->irq_all) {
		/* operate on all bits */
		for (reg = TFA98XX_INTERRUPT_ENABLE_REG;
			reg <= TFA98XX_INTERRUPT_ENABLE_REG
			+ tfa->irq_max / 16; reg++) {
			reg_write(tfa,
				(unsigned char)reg,
				state ? 0xffff : 0); /* all bits */
			tfa->interrupt_enable
				[reg - TFA98XX_INTERRUPT_ENABLE_REG]
				= state ? 0xffff : 0; /* all bits */
		}
	} else if (bit < tfa->irq_max) {
		/* only this bit */
		reg = TFA98XX_INTERRUPT_ENABLE_REG + (bit >> 4);
		mask = 1 << (bit & 0x0f);
		reg_read(tfa, (unsigned char)reg, &value);
		if (state) /* set */
			new_value = (uint16_t)(value | mask);
		else /* clear */
			new_value = value & ~mask;
		if (new_value != value) {
			reg_write(tfa,
				(unsigned char)reg,
				new_value); /* only this bit */
			tfa->interrupt_enable
				[reg - TFA98XX_INTERRUPT_ENABLE_REG]
				= new_value;
		}
	} else {
		return TFA_ERROR;
	}

	return 0;
}

/*
 * mask interrupts by disabling them
 */
int tfa_irq_mask(struct tfa_device *tfa)
{
	int reg;

	if (!tfa->dev_ops.get_mtpb) { /* new types have only 1 reg */
		reg_write(tfa, TFA98XX_INTERRUPT_ENABLE_REG, 0);
		return 0;
	}

	/* operate on all bits */
	for (reg = TFA98XX_INTERRUPT_ENABLE_REG;
		reg <= TFA98XX_INTERRUPT_ENABLE_REG
			+ tfa->irq_max / 16; reg++)
		reg_write(tfa, (unsigned char)reg, 0);

	return 0;
}

/*
 * unmask interrupts by enabling them again
 */
int tfa_irq_unmask(struct tfa_device *tfa)
{
	int reg;

	if (!tfa->dev_ops.get_mtpb) { /* new types have only 1 reg */
		reg_write(tfa, TFA98XX_INTERRUPT_ENABLE_REG,
			tfa->interrupt_enable[0]);
		return 0;
	}

	/* operate on all bits */
	for (reg = TFA98XX_INTERRUPT_ENABLE_REG;
		reg <= TFA98XX_INTERRUPT_ENABLE_REG
			+ tfa->irq_max / 16; reg++)
		reg_write(tfa, (unsigned char)reg,
			tfa->interrupt_enable
			[reg - TFA98XX_INTERRUPT_ENABLE_REG]);

	return 0;
}

/*
 * interrupt bit function that sets the polarity
 */
int tfa_irq_set_pol(struct tfa_device *tfa, int bit, int state)
{
	uint16_t value, new_value;
	int reg = 0, mask;

	if (bit == tfa->irq_all) {
		/* operate on all bits */
		for (reg = TFA98XX_INTERRUPT_POLARITY_REG;
			reg <= TFA98XX_INTERRUPT_POLARITY_REG
			+ tfa->irq_max / 16; reg++) {
			reg_write(tfa,
				(unsigned char)reg,
				state ? 0xffff : 0); /* all bits */
		}
	} else if (bit < tfa->irq_max) {
		/* only this bit */
		reg = TFA98XX_INTERRUPT_POLARITY_REG + (bit >> 4);
		mask = 1 << (bit & 0x0f);
		reg_read(tfa, (unsigned char)reg, &value);
		if (state) /* Active High */
			new_value = (uint16_t)(value | mask);
		else /* Active Low */
			new_value = value & ~mask;
		if (new_value != value)
			reg_write(tfa, (unsigned char)reg, new_value);
			/* only this bit */
	} else {
		return TFA_ERROR;
	}

	return 0;
}

/*
 * initialize interrupt registers (for single IRQ register)
 */
void tfa_irq_init(struct tfa_device *tfa)
{
	unsigned short irqmask = tfa->interrupt_enable[0];

	/* disable interrupts */
	reg_write(tfa, TFA98XX_INTERRUPT_ENABLE_REG, 0);
	/* clear all active interrupts */
	reg_write(tfa, TFA98XX_INTERRUPT_IN_REG, 0xffff);
	/* enable interrupts */
	reg_write(tfa, TFA98XX_INTERRUPT_ENABLE_REG, irqmask);
}

/*
 * report interrupt status (for single IRQ register)
 * mask and unmask is handled outside this function
 */
int tfa_irq_report(struct tfa_device *tfa)
{
	uint16_t irqmask, irqstatus, activemask;
	int irq, rc;
	struct tfa_device *tfa0 = NULL;
	int group;

	/* set head device */
	tfa0 = tfa98xx_get_tfa_device_from_index(-1);

	/* get status bits */
	rc = reg_read(tfa, TFA98XX_INTERRUPT_OUT_REG, &irqstatus);
	if (rc < 0)
		return -rc;

	/* get the saved mask */
	irqmask = tfa->interrupt_enable[0];
	activemask = irqmask & irqstatus;

	for (irq = 0; irq < tfa->irq_max; irq++)
		if (activemask & (1 << irq)) {
			pr_err("%s: device[%d]: interrupt: %s - %s\n",
				__func__, tfa->dev_idx,
				tfa98xx_irq_names[irq].irq_name,
				tfa98xx_irq_info[irq]);
			if (irq != tfa9866_irq_stnoclk
				|| !tfa->blackbox_enable || tfa0 == NULL)
				continue;
			group = tfa->dev_idx * ID_BLACKBOX_MAX;
			tfa0->log_data[group + ID_NOCLK_COUNT]++;
		}

	/* clear active irqs */
	reg_write(tfa, TFA98XX_INTERRUPT_IN_REG, activemask);

	return 0;
}

/*
 * set device info and register device ops
 */
void tfa_set_query_info(struct tfa_device *tfa)
{
	/* invalidate device struct cached values */
	tfa->hw_feature_bits = -1;
	tfa->sw_feature_bits[0] = -1;
	tfa->sw_feature_bits[1] = -1;
	tfa->profile = -1;
	tfa->next_profile = -1;
	tfa->vstep = -1;
	/* defaults */
	tfa->is_probus_device = 0;
	tfa->advance_keys_handling = 0; /*artf65038*/
	tfa->tfa_family = 2;
	tfa->daimap = TFA98XX_DAI_I2S;		/* all others */
	tfa->spkr_count = 1;
	tfa->spkr_select = 0;
	tfa->spkr_damaged = 0;
	tfa->support_tcoef = SUPPORT_YES;
	tfa->support_drc = SUPPORT_NOT_SET;
	/* respond to external DSP: -1:none, 0:no_dsp, 1:cold, 2:warm */
	tfa->ext_dsp = -1;
	tfa->bus = 0;
	tfa->convert_dsp32 = 1; /* conversion in kernel */
	tfa->sync_iv_delay = 0;

	tfa->temp = 0xffff;
	tfa->is_cold = 1;
	tfa->is_bypass = 0;
	tfa->is_configured = 0;
	tfa->mtpex = -1;
	tfa->reset_mtpex = 0;
	tfa->stream_state = 0;
	tfa->first_after_boot = 1;
	tfa->active_handle = 0xf;
	tfa->active_count = -1;
	tfa->swprof = -1;
	tfa->ampgain = -1;
	tfa->ramp_steps = 0; /* default (voided if negative) */
	tfa->individual_msg = 0;
	tfa->fw_itf_ver[0] = 0xff;
	tfa->fw_lib_ver[0] = 0xff;
	tfa->blackbox_enable = 1;
	tfa->blackbox_config_msg = 0;
	tfa->unset_log = 0;
	memset(tfa->log_data, 0, LOG_BUFFER_SIZE * sizeof(int));
	tfa->irq_all = 0;
	tfa->irq_max = 0;

	/* TODO use the getfeatures() for retrieving the features [artf103523]
	 * tfa->support_drc = SUPPORT_NOT_SET;
	 */

	pr_info("%s: device type (0x%04x)\n", __func__, tfa->rev);
	switch (tfa->rev & 0xff) {
	case 0: /* tfanone : non-i2c external DSP device */
		/* e.g. qc adsp */
		tfa->support_drc = SUPPORT_YES;
		tfa->tfa_family = 0;
		tfa->spkr_count = 0;
		tfa->daimap = 0;
		/* register device operations via tfa hal */
		tfanone_ops(&tfa->dev_ops);
		tfa->bus = 1;
		break;
	case 0x66:
		/* tfa986x */
		tfa->support_drc = SUPPORT_YES;
		tfa->tfa_family = 2;
		tfa->spkr_count = 1;
		tfa->is_probus_device = 1;
		tfa->ext_dsp = 1; /* set DSP-free by force */
		tfa->advance_keys_handling = 1; /*artf65038*/
		tfa->daimap = TFA98XX_DAI_TDM;
		tfa986x_ops(&tfa->dev_ops); /* register device operations */
		tfa->irq_all = tfa9866_irq_all;
		tfa->irq_max = tfa9866_irq_max;
		break;
	default:
		pr_err("unknown device type : 0x%02x\n", tfa->rev & 0xff);
		_ASSERT(0);
		break;
	}
}

enum tfa98xx_error tfa_buffer_pool(struct tfa_device *tfa,
	int index, int size, int control)
{
	struct tfa_device *tfa0 = NULL;

	/* set main device */
	tfa0 = tfa98xx_get_tfa_device_from_index(-1);

	if (tfa->dev_idx != tfa0->dev_idx) {
		pr_err("%s: failed to set main device (%d)\n",
			__func__, tfa->dev_idx);
		return TFA_ERROR;
	}

	switch (control) {
	case POOL_ALLOC: /* allocate */
		tfa->buf_pool[index].pool =
			kmalloc(size, GFP_KERNEL);
		if (tfa->buf_pool[index].pool == NULL) {
			tfa->buf_pool[index].size = 0;
			tfa->buf_pool[index].in_use = 1;
			pr_err("%s: buffer_pool[%d] - kmalloc error %d bytes\n",
				__func__, index, size);
			return TFA98XX_ERROR_FAIL;
		}
		pr_debug("%s: buffer_pool[%d] - kmalloc allocated %d bytes\n",
			__func__, index, size);
		tfa->buf_pool[index].size = size;
		tfa->buf_pool[index].in_use = 0;
		break;

	case POOL_FREE: /* deallocate */
		if (tfa->buf_pool[index].pool != NULL)
			kfree(tfa->buf_pool[index].pool);
		pr_debug("%s: buffer_pool[%d] - kfree\n",
			__func__, index);
		tfa->buf_pool[index].pool = NULL;
		tfa->buf_pool[index].size = 0;
		tfa->buf_pool[index].in_use = 0;
		break;

	default:
		pr_err("%s: wrong control\n", __func__);
		break;
	}

	return TFA98XX_ERROR_OK;
}

int tfa98xx_buffer_pool_access(int r_index,
	size_t g_size, uint8_t **buf, int control)
{
	int index;
	struct tfa_device *tfa = NULL;

	/* set main device */
	tfa = tfa98xx_get_tfa_device_from_index(-1);
	if (tfa == NULL) {
		pr_err("%s: failed to get main device\n",
			__func__);
		return TFA_ERROR;
	}

	switch (control) {
	case POOL_GET: /* get */
		if (tfa->verbose)
			pr_debug("%s: dev %d, request buffer_pool, size=%d\n",
				__func__, tfa->dev_idx, (int)g_size);
		*buf = NULL;
		for (index = 0; index < POOL_MAX_INDEX; index++) {
			if (tfa->buf_pool[index].in_use) {
				if (tfa->verbose)
					pr_debug("%s: buffer_pool[%d] is in use\n",
						__func__, index);
				continue;
			}
			if (tfa->buf_pool[index].size < (int)g_size) {
				if (tfa->verbose)
					pr_debug("%s: buffer_pool[%d] size %d < %d\n",
						__func__, index,
						tfa->buf_pool[index].size,
						(int)g_size);
				continue;
			}
			*buf = (uint8_t *)(tfa->buf_pool[index].pool);
			if (*buf == NULL) {
				pr_err("%s: found NULL in buffer_pool[%d]\n",
					__func__, index);
				continue;
			}
			tfa->buf_pool[index].in_use = 1;
			if (tfa->verbose)
				pr_debug("%s: get buffer_pool[%d]\n",
					__func__, index);
			return index;
		}

		pr_err("%s: failed to get buffer_pool\n",
			__func__);
		break;

	case POOL_RETURN: /* return */
		if (tfa->verbose)
			pr_debug("%s: dev %d, release buffer_pool[%d]\n",
				__func__, tfa->dev_idx, r_index);
		if (r_index < 0 || r_index >= POOL_MAX_INDEX) {
			pr_err("%s: out of range [%d]\n", __func__, r_index);
			return TFA_ERROR;
		}
		if (tfa->buf_pool[r_index].in_use == 0
			|| tfa->buf_pool[r_index].size == 0) {
			if (tfa->verbose)
				pr_debug("%s: buffer_pool[%d] is not in use\n",
					__func__, r_index);
			return TFA_ERROR; /* reset by force */
		}

		if (tfa->verbose)
			pr_debug("%s: return buffer_pool[%d]\n",
				__func__, r_index);
		memset(tfa->buf_pool[r_index].pool,
			0, tfa->buf_pool[r_index].size);
		tfa->buf_pool[r_index].in_use = 0;
		r_index = -1;

		return r_index;

	default:
		pr_err("%s: wrong control\n", __func__);
		break;
	}

	return TFA_ERROR;
}

/************************ query functions *******************************/
/* no device involved
 * return revision
 * Used by the LTT
 */
void tfa98xx_rev(int *major, int *minor, int *revision)
{
	char version_str[] = TFA98XX_API_REV_STR;
	char residual[20] = {'\0'};
	int ret;

	ret = sscanf(version_str, "v%d.%d.%d-%s",
		major, minor, revision, residual);
	if (ret != 4)
		pr_err("%s: failure reading API revision", __func__);
}

/*
 * tfa_supported_speakers
 * returns the number of the supported speaker count
 */
enum tfa98xx_error tfa_supported_speakers(struct tfa_device *tfa,
	int *spkr_count)
{
	if (tfa->in_use == 0)
		return TFA98XX_ERROR_NOT_OPEN;

	if (spkr_count)
		*spkr_count = tfa->spkr_count;

	return TFA98XX_ERROR_OK;
}

/*
 * tfa_get_channel_from_dev_idx
 * returns the channel of TDMSPKS for the device of given index
 */
int tfa_get_channel_from_dev_idx(struct tfa_device *tfa, int dev_idx)
{
	int channel = TFA_NOT_FOUND;
	int active_handle = 0, idx = 0;
	static int channelset[MAX_HANDLES] = {
		TFA_NOT_FOUND, TFA_NOT_FOUND, TFA_NOT_FOUND, TFA_NOT_FOUND
	};

	if (tfa == NULL) {
		active_handle = dev_idx;
		/* acive channel is -1 @ active_handle == 0xf */
		if (active_handle == 0 || active_handle == 0xf)
			return TFA_NOT_FOUND; /* device is not selected */

		for (idx = 0; idx < MAX_HANDLES; idx++) {
			if (!(active_handle & (1 << idx)))
				continue;
			tfa = tfa98xx_get_tfa_device_from_index(idx);
			break;
		}
		if (tfa == NULL)
			return TFA_NOT_FOUND; /* unable to find device */
		dev_idx = idx;
	} else {
		dev_idx = tfa->dev_idx;
	}

	if (channelset[dev_idx] != TFA_NOT_FOUND)
		return channelset[dev_idx];

	channel = (int)tfa98xx_get_cnt_bitfield(tfa,
		TFAxx_FAM(TDMSPKS)) % MAX_CHANNELS;
	channelset[dev_idx] = channel;

	return channel;
}

/*
 * tfa_get_dev_idx_from_inchannel
 * returns index of device which matches with given inchannel
 * (0: left (or mono) / 1: right)
 */
int tfa_get_dev_idx_from_inchannel(int inchannel)
{
	struct tfa_device *ntfa;
	int i;
	static int devset[MAX_CHANNELS] = {
		TFA_NOT_FOUND, TFA_NOT_FOUND
	};

	if (inchannel < 0 || inchannel >= MAX_CHANNELS)
		return TFA_NOT_FOUND; /* invalid inchannel */

	if (devset[inchannel] != TFA_NOT_FOUND)
		return devset[inchannel];

	for (i = 0; i < MAX_HANDLES; i++) {
		ntfa = tfa98xx_get_tfa_device_from_index(i);

		if (ntfa == NULL)
			continue;

		if (ntfa->inchannel == inchannel) {
			devset[inchannel] = ntfa->dev_idx;
			return ntfa->dev_idx; /* defined */
		}

		if (i >= ntfa->dev_count - 1)
			break;
	}

	/* if undefined */
	devset[inchannel] = TFA_INCHANNEL(inchannel);
	return devset[inchannel];
}
EXPORT_SYMBOL(tfa_get_dev_idx_from_inchannel);

/*
 * tfa98xx_set_stream_state
 * sets the stream: b0: pstream (Rx), b1: cstream (Tx)
 */
enum tfa98xx_error tfa98xx_set_stream_state(struct tfa_device *tfa,
	int stream_state)
{
	pr_debug("%s: set stream_state=0x%04x\n", __func__, stream_state);

	tfa->stream_state = stream_state;

	return TFA98XX_ERROR_OK;
}

/*************************** device specific ops ***************************/
/* the wrapper for DspReset, in case of full */
enum tfa98xx_error tfa98xx_dsp_reset(struct tfa_device *tfa, int state)
{
	if (!tfa->dev_ops.dsp_reset)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.dsp_reset)(tfa, state);
}

/* the ops wrapper for tfa98xx_dsp_SystemStable */
enum tfa98xx_error tfa98xx_dsp_system_stable(struct tfa_device *tfa,
	int *ready)
{
	if (!tfa->dev_ops.dsp_system_stable)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.dsp_system_stable)(tfa, ready);
}

/* the ops wrapper for tfa98xx_faim_protect */
enum tfa98xx_error tfa98xx_faim_protect(struct tfa_device *tfa, int state)
{
	if (!tfa->dev_ops.faim_protect)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.faim_protect)(tfa, state);
}

enum tfa98xx_error tfa98xx_dsp_write_tables(struct tfa_device *tfa,
	int sample_rate)
{
	if (!tfa->dev_ops.dsp_write_tables)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.dsp_write_tables)(tfa, sample_rate);
}

/* set internal oscillator into power down mode.
 *
 * @param[in] tfa device description structure
 * @param[in] state new state 0 - oscillator is on, 1 oscillator is off.
 *
 * @return TFA98XX_ERROR_OK when successful, error otherwise.
 */
enum tfa98xx_error tfa98xx_set_osc_powerdown(struct tfa_device *tfa, int state)
{
	if (!tfa->dev_ops.set_osc_powerdown)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.set_osc_powerdown)(tfa, state);
}

/* update low power mode of the device.
 *
 * @param[in] tfa device description structure
 * @param[in] state new state 0 - LPMODE is on, 1 LPMODE is off.
 *
 * @return TFA98XX_ERROR_OK when successful, error otherwise.
 */
enum tfa98xx_error tfa98xx_update_lpm(struct tfa_device *tfa, int state)
{
	if (!tfa->dev_ops.update_lpm)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.update_lpm)(tfa, state);
}

/*
 * bring the device into a state similar to reset
 */
enum tfa98xx_error tfa98xx_init(struct tfa_device *tfa)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	uint16_t value = 0;

	/* reset all i2C registers to default
	 * Write the register directly to avoid read in the bitfield function.
	 * The I2CR bit may overwrite full register because it is reset anyway.
	 * This will save a reg read transaction.
	 */
	TFA_SET_BF_VALUE(tfa, I2CR, 1, &value);
	TFA_WRITE_REG(tfa, I2CR, value);

	/* some other registers must be set for optimal amplifier behaviour
	 * This is implemented in a file specific for the type number
	 */
	if (tfa->dev_ops.tfa_init) {
		pr_debug("%s: device specific (0x%04x:0x%08x)\n",
			__func__, tfa->rev, tfa->revid);
		error = (tfa->dev_ops.tfa_init)(tfa);
	} else {
		pr_debug("%s: no init code\n", __func__);
	}

	return error;
}

/*
 * lock or unlock KEY2
 * lock = 1 will lock
 * lock = 0 will unlock
 * note that on return all the hidden key will be off
 */
void tfa98xx_key2(struct tfa_device *tfa, int lock)
{
	/* unhide lock registers */
	reg_write(tfa, (tfa->tfa_family == 1) ? 0x40 : 0x0f, 0x5a6b);
	/* lock/unlock key2 MTPK */
	//TFA_WRITE_REG(tfa, MTPKEY2, lock ? 0 : 0x5a);
	/* unhide lock registers */
	if (!tfa->advance_keys_handling) /*artf65038*/
		reg_write(tfa, (tfa->tfa_family == 1) ? 0x40 : 0x0f, 0);
}

static short twos(short x)
{
	return (x < 0) ? x + 512 : x;
}

void tfa98xx_set_exttemp(struct tfa_device *tfa, short ext_temp)
{
	/* for FW_PAR_ID_SET_CHIP_TEMP_SELECTOR */
	tfa->temp = ext_temp;
}

short tfa98xx_get_exttemp(struct tfa_device *tfa)
{
	return twos(tfa->temp);
}

/************* tfa simple bitfield interfacing ********************/
/* convenience functions */
enum tfa98xx_error tfa98xx_set_volume_level(struct tfa_device *tfa,
	unsigned short vol)
{
	if (tfa->in_use == 0)
		return TFA98XX_ERROR_NOT_OPEN;

	/* void volume setting via I2C on probus device */
	return TFA98XX_ERROR_OK;
}

static enum tfa98xx_error
tfa98xx_set_mute_tfa2(struct tfa_device *tfa, enum tfa98xx_mute mute)
{
	enum tfa98xx_error error;

	if (tfa->dev_ops.set_mute == NULL)
		return TFA98XX_ERROR_NOT_SUPPORTED;

	switch (mute) {
	case TFA98XX_MUTE_OFF:
		error = tfa->dev_ops.set_mute(tfa, 0);
		TFA_SET_BF(tfa, AMPE, 1);
		break;
	case TFA98XX_MUTE_AMPLIFIER:
	case TFA98XX_MUTE_DIGITAL:
		error = tfa->dev_ops.set_mute(tfa, 1);
		TFA_SET_BF(tfa, AMPE, 0);
		break;
	default:
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	return error;
}

enum tfa98xx_error
tfa98xx_set_mute(struct tfa_device *tfa, enum tfa98xx_mute mute)
{
	int cur_ampe;

	if (tfa->in_use == 0) {
		pr_err("device is not opened\n");
		return TFA98XX_ERROR_NOT_OPEN;
	}

	cur_ampe = TFA_GET_BF(tfa, AMPE);
	if ((mute == TFA98XX_MUTE_OFF && cur_ampe == 1)
		|| (mute == TFA98XX_MUTE_AMPLIFIER && cur_ampe == 0)) {
		pr_info("%s: skip mute request (%d, AMPE %d)\n",
			__func__, mute, cur_ampe);
		return TFA98XX_ERROR_OK;
	}

	return tfa98xx_set_mute_tfa2(tfa, mute);
}

/****************** patching ******************/

/* the patch contains a header with the following
 * IC revision register: 1 byte, 0xff means don't care
 * XMEM address to check: 2 bytes, big endian, 0xffff means don't care
 * XMEM value to expect: 3 bytes, big endian
 */
static enum tfa98xx_error
tfa98xx_check_ic_rom_version(struct tfa_device *tfa,
	const unsigned char patchheader[])
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned int revid, devid;
	unsigned short checkrev, msb_revid, lsb_revid;
	unsigned short checkaddress;
	int checkvalue;

	lsb_revid = patchheader[0];
	msb_revid = patchheader[4] << 8 | patchheader[5];
	checkrev = tfa->rev & 0xff; /* only compare lower byte */

	if ((lsb_revid != 0xff) && (checkrev != lsb_revid))
		return TFA98XX_ERROR_NOT_SUPPORTED;

	if ((msb_revid & 0xf) <= 0x5)
		msb_revid = msb_revid + 0x0a;
		/* in case the patch file contains numbers instead of a-f */

	checkaddress = (patchheader[1] << 8) + patchheader[2];
	checkvalue =
		(patchheader[3] << 16) + (patchheader[4] << 8) + msb_revid;
	if (checkaddress != 0xffff) {
		pr_err("patch file is not compatible with probus_device!\n");
	} else { /* == 0xffff */
		/* check if the revid subtype is in there */
		if (checkvalue != 0xffffff && checkvalue != 0) {
			devid = tfa_cont_get_devid(tfa->cnt, tfa->dev_idx);
			revid = ((unsigned int)msb_revid << 8)
				| lsb_revid; /* full revid */
			/* full revid */
			if (devid != tfa->revid && revid != tfa->revid) {
				pr_err("container patch and HW mismatch: expected: 0x%08x, actual 0x%08x\n",
					tfa->revid, revid);
				return TFA98XX_ERROR_NOT_SUPPORTED;
			}

			pr_info("%s: container patch: 0x%08x:0x%08x\n",
				__func__, revid, devid);
		}
	}

	return error;
}

#define PATCH_HEADER_LENGTH 6
enum tfa98xx_error
tfa_dsp_patch(struct tfa_device *tfa, int patch_length,
	const unsigned char *patch_bytes)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	if (tfa->in_use == 0)
		return TFA98XX_ERROR_NOT_OPEN;

	if (patch_length < PATCH_HEADER_LENGTH)
		return TFA98XX_ERROR_BAD_PARAMETER;

	tfa98xx_check_ic_rom_version(tfa, patch_bytes);

	return error;
}
/****************** end patching ******************/

/*
 * support functions for data conversion
 * convert memory bytes to signed 24 bit integers
 *	input:  bytes contains "num_bytes" byte elements
 *	output: data contains "num_bytes/3" int24 elements
 */
void tfa98xx_convert_bytes2data(int num_bytes,
	const unsigned char bytes[], int data[])
{
	int i;			/* index for data */
	int k;			/* index for bytes */
	int d;
	int num_data = num_bytes / 3;

	_ASSERT((num_bytes % 3) == 0);

	i = 0;
	k = 0;

	for (; i < num_data; ++i, k += 3) {
		d = (bytes[k] << 16) | (bytes[k + 1] << 8) | (bytes[k + 2]);
		_ASSERT(d >= 0);
		_ASSERT(d < (1 << 24));	/* max 24 bits in use */
		if (bytes[k] & 0x80)	/* sign bit was set */
			d = -((1 << 24) - d);

		data[i] = d;
	}
}

/*
 * convert signed 32 bit integers to 24 bit aligned bytes
 *	input:   data contains "num_data" int elements
 *	output:  bytes contains "3 * num_data" byte elements
 */
void tfa98xx_convert_data2bytes(int num_data, const int data[],
	unsigned char bytes[])
{
	int i; /* index for data */
	int k; /* index for bytes */
	int d;

	/* note: cannot just take the lowest 3 bytes from the 32 bit
	 * integer, because also need to take care of clipping any
	 * value > 2 & 23
	 */
	for (i = 0, k = 0; i < num_data; ++i, k += 3) {
		if (data[i] >= 0)
			d = MIN(data[i], (1 << 23) - 1);
		else {
			/* 2's complement */
			d = (1 << 24) - MIN(-data[i], 1 << 23);
		}
		_ASSERT(d >= 0);
		_ASSERT(d < (1 << 24));	/* max 24 bits in use */
		bytes[k] = (d >> 16) & 0xff;	/* MSB */
		bytes[k + 1] = (d >> 8) & 0xff;
		bytes[k + 2] = (d) & 0xff;	/* LSB */
	}
}

static enum tfa98xx_error _dsp_msg(struct tfa_device *tfa, int lastmessage)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	uint8_t *blob = NULL;
	int len;
	int buf_p_index = -1;

	buf_p_index = tfa98xx_buffer_pool_access
		(-1, 64 * 1024, &blob, POOL_GET);
	if (buf_p_index != -1) {
		pr_debug("%s: allocated from buffer_pool[%d] for 64 KB\n",
			__func__, buf_p_index);
	} else {
		/* max length is 64k */
		blob = kmalloc(64 * 1024, GFP_KERNEL);
		if (blob == NULL) {
			return TFA98XX_ERROR_FAIL;
		}
	}

	len = tfa_tib_dsp_msgmulti(tfa, -1, (const char *)blob);
	if (tfa->verbose)
		pr_debug("%s: send multi-message, length=%d (update at %s)\n",
			__func__, len,
			lastmessage ? "the last message" : "buffer full");
	if (len < 0) {
		error = len;
		goto _dsp_msg_exit;
	}

	/* send messages to the target selected */
	if (tfa98xx_count_active_stream(BIT_PSTREAM) > 0) {
		if (tfa->has_msg == 0) { /* via i2c/ipc */
			/* Send to the target selected */
			if (tfa->dev_ops.dsp_msg) {
				error = (tfa->dev_ops.dsp_msg)
					((void *)tfa, len, (const char *)blob);
				if (error != TFA98XX_ERROR_OK) {
					pr_err("%s: IPC error %d\n", __func__, error);
					error = TFA98XX_ERROR_OK;
				}
			} else {
				pr_err("%s: dsp_msg is NULL\n", __func__);
			}
		}
	} else {
		pr_info("%s: skip if PSTREAM is lost\n",
			__func__);
	}

_dsp_msg_exit:
	if (error != TFA98XX_ERROR_OK)
		pr_err("%s: error in sending messages (%d)\n",
			__func__, error);

	if (buf_p_index != -1)
		buf_p_index = tfa98xx_buffer_pool_access
			(buf_p_index, 0, &blob, POOL_RETURN);
	else
		kfree(blob);

	return error;
}

enum tfa98xx_error dsp_msg(struct tfa_device *tfa,
	int length24, const char *buf24)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	static int lastmessage;
	int i;
	int *intbuf = NULL;
	char *buf = (char *)buf24;
	int length = length24;

	if (tfa98xx_count_active_stream(BIT_PSTREAM) == 0) {
		pr_info("%s: skip if PSTREAM is lost\n", __func__);
		tfa->individual_msg = 0;
		return error;
	}

	if (tfa->convert_dsp32) {
		int idx = 0;

		length = 4 * length24 / 3;
		intbuf = kmem_cache_alloc(tfa->cachep, GFP_KERNEL);
		if (intbuf == NULL)
			return TFA98XX_ERROR_FAIL;
		buf = (char *)intbuf;

		/* convert 24 bit DSP messages to a 32 bit integer */
		for (i = 0; i < length24; i += 3) {
			int tmp = (buf24[i] << 16)
				+ (buf24[i + 1] << 8) + buf24[i + 2];

			/* Sign extend to 32-bit from 24-bit */
			intbuf[idx++] = ((int32_t)tmp << 8) >> 8;
		}
	}

	/* Only create multi-msg when the dsp is cold */
	if (tfa->ext_dsp == 1) {
		/* Creating the multi-msg */
		error = tfa_tib_dsp_msgmulti(tfa, length, buf);
		if (error == TFA_ERROR)
			goto dsp_msg_error_exit;

		/* if buffer is full we need to send the existing message
		 * and add the current message
		 */
		if (error == TFA98XX_ERROR_BUFFER_TOO_SMALL) {
			/* (a) send the existing (full) message */
			error = _dsp_msg(tfa, lastmessage);

			/* (b) add to a new multi-message */
			error = tfa_tib_dsp_msgmulti(tfa, length, buf);
			if (error == TFA_ERROR)
				goto dsp_msg_error_exit;
		}

		lastmessage = error;

		/* At the last message, send the multi-msg to the target */
		if (lastmessage == 1) {
			/* Get the full multi-msg data */
			error = _dsp_msg(tfa, lastmessage);

			/* reset to re-start */
			lastmessage = 0;
		}
	} else {
		if (tfa98xx_count_active_stream(BIT_PSTREAM) > 0) {
			if (tfa->has_msg == 0) { /* via i2c/ipc */
				if (tfa->dev_ops.dsp_msg)
					error = (tfa->dev_ops.dsp_msg)
						((void *)tfa,
						length,
						(const char *)buf);
			}
		} else {
			pr_info("%s: skip if PSTREAM is lost\n",
				__func__);
		}
	}

	if (error != TFA98XX_ERROR_OK) {
		/* Get actual error code from softDSP */
		//error = (enum tfa98xx_error)(error + TFA98XX_ERROR_BUFFER_RPC_BASE);
		pr_err("%s: IPC error %d\n", __func__, error);
		error = TFA98XX_ERROR_OK;
	}

	/* DSP verbose has argument 0x04 */
	if ((tfa->verbose & 0x04) != 0) {
		pr_debug("DSP W [%d]: ", length);
		for (i = 0; i < ((length > 3) ? 3 : length); i++)
			pr_debug("0x%02x ", (uint8_t)buf[i]);
		pr_debug("\n");
	}

dsp_msg_error_exit:
	tfa->individual_msg = 0;

	if (tfa->convert_dsp32)
		kmem_cache_free(tfa->cachep, intbuf);

	return error;
}

enum tfa98xx_error dsp_msg_read(struct tfa_device *tfa,
	int length24, unsigned char *bytes24)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int i;
	int length = length24;
	unsigned char *bytes = bytes24;

	if (tfa98xx_count_active_stream(BIT_PSTREAM) == 0) {
		pr_info("%s: skip if PSTREAM is lost\n", __func__);
		tfa->individual_msg = 0;
		return error;
	}

	if (tfa->convert_dsp32) {
		length = 4 * length24 / 3;
		bytes = kmem_cache_alloc(tfa->cachep, GFP_KERNEL);
		if (bytes == NULL)
			return TFA98XX_ERROR_FAIL;
	}

	if (tfa->has_msg == 0) { /* via i2c/ipc */
		if (tfa->dev_ops.dsp_msg_read)
			error = (tfa->dev_ops.dsp_msg_read)((void *)tfa,
				length, bytes);
	}
	if (error == TFA98XX_ERROR_OK)
		pr_debug("%s: OK\n", __func__);
	else {
		/* Get actual error code from softDSP */
		//error = (enum tfa98xx_error)(error + TFA98XX_ERROR_BUFFER_RPC_BASE);
		pr_err("%s: IPC error %d\n", __func__, error);
		error = TFA98XX_ERROR_OK;
	}

	/* DSP verbose has argument 0x04 */
	if ((tfa->verbose & 0x04) != 0) {
		pr_debug("DSP R [%d]: ", length);
		for (i = 0; i < ((length > 3) ? 3 : length); i++)
			pr_debug("0x%02x ", (uint8_t)bytes[i]);
		pr_debug("\n");
	}

	if (tfa->convert_dsp32) {
		int idx = 0;

		/* convert 32 bit LE to 24 bit BE */
		for (i = 0; i < length; i += 4) {
			bytes24[idx++] = bytes[i + 2];
			bytes24[idx++] = bytes[i + 1];
			bytes24[idx++] = bytes[i + 0];
		}
	}

	tfa->individual_msg = 0;

	if (tfa->convert_dsp32)
		kmem_cache_free(tfa->cachep, bytes);

	return error;
}

enum tfa98xx_error reg_read(struct tfa_device *tfa,
	unsigned char subaddress, unsigned short *value)
{
	enum tfa98xx_error error;

	error = (tfa->dev_ops.reg_read)(tfa, subaddress, value);
	if (error != TFA98XX_ERROR_OK)
		/* Get actual error code from softDSP */
		error = (enum tfa98xx_error)
			(error + TFA98XX_ERROR_BUFFER_RPC_BASE);

	return error;
}

enum tfa98xx_error reg_write(struct tfa_device *tfa,
	unsigned char subaddress, unsigned short value)
{
	enum tfa98xx_error error;

	error = (tfa->dev_ops.reg_write)(tfa, subaddress, value);
	if (error != TFA98XX_ERROR_OK)
		/* Get actual error code from softDSP */
		error = (enum tfa98xx_error)
			(error + TFA98XX_ERROR_BUFFER_RPC_BASE);

	return error;
}

int tfa_dsp_msg_rpc(void *data, int length, const char *buf)
{
	return TFA98XX_ERROR_NOT_SUPPORTED;
}

int tfa_dsp_msg_read_rpc(void *data, int length, unsigned char *bytes)
{
	return TFA98XX_ERROR_NOT_SUPPORTED;
}

enum tfa98xx_error
tfa_cont_write_filterbank(struct tfa_device *tfa, struct tfa_filter *filter)
{
	unsigned char biquad_index;
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	for (biquad_index = 0; biquad_index < 10; biquad_index++) {
		if (filter[biquad_index].enabled)
			error = tfa_dsp_cmd_id_write(tfa,
				MODULE_BIQUADFILTERBANK,
				biquad_index + 1, /* start @1 */
				sizeof(filter[biquad_index].biquad.bytes),
				filter[biquad_index].biquad.bytes);
		else
			error = tfa98xx_dsp_biquad_disable(tfa,
				biquad_index + 1);

		if (error)
			return error;
	}

	return error;
}

enum tfa98xx_error
tfa98xx_dsp_biquad_disable(struct tfa_device *tfa, int biquad_index)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int coeff_buffer[BIQUAD_COEFF_SIZE];
	unsigned char bytes[3 + BIQUAD_COEFF_SIZE * 3];
	int nr = 0;

	if (biquad_index > TFA98XX_BIQUAD_NUM
		|| biquad_index < 1)
		return TFA98XX_ERROR_BAD_PARAMETER;

	mutex_lock(&dsp_msg_lock);

	/* make opcode */
	bytes[nr++] = 0;
	bytes[nr++] = (0x80 | MODULE_BIQUADFILTERBANK);
	bytes[nr++] = (unsigned char)biquad_index;

	/* set in correct order and format for the DSP */
	coeff_buffer[0] = (int)-8388608;	/* -1.0f */
	coeff_buffer[1] = 0;
	coeff_buffer[2] = 0;
	coeff_buffer[3] = 0;
	coeff_buffer[4] = 0;
	coeff_buffer[5] = 0;

	/* convert to packed 24 */
	tfa98xx_convert_data2bytes(BIQUAD_COEFF_SIZE, coeff_buffer, &bytes[nr]);
	nr += BIQUAD_COEFF_SIZE * 3;

	error = dsp_msg(tfa, nr, (char *)bytes);

	mutex_unlock(&dsp_msg_lock);

	return error;
}

/* wrapper for dsp_msg that adds opcode */
enum tfa98xx_error tfa_dsp_cmd_id_write(struct tfa_device *tfa,
	unsigned char module_id,
	unsigned char param_id, int num_bytes,
	const unsigned char data[])
{
	enum tfa98xx_error error;
	unsigned char *buffer;
	int nr = 0;
	int buf_p_index = -1;

	mutex_lock(&dsp_msg_lock);

	buf_p_index = tfa98xx_buffer_pool_access
		(-1, num_bytes + 3, &buffer, POOL_GET);
	if (buf_p_index != -1)
		pr_debug("%s: allocated from buffer_pool[%d] for 0x%02x%02x (%d)\n",
			__func__, buf_p_index,
			(0x80 | module_id), param_id, num_bytes + 3);
	else
		buffer = kmalloc(num_bytes + 3, GFP_KERNEL);
	if (buffer == NULL) {
		mutex_unlock(&dsp_msg_lock);
		return TFA98XX_ERROR_FAIL;
	}

	buffer[nr++] = tfa->spkr_select;
	buffer[nr++] = (0x80 | module_id);
	buffer[nr++] = param_id;

	if (data != NULL && num_bytes > 0) {
		memcpy(&buffer[nr], data, num_bytes);
		nr += num_bytes;
	}

	error = dsp_msg(tfa, nr, (char *)buffer);

	if (buf_p_index != -1)
		buf_p_index = tfa98xx_buffer_pool_access
			(buf_p_index, 0, &buffer, POOL_RETURN);
	else
		kfree(buffer);

	mutex_unlock(&dsp_msg_lock);

	return error;
}

/* wrapper for dsp_msg that adds opcode */
/* this is as the former tfa98xx_dsp_get_param() */
enum tfa98xx_error tfa_dsp_cmd_id_write_read(struct tfa_device *tfa,
	unsigned char module_id,
	unsigned char param_id, int num_bytes,
	unsigned char data[])
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned char buffer[3];
	int nr = 0;

	if (num_bytes <= 0) {
		pr_debug("Error: The number of READ bytes is smaller or equal to 0!\n");
		return TFA98XX_ERROR_FAIL;
	}

	mutex_lock(&dsp_msg_lock);

	if ((tfa->is_probus_device) && (tfa->dev_count == 1)
		&& (param_id == SB_PARAM_GET_RE25C
		|| param_id == SB_PARAM_GET_LSMODEL
		|| param_id == SB_PARAM_GET_ALGO_PARAMS)) {
		/* Modifying the ID for GetRe25C */
		pr_debug("%s: CC bit: 4 (DS) for mono\n", __func__);
		/* CC: 4 (DS) for mono */
		buffer[nr++] = 4;
	} else {
		pr_debug("%s: CC bit: %d\n", __func__, tfa->spkr_select);
		/* CC: 0 (reset all) for stereo */
		buffer[nr++] = tfa->spkr_select;
	}

	buffer[nr++] = (0x80 | module_id);
	buffer[nr++] = param_id;

	tfa->individual_msg = 1;
	error = dsp_msg(tfa, nr, (char *)buffer);
	if (error != TFA98XX_ERROR_OK) {
		mutex_unlock(&dsp_msg_lock);
		return error;
	}

	/* read the data from the dsp */
	error = dsp_msg_read(tfa, num_bytes, data);

	mutex_unlock(&dsp_msg_lock);

	return error;
}

enum tfa98xx_error tfa98xx_powerdown(struct tfa_device *tfa, int powerdown)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	if (tfa->in_use == 0)
		return TFA98XX_ERROR_NOT_OPEN;

	error = TFA_SET_BF(tfa, PWDN, (uint16_t)powerdown);

	if (powerdown) {
		/* Workaround for ticket PLMA5337 */
		if (tfa->tfa_family == 2)
			TFA_SET_BF_VOLATILE(tfa, AMPE, 0);
	}

	return error;
}

enum tfa98xx_error
tfa98xx_select_mode(struct tfa_device *tfa, enum tfa98xx_mode mode)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	if (tfa->in_use == 0)
		return TFA98XX_ERROR_NOT_OPEN;

	if (error == TFA98XX_ERROR_OK) {
		switch (mode) {
		default:
			error = TFA98XX_ERROR_BAD_PARAMETER;
			break;
		}
	}

	return error;
}

int tfa_set_bf(struct tfa_device *tfa,
	const uint16_t bf, const uint16_t value)
{
	enum tfa98xx_error err;
	uint16_t regvalue, msk, oldvalue;
	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;
	uint8_t address = (bf >> 8) & 0xff;

	if (bf == -1) /* undefined bitfield */
		return 0;

	err = reg_read(tfa, address, &regvalue);
	if (err) {
		pr_err("Error getting bf :%d\n", -err);
		return -err;
	}

	oldvalue = regvalue;
	msk = ((1 << (len + 1)) - 1) << pos;
	regvalue &= ~msk;
	regvalue |= value << pos;

	/* Only write when the current register value is
	 * not the same as the new value
	 */
	if (oldvalue != regvalue) {
		err = reg_write(tfa, address, regvalue);
		if (err) {
			pr_err("Error setting bf :%d\n", -err);
			return -err;
		}
	}

	return 0;
}

int tfa_set_bf_volatile(struct tfa_device *tfa,
	const uint16_t bf, const uint16_t value)
{
	enum tfa98xx_error err;
	uint16_t regvalue, msk;
	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;
	uint8_t address = (bf >> 8) & 0xff;

	if (bf == -1) /* undefined bitfield */
		return 0;

	err = reg_read(tfa, address, &regvalue);
	if (err) {
		pr_err("Error getting bf :%d\n", -err);
		return -err;
	}

	msk = ((1 << (len + 1)) - 1) << pos;
	regvalue &= ~msk;
	regvalue |= value << pos;

	err = reg_write(tfa, address, regvalue);
	if (err) {
		pr_err("Error setting bf :%d\n", -err);
		return -err;
	}

	return 0;
}

int tfa_get_bf(struct tfa_device *tfa, const uint16_t bf)
{
	enum tfa98xx_error err;
	uint16_t regvalue, msk;
	uint16_t value;
	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;
	uint8_t address = (bf >> 8) & 0xff;

	if (bf == -1) /* undefined bitfield */
		return 0;

	err = reg_read(tfa, address, &regvalue);
	if (err) {
		pr_err("Error getting bf :%d\n", -err);
		return -err;
	}

	msk = ((1 << (len + 1)) - 1) << pos;
	regvalue &= msk;
	value = regvalue >> pos;

	return value;
}

int tfa_set_bf_value(const uint16_t bf,
	const uint16_t bf_value, uint16_t *p_reg_value)
{
	uint16_t regvalue, msk;
	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;

	if (bf == -1) /* undefined bitfield */
		return 0;

	regvalue = *p_reg_value;

	msk = ((1 << (len + 1)) - 1) << pos;
	regvalue &= ~msk;
	regvalue |= bf_value << pos;

	*p_reg_value = regvalue;

	return 0;
}

uint16_t tfa_get_bf_value(const uint16_t bf, const uint16_t reg_value)
{
	uint16_t msk, value;
	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;

	if (bf == -1) /* undefined bitfield */
		return 0;

	msk = ((1 << (len + 1)) - 1) << pos;
	value = (reg_value & msk) >> pos;

	return value;
}

int tfa_write_reg(struct tfa_device *tfa,
	const uint16_t bf, const uint16_t reg_value)
{
	enum tfa98xx_error err;
	/* bitfield enum - 8..15 : address */
	uint8_t address = (bf >> 8) & 0xff;

	if (bf == -1) /* undefined bitfield */
		return 0;

	err = reg_write(tfa, address, reg_value);
	if (err)
		return -err;

	return 0;
}

int tfa_read_reg(struct tfa_device *tfa, const uint16_t bf)
{
	enum tfa98xx_error err;
	uint16_t regvalue;
	/* bitfield enum - 8..15 : address */
	uint8_t address = (bf >> 8) & 0xff;

	if (bf == -1) /* undefined bitfield */
		return 0;

	err = reg_read(tfa, address, &regvalue);
	if (err)
		return -err;

	return regvalue;
}

/*
 * powerup the coolflux subsystem and wait for it
 */
enum tfa98xx_error tfa_cf_powerup(struct tfa_device *tfa)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int tries, status;

	/* power on the sub system */
	TFA_SET_BF_VOLATILE(tfa, PWDN, 0);

	/*
	 * wait until everything is stable,
	 * in case clock has been off
	 */
	if (tfa->verbose)
		pr_info("Waiting for DSP system stable...\n");

	for (tries = CFSTABLE_TRIES; tries > 0; tries--) {
		err = tfa98xx_dsp_system_stable(tfa, &status);
		_ASSERT(err == TFA98XX_ERROR_OK);
		if (status)
			break;

		/* wait 10ms to avoid busload */
		msleep_interruptible(BUSLOAD_INTERVAL);
	}

	if (tries == 0) { /* time out */
		pr_err("DSP subsystem start timed out\n");
		return TFA98XX_ERROR_STATE_TIMED_OUT;
	}

	return err;
}

/*
 * Print the current state of the hardware manager
 * Device manager status information, man_state from TFA9888_N1B_I2C_regmap_V12
 */
int tfa_get_manstate(struct tfa_device *tfa)
{
	int manstate = -1;

	if (tfa->tfa_family == 2)
		manstate = TFAxx_GET_BF(tfa, MANSTATE);

	return manstate;
}

enum tfa98xx_error tfa_show_current_state(struct tfa_device *tfa)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int manstate = -1;

	if (tfa->tfa_family == 2 && tfa->verbose) {
		manstate = tfa_get_manstate(tfa);
		if (manstate < 0)
			return -manstate;

		pr_info("%s: tfa (dev %d): current HW manager state: %d\n",
			__func__, tfa->dev_idx, manstate);

		switch (manstate) {
		case 0:
			pr_debug("%s: power_down_state\n",
				__func__);
			break;
		case 1:
			pr_debug("%s: wait_for_source_settings_state\n",
				__func__);
			break;
		case 2:
			pr_debug("%s: connnect_pll_input_state\n",
				__func__);
			break;
		case 3:
			pr_debug("%s: disconnect_pll_input_state\n",
				__func__);
			break;
		case 4:
			pr_debug("%s: enable_pll_state\n",
				__func__);
			break;
		case 5:
			pr_debug("%s: enable_cgu_state\n",
				__func__);
			break;
		case 6:
			pr_debug("%s: init_cf_state\n",
				__func__);
			break;
		case 7:
			pr_debug("%s: enable_amplifier_state\n",
				__func__);
			break;
		case 8:
			pr_debug("%s: alarm_state\n",
				__func__);
			break;
		case 9:
			pr_debug("%s: operating_state\n",
				__func__);
			break;
		case 10:
			pr_debug("%s: mute_audio_state\n",
				__func__);
			break;
		case 11:
			pr_debug("%s: disable_cgu_pll_state\n",
				__func__);
			break;
		default:
			pr_debug("%s: unable to find current state\n",
				__func__);
			break;
		}
	}

	return err;
}

#define VERSION_BIG_M_FILTER 0xff0000
#define VERSION_BIG_M_INDEX 16
#define VERSION_SMALL_M_FILTER 0x00ff00
#define VERSION_SMALL_M_INDEX 8
#define VERSION_BIG_U_FILTER 0x0000f8
#define VERSION_BIG_U_INDEX 3
#define VERSION_SMALL_U_FILTER 0x000007
#define VERSION_SMALL_U_INDEX 0
#define VERSION_BIG_U_FILTER_OLD 0x0000c0
#define VERSION_BIG_U_INDEX_OLD 6
#define VERSION_SMALL_U_FILTER_OLD 0x00003f
#define VERSION_SMALL_U_INDEX_OLD 0

enum tfa98xx_error tfa_get_fw_api_version(struct tfa_device *tfa,
	unsigned char *pfw_version)
{
	enum tfa98xx_error err = 0;
	int res_len = 3;
	unsigned char buf[3] = {0};
	int data[2], vitf;

	if (tfa == NULL)
		return TFA98XX_ERROR_BAD_PARAMETER;

	if (tfa->is_probus_device) {
		/* Read the API Value */
		res_len = 3;
		err = tfa_dsp_cmd_id_write_read(tfa,
			MODULE_FRAMEWORK,
			FW_PAR_ID_GET_API_VERSION,
			res_len, buf);
		if (err != 0) {
			pr_err("%s: failed to read value\n",
				__func__);
			return err;
		}
	}

	tfa98xx_convert_bytes2data(res_len, buf, data);
	vitf = data[0];

	pfw_version[0] = (vitf & VERSION_BIG_M_FILTER)
		>> VERSION_BIG_M_INDEX;
	pfw_version[1] = (vitf & VERSION_SMALL_M_FILTER)
		>> VERSION_SMALL_M_INDEX;
	if (pfw_version[0] != TFA_API_SBFW_BIG_M_88
		&& pfw_version[1] >= TFA_API_SBFW_9_00_00_SMALL_M) {
		pfw_version[2] = (vitf & VERSION_BIG_U_FILTER)
			>> VERSION_BIG_U_INDEX;
		pfw_version[3] = (vitf & VERSION_SMALL_U_FILTER)
			>> VERSION_SMALL_U_INDEX;
	} else {
		pfw_version[2] = (vitf & VERSION_BIG_U_FILTER_OLD)
			>> VERSION_BIG_U_INDEX_OLD;
		pfw_version[3] = (vitf & VERSION_SMALL_U_FILTER_OLD)
			>> VERSION_SMALL_U_INDEX_OLD;
	}

	/* Mono configuration */
	/*
	 * Workaround for FW 8.9.x (FW API x.31.y.z).
	 * Firmware cannot return the 4th field (mono/stereo),
	 * as it requires a certain set of messages
	 * before it can detect a mono/stereo configuration.
	 */
	if (tfa->dev_count == 1) {
		if ((pfw_version[0] == TFA_API_SBFW_8_09_00_BIG_M
			&& pfw_version[1] >= TFA_API_SBFW_8_09_00_SMALL_M)
			|| pfw_version[0] == TFA_API_SBFW_PO_BIG_M)
			pfw_version[3] = 1;
	}

	pr_info("%s: fw api (itf) version %d.%d.%d.%d\n",
		__func__, pfw_version[0], pfw_version[1],
		pfw_version[2], pfw_version[3]);

	return err;
}

enum tfa98xx_error tfa_get_fw_lib_version(struct tfa_device *tfa,
	unsigned char *plib_version, char *plib_residual)
{
	enum tfa98xx_error err = 0;
	int res_len;
	unsigned char buf[VERSION_STRING_LENGTH * 3] = {'\0'};
	char *version_word = VERSION_WORD;
	int i, j = 0, k = 0, l = 0;
	int num = 0;

	if (tfa == NULL)
		return TFA98XX_ERROR_BAD_PARAMETER;

	/* Read the LIB version string */
	res_len = VERSION_STRING_LENGTH * 3;
	err = tfa_dsp_cmd_id_write_read(tfa,
		MODULE_FRAMEWORK,
		FW_PAR_ID_GET_LIBRARY_VERSION,
		res_len, buf);
	if (err != 0) {
		pr_err("%s: failed to read value\n",
			__func__);
		return err;
	}

	for (i = 0; i < VERSION_STRING_LENGTH; i++) {
		char token = buf[i * 3 + 2];

		if (j < strlen(version_word)) {
			if (version_word[j] == token)
				j++;
			continue;
		}
		if (k >= 3) {
			plib_residual[l++] = token;
			if (token == 0)
				break;
			continue;
		}

		if (!(token >= '0' && token <= '9')
			&& token != '.' && token != '_'
			&& !(token == 0 && num > 0))
			continue;

		if ((token == '.' || token == '_' || token == 0)
			&& k < 3) {
			plib_version[k++] = num;
			num = 0;
			continue;
		}
		num = num * 10 + (token - '0');
	}

	if (l < VERSION_STRING_LENGTH)
		plib_residual[l] = '\0';

	if (k != 3) {
		pr_err("%s: invalid format for library version (%d entities)\n",
			__func__, k);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	pr_info("%s: %s library version %d.%d.%d %s\n",
		__func__, version_word,
		plib_version[0], plib_version[1], plib_version[2],
		plib_residual);

	return err;
}

/*
 * Write calibration values for probus / ext_dsp, to feed RE25C to algorithm
 */
enum tfa98xx_error tfa_set_calibration_values(struct tfa_device *tfa)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	unsigned char bytes[2 * 3] = {0};
	int value = 0;
	int active_channel = -1, channel = 0;
	static int need_cal, is_bypass, is_damaged;
	struct tfa_device *ntfa;
	int i;
	char reg_state[STAT_LEN] = {0};
	enum tfa_error ret = tfa_error_ok;

	/* at initial device only: to reset need_cal and is_bypass */
	if (tfa_count_status_flag(tfa, TFA_SET_CONFIG) == 0) {
		need_cal = 0;
		is_bypass = 0;
		is_damaged = 0;

		for (i = 0; i < tfa->dev_count; i++) {
			ntfa = tfa98xx_get_tfa_device_from_index(i);

			if (!tfa_is_active_device(ntfa))
				continue;

			if (ntfa->mtpex == -1)
				tfa_dev_mtp_get(ntfa, TFA_MTP_EX);
			need_cal |= (ntfa->mtpex == 0) ? 1 : 0;
			is_bypass |= ntfa->is_bypass;
			is_damaged |= ntfa->spkr_damaged;
		}

		pr_debug("%s: device %s calibrated; session %s; speaker %s\n",
			__func__,
			(need_cal) ? "needs to be" : "is already",
			(is_bypass) ? "runs in bypass" : "needs configuration",
			(is_damaged) ? "is damaged" : "has no problem");
	}

	tfa_set_status_flag(tfa, TFA_SET_CONFIG, 1);

	tfa->is_bypass = is_bypass;

	if (is_bypass
		|| (is_damaged && tfa->is_configured < 0)) {
		pr_info("%s: [%d] skip sending calibration data: bypass %d, damaged %d\n",
			__func__, tfa->dev_idx, is_bypass, is_damaged);

		tfa->unset_log = 1;

		/* CHECK: only once after buffering fully */
		/* at last device only: to reset and flush buffer */
		if (tfa_count_status_flag(tfa, TFA_SET_CONFIG)
			== tfa->active_count) {
			tfa_set_status_flag(tfa, TFA_SET_CONFIG, -1);

			if (tfa->ext_dsp == 1) {
				mutex_lock(&dsp_msg_lock);
				pr_info("%s: flush buffer in blob, in bypass\n",
					__func__);
				err = tfa_tib_dsp_msgmulti(tfa, -2, NULL);
				mutex_unlock(&dsp_msg_lock);
			}

			goto set_calibration_values_exit;
		}

		return err;
	}

	channel = tfa_get_channel_from_dev_idx(tfa, -1);
	pr_info("%s: dev %d, channel %d, MTPEX=%d\n",
		__func__, tfa->dev_idx, channel, tfa->mtpex);
	if (channel == -1)
		goto set_calibration_values_exit;

	value = tfa_dev_mtp_get(tfa, TFA_MTP_RE25);
	pr_info("%s: extract from MTP - %d mOhms\n", __func__, value);

	dsp_cal_value[channel] = TFA_ReZ_CALC(value, TFA_FW_ReZ_SHIFT);
	if (tfa->mtpex) {
		err = tfa_calibration_range_check(tfa, channel, value);
		if (err) {
			need_cal |= 1;
			err = TFA98XX_ERROR_OK;
			pr_info("%s: run calibration because of out-of-range\n",
				__func__);

			/* reset MTPEX to force calibration */
			ret = tfa_dev_mtp_set(tfa, TFA_MTP_EX, 0);
			if (ret != tfa_error_ok) {
				pr_err("%s: resetting MPTEX failed, device %d err (%d)\n",
					__func__, tfa->dev_idx, ret);
				tfa->reset_mtpex = 1;
			}
		}
	}
	if (value <= 0) /* run equivalent with calibration */
		need_cal |= 1;

	pr_info("%s: dev %d, channel %d - calibration data: %d [%s]\n",
		__func__, tfa->dev_idx, channel, value,
		(channel == 0) ? "Primary" : "Secondary");
	pr_info("%s: config_count=%d\n", __func__,
		tfa_count_status_flag(tfa, TFA_SET_CONFIG));

	if (need_cal) {
		/* check the configuration for cal profile */
		snprintf(reg_state, STAT_LEN, "DCA %d",
			TFA_GET_BF(tfa, DCA));
		snprintf(reg_state + strlen(reg_state),
			STAT_LEN - strlen(reg_state),
			", IPM %d", TFAxx_GET_BF(tfa, IPM));
		switch (tfa->rev & 0xff) {
		case 0x66:
			snprintf(reg_state + strlen(reg_state),
				STAT_LEN - strlen(reg_state),
				", LPM %d",
				TFAxx_GET_BF(tfa, LPM));
			snprintf(reg_state + strlen(reg_state),
				STAT_LEN - strlen(reg_state),
				", (MUSM %d",
				TFAxx_GET_BF(tfa, MUSMODE));
			snprintf(reg_state + strlen(reg_state),
				STAT_LEN - strlen(reg_state),
				", RCVM %d",
				tfa_get_bf(tfa, TFA9866_BF_RCVM));
			snprintf(reg_state + strlen(reg_state),
				STAT_LEN - strlen(reg_state),
				", LNM %d)",
				tfa_get_bf(tfa, TFA9866_BF_LNM));
			break;
		default:
			/* neither TFA987x */
			break;
		}
		pr_debug("%s: %s\n", __func__, reg_state);
	}

	tfa->unset_log = (need_cal) ? 1 : 0;

	if (need_cal == 1) {
		int tfa_state = tfa_dev_get_state(tfa);

		dsp_cal_value[channel] = -1;
		if (tfa_state != TFA_STATE_OPERATING) {
			pr_debug("%s: [%d] device is not ready though calibration is required\n",
				__func__, tfa->dev_idx);
			/* discount for retrial */
			tfa_set_status_flag(tfa, TFA_SET_CONFIG, 0);
			return err;
		}
	}

	/* trigger at the last device */
	if (tfa_count_status_flag(tfa, TFA_SET_CONFIG)
		< tfa->active_count) {
		pr_debug("%s: suspend setting calibration data till all devices are enabled\n",
			__func__);
		return err;
	}

	pr_info("%s: device count=%d, active count=%d\n",
		__func__, tfa->dev_count, tfa->active_count);

	/* If calibration is set to once we load from MTP, else send zero's */
	if (need_cal == 0) {
		pr_info("%s: last dev %d - MTPEX=%d\n",
			__func__, tfa->dev_idx, tfa->mtpex);

		if (tfa->dev_count == 1) /* mono: duplicate channel 0 */
			active_channel = 0;
		else if (tfa->dev_count >= 2) /* stereo and beyond */
			active_channel = tfa_get_channel_from_dev_idx(NULL,
				tfa->active_handle);

		if (active_channel != -1
			&& dsp_cal_value[active_channel] != -1) {
			pr_info("%s: copy cal from active dev 0x%x (channel %d)\n",
				__func__, tfa->active_handle,
				active_channel);
			for (i = 0; i < MAX_CHANNELS; i++)
				/* copy if not set */
				if (dsp_cal_value[i] == -1)
					dsp_cal_value[i]
						= dsp_cal_value[active_channel];
		}

		/* We have to copy it for both channels. Even when MONO! */
		bytes[0] = (uint8_t)((dsp_cal_value[0] >> 16) & 0xff);
		bytes[1] = (uint8_t)((dsp_cal_value[0] >> 8) & 0xff);
		bytes[2] = (uint8_t)(dsp_cal_value[0] & 0xff);

		bytes[3] = (uint8_t)((dsp_cal_value[1] >> 16) & 0xff);
		bytes[4] = (uint8_t)((dsp_cal_value[1] >> 8) & 0xff);
		bytes[5] = (uint8_t)(dsp_cal_value[1] & 0xff);

		if (tfa->blackbox_enable) {
			/* set logging once before configuring */
			pr_info("%s: set blackbox logging\n", __func__);
			tfa_configure_log(tfa->blackbox_enable);
		}
	} else { /* calibration is required */
		pr_info("%s: config ResetRe25C to do calibration\n", __func__);
		bytes[0] = 0;
		bytes[1] = 0;
		bytes[2] = 0;
		bytes[3] = 0;
		bytes[4] = 0;
		bytes[5] = 0;

		dsp_cal_value[0] = dsp_cal_value[1] = -1;

		/* force UNMUTE state before calibration, when INIT_CF done */
		for (i = 0; i < tfa->dev_count; i++) {
			ntfa = tfa98xx_get_tfa_device_from_index(i);

			if (ntfa == NULL)
				continue;

			ntfa->is_calibrating = 1;
			ntfa->spkr_damaged = 0; /* reset before calibration */

			pr_debug("%s: [%d] force UNMUTE before calibration\n",
				__func__, ntfa->dev_idx);
			tfa_dev_set_state(ntfa, TFA_STATE_UNMUTE, 1);
		}
	}

	tfa_set_status_flag(tfa, TFA_SET_CONFIG, -1);

	err = tfa_dsp_cmd_id_write
		(tfa, MODULE_SPEAKERBOOST, SB_PARAM_SET_RE25C,
		sizeof(bytes), bytes);
	if (err != TFA98XX_ERROR_OK)
		goto set_calibration_values_exit;

	if (need_cal == 0)
		goto set_calibration_values_exit;

	err = tfa_wait_cal(tfa);

set_calibration_values_exit:

	return err;
}

/*
 * Call tfa_set_calibration_values at once with loop
 */
enum tfa98xx_error tfa_set_calibration_values_once(struct tfa_device *tfa)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_device *ntfa;
	int i;

	/* first device or stopped */
	if (tfa_count_status_flag(tfa, TFA_SET_DEVICE) == 1
		|| tfa_count_status_flag(tfa, TFA_SET_CONFIG) > 0) {
		pr_info("%s: tfa_set_calibration_values\n", __func__);

		for (i = 0; i < tfa->dev_count; i++) {
			ntfa = tfa98xx_get_tfa_device_from_index(i);

			if (!tfa_is_active_device(ntfa))
				continue;

			ntfa->is_bypass = tfa->is_bypass;

			err = tfa_set_calibration_values(ntfa);
			if (err)
				pr_err("%s: dev %d, set calibration values error = %d\n",
					__func__, i, err);
		}
	}

	return err;
}

/*
 * start the speakerboost algorithm
 * this implies a full system startup when the system was not already started
 */
enum tfa98xx_error tfa_run_speaker_boost(struct tfa_device *tfa,
	int force, int profile)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int value;

	if (force) {
		tfa->is_cold = 1;
		err = tfa_run_coldstartup(tfa, profile);
		if (err)
			return err;
	}

	/* Returns 1 when device is "cold" and 0 when device is warm */
	value = tfa_is_cold(tfa);

	pr_info("%s: %s boot, ext_dsp = %d, profile = %d\n",
		__func__, value ? "cold" : "warm",
		tfa->ext_dsp, profile);

	pr_debug("%s: startup of device [%s]: %sstart\n",
		__func__, tfa_cont_device_name(tfa->cnt, tfa->dev_idx),
		value ? "cold" : "warm");

	/* CHECK: only once before buffering */
	/* at initial device only: to flush buffer */
	if (tfa_count_status_flag(tfa, TFA_SET_DEVICE) == 1) {
		/* flush message buffer */
		mutex_lock(&dsp_msg_lock);
		pr_debug("%s: flush buffer in blob, in cold start\n",
			__func__);
		err = tfa_tib_dsp_msgmulti(tfa, -2, NULL);
		mutex_unlock(&dsp_msg_lock);
	}

	/* cold start */
	if (value) {
		/* Run startup and write all files */
		pr_info("%s: cold start, speaker startup\n", __func__);
		err = tfa_run_speaker_startup(tfa, force, profile);
		if (err) {
			pr_info("%s: dev %d, speaker startup error = %d\n",
				__func__, tfa->dev_idx, err);
			return err;
		}

		/* Save the current profile and set the vstep to 0 */
		/* This needs to be overwritten even in CF bypass */
		tfa_dev_set_swprof(tfa, (unsigned short)profile);
		tfa_dev_set_swvstep(tfa, 0);

		/* stop in simple init case, without stream */
		if (tfa98xx_count_active_stream(BIT_PSTREAM) == 0
			&& tfa98xx_count_active_stream(BIT_CSTREAM) == 0)
			return err;
	}

	/* cold start */
	/* always send the SetRe25 message
	 * to indicate all messages are sent
	 */
	if (value)
		if (tfa->ext_dsp == 1)
			err = tfa_set_calibration_values_once(tfa);

	return err;
}

enum tfa98xx_error
tfa_run_speaker_startup(struct tfa_device *tfa, int force, int profile)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	enum tfa_error ret = tfa_error_ok;
	struct tfa_device *tfa0 = NULL;

	pr_debug("coldstart%s :", force ? " (forced)" : "");

	if (!force) { /* in case of force CF already running */
		err = tfa_run_startup(tfa, profile);
		PRINT_ASSERT(err);
		if (err) {
			pr_info("%s: tfa_run_startup error = %d\n",
				__func__, err);
			return err;
		}

		/* stop in simple init case, without stream */
		if (tfa98xx_count_active_stream(BIT_PSTREAM) == 0
			&& tfa98xx_count_active_stream(BIT_CSTREAM) == 0) {
			pr_info("%s: skip configuring when there's no stream\n",
				__func__);
			return err;
		}

		/* Startup with CF in bypass then return here */
		if (tfa_cf_enabled(tfa) == 0)
			return err;

		/* respond to external DSP: -1:none, 0:no_dsp, 1:cold, 2:warm */
		if (tfa->ext_dsp == -1) {
			err = tfa_run_start_dsp(tfa);
			if (err)
				return err;
		}
	}

	if (tfa->reset_mtpex) { /* reset MTPEX, if suspended */
		pr_info("%s: reset MTPEX (device %d)\n",
			__func__, tfa->dev_idx);
		tfa->reset_mtpex = 0;
		ret = tfa_dev_mtp_set(tfa, TFA_MTP_EX, 0);
		ret |= tfa_dev_mtp_set(tfa, TFA_MTP_OTC, 1);
		ret |= tfa_dev_mtp_set(tfa, TFA_MTP_RE25, 0);
		if (ret)
			pr_err("%s: resetting MTP failed (%d)\n",
				__func__, ret);
	}

	if (tfa->is_probus_device)
		/* write files only if it's not loaded */
		if (tfa_count_status_flag(tfa, TFA_SET_DEVICE) > 1
			|| tfa_count_status_flag(tfa, TFA_SET_CONFIG) > 0)
			return err;

	if (tfa->is_probus_device)
		/* set head device */
		tfa0 = tfa98xx_get_tfa_device_from_index(-1);

	/* DSP is running now */
	/* write all the files from the device list */
	if (tfa0 != NULL) {
		pr_info("%s: load dev files from main device %d\n",
			__func__, tfa0->dev_idx);
		/* CHECK: loading main device */
		err = tfa_cont_write_files(tfa0);
	} else {
		pr_info("%s: load dev files from individual device %d\n",
			__func__, tfa->dev_idx);
		err = tfa_cont_write_files(tfa);
	}
	if (err) {
		pr_debug("[%s] tfa_cont_write_files error = %d\n",
			__func__, err);
		return err;
	}

	tfa->is_bypass = 1; /* reset before start */
	tfa->blackbox_config_msg = 0; /* reset */
	/* write all the files from the profile list (use volumstep 0) */
	pr_info("%s: load prof files (device %d, profile %d)\n",
		__func__, tfa->dev_idx, profile);
	err = tfa_cont_write_files_prof(tfa, profile, 0);
	if (err) {
		pr_debug("[%s] tfa_cont_write_files_prof error = %d\n",
			__func__, err);
		return err;
	}

	return err;
}

/*
 * Run calibration
 */
enum tfa98xx_error
tfa_run_speaker_calibration(struct tfa_device *tfa)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int calibrate_done;

	/* return if there is no audio running */
	if ((tfa->tfa_family == 2) && TFAxx_GET_BF(tfa, NOCLK))
		return TFA98XX_ERROR_NO_CLOCK;

	/* When MTPOTC is set (cal=once) unlock key2 */
	if (tfa_dev_mtp_get(tfa, TFA_MTP_OTC) == 1)
		tfa98xx_key2(tfa, 0);

	/* await calibration, this should return ok */
	err = tfa_run_wait_calibration(tfa, &calibrate_done);
	if (!tfa->spkr_damaged) {
		err = tfa_dsp_get_calibration_impedance(tfa);
		PRINT_ASSERT(err);
	}

	/* When MTPOTC is set (cal=once) re-lock key2 */
	if (tfa_dev_mtp_get(tfa, TFA_MTP_OTC) == 1)
		tfa98xx_key2(tfa, 1);

	return err;
}

enum tfa98xx_error tfa_run_coldboot(struct tfa_device *tfa, int state)
{
	return TFA98XX_ERROR_OK;
}

enum tfa98xx_error tfa_run_start_dsp(struct tfa_device *tfa)
{
	return TFA98XX_ERROR_OK;
}

/*
 * start the clocks and wait until the AMP is switching
 * on return the DSP sub system will be ready for loading
 */
enum tfa98xx_error tfa_run_startup(struct tfa_device *tfa, int profile)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_device_list *dev = tfa_cont_device(tfa->cnt, tfa->dev_idx);
	int i, noinit = 0, audfs = 0, fractdel = 0;
	uint16_t value;
	char prof_name[MAX_CONTROL_NAME] = {0};
	int is_cold_amp;
	int tfa_state;

	if (dev == NULL)
		return TFA98XX_ERROR_FAIL;

	if (dev->bus) /* no i2c device, do nothing */
		return TFA98XX_ERROR_OK;

	is_cold_amp = tfa_is_cold_amp(tfa);
	pr_info("%s: is_cold_amp %d, first_after_boot %d\n",
		__func__, is_cold_amp, tfa->first_after_boot);
	if (tfa->first_after_boot || is_cold_amp == 1) {
		/* process the device list
		 * to see if the user implemented the noinit
		 */
		for (i = 0; i < dev->length; i++) {
			if (dev->list[i].type == dsc_no_init) {
				noinit = 1;
				break;
			}
		}

		if (!noinit) {
			/* Read AUDFS & FRACTDEL prior to (re)init. */
			value = (uint16_t)TFA_READ_REG(tfa, AUDFS);
			audfs = TFA_GET_BF_VALUE(tfa, AUDFS, value);
			fractdel = TFAxx_GET_BF_VALUE(tfa, FRACTDEL, value);

			/* load the optimal TFA98XX in HW settings */
			err = tfa98xx_init(tfa);
			PRINT_ASSERT(err);

			/* Restore audfs & fractdel after coldboot,
			 * so we can calibrate with correct fs setting.
			 * in case something else was given in cnt file,
			 * profile below will apply this.
			 */
			value = (uint16_t)TFA_READ_REG(tfa, AUDFS);
			TFA_SET_BF_VALUE(tfa, AUDFS, audfs, &value);
			TFAxx_SET_BF_VALUE(tfa, FRACTDEL, fractdel, &value);
			TFA_WRITE_REG(tfa, AUDFS, value);
		} else {
			pr_debug("Warning: No init keyword found in the cnt file. Init is skipped!\n");
		}

		/* I2S settings to define the audio input properties
		 * these must be set before the subsys is up
		 * this will run the list
		 * until a non-register item is encountered
		 */
		pr_info("%s: write registers under dev to device %d\n",
			__func__, tfa->dev_idx);
		err = tfa_cont_write_regs_dev(tfa);
		/* write device register settings */
		PRINT_ASSERT(err);
	} else {
		pr_info("%s: skip_init and writing registers under dev (%d:%d)\n",
			__func__, tfa->first_after_boot, is_cold_amp);
	}

	if ((tfa->first_after_boot || (is_cold_amp == 1))
		|| (profile != tfa_dev_get_swprof(tfa))) {
		/* also write register the settings from the default profile
		 * NOTE we may still have ACS=1
		 * so we can switch sample rate here
		 */
		pr_info("%s: write registers under profile (%d) to device %d\n",
			__func__, profile, tfa->dev_idx);
		err = tfa_cont_write_regs_prof(tfa, profile);
		PRINT_ASSERT(err);
	} else {
		pr_info("%s: skip writing registers under profile (%d) to device %d\n",
			__func__, profile, tfa->dev_idx);
	}

	/* Factory trimming for the Boost converter */
	tfa98xx_factory_trimmer(tfa);

	tfa->first_after_boot = 0;

	/* stop in simple init case, without stream */
	if (tfa98xx_count_active_stream(BIT_PSTREAM) == 0
		&& tfa98xx_count_active_stream(BIT_CSTREAM) == 0) {
		pr_info("%s: skip init_cf when there's no stream\n",
			__func__);
		goto tfa_run_startup_exit;
	}

	tfa_state = tfa_dev_get_state(tfa);
	if (tfa_state == TFA_STATE_INIT_CF
		|| tfa_state == TFA_STATE_INIT_FW
		|| tfa_state == TFA_STATE_OPERATING) {
		pr_info("%s: skip setting state to INIT_CF (%d)\n",
			__func__, tfa_state);
		goto tfa_run_startup_exit;
	}

	/* Go to the initCF state */
	memset(prof_name, 0, MAX_CONTROL_NAME);
	strncpy(prof_name, tfa_cont_profile_name(tfa->cnt,
		tfa->dev_idx, profile), MAX_CONTROL_NAME);
	tfa_dev_set_state(tfa, TFA_STATE_INIT_CF,
		strnstr(prof_name, ".cal", strlen(prof_name)) != NULL);

tfa_run_startup_exit:
	if (tfa->mute_state) {
		pr_info("%s: MUTE dev %d (by force)\n",
			__func__, tfa->dev_idx);
		tfa_dev_set_state(tfa, TFA_STATE_MUTE, 0);
	}

	err = tfa_show_current_state(tfa);

	return err;
}

/*
 * run the startup/init sequence and set ACS bit
 */
enum tfa98xx_error tfa_run_coldstartup(struct tfa_device *tfa, int profile)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;

	tfa->first_after_boot = 1;
	err = tfa_run_startup(tfa, profile);
	PRINT_ASSERT(err);
	if (err)
		return err;

	if (!tfa->is_probus_device) {
		/* force cold boot */
		err = tfa_run_coldboot(tfa, 1); /* set ACS */
		PRINT_ASSERT(err);
		if (err)
			return err;
	}

	if (tfa->ext_dsp == -1) {
		/* start */
		err = tfa_run_start_dsp(tfa);
		PRINT_ASSERT(err);
	}

	return err;
}

/*
 * run mute (optionally, with ramping down)
 */
enum tfa98xx_error tfa_run_mute(struct tfa_device *tfa)
{
	enum tfa98xx_error ret = TFA98XX_ERROR_OK;
	enum tfa_error err = tfa_error_ok;
	int cur_ampe;
	int steps = tfa->ramp_steps;

	if (steps == 0) /* default steps */
		steps = RAMPDOWN_DEFAULT;

	cur_ampe = TFA_GET_BF(tfa, AMPE);
	if (cur_ampe == 0 || steps <= 0)
		tfa_gain_rampdown(tfa, -1);
	else
		ret = tfa_gain_rampdown(tfa, steps);

	/* signal the TFA98XX to mute */
	/* err = tfa98xx_set_mute(tfa, TFA98XX_MUTE_AMPLIFIER); */
	err = tfa_dev_set_state(tfa, TFA_STATE_MUTE, 0);
	if (err != tfa_error_ok) {
		pr_err("%s: failed to set mute state (err %d)\n",
			__func__, err);
		return TFA98XX_ERROR_OTHER;
	}

	if (tfa->verbose)
		pr_debug("-------------------- muted ------------------\n");

	return ret;
}

/*
 * run unmute
 */
enum tfa98xx_error tfa_run_unmute(struct tfa_device *tfa)
{
	enum tfa98xx_error ret = TFA98XX_ERROR_OK;
	enum tfa_error err = tfa_error_ok;
	int cur_ampe;
	int steps = tfa->ramp_steps;

	/* signal the TFA98XX to mute */
	/* err = tfa98xx_set_mute(tfa, TFA98XX_MUTE_OFF); */
	err = tfa_dev_set_state(tfa, TFA_STATE_UNMUTE, 0);
	if (err != tfa_error_ok) {
		pr_err("%s: failed to set unmute state (err %d)\n",
			__func__, err);
		return TFA98XX_ERROR_OTHER;
	}

	if (tfa->ampgain != -1) {
		if (steps == 0) /* default steps */
			steps = RAMPDOWN_DEFAULT;

		cur_ampe = TFA_GET_BF(tfa, AMPE);
		if (cur_ampe == 0 || steps <= 0)
			tfa_gain_restore(tfa, -1);
		else
			ret = tfa_gain_restore(tfa, steps);
	}

	if (tfa->verbose)
		pr_debug("------------------- unmuted -----------------\n");

	return ret;
}

enum tfa98xx_error tfa_gain_rampdown(struct tfa_device *tfa, int count)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int step = 0, cur_ampgain, value = 0;

	cur_ampgain = TFAxx_GET_BF(tfa, AMPGAIN);
	if (cur_ampgain <= 0) {
		pr_debug("%s: ampgain is already rampdown (%d)\n",
			__func__, cur_ampgain);
		return TFA98XX_ERROR_OTHER;
	}
	tfa->ampgain = cur_ampgain;
	if (count < 0)
		pr_debug("%s: direct drop ampgain (%d to 0)\n",
			__func__, tfa->ampgain);
	else
		pr_debug("%s: ramp down ampgain (%d)\n",
			__func__, tfa->ampgain);

	if (tfa->ampgain == -1) {
		pr_debug("%s: reference gain is not valid\n", __func__);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	/* ramp down amplifier gain for "count" msec */
	if (count >= 0) {
		/* stepwise set */
		for (step = 0; step < count; step++) {
			value = tfa->ampgain >> (step + 1);
			err = TFAxx_SET_BF(tfa, AMPGAIN, value);
			if (err) {
				pr_err("%s: error in setting AMPGAIN\n",
					__func__);
				return err;
			}

			usleep_range(RAMPING_INTERVAL * 1000,
				RAMPING_INTERVAL * 1000 + 5);
		}
		if (value > 0)
			count = -1; /* to set 0 at final step */
	}

	if (count < 0) { /* direct set */
		err = TFAxx_SET_BF(tfa, AMPGAIN, 0);
		if (err)
			pr_err("%s: error in setting AMPGAIN\n",
				__func__);
	}

	return err;
}

enum tfa98xx_error tfa_gain_restore(struct tfa_device *tfa, int count)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int step = 0, cur_ampgain, value = 0;

	if (count < 0) { /* direct set */
		pr_debug("%s: direct set ampgain (0 to %d)\n",
			__func__, tfa->ampgain);
		err = TFAxx_SET_BF(tfa, AMPGAIN, tfa->ampgain);
		if (err)
			pr_err("%s: error in setting AMPGAIN\n",
				__func__);

		return err;
	}

	/* stepwise set */
	cur_ampgain = TFAxx_GET_BF(tfa, AMPGAIN);
	if ((cur_ampgain == tfa->ampgain)
		|| (cur_ampgain > 0 && tfa->ampgain == -1)) {
		pr_debug("%s: ampgain is already restorted (%d; %d)\n",
			__func__, cur_ampgain, tfa->ampgain);
		return TFA98XX_ERROR_OTHER;
	}
	pr_debug("%s: restore ampgain (%d)\n",
		__func__, tfa->ampgain);

	/* ramp up amplifier gain for "count" msec */
	/* stepwise set */
	for (step = 0; step < count; step++) {
		value = tfa->ampgain >> (count - step - 1);
		err = TFAxx_SET_BF(tfa, AMPGAIN, value);
		if (err) {
			pr_err("%s: error in setting AMPGAIN\n",
				__func__);
			return err;
		}

		usleep_range(RAMPING_INTERVAL * 1000,
			RAMPING_INTERVAL * 1000 + 5);
	}

	return err;
}

void tfa_set_spkgain(struct tfa_device *tfa)
{
	if (tfa == NULL || tfa->spkgain == -1)
		return;

	pr_info("%s: set speaker gain 0x%x inplev %d\n",
		__func__, tfa->spkgain, tfa->inplev);
	TFAxx_SET_BF(tfa, TDMSPKG, tfa->spkgain);

	if (tfa->inplev == -1)
		return;
	switch (tfa->rev & 0xff) {
	case 0x66:
		TFAxx_SET_BF(tfa, MUSMODE, (tfa->inplev == 1) ? 0 : 1);
		break;
	default:
		/* neither TFA987x */
		break;
	}
}

enum tfa98xx_error tfa_wait_cal(struct tfa_device *tfa)
{
	enum tfa98xx_error cal_err = TFA98XX_ERROR_OK;
	int calibration_done = 0;
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_device *ntfa;
	int i;

	pr_info("%s: [%d] triggered\n",
		__func__, tfa->dev_idx);

	cal_err = tfa_run_wait_calibration(tfa, &calibration_done);
	if (cal_err != TFA98XX_ERROR_OK || calibration_done == 0) {
		pr_err("%s: calibration is not done; stop processing\n",
			__func__);

		for (i = 0; i < tfa->dev_count; i++) {
			ntfa = tfa98xx_get_tfa_device_from_index(i);

			if (!tfa_is_active_device(ntfa))
				continue;

			tfaxx_status(ntfa);
		}
	}

	for (i = 0; i < tfa->dev_count; i++) {
		ntfa = tfa98xx_get_tfa_device_from_index(i);

		if (!tfa_is_active_device(ntfa))
			continue;

		if (ntfa->spkr_damaged)
			continue;

		pr_debug("%s: [%d] process calibration data\n",
			__func__, ntfa->dev_idx);
		err = tfa_dsp_get_calibration_impedance(ntfa);
		if (err != TFA98XX_ERROR_OK)
			PRINT_ASSERT(err);
	}

	tfa_restore_after_cal(0, cal_err);

	return cal_err;
}

void tfa_restore_after_cal(int index, int cal_err)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	enum tfa_error ret = tfa_error_ok;
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(index);
	struct tfa_device *ntfa;
	int i;
	int need_restore = 0;
	int active_profile = -1;

	pr_info("%s: enter with dev_idx %d (cal_err %d)\n",
		__func__, tfa->dev_idx, cal_err);

	/* mute and check if it needs restoring */
	for (i = 0; i < tfa->dev_count; i++) {
		ntfa = tfa98xx_get_tfa_device_from_index(i);

		if (!tfa_is_active_device(ntfa))
			continue;

		/* force MUTE after calibration, to set UNMUTE at sync later */
		pr_debug("%s: [%d] force MUTE after calibration\n",
			__func__, ntfa->dev_idx);
		tfa_dev_set_state(ntfa, TFA_STATE_MUTE, 1);

		active_profile = tfa_dev_get_swprof(ntfa);
		if (ntfa->next_profile == active_profile
			|| ntfa->next_profile < 0)
			continue;

		need_restore = 1;
	}

	if (need_restore) {
		/* reset counter */
		tfa_set_status_flag(tfa, TFA_SET_DEVICE, -1);
		tfa_set_status_flag(tfa, TFA_SET_CONFIG, -1);

		/*
		 * restore profile after calibration.
		 * typically, when calibration is done,
		 * profile should be updated in warm state.
		 */
		for (i = 0; i < tfa->dev_count; i++) {
			ntfa = tfa98xx_get_tfa_device_from_index(i);

			if (!tfa_is_active_device(ntfa))
				continue;

			tfa_set_status_flag(ntfa, TFA_SET_DEVICE, 1);

			active_profile = tfa_dev_get_swprof(ntfa);
			pr_info("%s: [%d] restore profile after calibration (active %d; next %d)\n",
				__func__, ntfa->dev_idx,
				active_profile, ntfa->next_profile);

			/* switch profile */
			if (cal_err != TFA98XX_ERROR_OK) {
				/* only with register setting at failure */
				pr_info("%s: apply only register setting at failure\n",
					__func__);

				err = tfa_cont_write_regs_prof(ntfa,
					ntfa->next_profile);
				if (err != TFA98XX_ERROR_OK)
					pr_err("%s: error in writing regs (%d)\n",
						__func__, err);
				else if (TFA_GET_BF(ntfa, PWDN) != 0)
					err = tfa98xx_powerdown(ntfa, 0);
			} else {
				/* with the entire setting at success */
				pr_info("%s: apply the whole profile setting at done\n",
					__func__);

				ret = tfa_dev_switch_profile(ntfa,
					ntfa->next_profile, ntfa->vstep);
				if (ret != tfa_error_ok)
					pr_err("%s: error in switch profile (%d)\n",
						__func__, ret);
			}
		}

		/* reset counter */
		tfa_set_status_flag(tfa, TFA_SET_DEVICE, -1);
		tfa_set_status_flag(tfa, TFA_SET_CONFIG, -1);
	}

	for (i = 0; i < tfa->dev_count; i++) {
		ntfa = tfa98xx_get_tfa_device_from_index(i);

		if (!tfa_is_active_device(ntfa))
			continue;

		if (cal_err != TFA98XX_ERROR_OK
			|| ntfa->spkr_damaged) {
			tfa_handle_damaged_speakers(ntfa);
			continue;
		}

		tfa_set_spkgain(ntfa);
		/* force UNMUTE after processing calibration */
		pr_debug("%s: [%d] force UNMUTE after processing calibration\n",
			__func__, ntfa->dev_idx);
		tfa_dev_set_state(ntfa, TFA_STATE_UNMUTE, 1);
	}
}
EXPORT_SYMBOL(tfa_restore_after_cal);

int tfa_run_damage_check(struct tfa_device *tfa,
	int dsp_event, int dsp_status)
{
	int damaged = 0, damage_event = 0;
	struct tfa_device *ntfa = NULL;
	int i;

	for (i = 0; i < tfa->dev_count; i++) {
		ntfa = tfa98xx_get_tfa_device_from_channel(i);

		if (!tfa_is_active_device(ntfa))
			continue;

		damage_event
			= (TFA_GET_BIT_VALUE(dsp_event,
			i + 1)) ? 1 : 0;
		/* set damage flag with status */
		ntfa->spkr_damaged
			= (TFA_GET_BIT_VALUE(dsp_status,
			i + 1)) ? 1 : 0;
		pr_info("%s: damage flag update %d (dev %d, channel %d)\n",
			__func__,
			ntfa->spkr_damaged,
			ntfa->dev_idx, i);
		damaged |= ntfa->spkr_damaged;
	}

	return damaged;
}

/*
 * wait for calibrate_done
 */
enum tfa98xx_error
tfa_run_wait_calibration(struct tfa_device *tfa, int *calibrate_done)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int tries = 0, mtp_busy = 1, tries_mtp_busy = 0;
	char buffer[2 * 3] = {0};
	int res_len;
	int fw_status[2] = {0};
	int dsp_event = 0, dsp_status = 0;
	struct tfa_device *ntfa;
	int i;
	int damaged = 0;

	*calibrate_done = 0;

	/* in case of calibrate once wait for MTPEX */
	if (!tfa->is_probus_device
		&& tfa_dev_mtp_get(tfa, TFA_MTP_OTC)) {
		/* Check if MTP_busy is clear! */
		while (tries_mtp_busy < MTPBWAIT_TRIES) {
			mtp_busy = tfa_dev_get_mtpb(tfa);
			if (mtp_busy == 1)
				/* wait 10ms to avoid busload */
				msleep_interruptible(BUSLOAD_INTERVAL);
			else
				break;
			tries_mtp_busy++;
		}

		if (tries_mtp_busy < MTPBWAIT_TRIES) {
			/* Because of the msleep
			 * TFA98XX_API_WAITRESULT_NTRIES is way to long!
			 * Setting this to 25 will take
			 * at least 25*50ms = 1.25 sec
			 */
			while ((*calibrate_done == 0)
				&& (tries < MTPEX_WAIT_NTRIES)) {
				*calibrate_done
					= tfa_dev_mtp_get(tfa, TFA_MTP_EX);
				if (*calibrate_done == 1)
					break;
				/* wait 50ms to avoid busload */
				msleep_interruptible(5 * BUSLOAD_INTERVAL);
				tries++;
			}

			if (tries >= MTPEX_WAIT_NTRIES)
				tries = TFA98XX_API_WAITRESULT_NTRIES;
		} else {
			pr_err("MTP busy after %d tries\n", MTPBWAIT_TRIES);
		}
	}

	if (tfa->is_probus_device) {
		res_len = 2 * 3;

		while ((*calibrate_done != 1)
			&& (tries < TFA98XX_API_WAITCAL_NTRIES)) {
			msleep_interruptible(CAL_STATUS_INTERVAL);

			err = tfa_dsp_cmd_id_write_read
				(tfa, MODULE_FRAMEWORK,
				FW_PAR_ID_GET_STATUS_CHANGE,
				res_len, (unsigned char *)buffer);
			if (err != TFA98XX_ERROR_OK)
				break;

			tfa98xx_convert_bytes2data(res_len,
				buffer, fw_status);
			dsp_event = fw_status[0];
			dsp_status = fw_status[1];
			pr_debug("%s: err (%d), status (0x%06x:0x%06x), count (%d)\n",
				__func__, err,
				dsp_event, dsp_status,
				tries + 1);

			/* wait until calibration done status is set */
			if (!(dsp_status & 0x1)) {
				tries++;
				continue;
			}

			*calibrate_done = 1;

			if ((dsp_status & 0x6) != 0) /* damage status */
				damaged = tfa_run_damage_check(tfa,
					dsp_event, dsp_status);

			if (damaged)
				*calibrate_done = 0; /* failure */
			break;
		}

		if (tries >= TFA98XX_API_WAITCAL_NTRIES)
			tries = TFA98XX_API_WAITRESULT_NTRIES;
	}

	if (*calibrate_done != 1) {
		pr_err("Calibration failed!\n");
		err = TFA98XX_ERROR_BAD_PARAMETER;
	} else if (tries == TFA98XX_API_WAITRESULT_NTRIES) {
		pr_err("Calibration has timedout!\n");
		err = TFA98XX_ERROR_STATE_TIMED_OUT;
	} else if (tries_mtp_busy == MTPBWAIT_TRIES) {
		pr_err("Calibrate Failed: MTP_busy stays high!\n");
		err = TFA98XX_ERROR_STATE_TIMED_OUT;
	} else {
		pr_info("Calibration succeeded!\n");
	}

	if (!tfa->is_probus_device)
		return err;

	/* success: probus device */
	if (err == TFA98XX_ERROR_OK) {
		/* reset damage flag at success */
		for (i = 0; i < tfa->dev_count; i++) {
			ntfa = tfa98xx_get_tfa_device_from_index(i);

			if (ntfa == NULL)
				continue;

			ntfa->is_calibrating = 0;
			ntfa->spkr_damaged = 0;
		}

		return err;
	}

	/* falure: probus device */
	/* set damage status with status at failure */
	pr_info("%s: speaker damage flag 0x%06x:0x%06x\n",
		__func__, fw_status[0], fw_status[1]);

	if ((fw_status[1] & 0x6) == 0)
		return err;

	for (i = 0; i < tfa->dev_count; i++) {
		ntfa = tfa98xx_get_tfa_device_from_channel(i);

		if (ntfa == NULL)
			continue;

		ntfa->is_calibrating = 0;
		ntfa->spkr_damaged
			= (TFA_GET_BIT_VALUE(fw_status[1], i + 1))
			? 1 : 0;
		if (ntfa->spkr_damaged)
			pr_info("%s: speaker damage is detected [%s] (dev %d, channel %d)\n",
				__func__,
				tfa_cont_device_name(ntfa->cnt, ntfa->dev_idx),
				ntfa->dev_idx, i);
	}

	return err;
}

enum tfa98xx_error
tfa_check_speaker_damage(struct tfa_device *tfa)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	char buffer[2 * 3] = {0};
	int res_len;
	int fw_status[2] = {0};
	int dsp_event = 0, dsp_status = 0;
	int damaged = 0;

	res_len = 2 * 3;

	err = tfa_dsp_cmd_id_write_read
		(tfa, MODULE_FRAMEWORK, FW_PAR_ID_GET_STATUS_CHANGE,
		res_len, (unsigned char *)buffer);
	if (err != TFA98XX_ERROR_OK)
		return err;

	tfa98xx_convert_bytes2data(res_len, buffer, fw_status);
	dsp_event = fw_status[0];
	dsp_status = fw_status[1];
	pr_info("%s: status (0x%06x:0x%06x)\n",
		__func__, dsp_event, dsp_status);

	if ((dsp_status & 0x6) != 0) /* damage status */
		damaged = tfa_run_damage_check(tfa,
			dsp_event, dsp_status);

	if (damaged)
		pr_info("%s: speaker damage is detected!\n",
			__func__);

	return err;
}

void tfa_set_active_handle(struct tfa_device *tfa, int profile)
{
	int dev;
	int active_handle = 0;
	int count = 0;
	struct tfa_device *ntfa = NULL;

	if (tfa == NULL)
		return;

	for (dev = 0; dev < tfa->dev_count; dev++) {
		ntfa = tfa98xx_get_tfa_device_from_index(dev);
		if (ntfa == NULL)
			continue;

		if (ntfa->set_active != 0) {
			active_handle |= (1 << dev);
			count++;
		}
	}
	if (count == tfa->dev_count)
		active_handle = 0xf;

	pr_info("%s: active handle: 0x%x, active count %d\n",
		__func__, active_handle, count);

	for (dev = 0; dev < tfa->dev_count; dev++) {
		ntfa = tfa98xx_get_tfa_device_from_index(dev);
		if (ntfa == NULL)
			continue;

		ntfa->active_handle = active_handle;
		ntfa->active_count = count;
		ntfa->is_bypass = 0; /* reset at start */
	}
}

void tfa_reset_active_handle(struct tfa_device *tfa)
{
	int dev;
	struct tfa_device *ntfa = NULL;

	if (tfa == NULL)
		return;

	for (dev = 0; dev < tfa->dev_count; dev++) {
		ntfa = tfa98xx_get_tfa_device_from_index(dev);
		if (ntfa == NULL)
			continue;

		ntfa->active_handle = 0xf;
		ntfa->active_count = -1;

		/* reload setting afterwards, if speaker gain is forced */
		if (ntfa->spkgain != -1)
			ntfa->first_after_boot = 1;
		ntfa->spkgain = -1;
		ntfa->inplev = -1;
	}
}

int tfa_is_active_device(struct tfa_device *tfa)
{
	int is_active = 0;

	if (tfa == NULL)
		return is_active;

	/* if not initialized, assume all active */
	if (tfa->active_count == -1)
		return 1;
	if (tfa->active_handle == 0xf)
		return 1;

	if (tfa->active_handle & (1 << tfa->dev_idx))
		is_active = 1;

	return is_active;
}

/*
 * tfa_dev_start will only do the basics:
 * Going from powerdown to operating or a profile switch.
 * for calibrating or acoustic shock
 * handling use tfa98xxCalibration function.
 */
enum tfa_error tfa_dev_start(struct tfa_device *tfa,
	int next_profile, int vstep)
{
	enum tfa_error ret = tfa_error_ok;
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int active_profile = -1, cal_profile = -1;
	static int tfa98xx_log_start_cnt;
	int forced = 0;
	int tfa_state_before, tfa_state;
	int cal_ready = 1;

	tfa98xx_log_start_cnt++;

	pr_debug("%s: tfa98xx_log_tfa_family=%d,",
		__func__, tfa->tfa_family);
	pr_debug("%s: tfa98xx_log_revision=0x%x,",
		__func__, tfa->revid & 0xff);
	pr_debug("%s: tfa98xx_log_subrevision=0x%x,",
		__func__, (tfa->revid >> 8) & 0xffff);
	pr_debug("%s: tfa98xx_log_i2c_responder_address=0x%x,",
		__func__, tfa->resp_address);
	pr_info("%s: tfa98xx_log_start_cnt=%d next_profile %d\n",
		__func__, tfa98xx_log_start_cnt, next_profile);

	tfa->next_profile = next_profile;
	tfa->swprof = -1; /* reset to read */
	tfa_dev_get_swprof(tfa);

	if (tfa_count_status_flag(tfa, TFA_SET_DEVICE) < 1) {
		pr_info("%s: initialize active handle\n", __func__);
		/* check activeness with profile */
		tfa_set_active_handle(tfa, next_profile);
	}

	if (tfa_is_active_device(tfa)) {
		pr_info("%s: active handle [%s]: 0x%x\n", __func__,
			tfa_cont_device_name(tfa->cnt, tfa->dev_idx),
			tfa->active_handle);
	} else {
		pr_info("%s: keep profile %d instead of changing to %d\n",
			__func__, tfa_dev_get_swprof(tfa),
			next_profile);

		pr_info("%s: stop unused dev %d, by force\n",
			__func__, tfa->dev_idx);
		tfa_dev_stop(tfa); /* stop inactive handle */

		goto tfa_dev_start_exit;
	}

	/* If the profile contains the .standby suffix go
	 * to powerdown else we should be in operating state
	 */
	if (tfa_cont_is_standby_profile(tfa, next_profile)) {
		tfa_dev_set_swprof(tfa, (unsigned short)next_profile);
		tfa_dev_set_swvstep(tfa, (unsigned short)vstep);

		pr_info("%s: skip starting dev %d for standby profile\n",
			__func__, tfa->dev_idx);

		goto tfa_dev_start_exit;
	}

	/* Get current profile */
	active_profile = tfa_dev_get_swprof(tfa);
	if (active_profile == 0xff)
		active_profile = -1;

	/* restore amplifier gain, if it's touched before */
	if (tfa->ampgain != -1) {
		int cur_ampe;

		cur_ampe = TFA_GET_BF(tfa, AMPE);
		if (cur_ampe == 0)
			tfa_gain_restore(tfa, -1);
		else
			err = tfa_gain_restore(tfa, RAMPDOWN_SHORT);
	}
	tfa->ampgain = -1;

	if (tfa->mtpex == -1)
		tfa_dev_mtp_get(tfa, TFA_MTP_EX);
	cal_ready &= (tfa->disable_auto_cal) ? 0 : 1;
	if (cal_ready) {
		if (!tfa->is_probus_device) {
			pr_info("%s: set cold by force in non-probus case\n",
				__func__);
			forced = 1;
		}

		cal_profile = tfa_cont_get_cal_profile(tfa);
		if (next_profile == cal_profile) {
			tfa->next_profile = (active_profile >= 0)
				? active_profile : 0;
			pr_info("%s: set next profile %d for dev %d, if cal profile is selected\n",
				__func__, tfa->next_profile,
				tfa->dev_idx);
		}
		if (cal_profile >= 0) {
			pr_info("%s: set profile for calibration profile %d\n",
				__func__, cal_profile);
			next_profile = cal_profile;
		}
		tfa->first_after_boot = 1;
	}

	/* TfaRun_SpeakerBoost implies un-mute */
	pr_debug("%s: active_profile:%s, next_profile:%s\n", __func__,
		tfa_cont_profile_name(tfa->cnt, tfa->dev_idx, active_profile),
		tfa_cont_profile_name(tfa->cnt, tfa->dev_idx, next_profile));
	pr_debug("%s: Starting device [%s]\n", __func__,
		tfa_cont_device_name(tfa->cnt, tfa->dev_idx));

	mutex_lock(&dev_lock);

	tfa_set_status_flag(tfa, TFA_SET_DEVICE, 1);

	if (tfa->bus != 0) { /* non i2c */
#ifndef __KERNEL__
		tfadsp_fw_start(tfa, next_profile, vstep);
#endif /* __KERNEL__ */
	} else {
		pr_debug("%s: device[%d] [%s] - tfa_run_speaker_boost profile=%d\n",
			__func__, tfa->dev_idx,
			tfa_cont_device_name(tfa->cnt, tfa->dev_idx),
			next_profile);

		/* need to update before checking is_cold */
		tfa_state_before = tfa_dev_get_state(tfa);

		/* Check if we need coldstart or ACS is set */
		err = tfa_run_speaker_boost(tfa, forced, next_profile);
		if (err != TFA98XX_ERROR_OK) {
			mutex_unlock(&dev_lock);
			goto tfa_dev_start_exit;
		}

		pr_info("%s:[%s] device:%s profile:%s\n",
			__func__, (tfa->is_cold) ? "cold" : "warm",
			tfa_cont_device_name(tfa->cnt, tfa->dev_idx),
			tfa_cont_profile_name(tfa->cnt, tfa->dev_idx,
			next_profile));

		/* stop in simple init case, without stream */
		if (tfa98xx_count_active_stream(BIT_PSTREAM) == 0
			&& tfa98xx_count_active_stream(BIT_CSTREAM) == 0) {
			pr_info("%s: skip operating when there's no stream\n",
				__func__);
			mutex_unlock(&dev_lock);
			goto tfa_dev_start_exit;
		}
		if (tfa_cont_is_standby_profile(tfa, next_profile)) {
			pr_info("%s: skip powering on, in standby profile!\n",
				__func__);
			mutex_unlock(&dev_lock);
			goto tfa_dev_start_exit;
		} else if (tfa_cont_is_standby_profile(tfa, active_profile)
			&& (next_profile != active_profile
			&& active_profile >= 0)) {
			pr_info("%s: skip powering on device, at exiting from standby profile!\n",
				__func__);
			/* Go to the Powerdown & Mute state */
			tfa_dev_set_state(tfa,
				TFA_STATE_POWERDOWN | TFA_STATE_MUTE, 0);
			goto after_setting_operating_mode;
		}

		/* Make sure internal oscillator is running
		 * for DSP devices (non-dsp and max1 this is no-op)
		 */
		tfa98xx_set_osc_powerdown(tfa, 0);

		/* Go to the Operating state */
		tfa_state = tfa_dev_get_state(tfa);
		if (tfa_state == TFA_STATE_OPERATING) {
			pr_info("%s: skip setting state to OPERATING (%d)\n",
				__func__, tfa_state);
			if (tfa_state_before == TFA_STATE_OPERATING) {
				pr_debug("%s: device already active\n",
					__func__);
			} else {
				/* at last device only: to skip mute */
				if (tfa_count_status_flag(tfa, TFA_SET_DEVICE)
					>= tfa->active_count) {
					pr_debug("%s: skip MUTE at the last device\n",
						__func__);
				} else {
					pr_debug("%s: set MUTE until all are ready\n",
						__func__);
					tfa_dev_set_state(tfa,
						TFA_STATE_MUTE, 0);
				}
			}
		} else {
			tfa_dev_set_state(tfa,
				TFA_STATE_OPERATING | TFA_STATE_MUTE,
				(cal_profile >= 0) ? 1 : 0);
		}
	}

after_setting_operating_mode:
	mutex_unlock(&dev_lock);

	if (cal_profile >= 0) {
		if (tfa->is_probus_device)
			/* skip as profile is restored by tfa_wait_cal */
			goto tfa_dev_start_exit;
		else
			err = tfa_process_re25(tfa);
	}

	/* Profile switching in call */
	mutex_lock(&dev_lock);
	ret = tfa_dev_switch_profile(tfa, next_profile, vstep);
	mutex_unlock(&dev_lock);
	if (ret != tfa_error_ok)
		goto tfa_dev_start_exit;

	tfa->pause_state = 0;

tfa_dev_start_exit:
	tfa_show_current_state(tfa);

	/* set ready for next action, once the current one completes */
	mutex_lock(&dev_lock);
	if (tfa_count_status_flag(tfa, TFA_SET_DEVICE)
		>= tfa->active_count)
		/* reset counter */
		tfa_set_status_flag(tfa, TFA_SET_DEVICE, -1);
	mutex_unlock(&dev_lock);

	if (err != TFA98XX_ERROR_OK)
		ret = tfa_convert_error_code(err);

	return ret;
}

enum tfa_error tfa_dev_switch_profile(struct tfa_device *tfa,
	int next_profile, int vstep)
{
	enum tfa_error ret = tfa_error_ok;
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int active_profile = -1;

	active_profile = tfa_dev_get_swprof(tfa);
	pr_info("%s: profile (active %d; next %d)\n",
		__func__, active_profile, next_profile);

	/* Profile switching */
	if (next_profile != active_profile && active_profile >= 0) {
		pr_debug("%s: switch profile from %d to %d\n",
			__func__, active_profile, next_profile);

		/* at initial device only: write files only once */
		if (tfa_count_status_flag(tfa, TFA_SET_DEVICE) == 1
			&& tfa_count_status_flag(tfa, TFA_SET_CONFIG) == 0) {
			tfa->is_bypass = 1; /* reset beforehand */
			tfa->blackbox_config_msg = 0; /* reset */
		}

		err = tfa_cont_write_profile(tfa, next_profile, vstep);
		if (err != TFA98XX_ERROR_OK)
			return tfa_error_other;
	}

	/* If the profile contains the .standby suffix go
	 * to powerdown else we should be in operating state
	 */
	if (tfa_cont_is_standby_profile(tfa, next_profile)) {
		tfa_dev_set_swprof(tfa, (unsigned short)next_profile);
		tfa_dev_set_swvstep(tfa, (unsigned short)vstep);

		pr_info("%s: skip switching dev %d for standby profile\n",
			__func__, tfa->dev_idx);
		ret = tfa_dev_stop(tfa);

		return ret;
	} else if (TFA_GET_BF(tfa, PWDN) != 0) {
		err = tfa98xx_powerdown(tfa, 0);
	}

	err = tfa_show_current_state(tfa);

	/* Always search and apply filters after a startup */
	err = tfa_set_filters(tfa, next_profile);
	if (err != TFA98XX_ERROR_OK)
		return tfa_error_other;

	tfa_dev_set_swprof(tfa, (unsigned short)next_profile);
	tfa_dev_set_swvstep(tfa, (unsigned short)vstep);

	if (err != TFA98XX_ERROR_OK)
		ret = tfa_convert_error_code(err);

	return ret;
}

enum tfa_error tfa_dev_stop(struct tfa_device *tfa)
{
	enum tfa_error ret = tfa_error_ok;
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int ramp_steps;

	pr_debug("Stopping device [%s]\n",
		tfa_cont_device_name(tfa->cnt, tfa->dev_idx));

	/* mute */
	ramp_steps = tfa->ramp_steps;
	tfa->ramp_steps = RAMPDOWN_SHORT;
	err = tfa_run_mute(tfa);
	tfa->ramp_steps = ramp_steps;

	/* Make sure internal oscillator is not running
	 * for DSP devices (non-dsp and max1 this is no-op)
	 */
	tfa98xx_set_osc_powerdown(tfa, 1);

	/* powerdown CF */
	err = tfa98xx_powerdown(tfa, 1);
	if (err != TFA98XX_ERROR_OK)
		return tfa_error_other;

	/* CHECK: only once after buffering fully */
	/* at last device only: to flush buffer */
	if (tfa->ext_dsp == 1) {
		if (tfa_count_status_flag(tfa, TFA_SET_DEVICE)
			>= tfa->active_count
			|| tfa_count_status_flag(tfa, TFA_SET_CONFIG) > 0) {
			/* flush message buffer */
			mutex_lock(&dsp_msg_lock);
			pr_debug("%s: flush buffer in blob, at stop\n",
				__func__);
			err = tfa_tib_dsp_msgmulti(tfa, -2, NULL);
			mutex_unlock(&dsp_msg_lock);
		}
		tfa->is_bypass = 0; /* reset at stop */
		tfa->blackbox_config_msg = 0; /* reset */
	}

	tfa->pause_state = 1;

	if (tfa98xx_count_active_stream(BIT_PSTREAM) == 0
		&& tfa98xx_count_active_stream(BIT_CSTREAM) == 0) {
		pr_info("%s: all stopped: no active stream\n",
			__func__);
		/* reset counters */
		tfa_set_status_flag(tfa, TFA_SET_DEVICE, -1);
		tfa_set_status_flag(tfa, TFA_SET_CONFIG, -1);

		tfa_reset_active_handle(tfa);
	}

	if (err != TFA98XX_ERROR_OK)
		ret = tfa_convert_error_code(err);

	return ret;
}

/*
 * int registers and coldboot dsp
 */
int tfa_reset(struct tfa_device *tfa)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int state = -1;
	int retry_cnt = 0;

	/* Check device state.
	 * Print warning if reset is done
	 * from other state than powerdown (when verbose)
	 */
	state = tfa_dev_get_state(tfa);
	if (tfa->verbose) {
		if (((tfa->tfa_family == 1)
			&& state != TFA_STATE_RESET)
			|| ((tfa->tfa_family == 2)
			&& state != TFA_STATE_POWERDOWN))
			pr_info("WARNING: Device reset should be performed in POWERDOWN state\n");
	}

	/* Probus devices needs extra protection to ensure proper reset
	 * behavior, this step is valid except in powerdown state
	 */
	if (tfa->is_probus_device && state != TFA_STATE_POWERDOWN) {
		err = TFA_SET_BF_VOLATILE(tfa, AMPE, 0);
		if (err)
			return err;
		err = tfa98xx_powerdown(tfa, 1);
		if (err) {
			PRINT_ASSERT(err);
			return err;
		}
	}

	err = TFA_SET_BF_VOLATILE(tfa, I2CR, 1);
	if (err)
		return err;

	/* Restore MANSCONF to POR state */
	err = TFA_SET_BF_VOLATILE(tfa, MANSCONF, 0);
	if (err)
		return err;

	/* Probus devices HW are already reseted here,
	 * Last step is to send init message to softDSP
	 */
	if (tfa->is_probus_device) {
		if (tfa->ext_dsp > 0) {
			/* ext_dsp from warm to cold after reset */
			if (tfa->ext_dsp == 2)
				tfa->ext_dsp = 1;
		}
	} else {
		/* Coolflux has to be powered on to ensure proper ACS
		 * bit state
		 */

		/* Powerup CF to access CF io */
		err = tfa98xx_powerdown(tfa, 0);
		if (err) {
			PRINT_ASSERT(err);
			return err;
		}

		/* For clock */
		err = tfa_cf_powerup(tfa);
		if (err) {
			PRINT_ASSERT(err);
			return err;
		}

		/* Force cold boot */
		err = tfa_run_coldboot(tfa, 1); /* Set ACS */
		if (err) {
			PRINT_ASSERT(err);
			return err;
		}

		/* Set PWDN = 1, set powerdown state */
		err = TFA_SET_BF_VOLATILE(tfa, PWDN, 1);
		if (err)
			return err;

		/* Powerdown state should be reached within 1ms */
		for (retry_cnt = 0;
			retry_cnt < TFA98XX_WAITRESULT_NTRIES;
			retry_cnt++) {
			state = tfa_get_manstate(tfa);
			if (state < 0)
				return err;

			/* Check for MANSTATE=Powerdown (0) */
			if (state == 0)
				break;
			msleep_interruptible(BUSLOAD_INTERVAL);
		}

		/* Reset all I2C registers to default values,
		 * now device state is consistent, same as after powerup
		 */
		err = TFA_SET_BF(tfa, I2CR, 1);
	}

	return err;
}

/*
 * fill the calibration value as milli ohms in the struct
 * assume that the device has been calibrated
 */
enum tfa98xx_error
tfa_dsp_get_calibration_impedance(struct tfa_device *tfa)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	if (tfa_dev_mtp_get(tfa, TFA_MTP_OTC)
		&& tfa_dev_mtp_get(tfa, TFA_MTP_EX) != 0) {
		pr_debug("%s: Getting calibration values from MTP\n",
			__func__);

		tfa->mohm[0] = tfa_dev_mtp_get(tfa, TFA_MTP_RE25);

		if (tfa->mohm[0] > 0)
			return error;
	}

	error = tfa_process_re25(tfa);

	return error;
}

static enum tfa98xx_error tfa_process_re25(struct tfa_device *tfa)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	enum tfa_error ret = tfa_error_ok;
	unsigned char bytes[2 * 3] = {0};
	int data[2];
	int nr_bytes, i, spkr_count = 0, cal_idx = 0;
	int scaled_data;
	int channel;
	int tries = 0;
	int readback = -1;

	error = tfa_supported_speakers(tfa, &spkr_count);
	if (error != TFA98XX_ERROR_OK) {
		pr_err("error in checking supported speakers\n");
		return error;
	}

	/* SoftDSP interface differs from hw-dsp interfaces */
	if (tfa->is_probus_device && tfa->dev_count > 1)
		spkr_count = tfa->dev_count;

	pr_info("%s: read SB_PARAM_GET_RE25C\n", __func__);

	nr_bytes = spkr_count * 3;
	channel = tfa_get_channel_from_dev_idx(tfa, -1);
	if (channel == -1) {
		pr_err("%s: failed in matching channel\n",
			__func__);
		return TFA98XX_ERROR_DEVICE;
	}
	error = tfa_dsp_cmd_id_write_read(tfa,
		MODULE_SPEAKERBOOST, SB_PARAM_GET_RE25C, nr_bytes, bytes);
	if (error == TFA98XX_ERROR_OK) {
		tfa98xx_convert_bytes2data(nr_bytes, bytes, data);

		pr_debug("%s: RE25C - spkr_count %d, dev %d, channel %d\n",
			__func__, spkr_count, tfa->dev_idx, channel);
		pr_debug("%s: RE25C - data[0]=%d - data[1]=%d\n",
			__func__, data[0], data[1]);

		for (i = 0; i < spkr_count; i++) {
			/* for probus devices, calibration values
			 * coming from soft-dsp speakerboost,
			 * are ordered in a different way.
			 * Re-align to standard representation.
			 */
			cal_idx = i;
			if (tfa->is_probus_device)
				cal_idx = 0;

			/* signed data has a limit of 30 Ohm */
			scaled_data = data[channel];

			tfa->mohm[cal_idx]
				= TFA_ReZ_FP_INT(scaled_data,
				TFA_FW_ReZ_SHIFT) * 1000
				+ TFA_ReZ_FP_FRAC(scaled_data,
				TFA_FW_ReZ_SHIFT);
		}

		pr_info("%s: %d mOhms\n", __func__, tfa->mohm[cal_idx]);

		/* special case for stereo calibration, from SB 3.5 PRC1 */
		if (tfa->mohm[cal_idx] == -128000) {
			pr_err("%s: wrong calibration! (damaged speaker)\n",
				__func__);
			tfa->spkr_damaged = 1;
		}
	} else {
		pr_err("%s: tfa_dsp_cmd_id_write_read is failed, err=%d\n",
			__func__, error);

		for (i = 0; i < spkr_count; i++) {
			cal_idx = i;
			if (tfa->is_probus_device)
				cal_idx = 0;

			tfa->mohm[cal_idx] = -1;
		}

		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	error = tfa_calibration_range_check(tfa,
		channel, tfa->mohm[cal_idx]);
	if (error != TFA98XX_ERROR_OK) {
		pr_err("%s: calibration data is out of range: device %d\n",
			__func__, tfa->dev_idx);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	if (!tfa->is_probus_device)
		return error;

	/*
	 * if (tfa_dev_mtp_get(tfa, TFA_MTP_OTC) != 0) {
	 *  pr_debug("%s: skip writing calibration data to MTP\n",
	 *  __func__);
	 *  return error;
	 * }
	 */
	/* store calibration data to MTP */
	ret = tfa_dev_mtp_set(tfa, TFA_MTP_OTC, 1);
	if (ret != tfa_error_ok)
		pr_debug("%s: error in setting MTPOTC\n",
			__func__);

	while (++tries < TFA98XX_API_REWRTIE_MTP_NTRIES) {
		msleep_interruptible(BUSLOAD_INTERVAL);

		/* set RE25 in shadow regiser */
		ret = tfa_dev_mtp_set(tfa,
			TFA_MTP_RE25, tfa->mohm[cal_idx]);
		if (ret != tfa_error_ok) {
			pr_err("%s: writing calibration data failed to MTP, device %d err (%d)\n",
				__func__, tfa->dev_idx, ret);
			return TFA98XX_ERROR_RPC_CALIB_FAILED;
		}

		msleep_interruptible(BUSLOAD_INTERVAL);

		readback = tfa_dev_mtp_get(tfa, TFA_MTP_RE25);
		if (readback < 0) {
			pr_err("%s: reading calibration data back failed from MTP, device %d readback (%d)\n",
				__func__, tfa->dev_idx, readback);
			return TFA98XX_ERROR_RPC_CALIB_FAILED;
		}
		pr_info("%s: readback from MTP - %d mOhms\n",
			__func__, readback);
		error = tfa_calibration_range_check(tfa,
			channel, readback);
		if (error != TFA98XX_ERROR_OK) {
			pr_err("%s: calibration data is out of range: device %d (to rewrite)\n",
				__func__, tfa->dev_idx);
			continue;
		}

		if (readback != tfa->mohm[cal_idx]) {
			pr_err("%s: calibration data was not written to MTP, device %d (%d != %d)\n",
				__func__, tfa->dev_idx,
				tfa->mohm[cal_idx], readback);
			continue;
		}

		break;
	}

	if (tries >= TFA98XX_API_REWRTIE_MTP_NTRIES) {
		pr_err("%s: writing calibration data timed out, device %d\n",
			__func__, tfa->dev_idx);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	/* set MTPEX */
	tries = 0;
	while (++tries < TFA98XX_API_REWRTIE_MTP_NTRIES) {
		msleep_interruptible(BUSLOAD_INTERVAL);

		ret = tfa_dev_mtp_set(tfa, TFA_MTP_EX, 1);
		if (ret != tfa_error_ok) {
			pr_err("%s: setting MPTEX failed, device %d err (%d)\n",
				__func__, tfa->dev_idx, ret);
			continue;
		}

		msleep_interruptible(BUSLOAD_INTERVAL);

		readback = tfa_dev_mtp_get(tfa, TFA_MTP_EX);
		if (readback < 0) {
			pr_err("%s: reading MTPEX back failed from MTP, device %d readback (%d)\n",
				__func__, tfa->dev_idx, readback);
			return TFA98XX_ERROR_RPC_CALIB_FAILED;
		}

		if (readback != 1) {
			pr_err("%s: setting MTPEX failed, device %d (readback %d)\n",
				__func__, tfa->dev_idx, readback);
			continue;
		}

		break;
	}

	if (tries >= TFA98XX_API_REWRTIE_MTP_NTRIES) {
		pr_err("%s: setting MTPEX timed out, device %d\n",
			__func__, tfa->dev_idx);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	return error;
}

/* start count from 1, 0 is invalid */
int tfa_dev_get_swprof(struct tfa_device *tfa)
{
	if (!tfa->dev_ops.get_swprof)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.get_swprof)(tfa);
}

int tfa_dev_set_swprof(struct tfa_device *tfa, unsigned short new_value)
{
	if (!tfa->dev_ops.set_swprof)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.set_swprof)(tfa, new_value + 1);
}

/* same value for all channels */
/* start count from 1, 0 is invalid */
int tfa_dev_get_swvstep(struct tfa_device *tfa)
{
	if (!tfa->dev_ops.get_swvstep)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.get_swvstep)(tfa);
}

int tfa_dev_set_swvstep(struct tfa_device *tfa, unsigned short new_value)
{
	if (!tfa->dev_ops.set_swvstep)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.set_swvstep)(tfa, new_value + 1);
}

/*
 * function overload for MTPB
 */
int tfa_dev_get_mtpb(struct tfa_device *tfa)
{
	if (!tfa->dev_ops.get_mtpb)
		return TFA98XX_ERROR_NOT_IMPLEMENTED;

	return (tfa->dev_ops.get_mtpb)(tfa);
}

int tfa_is_cold(struct tfa_device *tfa)
{
	int value = 0;

	/*
	 * check for cold boot status
	 */
	if (tfa->is_probus_device) {
		if (tfa->ext_dsp > 0) {
			/* need to update before checking is_cold */
			/* tfa_dev_get_state(tfa); */
			if (tfa->state == TFA_STATE_OPERATING)
				value = 0; /* warm */
			else
				value = 1; /* cold */
		} else {
			value = (TFA_GET_BF(tfa, MANSCONF) == 0);
		}
	}

	value |= tfa->reset_mtpex; /* forced cold start */

	tfa->is_cold = value;

	return value;
}

int tfa_is_cold_amp(struct tfa_device *tfa)
{
	int value;

	/*
	 * for non-dsp device reading MANSCONF
	 * (src_set_configured) is a way
	 * to check for cold boot status
	 */
	value = (TFA_GET_BF(tfa, MANSCONF) == 0);

	return value;
}

int tfa_count_status_flag(struct tfa_device *tfa, int type)
{
	struct tfa_device *ntfa = NULL;
	int i, status, value = 0;

	if (tfa == NULL)
		return value;

	for (i = 0; i < tfa->dev_count; i++) {
		ntfa = tfa98xx_get_tfa_device_from_index(i);

		if (ntfa == NULL)
			continue;

		switch (type) {
		case TFA_SET_DEVICE:
			status = (ntfa->set_device) ? 1 : 0;
			break;
		case TFA_SET_CONFIG:
			status = (ntfa->set_config) ? 1 : 0;
			break;
		default:
			status = 0;
			break;
		}

		value += status;
	}

	if (tfa->verbose)
		pr_debug("%s: count (type %d): %d\n",
			__func__, type, value);

	return value;
}

void tfa_set_status_flag(struct tfa_device *tfa, int type, int value)
{
	struct tfa_device *ntfa = NULL;
	int i;

	if (tfa == NULL)
		return;

	if (value == 0 || value == 1) {
		switch (type) {
		case TFA_SET_DEVICE:
			tfa->set_device = value;
			break;
		case TFA_SET_CONFIG:
			tfa->set_config = value;
			break;
		default:
			break;
		}

		if (tfa->verbose)
			pr_debug("%s: set flag (type %d, dev %d) with %d\n",
				__func__, type, tfa->dev_idx, value);

		return;
	}

	/* reset all */
	for (i = 0; i < tfa->dev_count; i++) {
		ntfa = tfa98xx_get_tfa_device_from_index(i);

		if (ntfa == NULL)
			continue;

		switch (type) {
		case TFA_SET_DEVICE:
			ntfa->set_device = 0;
			break;
		case TFA_SET_CONFIG:
			ntfa->set_config = 0;
			break;
		default:
			break;
		}
	}

	if (tfa->verbose)
		pr_debug("%s: reset flag (type %d) for all\n",
			__func__, type);
}

int tfa_cf_enabled(struct tfa_device *tfa)
{
	int value = 1;

	/* For probus, there is no CF */
	if (tfa->is_probus_device)
		value = (tfa->ext_dsp != 0);

	return value;
}

#define NR_COEFFS 6
#define NR_BIQUADS 28
#define BQ_SIZE (3 * NR_COEFFS)
#define DSP_MSG_OVERHEAD 27

#pragma pack(push, 1)
struct dsp_msg_all_coeff {
	uint8_t select_eq[3];
	uint8_t biquad[NR_BIQUADS][NR_COEFFS][3];
};
#pragma pack(pop)

/* number of biquads for each equalizer */
static const int eq_biquads[] = {
	10, 10, 2, 2, 2, 2
};

#define NR_EQ (int)(sizeof(eq_biquads) / sizeof(int))

/* fill context info */
int tfa_dev_probe(int resp_addr, struct tfa_device *tfa)
{
	unsigned short rev, rev1;
	unsigned int revid = 0;
	int rc = 0;

	tfa->resp_address = (unsigned char)resp_addr;

	/* read revid via low level hal, register 3 */
	if (tfa98xx_read_register16(tfa,
		TFA98XX_DEVICE_REVISION0, &rev)) {
		pr_debug("Error: Unable to read revid from responder:0x%02x\n",
			resp_addr);
		return TFA_ERROR;
	}
	/* new family has 0x98 in rev MSB, and uses DEVREV MSB */
	if (((rev >> 8) & 0xff) == 0x98) {
		if (tfa98xx_read_register16(tfa,
			TFA98XX_DEVICE_REVISION1, &rev1)) {
			pr_debug("Error: Unable to read devrev from responder:0x%02x\n",
				resp_addr);
			return TFA_ERROR;
		}
		rc = TFAxx_GET_BF_VALUE(tfa, DEVREV, rev1);
		if (rc >= 0) {
			if ((rev & 0xff) == 0x65)
				rev = ((unsigned short)rc + 0x0a) << 8
					| (rev & 0xff);
			else
				rev = ((unsigned short)((rc & 0xf0)
					+ ((rc & 0x0c) >> 2)) + 0x0a) << 8
					| (rev & 0xff);
		}
	}

	tfa->rev = rev;
	tfa->dev_idx = -1;
	tfa->state = TFA_STATE_UNKNOWN;

	tfa->set_active = -1; /* undefined by default */
	tfa->mute_state = 0; /* unmute by default */
	tfa->pause_state = 0; /* not paused by default */
	tfa->spkgain = -1; /* undefined */
	tfa->inplev = -1; /* undefined */

	tfa_set_query_info(tfa);

	tfa->in_use = 1;
	tfa->is_calibrating = 0;
	tfa->disable_auto_cal = 1;

	tfa->dev_count = tfa->cnt->ndev;

	tfa->set_device = 0;
	tfa->set_config = 0;

	tfa->dev_idx = tfa_cont_get_idx(tfa);
	if (tfa->dev_idx < 0)
		return TFA_ERROR;

	/* device tfadsp of address 0x0 */
	tfa->dev_tfadsp = tfa_cont_get_idx_tfadsp(tfa, 0);
	if (tfa->dev_tfadsp != -1) {
		pr_info("%s: dev %d - found tfadsp device %d\n",
			__func__, tfa->dev_idx, tfa->dev_tfadsp);
		tfa->dev_count--;
	}

	revid = (unsigned int)rev;
	if (((rev & 0xff) == 0x66)
		&& (tfa_get_bf(tfa, 0xf3b1) == 1)) { /* type */
		revid--; /* 0x65 */
		if (tfa_get_bf(tfa, 0xf090)) /* disabled minion */
			revid |= 0x00000000; /* 0x65Nxxx */
		else
			revid |= 0x00010000; /* 0x65MNxxx*/
	}

	if ((rev & 0xff) == 0x66 && rc >= 0)
		/* TFA986xN2/N3, TFA986xMN2/MN3 */
		revid += (unsigned int)(rc & 0x03) << 20;

	tfa->revid = revid;

	return 0;
}

enum tfa_error tfa_dev_set_state(struct tfa_device *tfa,
	enum tfa_state state, int is_calibration)
{
	enum tfa98xx_error ret = TFA98XX_ERROR_OK;
	int loop = 50, ready = 0;
	int count;

	pr_info("%s: [%d] state = 0x%02x\n",
		__func__, tfa->dev_idx, state & 0xff);

	/* Base states */
	/* Do not change the order of setting bits as this is important! */
	switch (state & 0x0f) {
	case TFA_STATE_POWERDOWN:
		/* PLL in powerdown, Algo up */
		break;
	case TFA_STATE_INIT_HW:
		/* load I2C/PLL hardware setting (~wait2srcsettings) */
		break;
	case TFA_STATE_INIT_CF:
		/* We want to leave Wait4SrcSettings state for max2 */
		if (tfa->tfa_family == 2)
			TFA_SET_BF(tfa, MANSCONF, 1);

		/* And finally set PWDN to 0 to leave powerdown state */
		TFA_SET_BF(tfa, PWDN, 0);

		/* Make sure the DSP is running! */
		do {
			ret = tfa98xx_dsp_system_stable(tfa, &ready);
			if (ret != TFA98XX_ERROR_OK)
				return tfa_error_dsp;
			if (ready)
				break;
		} while (loop--);

		if ((!tfa->is_probus_device && is_calibration)
			|| ((tfa->rev & 0xff) == 0x13)) {
			/*
			 * Enable FAIM when clock is stable,
			 * to avoid MTP corruption
			 */
			ret = tfa98xx_faim_protect(tfa, 1);
			if (tfa->verbose)
				pr_debug("FAIM enabled (err %d).\n", ret);
		}
		break;
	case TFA_STATE_INIT_FW:
		/* DSP framework active (~patch loaded) */
		break;
	case TFA_STATE_OPERATING:
		/* Amp and Algo running */
		/* Depending on our previous state we need to set 3 bits */
		TFA_SET_BF(tfa, PWDN, 0); /* Coming from state 0 */
		TFA_SET_BF(tfa, MANSCONF, 1); /* Coming from state 1 */
		/* No SBSL for probus device, we set AMPE to 1 */
		TFA_SET_BF(tfa, AMPE, 1);

		/*
		 * Disable MTP clock to protect memory.
		 * However in case of calibration wait for DSP!
		 * (This should be case only during calibration).
		 */
		if (!tfa->is_probus_device && is_calibration) {
			count = MTPEX_WAIT_NTRIES * 4;
			if (tfa_dev_mtp_get(tfa, TFA_MTP_OTC) == 1
				&& tfa->tfa_family == 2) {
				/* Calibration takes a lot of time */
				while ((tfa_dev_mtp_get(tfa, TFA_MTP_EX) != 1)
					&& count) {
					msleep_interruptible(BUSLOAD_INTERVAL);
					count--;
				}
			}
			if (count <= 0)
				pr_err("%s: MTPEX is not set - timeout\n",
					__func__);

			if ((tfa->rev & 0xff) == 0x13) {
				ret = tfa98xx_faim_protect(tfa, 0);
				if (tfa->verbose)
					pr_debug("%s: FAIM disabled (err %d).\n",
						__func__, ret);
			}
		}
		break;
	case TFA_STATE_FAULT:
		/* An alarm or error occurred */
		break;
	case TFA_STATE_RESET:
		/* I2C reset and ACS set */
		tfa98xx_init(tfa);
		break;
	default:
		if (state & 0x0f)
			return tfa_error_bad_param;
		break;
	}

	/* state modifiers */

	if (state & TFA_STATE_MUTE)
		tfa98xx_set_mute(tfa, TFA98XX_MUTE_AMPLIFIER);

	if (state & TFA_STATE_UNMUTE) {
		if (tfa->mute_state)
			pr_info("%s: skip UNMUTE dev %d (by force)\n",
				__func__, tfa->dev_idx);
		else
			tfa98xx_set_mute(tfa, TFA98XX_MUTE_OFF);
	}

	/* tfa->state = state; */ /* to correct with real state of device */
	tfa_dev_get_state(tfa);

	return tfa_error_ok;
}

enum tfa_state tfa_dev_get_state(struct tfa_device *tfa)
{
	int manstate;

	manstate = tfa_get_manstate(tfa);
	pr_debug("%s: [%d] manstate = %d\n",
		__func__, tfa->dev_idx, manstate);

	switch (manstate) {
	case 0:
		tfa->state = TFA_STATE_POWERDOWN;
		break;
	case 8: /* if dsp reset if off assume framework is running */
		tfa->state = TFA_STATE_INIT_FW;
		break;
	case 9:
		tfa->state = TFA_STATE_OPERATING;
		break;
	default:
		tfa->state = TFA_STATE_UNKNOWN;
		break;
	}

	return tfa->state;
}

int tfa_dev_mtp_get(struct tfa_device *tfa, enum tfa_mtp item)
{
	int value = 0;

	/* read data from efs */
	switch (item) {
	case TFA_MTP_OTC:
		/* read one-time calibration setting */
		value = 1; /* always one-time as it's written */
		break;
	case TFA_MTP_EX:
		/* read MTPEX, if calibration is done */
		value = (tfa->mohm[0] > 0) ? 1 : 0;
		tfa->mtpex = value;
		break;
	case TFA_MTP_RE25:
	case TFA_MTP_RE25_PRIM:
	case TFA_MTP_RE25_SEC:
		/* read RE25, calibration data */
		value = tfa->mohm[0];
		break;
	case TFA_MTP_LOCK:
		break;
	default:
		/* wrong item */
		break;
	}

	return value;
}

enum tfa_error tfa_dev_mtp_set(struct tfa_device *tfa,
	enum tfa_mtp item, int value)
{
	enum tfa_error err = tfa_error_ok;
	enum tfa98xx_error ret = TFA98XX_ERROR_OK;
	int rdc = 0;

	/* write data to efs */
	switch (item) {
	case TFA_MTP_OTC:
		/* write one-time calibration setting */
		break;
	case TFA_MTP_EX:
		/* write MTPEX, if calibration is done */
		if (value == 0) {
			tfa->mohm[0] = 0;
			tfa->reset_mtpex = 0;
		} else {
			if (tfa->mohm[0] <= 0) {
				rdc = 6000; /* hard-coded */
				tfa->mohm[0] = rdc;
			}
		}
		tfa->mtpex = value;
		break;
	case TFA_MTP_RE25:
	case TFA_MTP_RE25_PRIM:
	case TFA_MTP_RE25_SEC:
		if (value != 0) { /* check range for non-zero value */
			ret = tfa_calibration_range_check(tfa,
				tfa_get_channel_from_dev_idx(tfa, -1), value);
			if (ret != TFA98XX_ERROR_OK) {
				err = tfa_error_bad_param;
				goto tfa_dev_mtp_set_exit;
			}
		}
		/* write RE25, calibration data */
		tfa->mohm[0] = value;
		pr_info("%s: ch. %d, mohm[0] %d\n", __func__, 
			tfa_get_channel_from_dev_idx(tfa, -1), tfa->mohm[0]);
		break;
	case TFA_MTP_LOCK:
		break;
	default:
		/* wrong item */
		break;
	}

tfa_dev_mtp_set_exit:
	if (err != tfa_error_ok)
		pr_err("%s: error (%d) in setting MTP (item %d with %d)\n",
			__func__, err, item, value);

	return err;
}

int tfa_reset_sticky_bits(struct tfa_device *tfa)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	uint16_t status0, status3;

	err = reg_read(tfa, TFA98XX_STATUS_FLAGS0, &status0);
	if (err != TFA98XX_ERROR_OK)
		status0 = 0;
	err = reg_read(tfa, TFA98XX_STATUS_FLAGS3, &status3);
	if (err != TFA98XX_ERROR_OK)
		status3 = 0;
	pr_debug("%s: read sticky bits: (STATUS_FLAGS0 0x%04x, STATUS_FLAGS3 0x%04x)\n",
		__func__, status0, status3);

	/* reset by writing on the flag */
	switch (tfa->rev & 0xff) {
	case 0x66:
		status0 |= 0xfeff;
		break;
	default:
		break;
	}
	status3 |= 0x0003;

	err = reg_write(tfa, TFA98XX_STATUS_FLAGS0, status0);
	err |= reg_write(tfa, TFA98XX_STATUS_FLAGS3, status3);

	return err;
}

enum tfa98xx_error tfaxx_status(struct tfa_device *tfa)
{
	int value;
	uint16_t val;
	int state, control;
	char reg_state[STAT_LEN] = {0};
	int idle_power = 0, low_power = 0, low_noise = 0;
	int musmode;
	int rcvmode;

	if (tfa->dev_ops.get_status != NULL)
		return(tfa->dev_ops.get_status(tfa));

	/*
	 * check IC status bits: cold start
	 * and DSP watch dog bit to re init
	 */
	value = TFAxx_READ_REG(tfa, VDDS); /* STATUS_FLAGS0 */
	if (value < 0)
		return -value;
	val = (uint16_t)value;

	/* Check secondary errors */
	if (!TFAxx_GET_BF_VALUE(tfa, UVDS, val)
		|| !TFAxx_GET_BF_VALUE(tfa, OVDS, val)
		|| TFAxx_GET_BF_VALUE(tfa, OCDS, val)
		|| !TFAxx_GET_BF_VALUE(tfa, OTDS, val)
		|| !((tfa->daimap & TFA98XX_DAI_TDM)
		|| TFAxx_GET_BF_VALUE(tfa, NOCLK, val)
		|| TFAxx_GET_BF_VALUE(tfa, VDDS, val)))
		pr_err("%s: Misc errors in #1 detected: STATUS_FLAG0 = 0x%x\n",
			__func__, val);
	if (TFAxx_GET_BF_VALUE(tfa, CLKOOR, val)
		|| !TFAxx_GET_BF_VALUE(tfa, DCOCPOK, val)
		|| !TFAxx_GET_BF_VALUE(tfa, OCPOAP, val)
		|| !TFAxx_GET_BF_VALUE(tfa, OCPOAN, val)
		|| !TFAxx_GET_BF_VALUE(tfa, OCPOBP, val)
		|| !TFAxx_GET_BF_VALUE(tfa, OCPOBN, val)
		|| (TFAxx_GET_BF_VALUE(tfa, DCTH, val)
		&& TFAxx_GET_BF(tfa, MANEDCTH)))
		pr_err("%s: Misc errors in #2 detected: STATUS_FLAG0 = 0x%x\n",
			__func__, val);

	snprintf(reg_state, STAT_LEN, "device [%d]", tfa->dev_idx);

	state = TFAxx_GET_BF(tfa, IPMS);
	snprintf(reg_state + strlen(reg_state),
		STAT_LEN - strlen(reg_state), ", IPMS %d", state);
	control = TFAxx_GET_BF(tfa, IPM);
	if ((control == 0x0 || control == 0x3)
		&& (state == 0x1))
		idle_power = 1;
	switch (tfa->rev & 0xff) {
	case 0x66:
		state = TFAxx_GET_BF(tfa, LPMS);
		snprintf(reg_state + strlen(reg_state),
			STAT_LEN - strlen(reg_state),
			", LPMS %d", state);
		control = TFAxx_GET_BF(tfa, LPM);
		if ((control == 0x0 || control == 0x3)
			&& (state == 0x1))
			low_power = 1;
		state = TFAxx_GET_BF(tfa, LDMS);
		control = TFAxx_GET_BF(tfa, LDM);
		snprintf(reg_state + strlen(reg_state),
			STAT_LEN - strlen(reg_state),
			", LDMS %d (LDM %d)",
			state, control);
		state = TFAxx_GET_BF(tfa, LNMS);
		snprintf(reg_state + strlen(reg_state),
			STAT_LEN - strlen(reg_state),
			", LNMS %d", state);
		musmode = TFAxx_GET_BF(tfa, MUSMODE);
		rcvmode = tfa_get_bf(tfa, TFA9866_BF_RCVM);
		control = tfa_get_bf(tfa, TFA9866_BF_LNM);
		if ((control == 0x0)
			&& (state == 0x1 && musmode == 0x1))
			low_noise = 1;
		snprintf(reg_state + strlen(reg_state),
			STAT_LEN - strlen(reg_state),
			" (MUSM %d, RCVM %d, LNM %d)",
			musmode, rcvmode, control);
		break;
	default:
		/* neither TFA987x */
		break;
	}

	pr_debug("%s: %s\n", __func__, reg_state);

	value = TFAxx_READ_REG(tfa, MANSTATE); /* STATUS_FLAGS4/2 */
	if (value < 0)
		return -value;
	val = (uint16_t)value;

	pr_info("%s: manstate %d, ampstate %d\n", __func__,
		TFAxx_GET_BF_VALUE(tfa, MANSTATE, val),
		TFAxx_GET_BF_VALUE(tfa, AMPSTE, val));

	value = TFAxx_READ_REG(tfa, PLLS); /* STATUS_FLAGS1 */
	if (value < 0)
		return -value;
	val = (uint16_t)value;

	if (!TFAxx_GET_BF_VALUE(tfa, SWS, val)) {
		if (idle_power)
			pr_info("%s: idle power disabled amplifier\n",
				__func__);
		else
			pr_err("%s: ERROR: SWS\n", __func__);
	}
	if (!TFAxx_GET_BF_VALUE(tfa, CLKS, val))
		pr_err("%s: ERROR: CLKS\n", __func__);
	if (!TFAxx_GET_BF_VALUE(tfa, PLLS, val))
		pr_err("%s: ERROR: PLLS\n", __func__);

	if ((tfa->daimap & TFA98XX_DAI_TDM) && (tfa->tfa_family == 2)) {
		if (TFAxx_GET_BF(tfa, TDMERR)) {
			if ((low_power || idle_power)
				&& TFAxx_GET_BF(tfa, TDMSTAT) == 0x7)
				pr_info("%s: low power disabled sensing block\n",
					__func__);
			else
				pr_err("%s: TDM related errors: STATUS_FLAG1 = 0x%x, STATUS_FLAG2 = 0x%x\n",
					__func__, TFAxx_READ_REG(tfa, TDMERR),
					TFAxx_READ_REG(tfa, TDMSTAT));
		}
		if (TFAxx_GET_BF(tfa, TDMLUTER))
			pr_err("%s: TDM related errors: STATUS_FLAG1 = 0x%x\n",
				__func__, TFAxx_READ_REG(tfa, TDMLUTER));
	}
	if (low_noise)
		pr_debug("%s: low noise detected\n", __func__);

	value = TFAxx_READ_REG(tfa, BODNOK); /* STATUS_FLAGS3 */
	if (value < 0)
		return -value;
	val = (uint16_t)value;

	if ((TFAxx_GET_BF_VALUE(tfa, BODNOK, val)
		&& TFAxx_GET_BF(tfa, MANROBOD))
		|| (TFAxx_GET_BF_VALUE(tfa, QPFAIL, val)
		&& TFAxx_GET_BF(tfa, QALARM)))
		pr_err("%s: Misc errors detected: STATUS_FLAG3 = 0x%x\n",
			__func__, val);

	return TFA98XX_ERROR_OK;
}

int tfa_wait4manstate(struct tfa_device *tfa,
	uint16_t bf, uint16_t wait_value, int loop)
{
	/* TODO: use tfa->reg_time */
	int value, rc;
	int loop_arg = loop;

	do {
		value = tfa_get_bf(tfa, bf); /* read */
	} while (value < wait_value && --loop);

	rc = loop ? 0 : -ETIME;

	if (rc == -ETIME)
		pr_err("%s: timeout waiting for bitfield:0x%04x, value:%d, %d times\n",
			__func__, bf, wait_value, loop_arg);

	return rc;
}

int tfa_ext_event_handler(enum tfadsp_event_en tfadsp_event)
{
	int dirt_flag = 0;

	pr_info("%s: tfadsp event 0x%04x\n", __func__, tfadsp_event);

	if (tfadsp_event & TFADSP_CMD_ACK) {
		/* action for TFADSP_CMD_ACK */
		dirt_flag = 1;
	}
	if (tfadsp_event & TFADSP_SOFT_MUTE_READY) {
		/* action for TFADSP_SOFT_MUTE_READY */
		dirt_flag = 1;
	}
	if (tfadsp_event & TFADSP_VOLUME_READY) {
		/* action for TFADSP_VOLUME_READY */
		dirt_flag = 1;
	}
	if (tfadsp_event & TFADSP_DAMAGED_SPEAKER) {
		/* action for TFADSP_DAMAGED_SPEAKER */
		dirt_flag = 1;
	}
	if (tfadsp_event & TFADSP_CALIBRATE_DONE) {
		/* action for TFADSP_CALIBRATE_DONE */
		dirt_flag = 1;
	}
	if (tfadsp_event & TFADSP_SPARSESIG_DETECTED) {
		/* action for TFADSP_SPARSESIG_DETECTED */
		dirt_flag = 1;
	}
	if (tfadsp_event & TFADSP_CMD_READY) {
		/* action for TFADSP_CMD_READY */
		dirt_flag = 1;
	}
	if (tfadsp_event & TFADSP_EXT_PWRUP) {
		/* action for TFADSP_EXT_PWRUP */
		dirt_flag = 1;
	}
	if (tfadsp_event & TFADSP_EXT_PWRDOWN) {
		/* action for TFADSP_EXT_PWRDOWN */
		dirt_flag = 1;
	}
	if (tfadsp_event & TFADSP_EXT_PWRDOWN) {
		/* action for TFADSP_EXT_PWRDOWN */
		dirt_flag = 1;
	}

	if (!dirt_flag)
		pr_err("%s: undefined (0x%04x)\n", __func__, tfadsp_event);

	return 0;
}

static enum tfa98xx_error
tfa_calibration_range_check(struct tfa_device *tfa,
	unsigned int channel, int mohm)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int lower_limit_cal, upper_limit_cal;

	lower_limit_cal = tfa->lower_limit_cal;
	upper_limit_cal = tfa->upper_limit_cal;

	pr_info("%s: 0x%08x [%d] - calibration range check [%d, %d] with %d\n",
		__func__, tfa->revid, channel,
		lower_limit_cal, upper_limit_cal, mohm);
	if (mohm < lower_limit_cal || mohm > upper_limit_cal)
		err = TFA98XX_ERROR_BAD_PARAMETER;
	if (mohm == 0)
		err = TFA98XX_ERROR_BAD_PARAMETER;

	return err;
}

int tfa_wait_until_calibration_done(struct tfa_device *tfa)
{
	int tries = 0;

	if (!tfa->is_calibrating)
		return 0; /* not running now */

	pr_info("%s: calibration / V validation now runs\n",
		__func__);
	while (tries < TFA98XX_API_WAITCAL_NTRIES) {
		msleep_interruptible(CAL_STATUS_INTERVAL);
		if (tfa->is_calibrating == 0)
			return 1; /* done */
		tries++;
	}

	return 0; /* timeout */
}

enum tfa98xx_error tfa_configure_log(int enable)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_device *tfa = NULL;
	uint8_t cmd_buf[3];

	/* set head device */
	tfa = tfa98xx_get_tfa_device_from_index(-1);
	if (tfa == NULL)
		return TFA98XX_ERROR_DEVICE; /* unused device */

	pr_info("%s: set blackbox control (%d)\n",
		__func__, enable);

	if (!tfa->is_bypass) {
		cmd_buf[0] = 0;
		cmd_buf[1] = 0;
		/* 0 - disable logger, 1 - enable logger */
		cmd_buf[2] = (enable) ? 1 : 0;

		pr_info("%s: set blackbox (%d)\n",
			__func__, enable);
		err = tfa_dsp_cmd_id_write(tfa, MODULE_SPEAKERBOOST,
			SB_PARAM_SET_DATA_LOGGER, 3, cmd_buf);
		if (err) {
			pr_err("%s: error in setting blackbox, err = %d\n",
				__func__, err);
			return err;
		}
	} else {
		pr_info("%s: skip but store setting (%d) in bypass\n",
			__func__, enable);
	}

	tfa->blackbox_enable = enable;

	return err;
}

enum tfa98xx_error tfa_update_log(void)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_device *tfa = NULL, *ntfa = NULL;
	int ndev, idx, channel, offset, group, i;
	uint8_t cmd_buf[TFA_LOG_MAX_COUNT * MAX_HANDLES * 3] = {0};
	int data[MAX_HANDLES * TFA_LOG_MAX_COUNT] = {0};
	int read_size;


	/* set head device */
	tfa = tfa98xx_get_tfa_device_from_index(-1);
	if (tfa == NULL)
		return TFA98XX_ERROR_DEVICE; /* unused device */

	if (!tfa->blackbox_enable) {
		pr_info("%s: blackbox is inactive\n", __func__);
		return err;
	}

	ndev = tfa->dev_count;
	if (ndev < 1)
		return TFA98XX_ERROR_DEVICE;

	read_size = TFA_LOG_MAX_COUNT * ndev * 3;

	pr_info("%s: read from blackbox\n", __func__);
	err = tfa_dsp_cmd_id_write_read(tfa, MODULE_SPEAKERBOOST,
		SB_PARAM_GET_DATA_LOGGER, read_size, cmd_buf);
	if (err) {
		pr_err("%s: failed to read data from blackbox, err = %d\n",
			__func__, err);
		return err;
	}

	tfa98xx_convert_bytes2data(read_size, cmd_buf, data);

	for (idx = 0; idx < ndev; idx++) {
		ntfa = tfa98xx_get_tfa_device_from_index(idx);

		if (ntfa == NULL)
			continue;
		if (!tfa_is_active_device(ntfa))
			continue;

		channel = tfa_get_channel_from_dev_idx(ntfa, -1);
		if (channel == -1)
			continue;

		offset = channel * TFA_LOG_MAX_COUNT;
		group = idx * ID_BLACKBOX_MAX;

		pr_info("%s: dev %d - raw blackbox: [X = 0x%08x, T = 0x%08x]\n",
			__func__, idx,
			data[offset + ID_MAXX_LOG],
			data[offset + ID_MAXT_LOG]);

		/* maximum x (um) */
		data[offset + ID_MAXX_LOG] = (int)
			((unsigned int)data[offset + ID_MAXX_LOG]
			/ TFA2_FW_X_DATA_UM_SCALE);
		if (tfa->log_data[group + ID_MAXX_LOG]
			< data[offset + ID_MAXX_LOG])
			tfa->log_data[group + ID_MAXX_LOG]
				= data[offset + ID_MAXX_LOG];

		/* maximum x kept without reset */
		if (tfa->log_data[group + ID_MAXX_KEEP_LOG]
			< data[offset + ID_MAXX_LOG])
			tfa->log_data[group + ID_MAXX_KEEP_LOG]
				= data[offset + ID_MAXX_LOG];

		/* maximum t (degC) */
		data[offset + ID_MAXT_LOG] = (int)
			(data[offset + ID_MAXT_LOG]
			/ TFA2_FW_T_DATA_SCALE);
		if (tfa->log_data[group + ID_MAXT_LOG]
			< data[offset + ID_MAXT_LOG])
			tfa->log_data[group + ID_MAXT_LOG]
				= data[offset + ID_MAXT_LOG];

		/* maximum t kept without reset */
		if (tfa->log_data[group + ID_MAXT_KEEP_LOG]
			< data[offset + ID_MAXT_LOG])
			tfa->log_data[group + ID_MAXT_KEEP_LOG]
				= data[offset + ID_MAXT_LOG];

		/* counter of x > x_max */
		tfa->log_data[group + ID_OVERXMAX_COUNT]
			+= data[offset + ID_OVERXMAX_COUNT];

		/* counter of t > t_max */
		tfa->log_data[group + ID_OVERTMAX_COUNT]
			+= data[offset + ID_OVERTMAX_COUNT];

		for (i = 0; i < TFA_LOG_MAX_COUNT; i++)
			pr_info("%s: dev %d - blackbox: data[%d] = %d (<- %d)\n",
				__func__, idx, i,
				tfa->log_data[group + i],
				data[offset + i]);
		pr_info("%s: dev %d - blackbox: data[%d] = %d\n",
			__func__, idx, ID_MAXX_KEEP_LOG,
			tfa->log_data[group + ID_MAXX_KEEP_LOG]);
		pr_info("%s: dev %d - blackbox: data[%d] = %d\n",
			__func__, idx, ID_MAXT_KEEP_LOG,
			tfa->log_data[group + ID_MAXT_KEEP_LOG]);
	}

	return err;
}

#define TEMP_INDEX	0
enum tfa98xx_error tfa_read_tspkr(struct tfa_device *tfa, int *spkt)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	struct tfa_device *ntfa = NULL;
	unsigned char bytes[(TEMP_INDEX + 2) * 3] = {0};
	int channel = 0;
	int data[TEMP_INDEX + 2];
	int nr_bytes, i, spkr_count = 0;

	if (tfa_count_status_flag(tfa, TFA_SET_DEVICE) != 0
		|| tfa_count_status_flag(tfa, TFA_SET_CONFIG) != 0) {
		pr_info("%s: skip if device is being configured\n", __func__);
		return TFA98XX_ERROR_DSP_NOT_RUNNING;
	}

	error = tfa_supported_speakers(tfa, &spkr_count);
	if (error != TFA98XX_ERROR_OK) {
		pr_err("error in checking supported speakers\n");
		return error;
	}

	/* SoftDSP interface differs from hw-dsp interfaces */
	if (tfa->is_probus_device && tfa->dev_count > 1)
		spkr_count = tfa->dev_count;

	nr_bytes = (TEMP_INDEX + spkr_count) * 3;

	pr_info("%s: read SB_PARAM_GET_TSPKR\n", __func__);
	error = tfa_dsp_cmd_id_write_read(tfa,
		MODULE_SPEAKERBOOST,
		SB_PARAM_GET_TSPKR, nr_bytes, bytes);

	if (error != TFA98XX_ERROR_OK) {
		pr_info("%s: failure in reading speaker temperature (err %d)\n",
			__func__, error);
		return error;
	}

	tfa98xx_convert_bytes2data(nr_bytes, bytes, data);

	pr_debug("%s: SPKR_TEMP - spkr_count %d, dev_count %d\n",
		__func__, spkr_count, tfa->dev_count);
	pr_debug("%s: SPKR_TEMP - data[0]=%d, data[1]=%d\n",
		__func__, data[TEMP_INDEX], data[TEMP_INDEX + 1]);

	/* real-time t (degC) */
	for (i = 0; i < tfa->dev_count; i++) {
		ntfa = tfa98xx_get_tfa_device_from_index(i);

		if (!tfa_is_active_device(ntfa)) {
			spkt[i] = 0xffff; /* inactive */
			continue;
		}

		channel = tfa_get_channel_from_dev_idx(ntfa, -1);
		if (channel == -1) {
			spkt[i] = 0xffff; /* invalid channel */
			continue;
		}

		spkt[i] = (int)(data[TEMP_INDEX + channel]
			/ TFA2_FW_T_DATA_SCALE);
	}

	return error;
}

enum tfa98xx_error tfa_write_volume(struct tfa_device *tfa, int *sknt)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int i, spkr_count = 0;
	unsigned char bytes[2 * 3] = {0};
	int data = 0;

	if (tfa_count_status_flag(tfa, TFA_SET_DEVICE) != 0
		|| tfa_count_status_flag(tfa, TFA_SET_CONFIG) != 0) {
		pr_info("%s: skip if device is being configured\n", __func__);
		return TFA98XX_ERROR_DSP_NOT_RUNNING;
	}

	error = tfa_supported_speakers(tfa, &spkr_count);
	if (error != TFA98XX_ERROR_OK) {
		pr_err("error in checking supported speakers\n");
		return error;
	}

	if (sknt == NULL) {
		pr_err("bad parameter error as sknt is null\n");
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	for (i = 0; i < MAX_CHANNELS; i++) {
		pr_info("%s: dev %d - surface temperature (%d)\n",
			__func__, i, sknt[i]);
		data = (int)sknt[i]; /* send value directly */

		bytes[i * 3] = (uint8_t)((data >> 16) & 0xffff);
		bytes[i * 3 + 1] = (uint8_t)((data >> 8) & 0xff);
		bytes[i * 3 + 2] = (uint8_t)(data & 0xff);
	}

	tfa->individual_msg = 1;

	pr_info("%s: write CUSTOM_PARAM_SET_TSURF\n", __func__);
	error = tfa_dsp_cmd_id_write
		(tfa, MODULE_CUSTOM, CUSTOM_PARAM_SET_TSURF,
		sizeof(bytes), bytes);
	if (error != TFA98XX_ERROR_OK)
		pr_info("%s: failure in writing surface temperature (err %d)\n",
			__func__, error);

	return error;
}

