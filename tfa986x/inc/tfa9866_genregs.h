/** Filename: tfa9866_genregs.h
 *  This file was generated automatically on 07/18/23 at 09:30:20. 
 *  Source file: TFA9866_GPA2_N1A1_I2C_RegisterMap.xlsx
 */

#ifndef _TFA9866_GENREGS_H
#define _TFA9866_GENREGS_H


#define TFA98XX_SYS_CONTROL0                        0x00
#define TFA98XX_SYS_CONTROL1                        0x01
#define TFA98XX_SYS_CONTROL2                        0x02
#define TFA98XX_DEVICE_REVISION0                    0x03
#define TFA98XX_CLOCK_CONTROL                       0x04
#define TFA98XX_KEY2_PROTECTED_CLOCK_GATING_CONTROL 0x05
#define TFA98XX_DEVICE_REVISION1                    0x06
#define TFA98XX_STATUS_FLAGS0                       0x10
#define TFA98XX_STATUS_FLAGS1                       0x11
#define TFA98XX_STATUS_FLAGS2                       0x12
#define TFA98XX_STATUS_FLAGS3                       0x13
#define TFA98XX_BATTERY_VOLTAGE                     0x15
#define TFA98XX_TEMPERATURE                         0x16
#define TFA98XX_VDDP_VOLTAGE                        0x17
#define TFA98XX_TDM_CONFIG0                         0x20
#define TFA98XX_TDM_CONFIG1                         0x21
#define TFA98XX_TDM_CONFIG2                         0x22
#define TFA98XX_INTERRUPT_OUT_REG                   0x40
#define TFA98XX_INTERRUPT_IN_REG                    0x44
#define TFA98XX_INTERRUPT_ENABLE_REG                0x48
#define TFA98XX_INTERRUPT_POLARITY_REG              0x4c
#define TFA98XX_BAT_PROT_CONFIG                     0x50
#define TFA98XX_AUDIO_CONTROL                       0x51
#define TFA98XX_AMPLIFIER_CONFIG                    0x52
#define TFA98XX_BSS_CONFIG                          0x5a
#define TFA98XX_BSS_CONFIG1                         0x5b
#define TFA98XX_BSS_CONFIG2                         0x5c
#define TFA98XX_GAIN_ATT                            0x5f
#define TFA98XX_IDLE_POWER_DETECTOR1                0x60
#define TFA98XX_LOW_DRIVE_DETECTOR1                 0x62
#define TFA98XX_LOW_DRIVE_DETECTOR2                 0x63
#define TFA98XX_LOW_POWER_DETECTOR1                 0x66
#define TFA98XX_TDM_SOURCE_CTRL                     0x68
#define TFA98XX_TDM_SOURCE_CTRL2                    0x69
#define TFA98XX_STATUS_FLAGS5                       0x6e
#define TFA98XX_CURSENSE_COMP                       0x6f
#define TFA98XX_DCDC_CONTROL0                       0x70
#define TFA98XX_DCDC_CONTROL4                       0x74
#define TFA98XX_DCDC_CONTROL5                       0x75
#define TFA98XX_DCDC_CONTROL6                       0x76
#define TFA98XX_LOW_NOISE_CTRL2                     0x7c
#define TFA98XX_VOLSENSE_DC_PROT_CTRL               0x84
#define TFA98XX_EFUSEKEY2_REG                       0xa1
#define TFA98XX_EFUSE_STATUS                        0xa2
#define TFA98XX_KEY1_PLL_CONTROL2                   0xc9
#define TFA98XX_KEY1_PLL_CONTROL3                   0xca
#define TFA98XX_KEY2_PROTECTED_PLL_CONTROL7         0xce
#define TFA98XX_KEY2_PROTECTED_MIS_CTRL             0xd1
#define TFA98XX_BST_ATB_CONFIG4                     0xdd
#define TFA98XX_SOFTWARE_PROFILE                    0xe0
#define TFA98XX_SOFTWARE_VSTEP                      0xe1
#define TFA98XX_KEY1_PROTECTED_EFUSE6               0xf6

/*
 * (0x00)-sys_control0
 */

/*
 * powerdown
 */

/*
 * reset
 */

/*
 * enbl_amplifier
 */

/*
 * enbl_boost
 */

/*
 * enbl_amp_hiz
 */

/*
 * enable_dynamic_uvp
 */

/*
 * int_pad_io
 */

/*
 * man_enbl_qpump_ok
 */

/*
 * bypass_uvp
 */

/*
 * bypass_ocp
 */

/*
 * enbl_dpsa_qpump_ok
 */

/*
 * enbl_amp_qpump_ok
 */

/*
 * enbl_pll_synchronisation
 */

/*
 * sel_hysteresis
 */


/*
 * (0x01)-sys_control1
 */

/*
 * src_set_configured
 */

/*
 * disable_mute_time_out
 */

/*
 * man_enbl_brown
 */

/*
 * man_enbl_dc_too_high
 */


/*
 * (0x02)-sys_control2
 */

/*
 * audio_fs
 */

/*
 * cs_frac_delay
 */


/*
 * (0x03)-device_revision0
 */

/*
 * product
 */


/*
 * (0x04)-clock_control
 */

/*
 * pll_clkin_sel
 */

/*
 * bypass_tdm_glitch_filter
 */

/*
 * enbl_osc_auto_off
 */

/*
 * enbl_fs_sync
 */

/*
 * enbl_clkref_sync
 */

/*
 * sel_pwm_freq
 */


/*
 * (0x05)-KEY2_protected_clock_gating_control
 */

/*
 * disable_cgu_sync_cgate
 */


/*
 * (0x06)-device_revision1
 */

/*
 * category
 */

/*
 * version
 */


/*
 * (0x10)-status_flags0
 */

/*
 * flag_por
 */

/*
 * flag_otpok
 */

/*
 * flag_uvpok
 */

/*
 * flag_ovpok
 */

/*
 * flag_ocp_alarm
 */

/*
 * flag_lost_clk
 */

/*
 * flag_clk_out_of_range
 */

/*
 * flag_bst_ocpok
 */

/*
 * flag_minionocpokn
 */

/*
 * flag_minionocpokp
 */

/*
 * flag_ocpokap
 */

/*
 * flag_ocpokan
 */

/*
 * flag_ocpokbp
 */

/*
 * flag_ocpokbn
 */

/*
 * flag_dc_too_high
 */


/*
 * (0x11)-status_flags1
 */

/*
 * flag_clocks_stable
 */

/*
 * flag_efuse_busy
 */

/*
 * flag_tdm_error
 */

/*
 * flag_low_power_mode
 */

/*
 * flag_low_noise_mode
 */

/*
 * flag_follower_mode
 */

/*
 * flag_boost_mode
 */

/*
 * flag_pll_lock
 */

/*
 * flag_tdm_lut_error
 */

/*
 * flag_engage
 */

/*
 * flag_enbl_amp
 */

/*
 * flag_enbl_ref
 */

/*
 * flag_clip
 */

/*
 * flag_low_drive_mode
 */

/*
 * flag_vddp_gt_vbat
 */

/*
 * flag_bss
 */


/*
 * (0x12)-status_flags2
 */

/*
 * man_state
 */

/*
 * amp_ctrl_state
 */

/*
 * flag_tdm_status
 */

/*
 * flag_qpump_clk_freq
 */

/*
 * flag_waiting_for_sync
 */

/*
 * flag_active_pst_sections
 */


/*
 * (0x13)-status_flags3
 */

/*
 * flag_bod_vddd_nok
 */

/*
 * flag_qpump_fail
 */


/*
 * (0x15)-battery_voltage
 */

/*
 * bat_adc
 */


/*
 * (0x16)-temperature
 */

/*
 * temp_adc
 */


/*
 * (0x17)-vddp_voltage
 */

/*
 * vddp_adc
 */


/*
 * (0x20)-tdm_config0
 */

/*
 * tdm_enable
 */

/*
 * tdm_vamp_sel
 */

/*
 * tdm_input_level
 */

/*
 * tdm_clk_inversion
 */

/*
 * tdm_fs_ws_polarity
 */

/*
 * tdm_nb_of_slots
 */

/*
 * tdm_slot_length
 */

/*
 * tdm_sample_size
 */

/*
 * tdm_nbck
 */


/*
 * (0x21)-tdm_config1
 */

/*
 * tdm_data_delay
 */

/*
 * tdm_data_adjustment
 */

/*
 * tdm_sink0_enable
 */

/*
 * tdm_sink1_enable
 */

/*
 * tdm_source0_enable
 */

/*
 * tdm_source1_enable
 */

/*
 * tdm_source2_enable
 */

/*
 * tdm_sink0_slot
 */

/*
 * tdm_sink1_slot
 */


/*
 * (0x22)-tdm_config2
 */

/*
 * tdm_source0_slot
 */

/*
 * tdm_source1_slot
 */

/*
 * tdm_source2_slot
 */


/*
 * (0x40)-interrupt_out_reg
 */

/*
 * int_out_flag_por
 */

/*
 * int_out_flag_bst_ocpok
 */

/*
 * int_out_flag_otpok
 */

/*
 * int_out_flag_ocp_alarm
 */

/*
 * int_out_flag_uvpok
 */

/*
 * int_out_flag_tdm_error
 */

/*
 * int_out_flag_lost_clk
 */

/*
 * int_out_flag_dc_too_high
 */

/*
 * int_out_flag_bod_vddd_nok
 */

/*
 * int_out_flag_clk_out_of_range
 */

/*
 * int_out_flag_ovpok
 */

/*
 * int_out_flag_qpump_fail
 */


/*
 * (0x44)-interrupt_in_reg
 */

/*
 * int_in_flag_por
 */

/*
 * int_in_flag_bst_ocpok
 */

/*
 * int_in_flag_otpok
 */

/*
 * int_in_flag_ocp_alarm
 */

/*
 * int_in_flag_uvpok
 */

/*
 * int_in_flag_tdm_error
 */

/*
 * int_in_flag_lost_clk
 */

/*
 * int_in_flag_dc_too_high
 */

/*
 * int_in_flag_bod_vddd_nok
 */

/*
 * int_in_flag_clk_out_of_range
 */

/*
 * int_in_flag_ovpok
 */

/*
 * int_in_flag_qpump_fail
 */


/*
 * (0x48)-interrupt_enable_reg
 */

/*
 * int_enable_flag_por
 */

/*
 * int_enable_flag_bst_ocpok
 */

/*
 * int_enable_flag_otpok
 */

/*
 * int_enable_flag_ocp_alarm
 */

/*
 * int_enable_flag_uvpok
 */

/*
 * int_enable_flag_tdm_error
 */

/*
 * int_enable_flag_lost_clk
 */

/*
 * int_enable_flag_dc_too_high
 */

/*
 * int_enable_flag_bod_vddd_nok
 */

/*
 * int_enable_flag_clk_out_of_range
 */

/*
 * int_enable_flag_ovpok
 */

/*
 * int_enable_flag_qpump_fail
 */


/*
 * (0x4c)-interrupt_polarity_reg
 */

/*
 * int_polarity_flag_por
 */

/*
 * int_polarity_flag_bst_ocpok
 */

/*
 * int_polarity_flag_otpok
 */

/*
 * int_polarity_flag_ocp_alarm
 */

/*
 * int_polarity_flag_uvpok
 */

/*
 * int_polarity_flag_tdm_error
 */

/*
 * int_polarity_flag_lost_clk
 */

/*
 * int_polarity_flag_dc_too_high
 */

/*
 * int_polarity_flag_bod_vddd_nok
 */

/*
 * int_polarity_flag_clk_out_of_range
 */

/*
 * int_polarity_flag_ovpok
 */

/*
 * int_polarity_flag_qpump_fail
 */


/*
 * (0x50)-bat_prot_config
 */

/*
 * rst_min_vbat
 */

/*
 * sel_vbat
 */

/*
 * bypass_clipper
 */


/*
 * (0x51)-audio_control
 */

/*
 * bypass_hp
 */


/*
 * (0x52)-amplifier_config
 */

/*
 * gain
 */

/*
 * bypass_dly_line
 */


/*
 * (0x5a)-bss_config
 */

/*
 * bss_minvbat_select
 */

/*
 * bss_dcmcc
 */

/*
 * bss_threshold
 */

/*
 * bss_attack_rate
 */

/*
 * bss_release_rate
 */


/*
 * (0x5b)-bss_config1
 */

/*
 * bss_hold_time
 */

/*
 * bss_steepness
 */

/*
 * bss_reduction_limit
 */

/*
 * bssdc_threshold
 */

/*
 * bssdc_attact_rate
 */

/*
 * dualcell
 */


/*
 * (0x5c)-bss_config2
 */

/*
 * bssdc_release_rate
 */

/*
 * bssdc_hold_time
 */

/*
 * bssdc_steepness
 */

/*
 * bssdc_reduction_limit
 */

/*
 * bss_flag_select
 */


/*
 * (0x5f)-gain_att
 */

/*
 * ctrl_attr
 */


/*
 * (0x60)-idle_power_detector1
 */

/*
 * idle_power_mode
 */


/*
 * (0x62)-low_drive_detector1
 */

/*
 * amp_drive_low_drive_mode
 */


/*
 * (0x63)-low_drive_detector2
 */

/*
 * low_drive_detector_mode
 */

/*
 * amp_lnm
 */


/*
 * (0x66)-low_power_detector1
 */

/*
 * low_power_mode
 */


/*
 * (0x68)-tdm_source_ctrl
 */

/*
 * tdm_source_mapping
 */

/*
 * tdm_sourcea_frame_sel
 */

/*
 * tdm_sourceb_frame_sel
 */

/*
 * tdm_source0_clip_sel
 */


/*
 * (0x69)-tdm_source_ctrl2
 */

/*
 * tdm_sourcec_frame_sel
 */

/*
 * tdm_sourced_frame_sel
 */

/*
 * tdm_sourcee_frame_sel
 */


/*
 * (0x6e)-status_flags5
 */

/*
 * flag_idle_power_mode
 */


/*
 * (0x6f)-cursense_comp
 */

/*
 * pwm_clip_lvl
 */


/*
 * (0x70)-dcdc_control0
 */

/*
 * boost_intel
 */


/*
 * (0x74)-dcdc_control4
 */

/*
 * dcdc_disable_ns
 */

/*
 * dcdc_disable_mod8bit
 */

/*
 * boost_trip_lvl_1st
 */

/*
 * track_hold_time
 */

/*
 * sel_dcdc_envelope_8fs
 */


/*
 * (0x75)-dcdc_control5
 */

/*
 * boost_trip_lvl_track
 */

/*
 * bypass_dcdc_lpf
 */


/*
 * (0x76)-dcdc_control6
 */

/*
 * scnd_boost_voltage
 */


/*
 * (0x7c)-low_noise_ctrl2
 */

/*
 * amp_gain_mode
 */

/*
 * lownoisegain_mode
 */


/*
 * (0x84)-volsense_dc_prot_ctrl
 */

/*
 * volsense_dc_prot_time
 */

/*
 * volsense_dc_prot_level
 */


/*
 * (0xa1)-efusekey2_reg
 */

/*
 * key2
 */


/*
 * (0xa2)-efuse_status
 */

/*
 * key01_locked
 */

/*
 * key02_locked
 */


/*
 * (0xc9)-key1_pll_control2
 */


/*
 * (0xca)-key1_pll_control3
 */


/*
 * (0xce)-KEY2_protected_pll_control7
 */

/*
 * sel_pll_startup_time
 */


/*
 * (0xd1)-KEY2_protected_mis_ctrl
 */

/*
 * dcdcoff_mode
 */


/*
 * (0xdd)-bst_atb_config4
 */

/*
 * amp_pst_slope0
 */

/*
 * bst_pst_slope0
 */


/*
 * (0xe0)-software_profile
 */

/*
 * sw_profile
 */


/*
 * (0xe1)-software_vstep
 */

/*
 * sw_vstep
 */


/*
 * (0xf6)-KEY1_protected_EFUSE6
 */


#endif /* _TFA9866_GENREGS_H */

