#include "license.txt" // Oscar Steila ik1xpv
#include <stdio.h>
#include <string.h>
#include "mytypes.h"

#include "BBRF103.h" // i2c stuff
#include "R820T2.h"
#include "config.h"
#define FLAG_TIMEOUT         ((UINT32)0x1000)
#define LONG_TIMEOUT         ((UINT32)(10 * FLAG_TIMEOUT))

#define CALIBRATION_LO 88000000

/* registers */


using namespace std;

rt820tuner R820T2; // the class


const int r820t2_lna_gain_steps[16] = {
	0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30
};

static const int r820t2_mixer_gain_steps[16] = {
	0, 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
};

static const int r820t2_vga_gain_steps[16] = {
	0, 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
};


/* initial freq @ 128MHz -> ~5MHz IF due to xtal mismatch */
const UINT8 r82xx_init_array[R820T2_NUM_REGS] =
{
	0x00, 0x00, 0x00, 0x00,	0x00,		/* 00 to 04 */
	/* 05 */ 0x90, // 0x90 LNA manual gain mode, init to 0
	/* 06 */ 0x80,
	/* 07 */ 0x60,
	/* 08 */ 0x80, // Image Gain Adjustment
	/* 09 */ 0x40, //  40 Image Phase Adjustment
	/* 0A */ 0xA0, //  A8 Channel filter [0..3]: 0 = widest, f = narrowest - Optimal. Don't touch!
	/* 0B */ 0x6F, //  0F High pass filter - Optimal. Don't touch!
	/* 0C */ 0x40, // 0x480x40 VGA control by code, init at 0
	/* 0D */ 0x63, // LNA AGC settings: [0..3]: Lower threshold; [4..7]: High threshold
	/* 0E */ 0x75,
	/* 0F */ 0xF8, // F8 Filter Widest, LDO_5V OFF, clk out OFF,
	/* 10 */ 0x7C,
	/* 11 */ 0x83,
	/* 12 */ 0x80,
	/* 13 */ 0x00,
	/* 14 */ 0x0F,
	/* 15 */ 0x00,
	/* 16 */ 0xC0,
	/* 17 */ 0x30,
	/* 18 */ 0x48,
	/* 19 */ 0xCC, //0xCC
	/* 1A */ 0x62, //0x60
	/* 1B */ 0x00,
	/* 1C */ 0x54,
	/* 1D */ 0xAE,
	/* 1E */ 0x0A,
	/* 1F */ 0xC0
};



#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/*
 * Tuner frequency ranges
 * Kanged & modified from airspy firmware to include freq for scanning table
 * "Copyright (C) 2013 Mauro Carvalho Chehab"
 * https://stuff.mit.edu/afs/sipb/contrib/linux/drivers/media/tuners/r820t.c
 */
struct r820t_freq_range
{
  UINT16 freq;
  UINT8 open_d;
  UINT8 rf_mux_ploy;
  UINT8 tf_c;
};

/* Tuner frequency ranges
"Copyright (C) 2013 Mauro Carvalho Chehab"
https://stuff.mit.edu/afs/sipb/contrib/linux/drivers/media/tuners/r820t.c
part of freq_ranges()
*/
static const struct r820t_freq_range freq_ranges[] =
{
  {
  /* 0 MHz */ 0,
  /* .open_d = */     0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0xdf, /* R27[7:0]  band2,band0 */
  }, {
  /* 50 MHz */ 50,
  /* .open_d = */     0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0xbe, /* R27[7:0]  band4,band1  */
  }, {
  /* 55 MHz */ 55,
  /* .open_d = */     0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x8b, /* R27[7:0]  band7,band4 */
  }, {
  /* 60 MHz */ 60,
  /* .open_d = */     0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x7b, /* R27[7:0]  band8,band4 */
  }, {
  /* 65 MHz */ 65,
  /* .open_d = */     0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x69, /* R27[7:0]  band9,band6 */
  }, {
  /* 70 MHz */ 70,
  /* .open_d = */     0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x58, /* R27[7:0]  band10,band7 */
  }, {
  /* 75 MHz */ 75,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x44, /* R27[7:0]  band11,band11 */
  }, {
  /* 80 MHz */ 80,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x44, /* R27[7:0]  band11,band11 */
  }, {
  /* 90 MHz */ 90,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x34, /* R27[7:0]  band12,band11 */
  }, {
  /* 100 MHz */ 100,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x34, /* R27[7:0]  band12,band11 */
  }, {
  /* 110 MHz */ 110,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x24, /* R27[7:0]  band13,band11 */
  }, {
  /* 120 MHz */ 120,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x24, /* R27[7:0]  band13,band11 */
  }, {
  /* 140 MHz */ 140,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x14, /* R27[7:0]  band14,band11 */
  }, {
  /* 180 MHz */ 180,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x13, /* R27[7:0]  band14,band12 */
  }, {
  /* 220 MHz */ 220,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x13, /* R27[7:0]  band14,band12 */
  }, {
  /* 250 MHz */ 250,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x11, /* R27[7:0]  highest,highest */
  }, {
  /* 280 MHz */ 280,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x00, /* R27[7:0]  highest,highest */
  }, {
  /* 310 MHz */ 310,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x41, /* R26[7:6]=1 (bypass)  R26[1:0]=1 (middle) */
  /* .tf_c = */     0x00, /* R27[7:0]  highest,highest */
  }, {
  /* 450 MHz */ 450,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x41, /* R26[7:6]=1 (bypass)  R26[1:0]=1 (middle) */
  /* .tf_c = */     0x00, /* R27[7:0]  highest,highest */
  }, {
  /* 588 MHz */ 588,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x40, /* R26[7:6]=1 (bypass)  R26[1:0]=0 (highest) */
  /* .tf_c = */     0x00, /* R27[7:0]  highest,highest */
  }, {
  /* 650 MHz */ 650,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x40, /* R26[7:6]=1 (bypass)  R26[1:0]=0 (highest) */
  /* .tf_c = */     0x00, /* R27[7:0]  highest,highest */
  }
};


rt820tuner::rt820tuner()
{
    r_xtal = R820T_FREQ;
    r_ifreq = IFR820T;
    r_freq = 101800000;
	UINT8 data[R820T2_NUM_REGS];
	memset(data, 0, sizeof(data));
	memcpy(r820t_regs, r82xx_init_array, R820T2_NUM_REGS);
}

/*
 * Initialize the R820T
 */
bool rt820tuner::init(void)
{
    bool r= false;
	DbgPrintf("R820T2 init\n");
    r = i2c_write(R820T2_WRITE_START,(UINT8 *) &r82xx_init_array[R820T2_WRITE_START], R820T2_NUM_REGS - R820T2_WRITE_START);
    if (r == false) return r;
    /* Calibrate */
    calibrate();
    /* set freq after calibrate */
    set_freq(r_freq);
    m_lnagain_index  = 0;
    m_mixergain_index  = 0;
    m_vgagain_index  = 0;
    return r;
}


// Send a block of data to the R820T2 via I2C
bool rt820tuner::i2c_write(UINT8 reg, UINT8 *data, UINT8 sz)
{
    bool r= false;
	if (sz <= R820T2_NUM_REGS)
		r = BBRF103.SendI2cbytes(R820T2_ADDRESS, reg, data, sz);
    return r;
}

/*
 * Write single R820T2 reg via I2C
 */
void rt820tuner::i2c_write_reg(UINT8 reg, UINT8 data)
{
    if(reg>=R820T2_NUM_REGS) return;
    r820t_regs[reg] = data;
	BBRF103.SendI2cbyte(R820T2_ADDRESS, reg, r820t_regs[reg]);
}


// Write single R820T2 reg via I2C with mask vs cached

void rt820tuner::i2c_write_cache_mask(UINT8 reg, UINT8 data, UINT8 mask)
{
    data = (data & mask) | (r820t_regs[reg] & ~mask);    // update with mask
    r820t_regs[reg] = data;                              // update buffer
	BBRF103.SendI2cbyte(R820T2_ADDRESS, reg, r820t_regs[reg]);
}

// Nibble-level bit reverse table
const UINT8 bitrevnibble[16] = { 0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
      0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf };


 // get all regs up to & including desired reg
void rt820tuner::i2c_read_raw(UINT8 *data, UINT8 sz)
{
	BBRF103.ReadI2cbytes(R820T2_ADDRESS, 0, data, sz);  // Get block of data
	for (int i =0; i <sz; i++) // nibble reverse
	{
         data[i] = (bitrevnibble[data[i] & 0xf] << 4) | bitrevnibble[data[i] >> 4]; // bit reverse
	}
}

/*
 * Read R820T2 reg - uncached
 */
UINT8 rt820tuner::i2c_read_reg_uncached(UINT8 reg)
{
    UINT8 sz = reg+1;
    UINT8 *data = r820t_regs;
    /* get all regs up to & including desired reg */
    i2c_read_raw(data, sz);
    return r820t_regs[reg];
}

/*
 * Read R820T2 reg - cached
 */
UINT8 rt820tuner::i2c_read_reg_cached(UINT8 reg)
{
    /* return desired */
    return r820t_regs[reg];
}

/*
 * r820t tuning logic
 */
#ifdef OPTIM_SET_MUX
int r820t_set_mux_freq_idx = -1; /* Default set to invalid value in order to force set_mux */
#endif

/*
 * Update Tracking Filter
 *
 * "inspired by Mauro Carvalho Chehab set_mux technique"
 * https://stuff.mit.edu/afs/sipb/contrib/linux/drivers/media/tuners/r820t.c
 * part of r820t_set_mux() (set tracking filter)
 */
void rt820tuner::set_tf(UINT32 freq)
{
    const struct r820t_freq_range *range;
    unsigned int i;
	// Get the proper frequency range in MHz instead of Hz
    freq = (UINT32)((unsigned long long int)freq * 4295 >> 32);

    // Scan array for proper range
    for(i=0;i<ARRAY_SIZE(freq_ranges)-1;i++)
    {
        if (freq < freq_ranges[i + 1].freq)
            break;
    }
    range = &freq_ranges[i];
    i2c_write_cache_mask(0x17, range->open_d, 0x08); // Open Drain
    i2c_write_cache_mask(0x1a, range->rf_mux_ploy, 0xc3); // RF_MUX,Polymux
    i2c_write_reg(0x1b, range->tf_c); // TF Band

    /* XTAL CAP & Drive */
    i2c_write_cache_mask(0x10, 0x00, 0x0b);  // Internal xtal no cap,  bit3 = 0 ?
	i2c_write_cache_mask(0x08, 0x80, 0xff);  // Mixer buffer power on, high current, Image Gain Adjustment min
	i2c_write_cache_mask(0x09, 0x00, 0xff);  // IF Filter power on, high current, Image Gain Adjustment min
}

/*
 * Update LO PLL
 */
#define  NO_PLLDEBUG_ // if debugtrace required
void rt820tuner::set_pll(UINT32 freq)
{
	const UINT32 vco_min = 1770000000;
	const UINT32 vco_max = 3900000000LL;
	UINT32 pll_ref = (r_xtal >> 1);
	UINT32 pll_ref_2x = r_xtal;

	UINT32 vco_exact;
	UINT32 vco_frac;
	UINT32 con_frac;
	UINT32 div_num;
	UINT32 n_sdm;
	UINT16 sdm;
	UINT8 ni;
	UINT8 si;
	UINT8 nint;
	UINT8 tmp;


	/* Calculate vco output divider */
	for (div_num = 0; div_num < 5; div_num++)
	{
		vco_exact = (UINT32) ((unsigned long long int) freq << (div_num + 1));  // https://stackoverflow.com/questions/7401888/why-doesnt-left-bit-shift-for-32-bit-integers-work-as-expected-when-used
		if (vco_exact >= vco_min && vco_exact <= vco_max)
		{
			break;
		}
	}

	/* Calculate the integer PLL feedback divider */
	vco_exact = (UINT32) ((unsigned long long int)freq << (div_num + 1));
	nint = (UINT8)((vco_exact + (pll_ref >> 16)) / pll_ref_2x);
	vco_frac = vco_exact - pll_ref_2x * nint;

	nint -= 13;
	ni = (nint >> 2);
	si = nint - (ni << 2);

	i2c_write_cache_mask( 0x10, 0x10, 0x10);  // REF /2?????
	/* Set the vco output divider */
	i2c_write_cache_mask(0x10, (UINT8)(div_num << 5), 0xe0);
#ifdef _PLLDEBUG_
	DbgPrintf((char *) "R820T2 vco output divider = %d\n", div_num);
#endif
	/* Set the PLL Feedback integer divider */
	tmp = (UINT8)(ni + (si << 6));
	i2c_write_reg(0x14, tmp);
#ifdef _PLLDEBUG_
	DbgPrintf((char *) "R820T2 PLL Feedback integer divider = %d\n", tmp);
#endif
	/* Update Fractional PLL */
	if (vco_frac == 0)
	{
#ifdef _PLLDEBUG_
		DbgPrintf((char *) "R820T2  Disable frac pll\n");
#endif
		/* Disable frac pll */
		i2c_write_cache_mask(0x12, 0x08, 0x08);
	}
	else
	{
#ifdef _PLLDEBUG_
		DbgPrintf((char *) "R820T2 Compute the Sigma-Delta Modulator\n");
#endif
		/* Compute the Sigma-Delta Modulator */
		vco_frac += pll_ref >> 16;
		sdm = 0;
		for (n_sdm = 0; n_sdm < 16; n_sdm++)
		{
			con_frac = (UINT32)((unsigned long long int)pll_ref >> n_sdm);
			if (vco_frac >= con_frac)
			{
				sdm |= (UINT16)((unsigned long long int)0x8000 >> n_sdm);
				vco_frac -= con_frac;
				if (vco_frac == 0)
					break;
			}
		}
#ifdef _PLLDEBUG_
		DbgPrintf((char *) "R820T2 smd = %d\n", sdm);
		UINT32 actual_freq = (((nint << 16) + sdm) * (unsigned long long int) pll_ref_2x) >> (div_num + 1 + 16);
        UINT32 delta = freq - actual_freq;
		if (actual_freq != freq)
		{
			DbgPrintf((char *) "Tuning delta: %d Hz\n", delta);
		}
#endif

		/* Update Sigma-Delta Modulator */
		i2c_write_reg(0x15, (UINT8)(sdm & 0xff));
		i2c_write_reg(0x16, (UINT8)(sdm >> 8));

		/* Enable frac pll */
		i2c_write_cache_mask(0x12, 0x00, 0x08);
	}

	int i;
	UINT8 data[4];
	for (i = 0; i < 2; i++)
	{
	 	SleepEx(1,false);
		/* Check if PLL has locked */
		i2c_read_raw(data, 3);
		if (data[2] & 0x40)
			break;

		if (!i) {
			/* Didn't lock. Increase VCO current */
			i2c_write_cache_mask(0x12, 0x60, 0xe0);
		}
	}

	if (data[2] & 0x40)
	{
#ifdef _PLLDEBUG_
		DbgPrintf((char *) "R820T2 Pll set reg2 = %x\n", data[2]);
#endif
		return;
	}
#ifdef _PLLDEBUG_
	DbgPrintf((char *) "R820T2 Pll has lock at frequency %d kHz\n", freq);
#endif
	/* set pll autotune = 8kHz    ??? */
	i2c_write_cache_mask( 0x1a, 0x08, 0x08);

	return;
}

/*
 * Update Tracking Filter and LO to frequency
 */
void rt820tuner::set_freq(UINT32 freq)
{
  UINT32 lo_freq = freq + r_ifreq;

  set_tf(freq);
  set_pll(lo_freq);
  r_freq = freq;
}

/*
 * Update LNA Gain
 */
void rt820tuner::set_lna_gain(UINT8 gain_index)
{
  gain_index %= 0xf;
  if (m_lnagain_index != gain_index)
  {
      DbgPrintf("R820T2 set_lna_gain %d\n",gain_index);
      i2c_write_cache_mask(0x05, gain_index, 0x0f);
      m_lnagain_index = gain_index;
  }
}

/*
 * Update Mixer Gain
 */
void rt820tuner::set_mixer_gain(UINT8 gain_index)
{
  gain_index %= 0xf;
  if (m_mixergain_index != gain_index)
  {
      DbgPrintf("set_mixer_gain %d\n",gain_index);
      i2c_write_cache_mask(0x07, gain_index, 0x0f);
      m_mixergain_index = gain_index;
  }
}

/*
 * Update VGA Gain
 */
void rt820tuner::set_vga_gain(UINT8 gain_index)
{
  gain_index %= 0xf;
  if (m_vgagain_index != gain_index)
  {
      i2c_write_cache_mask(0x0c, gain_index, 0x0f);
      DbgPrintf("set_vga_gain %d\n",gain_index);
      m_vgagain_index = gain_index;
  }
}

/*
 * Enable/Disable LNA AGC
 */
void rt820tuner::set_lna_agc(UINT8 value)
{
  value = value != 0 ? 0x00 : 0x10;
  i2c_write_cache_mask(0x05, value, 0x10);
}

/*
 * Enable/Disable Mixer AGC
 */
void rt820tuner::set_mixer_agc(UINT8 value)
{
  value = value != 0 ? 0x10 : 0x00;
  i2c_write_cache_mask(0x07, value, 0x10);
}

/*
 * Set IF Bandwidth
 */
void rt820tuner::set_if_bandwidth(UINT8 bw)
{
    const UINT8 modes[] = { 0xE0, 0x80, 0x60, 0x00 };
    UINT8 a = 0xB0 | (0x0F-(bw & 0x0F));
    UINT8 b = 0x0F | modes[(bw & 0x3) >> 4];
    i2c_write_reg(0x0A, a);
    i2c_write_reg(0x0B, b);
}

/*
 * Calibrate
 * Kanged from airspy firmware
 * "inspired by Mauro Carvalho Chehab calibration technique"
 * https://stuff.mit.edu/afs/sipb/contrib/linux/drivers/media/tuners/r820t.c
 *
 */
INT32 rt820tuner::calibrate(void)
{
	INT32 i, cal_code;

	for (i = 0; i < 5; i++) // 5 try
	{
		// Set filt_cap
		i2c_write_cache_mask(0x0b, 0x08, 0x60);

		// set cali clk =on
		i2c_write_cache_mask(0x0f, 0x04, 0x04);

		// X'tal cap 0pF for PLL
		i2c_write_cache_mask(0x10, 0x00, 0x03);

		// freq used for calibration
		set_pll(CALIBRATION_LO);

		// Start Trigger
		i2c_write_cache_mask(0x0b, 0x10, 0x10);

		Sleep(2); // 2ms

		// Stop Trigger
		i2c_write_cache_mask(0x0b, 0x00, 0x10);

		/* set cali clk =off */
		i2c_write_cache_mask(0x0f, 0x00, 0x04);

		/* Check if calibration worked */
		cal_code = i2c_read_reg_uncached(0x04) & 0x0f;
		if (cal_code && cal_code != 0x0f)
		{
            DbgPrintf("R820T2 calibrated step %d\n",i);
			return 0;
		}
	}
	DbgPrintf("R820T2 calibration failed\n");
	return -1;
}



void rt820tuner::set_stdby(void)
{
	UINT8 data[R820T2_NUM_REGS];
	memset(data, 0, sizeof(data));
	memcpy(r820t_regs, data, R820T2_NUM_REGS);
	DbgPrintf("R820T2_set_stdby\n");
}


