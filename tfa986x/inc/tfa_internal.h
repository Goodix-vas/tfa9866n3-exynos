/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 */
/*
 * internal functions for TFA layer (not shared with SRV and HAL layer!)
 */

#ifndef __TFA_INTERNAL_H__
#define __TFA_INTERNAL_H__

#include "tfa_dsp_fw.h"
#include "tfa_ext.h"

#if __GNUC__ >= 4
  #define TFA_INTERNAL __attribute__((visibility("hidden")))
#else
  #define TFA_INTERNAL
#endif

#define TFA98XX_GENERIC_RESP_ADDRESS 0x1C

/* 2 channels per SB */
#define MAX_HANDLES 4
#define MAX_CHANNELS 2

/* max. length of a alsa mixer control name */
#define MAX_CONTROL_NAME 48

#define MIN_CALIBRATION_DATA 0
#define MAX_CALIBRATION_DATA 32000
#define DUMMY_CALIBRATION_DATA 6000

#define DEFAULT_REF_TEMP 25
#define TFA_NOT_FOUND -1

enum instream_state {
	BIT_PSTREAM = 1, /* b0 */
	BIT_CSTREAM = 2, /* b1 */
};

/* tfa98xx: tfa_device_ops */
void tfanone_ops(struct tfa_device_ops *ops);
void tfa986x_ops(struct tfa_device_ops *ops);

/* bitfield extraction macros */
#define TFA_BF_REG(bf) ((bf & (0xff00)) >> 8)
#define TFA_BF_POS(bf) ((bf & (0xf0)) >> 4)
#define TFA_BF_WIDTH(bf) ((bf & (0x0f)) + 1)
#define TFA_BF_MSK(bf) (((1 << TFA_BF_WIDTH(bf)) - 1) << TFA_BF_POS(bf))

enum tfa98xx_error tfa_buffer_pool(struct tfa_device *tfa,
	int index, int size, int control);
int tfa98xx_buffer_pool_access(int r_index,
	size_t g_size, uint8_t **buf, int control);

int tfa98xx_get_fssel(unsigned int rate);

struct tfa_device *tfa98xx_get_tfa_device_from_index(int index);
struct tfa_device *tfa98xx_get_tfa_device_from_channel(int channel);

int tfa_get_channel_from_dev_idx(struct tfa_device *tfa, int dev_idx);
int tfa_get_dev_idx_from_inchannel(int inchannel);

int tfa98xx_count_active_stream(int stream_flag);

int tfa_ext_event_handler(enum tfadsp_event_en tfadsp_event);

void tfa_set_ipc_loaded(int status);
int tfa_get_ipc_loaded(void);

void tfa_set_active_handle(struct tfa_device *tfa, int profile);
void tfa_reset_active_handle(struct tfa_device *tfa);
int tfa_is_active_device(struct tfa_device *tfa);

void tfa_handle_damaged_speakers(struct tfa_device *tfa);

void tfa_set_spkgain(struct tfa_device *tfa);

void tfa_restore_after_cal(int index, int cal_err);
enum tfa98xx_error tfa_run_cal(int index, uint16_t *value);
enum tfa98xx_error tfa_get_cal_data(int index, uint16_t *value);
enum tfa98xx_error tfa_get_cal_data_channel(int channel, uint16_t *value);
enum tfa98xx_error tfa_set_cal_data(int index, uint16_t value);
enum tfa98xx_error tfa_set_cal_data_channel(int channel, uint16_t value);
enum tfa98xx_error tfa_get_cal_temp(int index, uint16_t *value);
enum tfa98xx_error tfa_get_cal_temp_channel(int channel, uint16_t *value);

#define TFA_LOG_MAX_COUNT	4
int tfa_set_blackbox(int enable);
enum tfa98xx_error tfa_configure_log(int enable);
enum tfa98xx_error tfa_update_log(void);

int tfa_get_power_state(int index);

int tfa_wait_until_calibration_done(struct tfa_device *tfa);

enum tfa98xx_error tfa98xx_faim_protect(struct tfa_device *tfa, int state);

enum tfa98xx_error tfa98xx_set_osc_powerdown(struct tfa_device *tfa, int state);

enum tfa98xx_error tfa98xx_update_lpm(struct tfa_device *tfa, int state);

#endif /* __TFA_INTERNAL_H__ */

