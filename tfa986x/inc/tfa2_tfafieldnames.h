/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

enum tfa2_bf_enum_list {
	TFA2_BF_PWDN = 0x0000,
	TFA2_BF_I2CR = 0x0010,
	TFA2_BF_GPIOEN = 0x0020,
	TFA2_BF_AMPE = 0x0030,
	TFA2_BF_DCA = 0x0040,
	TFA2_BF_EAMPHZEN = 0x0050,
	TFA2_BF_BYPDYUVP = 0x0060,
	TFA2_BF_INTP = 0x0071,
	TFA2_BF_QALARM = 0x0090,
	TFA2_BF_BYPUVP = 0x00a0,
	TFA2_BF_BYPOCP = 0x00b0,
	TFA2_BF_QPUMPOKEN = 0x00c0,
	TFA2_BF_AMPQOKEN = 0x00d0,
	TFA2_BF_ENPLLSYNC = 0x00e0,
	TFA2_BF_COORHYS = 0x00f0,
	TFA2_BF_MANSCONF = 0x0120,
	TFA2_BF_MUTETO = 0x0160,
	TFA2_BF_MANROBOD = 0x0170,
	TFA2_BF_MANEDCTH = 0x01d0,
	TFA2_BF_AUDFS = 0x0203,
	TFA2_BF_FRACTDEL = 0x0256,
	TFA2_BF_REV = 0x030f,
	TFA2_BF_REFCKEXT = 0x0401,
	TFA2_BF_BYTDMGLF = 0x0420,
	TFA2_BF_MANAOOSC = 0x0460,
	TFA2_BF_FSSYNCEN = 0x0480,
	TFA2_BF_CLKREFSYNCEN = 0x0490,
	TFA2_BF_PWMFREQ = 0x04a0,
	TFA2_BF_CGUSYNCDCG = 0x0500,
	TFA2_BF_IPMBYP = 0x0510,
	TFA2_BF_DEVCAT = 0x0607,
	TFA2_BF_DEVREV = 0x0687,
	TFA2_BF_VDDS = 0x1000,
	TFA2_BF_OTDS = 0x1010,
	TFA2_BF_UVDS = 0x1020,
	TFA2_BF_OVDS = 0x1030,
	TFA2_BF_OCDS = 0x1040,
	TFA2_BF_NOCLK = 0x1050,
	TFA2_BF_CLKOOR = 0x1060,
	TFA2_BF_DCOCPOK = 0x1070,
	TFA2_BF_OCPOMN = 0x1090,
	TFA2_BF_OCPOMP = 0x10a0,
	TFA2_BF_OCPOAP = 0x10b0,
	TFA2_BF_OCPOAN = 0x10c0,
	TFA2_BF_OCPOBP = 0x10d0,
	TFA2_BF_OCPOBN = 0x10e0,
	TFA2_BF_DCTH = 0x10f0,
	TFA2_BF_CLKS = 0x1100,
	TFA2_BF_OTPB = 0x1110,
	TFA2_BF_TDMERR = 0x1120,
	TFA2_BF_LPMS = 0x1130,
	TFA2_BF_LNMS = 0x1140,
	TFA2_BF_FWRMS = 0x1150,
	TFA2_BF_BSTMS = 0x1160,
	TFA2_BF_PLLS = 0x1170,
	TFA2_BF_TDMLUTER = 0x1180,
	TFA2_BF_SWS = 0x1190,
	TFA2_BF_AMPS = 0x11a0,
	TFA2_BF_AREFS = 0x11b0,
	TFA2_BF_CLIPS = 0x11c0,
	TFA2_BF_VGBS = 0x11e0,
	TFA2_BF_FLGBSS = 0x11f0,
	TFA2_BF_MANSTATE = 0x1203,
	TFA2_BF_AMPSTE = 0x1243,
	TFA2_BF_TDMSTAT = 0x1282,
	TFA2_BF_QPCLKSTS = 0x12b1,
	TFA2_BF_WAITSYNC = 0x12d0,
	TFA2_BF_LDMS = 0x12e0,
	TFA2_BF_BODNOK = 0x1300,
	TFA2_BF_QPFAIL = 0x1310,
	TFA2_BF_JSEQBUSY = 0x1320,
	TFA2_BF_BATS = 0x1509,
	TFA2_BF_TEMPS = 0x1608,
	TFA2_BF_VDDPS = 0x1709,
	TFA2_BF_TDME = 0x2000,
	TFA2_BF_AMPINSEL = 0x2011,
	TFA2_BF_INPLEV = 0x2030,
	TFA2_BF_TDMCLINV = 0x2040,
	TFA2_BF_TDMFSPOL = 0x2050,
	TFA2_BF_TDMSLOTS = 0x2061,
	TFA2_BF_TDMSLLN = 0x2081,
	TFA2_BF_TDMSSIZE = 0x20a1,
	TFA2_BF_TDMNBCK = 0x20c3,
	TFA2_BF_TDMDEL = 0x2100,
	TFA2_BF_TDMADJ = 0x2110,
	TFA2_BF_TDMSPKE = 0x2120,
	TFA2_BF_TDMDCE = 0x2130,
	TFA2_BF_TDMSRC0E = 0x2140,
	TFA2_BF_TDMSRC1E = 0x2150,
	TFA2_BF_TDMSRC2E = 0x2160,
	TFA2_BF_TDMSPKS = 0x2183,
	TFA2_BF_TDMDCS = 0x21c3,
	TFA2_BF_TDMSRC0S = 0x2203,
	TFA2_BF_TDMSRC1S = 0x2243,
	TFA2_BF_TDMSRC2S = 0x2283,
	TFA2_BF_ISTVDDS = 0x4000,
	TFA2_BF_ISTBSTOC = 0x4010,
	TFA2_BF_ISTOTDS = 0x4020,
	TFA2_BF_ISTOCPR = 0x4030,
	TFA2_BF_ISTUVDS = 0x4040,
	TFA2_BF_ISTTDMER = 0x4050,
	TFA2_BF_ISTNOCLK = 0x4060,
	TFA2_BF_ISTDCTH = 0x4070,
	TFA2_BF_ISTBODNOK = 0x4080,
	TFA2_BF_ISTCOOR = 0x4090,
	TFA2_BF_ISTOVDS = 0x40a0,
	TFA2_BF_ISTQPFAIL = 0x40b0,
	TFA2_BF_ICLVDDS = 0x4400,
	TFA2_BF_ICLBSTOC = 0x4410,
	TFA2_BF_ICLOTDS = 0x4420,
	TFA2_BF_ICLOCPR = 0x4430,
	TFA2_BF_ICLUVDS = 0x4440,
	TFA2_BF_ICLTDMER = 0x4450,
	TFA2_BF_ICLNOCLK = 0x4460,
	TFA2_BF_ICLDCTH = 0x4470,
	TFA2_BF_ICLBODNOK = 0x4480,
	TFA2_BF_ICLCOOR = 0x4490,
	TFA2_BF_ICLOVDS = 0x44a0,
	TFA2_BF_ICLQPFAIL = 0x44b0,
	TFA2_BF_IEVDDS = 0x4800,
	TFA2_BF_IEBSTOC = 0x4810,
	TFA2_BF_IEOTDS = 0x4820,
	TFA2_BF_IEOCPR = 0x4830,
	TFA2_BF_IEUVDS = 0x4840,
	TFA2_BF_IETDMER = 0x4850,
	TFA2_BF_IENOCLK = 0x4860,
	TFA2_BF_IEDCTH = 0x4870,
	TFA2_BF_IEBODNOK = 0x4880,
	TFA2_BF_IECOOR = 0x4890,
	TFA2_BF_IEOVDS = 0x48a0,
	TFA2_BF_IEQPFAIL = 0x48b0,
	TFA2_BF_IPOVDDS = 0x4c00,
	TFA2_BF_IPOBSTOC = 0x4c10,
	TFA2_BF_IPOOTDS = 0x4c20,
	TFA2_BF_IPOOCPR = 0x4c30,
	TFA2_BF_IPOUVDS = 0x4c40,
	TFA2_BF_IPOTDMER = 0x4c50,
	TFA2_BF_IPONOCLK = 0x4c60,
	TFA2_BF_IPODCTH = 0x4c70,
	TFA2_BF_IPOBODNOK = 0x4c80,
	TFA2_BF_IPOCOOR = 0x4c90,
	TFA2_BF_IPOOVDS = 0x4ca0,
	TFA2_BF_IPOQPFAIL = 0x4cb0,
	TFA2_BF_BSSCLRST = 0x50d0,
	TFA2_BF_BSSR = 0x50e0,
	TFA2_BF_BSSBY = 0x50f0,
	TFA2_BF_USOUND = 0x5130,
	TFA2_BF_DCDITH = 0x5140,
	TFA2_BF_HPFBYP = 0x5150,
	TFA2_BF_PWMPH = 0x5203,
	TFA2_BF_AMPGAIN = 0x5257,
	TFA2_BF_BYPDLYLINE = 0x52f0,
	TFA2_BF_AMPSLP = 0x5481,
	TFA2_BF_BSTSLP = 0x54a1,
	TFA2_BF_BSSMVBS = 0x5a00,
	TFA2_BF_DCMCC = 0x5a13,
	TFA2_BF_BSST = 0x5a53,
	TFA2_BF_BSSAR = 0x5a91,
	TFA2_BF_BSSRR = 0x5ab3,
	TFA2_BF_BSSMULT = 0x5af0,
	TFA2_BF_BSSHT = 0x5b03,
	TFA2_BF_BSSS = 0x5b41,
	TFA2_BF_BSSRL = 0x5b62,
	TFA2_BF_BSSDCT = 0x5b93,
	TFA2_BF_BSSDCAR = 0x5bd1,
	TFA2_BF_DUALCELL = 0x5bf0,
	TFA2_BF_BSSDCRR = 0x5c03,
	TFA2_BF_BSSDCHT = 0x5c43,
	TFA2_BF_BSSDCS = 0x5c81,
	TFA2_BF_BSSDCRL = 0x5ca2,
	TFA2_BF_DCLIPFLS = 0x5cd1,
	TFA2_BF_BSSBYP = 0x5cf0,
	TFA2_BF_TDMSPKG = 0x5f54,
	TFA2_BF_IPM = 0x60e1,
	TFA2_BF_LDMSEG = 0x62b2,
	TFA2_BF_LDM = 0x63c1,
	TFA2_BF_RCVM = 0x63e1,
	TFA2_BF_LPM = 0x66e1,
	TFA2_BF_TDMSRCMAP = 0x6802,
	TFA2_BF_TDMSRCAS = 0x6842,
	TFA2_BF_TDMSRCBS = 0x6872,
	TFA2_BF_TDMSRCACLIP = 0x68a1,
	TFA2_BF_TDMSRCCS = 0x6902,
	TFA2_BF_TDMSRCDS = 0x6932,
	TFA2_BF_TDMSRCES = 0x6962,
	TFA2_BF_TDMVBAT = 0x6990,
	TFA2_BF_IPMS = 0x6e00,
	TFA2_BF_LVLCLPPWM = 0x6f72,
	TFA2_BF_DCIE = 0x7060,
	TFA2_BF_DCNS = 0x7400,
	TFA2_BF_DCNSRST = 0x7410,
	TFA2_BF_DCOFFSET = 0x7424,
	TFA2_BF_DCHOLD = 0x7494,
	TFA2_BF_DCINT = 0x74e0,
	TFA2_BF_DCTRIP = 0x7509,
	TFA2_BF_DCTRIPT = 0x75a4,
	TFA2_BF_BYPDCLPF = 0x75f0,
	TFA2_BF_DCVOS = 0x7687,
	TFA2_BF_MUSMODE = 0x7cc0,
	TFA2_BF_LNM = 0x7ce1,
	TFA2_BF_DCPTC = 0x8401,
	TFA2_BF_DCPL = 0x842c,
	TFA2_BF_EFUSEK = 0xa107,
	TFA2_BF_KEY1LOCKED = 0xa200,
	TFA2_BF_KEY2LOCKED = 0xa210,
	TFA2_BF_PLLPDIV = 0xc934,
	TFA2_BF_PLLNDIV = 0xc987,
	TFA2_BF_PLLMDIV = 0xca0f,
	TFA2_BF_DCDIS = 0xce30,
	TFA2_BF_PLLSTRTM = 0xce42,
	TFA2_BF_SWPROFIL = 0xe00f,
	TFA2_BF_SWVSTEP = 0xe10f,
	TFA2_BF_FMDIV = 0xe20f,
	TFA2_BF_FNDIV = 0xe307,
	TFA2_BF_VTM0 = 0xe383,
	TFA2_BF_VFREQ1 = 0xe408,
	TFA2_BF_VPLO1 = 0xe490,
	TFA2_BF_VGAIN1 = 0xe507,
	TFA2_BF_VTM1 = 0xe587,
	TFA2_BF_VFREQ2 = 0xe608,
	TFA2_BF_VPLO2 = 0xe690,
	TFA2_BF_VGAIN2 = 0xe707,
	TFA2_BF_VTM2 = 0xe787,
	TFA2_BF_VFREQ3 = 0xe808,
	TFA2_BF_VPLO3 = 0xe890,
	TFA2_BF_VGAIN3 = 0xe907,
	TFA2_BF_VTM3 = 0xe987,
	TFA2_BF_ADC11GAIN = 0xf6a5,
};

