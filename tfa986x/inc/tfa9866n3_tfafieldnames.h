/** Filename: tfa9866n3_tfafieldnames.h
 *  This file was generated automatically on 02/17/25 at 14:56:12. 
 *  Source file: TFA9866_SAT1_N3A0_I2C_RegisterMap.xlsx
 */

#ifndef _TFA9866_TFAFIELDNAMES_H
#define _TFA9866_TFAFIELDNAMES_H


#define TFA9866_I2CVERSION    12

enum tfa9866_bf_enum_list {
    TFA9866_BF_PWDN  = 0x0000,    /*!< Powerdown selection                                */
    TFA9866_BF_I2CR  = 0x0010,    /*!< I2C reset - auto clear                             */
    TFA9866_BF_VIBEN = 0x0020,    /*!< Vib enable (auto cleared)                          */
    TFA9866_BF_AMPE  = 0x0030,    /*!< Activate amplifier                                 */
    TFA9866_BF_DCA   = 0x0040,    /*!< Activate DC-to-DC converter                        */
    TFA9866_BF_EAMPHZEN= 0x0050,    /*!< Amplifer outputs state when OFF                    */
    TFA9866_BF_BYPDYUVP= 0x0060,    /*!< Enable dynamic uvp                                 */
    TFA9866_BF_INTP  = 0x0071,    /*!< Interrupt pin configuration                        */
    TFA9866_BF_QALARM= 0x0090,    /*!< Device response to Qpump OK flag                   */
    TFA9866_BF_BYPUVP= 0x00a0,    /*!< Bypass UVP                                         */
    TFA9866_BF_BYPOCP= 0x00b0,    /*!< Bypass OCP                                         */
    TFA9866_BF_QPUMPOKEN= 0x00c0,    /*!< Low drive   response to Qpump OK flag              */
    TFA9866_BF_AMPQOKEN= 0x00d0,    /*!< Low power response to Qpump OK flag                */
    TFA9866_BF_ENPLLSYNC= 0x00e0,    /*!< Manager control for enabling synchronisation with PLL FS */
    TFA9866_BF_COORHYS= 0x00f0,    /*!< Select hysteresis for clock range detector         */
    TFA9866_BF_MANSCONF= 0x0120,    /*!< I2C configured                                     */
    TFA9866_BF_MUTETO= 0x0160,    /*!< Time out mute sequence                             */
    TFA9866_BF_MANROBOD= 0x0170,    /*!< Device response to BOD                             */
    TFA9866_BF_MANEDCTH= 0x01d0,    /*!< Device response to too high DC level flag (DCTH is 1) */
    TFA9866_BF_AUDFS = 0x0203,    /*!< Sample rate (Fs)                                   */
    TFA9866_BF_FRACTDEL= 0x0256,    /*!< V/I Fractional delay                               */
    TFA9866_BF_REV   = 0x030f,    /*!< Product                                            */
    TFA9866_BF_REFCKEXT= 0x0401,    /*!< PLL external ref clock                             */
    TFA9866_BF_BYTDMGLF= 0x0420,    /*!< Bypass TDM FS/BCK/DATAI giltch filter              */
    TFA9866_BF_MANAOOSC= 0x0460,    /*!< Internal oscillator control during power down mode */
    TFA9866_BF_FSSYNCEN= 0x0480,    /*!< Enable FS synchronisation for clock divider        */
    TFA9866_BF_CLKREFSYNCEN= 0x0490,    /*!< Enable PLL reference clock synchronisation for clock divider */
    TFA9866_BF_PWMFREQ= 0x04a0,    /*!< PWM output frequency select                        */
    TFA9866_BF_CGUSYNCDCG= 0x0500,    /*!< Clock gating control for CGU synchronisation module */
    TFA9866_BF_IPMBYP= 0x0510,    /*!< Bypass ipm synchromizser in DPSA block             */
    TFA9866_BF_DEVCAT= 0x0607,    /*!< Product category                                   */
    TFA9866_BF_DEVREV= 0x0687,    /*!< Version                                            */
    TFA9866_BF_VDDS  = 0x1000,    /*!< POR (sticky flag, clear on write a '1')            */
    TFA9866_BF_OTDS  = 0x1010,    /*!< OTP alarm (sticky flag,  clear on write a '1')     */
    TFA9866_BF_UVDS  = 0x1020,    /*!< UVP alarm (sticky flag,  clear on write a '1')     */
    TFA9866_BF_OVDS  = 0x1030,    /*!< OVP alarm (sticky flag,  clear on write a '1')     */
    TFA9866_BF_OCDS  = 0x1040,    /*!< OCP amplifier (sticky flag,  clear on write a '1') */
    TFA9866_BF_NOCLK = 0x1050,    /*!< Lost clock (sticky flag,  clear on write a '1')    */
    TFA9866_BF_CLKOOR= 0x1060,    /*!< External clock status (sticky flag,  clear on write a '1') */
    TFA9866_BF_DCOCPOK= 0x1070,    /*!< Booster input current reaching DCMCC (sticky flag,  clear on write a '1') */
    TFA9866_BF_OCPOMN= 0x1090,    /*!< OCPOK Minion nmos  (sticky flag,  clear on write a '1') */
    TFA9866_BF_OCPOMP= 0x10a0,    /*!< OCPOK Minion pmos (sticky flag,  clear on write a '1') */
    TFA9866_BF_OCPOAP= 0x10b0,    /*!< OCPOK High side OUTP (sticky flag,  clear on write a '1') */
    TFA9866_BF_OCPOAN= 0x10c0,    /*!< OCPOK  High side OUTN (sticky flag,  clear on write a '1') */
    TFA9866_BF_OCPOBP= 0x10d0,    /*!< OCPOK  Low side OUTP (sticky flag,  clear on write a '1') */
    TFA9866_BF_OCPOBN= 0x10e0,    /*!< OCPOK Low side OUTN (sticky flag,  clear on write a '1')  */
    TFA9866_BF_DCTH  = 0x10f0,    /*!< DC level on voltage sense too high (sticky flag,  clear on write a '1') */
    TFA9866_BF_CLKS  = 0x1100,    /*!< Clocks stable                                      */
    TFA9866_BF_OTPB  = 0x1110,    /*!< EFUSE busy                                         */
    TFA9866_BF_TDMERR= 0x1120,    /*!< TDM error                                          */
    TFA9866_BF_LPMS  = 0x1130,    /*!< Flag in low power mode                             */
    TFA9866_BF_LNMS  = 0x1140,    /*!< Flag in low noise mode                             */
    TFA9866_BF_FWRMS = 0x1150,    /*!< Flag in follower mode                              */
    TFA9866_BF_BSTMS = 0x1160,    /*!< Flag in boost mode                                 */
    TFA9866_BF_PLLS  = 0x1170,    /*!< PLL lock                                           */
    TFA9866_BF_TDMLUTER= 0x1180,    /*!< TDM LUT error                                      */
    TFA9866_BF_SWS   = 0x1190,    /*!< Amplifier engage                                   */
    TFA9866_BF_AMPS  = 0x11a0,    /*!< Amplifier enable                                   */
    TFA9866_BF_AREFS = 0x11b0,    /*!< References enable                                  */
    TFA9866_BF_CLIPS = 0x11c0,    /*!< Amplifier clipping                                 */
    TFA9866_BF_VGBS  = 0x11e0,    /*!< Vddp greater than vbat                             */
    TFA9866_BF_FLGBSS= 0x11f0,    /*!< BSS flag                                           */
    TFA9866_BF_MANSTATE= 0x1203,    /*!< Device manager status                              */
    TFA9866_BF_AMPSTE= 0x1243,    /*!< Amplifier control status                           */
    TFA9866_BF_TDMSTAT= 0x1282,    /*!< TDM status bits                                    */
    TFA9866_BF_QPCLKSTS= 0x12b1,    /*!< QPUMP clock status                                 */
    TFA9866_BF_WAITSYNC= 0x12d0,    /*!< CGU and PLL synchronisation status flag from CGU   */
    TFA9866_BF_LDMS  = 0x12e0,    /*!< Flag in Low drive mode                             */
    TFA9866_BF_BODNOK= 0x1300,    /*!< BOD Flag VDD NOT OK (sticky flag,  clear on write a '1') */
    TFA9866_BF_QPFAIL= 0x1310,    /*!< QPUMP Fail Flag (sticky flag,  clear on write a '1') */
    TFA9866_BF_JSEQBUSY= 0x1320,    /*!< Sequency busy                                      */
    TFA9866_BF_BATS  = 0x1509,    /*!< Battery voltage monitoring (V)                     */
    TFA9866_BF_TEMPS = 0x1608,    /*!< IC Temperature monitoring (C)                      */
    TFA9866_BF_VDDPS = 0x1709,    /*!< Amplifier supply / booster voltage monitoring (V)  */
    TFA9866_BF_TDME  = 0x2000,    /*!< Enable interface                                   */
    TFA9866_BF_AMPINSEL= 0x2011,    /*!< Amplifier input selection                          */
    TFA9866_BF_INPLEV= 0x2030,    /*!< TDM output attenuation                             */
    TFA9866_BF_TDMCLINV= 0x2040,    /*!< Reception data to BCK clock                        */
    TFA9866_BF_TDMFSPOL= 0x2050,    /*!< FS polarity                                        */
    TFA9866_BF_TDMSLOTS= 0x2061,    /*!< N-slots in Frame                                   */
    TFA9866_BF_TDMSLLN= 0x2081,    /*!< N-bits in slot                                     */
    TFA9866_BF_TDMSSIZE= 0x20a1,    /*!< Sample size per slot                               */
    TFA9866_BF_TDMNBCK= 0x20c3,    /*!< N-BCK's in FS                                      */
    TFA9866_BF_TDMDEL= 0x2100,    /*!< Data delay to FS                                   */
    TFA9866_BF_TDMADJ= 0x2110,    /*!< Data adjustment                                    */
    TFA9866_BF_TDMSPKE= 0x2120,    /*!< Control audio TDM input channel 0                  */
    TFA9866_BF_TDMDCE= 0x2130,    /*!< Control audio TDM input channel 1                  */
    TFA9866_BF_TDMSRC0E= 0x2140,    /*!< Control TDM source0 data channel                   */
    TFA9866_BF_TDMSRC1E= 0x2150,    /*!< Control TDM source1 data channel                   */
    TFA9866_BF_TDMSRC2E= 0x2160,    /*!< Control TDM source2 data channel                   */
    TFA9866_BF_TDMSPKS= 0x2183,    /*!< TDM slot for input channel 0                       */
    TFA9866_BF_TDMDCS= 0x21c3,    /*!< TDM slot for input channel 1                       */
    TFA9866_BF_TDMSRC0S= 0x2203,    /*!< Slot Position of TDM source0 channel data          */
    TFA9866_BF_TDMSRC1S= 0x2243,    /*!< Slot Position of TDM source1 channel data          */
    TFA9866_BF_TDMSRC2S= 0x2283,    /*!< Slot Position of TDM source2 channel data          */
    TFA9866_BF_ISTVDDS= 0x4000,    /*!< Interrupt status POR                               */
    TFA9866_BF_ISTBSTOC= 0x4010,    /*!< Interrupt status DCDC OCP alarm                    */
    TFA9866_BF_ISTOTDS= 0x4020,    /*!< Interrupt status OTP alarm                         */
    TFA9866_BF_ISTOCPR= 0x4030,    /*!< Interrupt status OCP alarm                         */
    TFA9866_BF_ISTUVDS= 0x4040,    /*!< Interrupt status UVP alarm                         */
    TFA9866_BF_ISTTDMER= 0x4050,    /*!< Interrupt status TDM error                         */
    TFA9866_BF_ISTNOCLK= 0x4060,    /*!< Interrupt status lost clock                        */
    TFA9866_BF_ISTDCTH= 0x4070,    /*!< Interrupt status dc too high                       */
    TFA9866_BF_ISTBODNOK= 0x4080,    /*!< Interrupt status brown out detected                */
    TFA9866_BF_ISTCOOR= 0x4090,    /*!< Interrupt status clock out of range                */
    TFA9866_BF_ISTOVDS= 0x40a0,    /*!< Interrupt status OVP alarm                         */
    TFA9866_BF_ISTQPFAIL= 0x40b0,    /*!< Interrupt status qpump failure                     */
    TFA9866_BF_ICLVDDS= 0x4400,    /*!< Clear interrupt status POR                         */
    TFA9866_BF_ICLBSTOC= 0x4410,    /*!< Clear interrupt status DCDC OCP                    */
    TFA9866_BF_ICLOTDS= 0x4420,    /*!< Clear interrupt status OTP alarm                   */
    TFA9866_BF_ICLOCPR= 0x4430,    /*!< Clear interrupt status OCP alarm                   */
    TFA9866_BF_ICLUVDS= 0x4440,    /*!< Clear interrupt status UVP alarm                   */
    TFA9866_BF_ICLTDMER= 0x4450,    /*!< Clear interrupt status TDM error                   */
    TFA9866_BF_ICLNOCLK= 0x4460,    /*!< Clear interrupt status lost clk                    */
    TFA9866_BF_ICLDCTH= 0x4470,    /*!< Clear interrupt status dc too high                 */
    TFA9866_BF_ICLBODNOK= 0x4480,    /*!< Clear interrupt status brown out detected          */
    TFA9866_BF_ICLCOOR= 0x4490,    /*!< Clear interrupt status clock out of range          */
    TFA9866_BF_ICLOVDS= 0x44a0,    /*!< Clear interrupt status OVP alarm                   */
    TFA9866_BF_ICLQPFAIL= 0x44b0,    /*!< Clear interrupt status qpump failure               */
    TFA9866_BF_IEVDDS= 0x4800,    /*!< Enable interrupt POR                               */
    TFA9866_BF_IEBSTOC= 0x4810,    /*!< Enable interrupt DCDC OCP                          */
    TFA9866_BF_IEOTDS= 0x4820,    /*!< Enable interrupt OTP alarm                         */
    TFA9866_BF_IEOCPR= 0x4830,    /*!< Enable interrupt OCP alarm                         */
    TFA9866_BF_IEUVDS= 0x4840,    /*!< Enable interrupt UVP alarm                         */
    TFA9866_BF_IETDMER= 0x4850,    /*!< Enable interrupt TDM error                         */
    TFA9866_BF_IENOCLK= 0x4860,    /*!< Enable interrupt lost clk                          */
    TFA9866_BF_IEDCTH= 0x4870,    /*!< Enable interrupt dc too high                       */
    TFA9866_BF_IEBODNOK= 0x4880,    /*!< Enable interrupt brown out detect                  */
    TFA9866_BF_IECOOR= 0x4890,    /*!< Enable interrupt clock out of range                */
    TFA9866_BF_IEOVDS= 0x48a0,    /*!< Enable interrupt OVP alarm                         */
    TFA9866_BF_IEQPFAIL= 0x48b0,    /*!< Enable interrupt qpump failure                     */
    TFA9866_BF_IPOVDDS= 0x4c00,    /*!< Interrupt polarity POR                             */
    TFA9866_BF_IPOBSTOC= 0x4c10,    /*!< Interrupt polarity DCDC OCP                        */
    TFA9866_BF_IPOOTDS= 0x4c20,    /*!< Interrupt polarity OTP alarm                       */
    TFA9866_BF_IPOOCPR= 0x4c30,    /*!< Interrupt polarity OCP alarm                       */
    TFA9866_BF_IPOUVDS= 0x4c40,    /*!< Interrupt polarity UVP alarm                       */
    TFA9866_BF_IPOTDMER= 0x4c50,    /*!< Interrupt polarity TDM error                       */
    TFA9866_BF_IPONOCLK= 0x4c60,    /*!< Interrupt polarity lost clk                        */
    TFA9866_BF_IPODCTH= 0x4c70,    /*!< Interrupt polarity dc too high                     */
    TFA9866_BF_IPOBODNOK= 0x4c80,    /*!< Interrupt polarity brown out detect                */
    TFA9866_BF_IPOCOOR= 0x4c90,    /*!< Interrupt polarity clock out of range              */
    TFA9866_BF_IPOOVDS= 0x4ca0,    /*!< Interrupt polarity OVP alarm                       */
    TFA9866_BF_IPOQPFAIL= 0x4cb0,    /*!< Interrupt polarity qpump failure                   */
    TFA9866_BF_BSSCLRST= 0x50d0,    /*!< Reset clipper  auto-clear                          */
    TFA9866_BF_BSSR  = 0x50e0,    /*!< Battery voltage read out in BATS bitfield          */
    TFA9866_BF_BSSBY = 0x50f0,    /*!< Bypass battery safeguard                           */
    TFA9866_BF_USOUND= 0x5130,    /*!< Ultrasound mode                                    */
    TFA9866_BF_DCDITH= 0x5140,    /*!< DC dithering selection                             */
    TFA9866_BF_HPFBYP= 0x5150,    /*!< Bypass HPF                                         */
    TFA9866_BF_PWMPH = 0x5203,    /*!< Select pwm phase wrt TDM_FS signal                 */
    TFA9866_BF_AMPGAIN= 0x5257,    /*!< Amplifier gain                                     */
    TFA9866_BF_BYPDLYLINE= 0x52f0,    /*!< Bypass the interpolator delay line                 */
    TFA9866_BF_AMPSLP= 0x5481,    /*!< Extreme amplifier slope control                    */
    TFA9866_BF_BSTSLP= 0x54a1,    /*!< Booster slope control                              */
    TFA9866_BF_BSSMVBS= 0x5a00,    /*!< Min_vbat source select                             */
    TFA9866_BF_DCMCC = 0x5a13,    /*!< Battery current limiter initial value              */
    TFA9866_BF_BSST  = 0x5a53,    /*!< BSS threshold Vbat value (reducing gain)           */
    TFA9866_BF_BSSAR = 0x5a91,    /*!< BSS attack rate (reducing gain)                    */
    TFA9866_BF_BSSRR = 0x5ab3,    /*!< BSS release rate  (reducing gain)                  */
    TFA9866_BF_BSSMULT= 0x5af0,    /*!< Multiply attack rate by 8                          */
    TFA9866_BF_BSSHT = 0x5b03,    /*!< BSS hold time (reducing gain)                      */
    TFA9866_BF_BSSS  = 0x5b41,    /*!< BSS steepness (reducing gain)                      */
    TFA9866_BF_BSSRL = 0x5b62,    /*!< BSS allowed reduction level (reducing gain)        */
    TFA9866_BF_BSSDCT= 0x5b93,    /*!< BSS threshold Vbat value (reducing DCMCC)          */
    TFA9866_BF_BSSDCAR= 0x5bd1,    /*!< BSS attack rate  (reducing DCMCC)                  */
    TFA9866_BF_DUALCELL= 0x5bf0,    /*!< Single or dual cell Battery                        */
    TFA9866_BF_BSSDCRR= 0x5c03,    /*!< BSS release rate  (reducing DCMCC)                 */
    TFA9866_BF_BSSDCHT= 0x5c43,    /*!< BSS hold time (reducing DCMCC)                     */
    TFA9866_BF_BSSDCS= 0x5c81,    /*!< Battery supply safeguard steepness (reducing DCMCC) */
    TFA9866_BF_BSSDCRL= 0x5ca2,    /*!< BSS allowed reduction level (reducing DCMCC)       */
    TFA9866_BF_DCLIPFLS= 0x5cd1,    /*!< Select source for BSS flag                         */
    TFA9866_BF_BSSBYDB= 0x5cf0,    /*!< Bypass the effect of BSS on boost voltage          */
    TFA9866_BF_TDMSPKG= 0x5f54,    /*!< TDM signal attenuation                             */
    TFA9866_BF_IPM   = 0x60e1,    /*!< Idle power mode control                            */
    TFA9866_BF_LDMSEG= 0x62b2,    /*!< Amplifier low drive   mode                         */
    TFA9866_BF_LDM   = 0x63c1,    /*!< Low drive mode detector control                    */
    TFA9866_BF_RCVM  = 0x63e1,    /*!< Handset/Receiver mode                              */
    TFA9866_BF_VBATHOLD= 0x6555,    /*!< Select hold time for VBAT(min/max)                 */
    TFA9866_BF_VBATMAX= 0x65b0,    /*!< Select between vbat minimum or maximum for Chip Sense output on TDM  VBAT(min/max) */
    TFA9866_BF_LPM   = 0x66e1,    /*!< Low power mode control                             */
    TFA9866_BF_TDMSRCMAP= 0x6802,    /*!< TDM source mapping                                 */
    TFA9866_BF_TDMSRCAS= 0x6842,    /*!< Sensed value A                                     */
    TFA9866_BF_TDMSRCBS= 0x6872,    /*!< Sensed value B                                     */
    TFA9866_BF_TDMSRCACLIP= 0x68a1,    /*!< Clip flag information with combined clip flag      */
    TFA9866_BF_TDMSRCCS= 0x6902,    /*!< Sensed value C                                     */
    TFA9866_BF_TDMSRCDS= 0x6932,    /*!< Sensed value D                                     */
    TFA9866_BF_TDMSRCES= 0x6962,    /*!< Sensed value E                                     */
    TFA9866_BF_TDMVBAT= 0x6990,    /*!< Select vbat to return to TDM interface             */
    TFA9866_BF_IPMS  = 0x6e00,    /*!< Idle power mode                                    */
    TFA9866_BF_LVLCLPPWM= 0x6f72,    /*!< Clip detect threshold control                      */
    TFA9866_BF_DCIE  = 0x7060,    /*!< Adaptive boost mode                                */
    TFA9866_BF_DCNS  = 0x7400,    /*!< Disable control of noise shaper in DCDC control    */
    TFA9866_BF_DCNSRST= 0x7410,    /*!< Disable control of reset of noise shaper when 8 bit value for dcdc control occurs */
    TFA9866_BF_DCOFFSET= 0x7424,    /*!< Boost headroom  Offset                             */
    TFA9866_BF_DCHOLD= 0x7494,    /*!< Hold time for booster                              */
    TFA9866_BF_DCINT = 0x74e0,    /*!< Selection of data for adaptive boost algorithm, effective only when DCIE is set to 1 */
    TFA9866_BF_DCTRIP= 0x7509,    /*!< 1st Adaptive boost trip level, effective only when DCIE is set to 1 */
    TFA9866_BF_DCTRIPT= 0x75a4,    /*!< Booster headroom gain, increasing headroom when signal amplitude increase */
    TFA9866_BF_BYPDCLPF= 0x75f0,    /*!< Bypass control of DCDC control low pass filter for quantization noise suppression */
    TFA9866_BF_DCVOS = 0x7687,    /*!< Maximum boost voltage                              */
    TFA9866_BF_MUSMODE= 0x7cc0,    /*!< Music Mode                                         */
    TFA9866_BF_LNM   = 0x7ce1,    /*!< Low Noise Mode control when in Music Mode          */
    TFA9866_BF_CS_KTEMP = 0x8305, /*!< Current sense temperature compensation trimming (1 - VALUE*TEMP)*signal */
    TFA9866_BF_DCPTC = 0x8401,    /*!< Voltage sense dc speaker protection time           */
    TFA9866_BF_DCPL  = 0x842c,    /*!< Voltage sense dc speaker protection level          */
    TFA9866_BF_DCPROT= 0x84f0,    /*!< Voltage sense dc speaker protection enable         */
    TFA9866_BF_EFUSEK= 0xa107,    /*!< EFUSE KEY2 register                                */
    TFA9866_BF_KEY1LOCKED= 0xa200,    /*!< Indicates KEY1 is locked                           */
    TFA9866_BF_KEY2LOCKED= 0xa210,    /*!< Indicates KEY2 is locked                           */
    TFA9866_BF_UVPSET= 0xb103,    /*!< Uvp set                                            */
    TFA9866_BF_PLLPDIV= 0xc934,    /*!< PLL PDIV when pll_use_direct_ctrls set to 1        */
    TFA9866_BF_PLLNDIV= 0xc987,    /*!< PLL NDIV when pll_use_direct_ctrls set to 1        */
    TFA9866_BF_PLLMDIV= 0xca0f,    /*!< PLL MDIV in PLL when pll_use_direct_ctrls set to 1 */
    TFA9866_BF_DCDIS = 0xce30,    /*!< DCDC on/off                                        */
    TFA9866_BF_PLLSTRTM= 0xce42,    /*!< PLL startup time selection control                 */
    TFA9866_BF_SWPROFIL= 0xe00f,    /*!< Software profile data                              */
    TFA9866_BF_SWVSTEP= 0xe10f,    /*!< Software vstep information                         */
    TFA9866_BF_FMDIV = 0xe20f,    /*!< Pll m divider when clock by FRO                    */
    TFA9866_BF_FNDIV = 0xe307,    /*!< Pll n divider when clock by FRO                    */
    TFA9866_BF_VTM0  = 0xe383,    /*!< Boost start up time                                */
    TFA9866_BF_VFREQ1= 0xe408,    /*!< Boost  vibration frequency                         */
    TFA9866_BF_VPLO1 = 0xe490,    /*!< Boost vibration Phase                              */
    TFA9866_BF_VGAIN1= 0xe507,    /*!< Boost vibration gain                               */
    TFA9866_BF_VTM1  = 0xe587,    /*!< Boost vibration time                               */
    TFA9866_BF_VFREQ2= 0xe608,    /*!< Core vibration  frequency                          */
    TFA9866_BF_VPLO2 = 0xe690,    /*!< Core vibration Phase                               */
    TFA9866_BF_VGAIN2= 0xe707,    /*!< Core  vibration  gain                              */
    TFA9866_BF_VTM2  = 0xe787,    /*!< Core  vibration time                               */
    TFA9866_BF_VFREQ3= 0xe808,    /*!< Brake vibration  frequency                         */
    TFA9866_BF_VPLO3 = 0xe890,    /*!< Brake vibration Phase                              */
    TFA9866_BF_VGAIN3= 0xe907,    /*!< Brake  vibration  gain                             */
    TFA9866_BF_VTM3  = 0xe987,    /*!< Brake  vibration time                              */
    TFA9866_BF_SPARE_F0_15_10 = 0xf0a5,
    TFA9866_BF_ADC11GAIN= 0xf6a5,    /*!< Adc11b gain compensation setting, compensate factor (2's complement) */
};

enum tfa9866_irq {
	tfa9866_irq_stvdds = 0,
	tfa9866_irq_stbstoc = 1,
	tfa9866_irq_stotds = 2,
	tfa9866_irq_stocpr = 3,
	tfa9866_irq_stuvds = 4,
	tfa9866_irq_sttdmer = 5,
	tfa9866_irq_stnoclk = 6,
	tfa9866_irq_stdcth = 7,
	tfa9866_irq_stbodnok = 8,
	tfa9866_irq_stcoor = 9,
	tfa9866_irq_stovds = 10,
	tfa9866_irq_stqpfail = 11,
	tfa9866_irq_max = 12,
	tfa9866_irq_all = -1 /* all irqs */
};

#define TFA9866_IRQ_NAMETABLE struct tfa_irq_name \
tfa98xx_irq_names[] = {\
	{0, "STVDDS"},\
	{1, "STBSTOC"},\
	{2, "STOTDS"},\
	{3, "STOCPR"},\
	{4, "STUVDS"},\
	{5, "STTDMER"},\
	{6, "STNOCLK"},\
	{7, "STDCTH"},\
	{8, "STBODNOK"},\
	{9, "STCOOR"},\
	{10, "STOVDS"},\
	{11, "STQPFAIL"},\
	{12, "12"},\
}

#define TFA9866_IRQ_INFO char *tfa98xx_irq_info[] = {\
	"Power on reset",\
	"Overcurrent booster",\
	"Overtemperature",\
	"Overcurrent amp",\
	"Undervoltage",\
	"TDM error",\
	"Lost clock",\
	"DC too high amp",\
	"Brown out VDDD",\
	"Clock out of range",\
	"Overvoltage protection",\
	"Qpump fail",\
	"",\
}
#endif /* _TFA9866_TFAFIELDNAMES_H */
