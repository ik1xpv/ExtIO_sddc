/*
 * I2C_R820T2.c - driver
 */

#include "i2c_r820t2.h"

/*
 * I2C 
 */

#define FLAG_TIMEOUT         ((uint32_t)0x1000)
#define LONG_TIMEOUT         ((uint32_t)(10 * FLAG_TIMEOUT))

/*
 * Freq calcs
 */


#define CALIBRATION_LO 88000000

/* registers */
#define R820T2_NUM_REGS     0x20
#define R820T2_WRITE_START	5
/*
static const int r820t2_lna_gain_steps[] = {
	0, 9, 13, 40, 38, 13, 31, 22,
	26, 31, 26, 14, 19, 5, 35, 13
};
*/

const int r820t2_lna_gain_steps[16] = {
	0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30
};

const int r820t2_mixer_gain_steps[16] = {
	0, 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
};

const int r820t2_vga_gain_steps[16] = {
	0, 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
};


/* initial freq @ 128MHz -> ~5MHz IF due to xtal mismatch */
static const uint8_t r82xx_init_array[R820T2_NUM_REGS] =
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
	/* 19 */ 0xCC,
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
  uint16_t freq;
  uint8_t open_d;
  uint8_t rf_mux_ploy;
  uint8_t tf_c;
};

/* Tuner frequency ranges
"Copyright (C) 2013 Mauro Carvalho Chehab"
https://stuff.mit.edu/afs/sipb/contrib/linux/drivers/media/tuners/r820t.c
part of freq_ranges()
*/
const struct r820t_freq_range freq_ranges[] =
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

/*
 * local vars
 */
uint8_t r820t_regs[R820T2_NUM_REGS];
uint32_t r_freq;
uint32_t r_xtal;
uint32_t r_ifreq;


// Send a block of data to the R820T2 via I2C
void R820T2_i2c_write(uint8_t reg, uint8_t *data, uint8_t sz)
{
	if (sz <= R820T2_NUM_REGS)
		/* Configure slave address, nbytes, reload and generate start */
		SendI2cbytes(R820T2_ADDRESS, reg, data, sz);
}

/*
 * Write single R820T2 reg via I2C
 */
void R820T2_i2c_write_reg(uint8_t reg, uint8_t data)
{
    if(reg>=R820T2_NUM_REGS) return;
    r820t_regs[reg] = data;
	SendI2cbyte(R820T2_ADDRESS, reg, r820t_regs[reg]);
}


// Write single R820T2 reg via I2C with mask vs cached

void R820T2_i2c_write_cache_mask(uint8_t reg, uint8_t data, uint8_t mask)
{
    data = (data & mask) | (r820t_regs[reg] & ~mask);
    r820t_regs[reg] = data;
	SendI2cbyte(R820T2_ADDRESS, reg, r820t_regs[reg]);
}

// Nibble-level bit reverse table 
const uint8_t bitrevnibble[16] = { 0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
      0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf };


 // get all regs up to & including desired reg 
void R820T2_i2c_read_raw(uint8_t *data, uint8_t sz)
{ 
	ReadI2cbytes(R820T2_ADDRESS, 0, data, sz);  // Get block of data 
	for (int i =0; i <sz; i++) // nibble reverse
	{
         data[i] = (bitrevnibble[data[i] & 0xf] << 4) | bitrevnibble[data[i] >> 4]; // bit reverse 
	}
}

/*
 * Read R820T2 reg - uncached
 */
uint8_t R820T2_i2c_read_reg_uncached(uint8_t reg)
{
    uint8_t sz = reg+1;
    uint8_t *data = r820t_regs;
    /* get all regs up to & including desired reg */
    R820T2_i2c_read_raw(data, sz);
    return r820t_regs[reg];
}

/*
 * Read R820T2 reg - cached
 */
uint8_t R820T2_i2c_read_reg_cached(uint8_t reg)
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
static void R820T2_set_tf(uint32_t freq)
{
    const struct r820t_freq_range *range;
    unsigned int i;
	// Get the proper frequency range in MHz instead of Hz
    freq = (uint32_t)((uint64_t)freq * 4295 >> 32); 
    
    // Scan array for proper range 
    for(i=0;i<ARRAY_SIZE(freq_ranges)-1;i++)
    {
        if (freq < freq_ranges[i + 1].freq)
            break;
    }
    range = &freq_ranges[i];  
    R820T2_i2c_write_cache_mask(0x17, range->open_d, 0x08); // Open Drain
    R820T2_i2c_write_cache_mask(0x1a, range->rf_mux_ploy, 0xc3); // RF_MUX,Polymux
    R820T2_i2c_write_reg(0x1b, range->tf_c); // TF Band

    /* XTAL CAP & Drive */
    R820T2_i2c_write_cache_mask(0x10, 0x00, 0x0b);  // Internal xtal no cap,  bit3 = 0 ?
	R820T2_i2c_write_cache_mask(0x08, 0x80, 0xff);  // Mixer buffer power on, high current, Image Gain Adjustment min
	R820T2_i2c_write_cache_mask(0x09, 0x00, 0xff);  // IF Filter power on, high current, Image Gain Adjustment min
}

/*
 * Update LO PLL
 */
//#define  _PLLDEBUG_ // if debugtrace required
void R820T2_set_pll(uint32_t freq)
{
	const uint32_t vco_min = 1770000000;
	const uint32_t vco_max = 3900000000;
	uint32_t pll_ref = (r_xtal >> 1);
	uint32_t pll_ref_2x = r_xtal;

	uint32_t vco_exact;
	uint32_t vco_frac;
	uint32_t con_frac;
	uint32_t div_num;
	uint32_t n_sdm;
	uint16_t sdm;
	uint8_t ni;
	uint8_t si;
	uint8_t nint;
	uint8_t tmp;

	/* Calculate vco output divider */
	for (div_num = 0; div_num < 5; div_num++)
	{
		vco_exact = freq << (div_num + 1);
		if (vco_exact >= vco_min && vco_exact <= vco_max)
		{
			break;
		}
	}

	/* Calculate the integer PLL feedback divider */
	vco_exact = freq << (div_num + 1);
	nint = (uint8_t)((vco_exact + (pll_ref >> 16)) / pll_ref_2x);
	vco_frac = vco_exact - pll_ref_2x * nint;

	nint -= 13;
	ni = (nint >> 2);
	si = nint - (ni << 2);

	R820T2_i2c_write_cache_mask( 0x10, 0x10, 0x10);  // REF /2?????
	/* Set the vco output divider */
	R820T2_i2c_write_cache_mask(0x10, (uint8_t)(div_num << 5), 0xe0);
#ifdef _PLLDEBUG_
	DbgPrintf("\nR820T2 vco output divider = %d", div_num);
#endif
	/* Set the PLL Feedback integer divider */
	tmp = (uint8_t)(ni + (si << 6));
	R820T2_i2c_write_reg(0x14, tmp);
#ifdef _PLLDEBUG_
	DbgPrintf("\nR820T2 PLL Feedback integer divider = %d", tmp);
#endif
	/* Update Fractional PLL */
	if (vco_frac == 0)
	{
#ifdef _PLLDEBUG_
		DbgPrintf("\nR820T2  Disable frac pll");
#endif
		/* Disable frac pll */
		R820T2_i2c_write_cache_mask(0x12, 0x08, 0x08);
	}
	else
	{
#ifdef _PLLDEBUG_
		DbgPrintf("\nR820T2 Compute the Sigma-Delta Modulator");
#endif
		/* Compute the Sigma-Delta Modulator */
		vco_frac += pll_ref >> 16;
		sdm = 0;
		for (n_sdm = 0; n_sdm < 16; n_sdm++)
		{
			con_frac = pll_ref >> n_sdm;
			if (vco_frac >= con_frac)
			{
				sdm |= (uint16_t)(0x8000 >> n_sdm);
				vco_frac -= con_frac;
				if (vco_frac == 0)
					break;
			}
		}
#ifdef _PLLDEBUG_
		DbgPrintf("\nR820T2 smd = %ld", sdm);
#endif	
		uint32_t actual_freq = (((nint << 16) + sdm) * (uint64_t) pll_ref_2x) >> (div_num + 1 + 16);
		uint32_t delta = freq - actual_freq;
#ifdef _PLLDEBUG_
		if (actual_freq != freq)
		{
			DbgPrintf("\nTunning delta: %d Hz", delta);
		}
#endif		

		/* Update Sigma-Delta Modulator */
		R820T2_i2c_write_reg(0x15, (uint8_t)(sdm & 0xff));
		R820T2_i2c_write_reg(0x16, (uint8_t)(sdm >> 8));

		/* Enable frac pll */
		R820T2_i2c_write_cache_mask(0x12, 0x00, 0x08);
	}

	int i;
	uint8_t data[4];
	for (i = 0; i < 2; i++)
	{
	 	SleepEx(1,false);
		/* Check if PLL has locked */
		R820T2_i2c_read_raw(data, 3);
		if (data[2] & 0x40)
			break;

		if (!i) {
			/* Didn't lock. Increase VCO current */
			R820T2_i2c_write_cache_mask(0x12, 0x60, 0xe0);
		}
	}

	if (data[2] & 0x40)
	{
#ifdef _PLLDEBUG_
		DbgPrintf("\nR820T2 Pll set reg2 = %x\n", data[2]);
#endif
		return;
	}
#ifdef _PLLDEBUG_
	DbgPrintf("\nR820T2 Pll has lock at frequency %d kHz\n", freq);
#endif
	/* set pll autotune = 8kHz    ??? */
	R820T2_i2c_write_cache_mask( 0x1a, 0x08, 0x08);

	return;
}

/*
 * Update Tracking Filter and LO to frequency
 */
void R820T2_set_freq(uint32_t freq)
{
  uint32_t lo_freq = freq + r_ifreq;

  R820T2_set_tf(freq);
  R820T2_set_pll(lo_freq);
  r_freq = freq;
}

/*
 * Update LNA Gain
 */
void R820T2_set_lna_gain(uint8_t gain_index)
{
  R820T2_i2c_write_cache_mask(0x05, gain_index, 0x0f);
}

/*
 * Update Mixer Gain
 */
void R820T2_set_mixer_gain(uint8_t gain_index)
{
  R820T2_i2c_write_cache_mask(0x07, gain_index, 0x0f);
}

/*
 * Update VGA Gain
 */
void R820T2_set_vga_gain(uint8_t gain_index)
{
  R820T2_i2c_write_cache_mask(0x0c, gain_index, 0x0f);
}

/*
 * Enable/Disable LNA AGC
 */
void R820T2_set_lna_agc(uint8_t value)
{
  value = value != 0 ? 0x00 : 0x10;
  R820T2_i2c_write_cache_mask(0x05, value, 0x10);
}

/*
 * Enable/Disable Mixer AGC
 */
void R820T2_set_mixer_agc(uint8_t value)
{
  value = value != 0 ? 0x10 : 0x00;
  R820T2_i2c_write_cache_mask(0x07, value, 0x10);
}

/*
 * Set IF Bandwidth
 */
void R820T2_set_if_bandwidth(uint8_t bw)
{
    const uint8_t modes[] = { 0xE0, 0x80, 0x60, 0x00 };
    uint8_t a = 0xB0 | (0x0F-(bw & 0x0F));
    uint8_t b = 0x0F | modes[(bw & 0x3) >> 4];
    R820T2_i2c_write_reg(0x0A, a);
    R820T2_i2c_write_reg(0x0B, b);
}

/*
 * Calibrate 
 * Kanged from airspy firmware
 * "inspired by Mauro Carvalho Chehab calibration technique"
 * https://stuff.mit.edu/afs/sipb/contrib/linux/drivers/media/tuners/r820t.c
 *
 */
int32_t R820T2_calibrate(void)
{
	int32_t i, cal_code;

	for (i = 0; i < 5; i++) // 5 try
	{
		// Set filt_cap 
		R820T2_i2c_write_cache_mask(0x0b, 0x08, 0x60);

		// set cali clk =on 
		R820T2_i2c_write_cache_mask(0x0f, 0x04, 0x04);

		// X'tal cap 0pF for PLL 
		R820T2_i2c_write_cache_mask(0x10, 0x00, 0x03);

		// freq used for calibration 
		R820T2_set_pll(CALIBRATION_LO);

		// Start Trigger 
		R820T2_i2c_write_cache_mask(0x0b, 0x10, 0x10);

		Sleep(2); // 2ms

		// Stop Trigger 
		R820T2_i2c_write_cache_mask(0x0b, 0x00, 0x10);

		/* set cali clk =off */
		R820T2_i2c_write_cache_mask(0x0f, 0x00, 0x04);

		/* Check if calibration worked */
		cal_code = R820T2_i2c_read_reg_uncached(0x04) & 0x0f;
		if (cal_code && cal_code != 0x0f)
		{
//			DbgPrintf("\nR820T2 calibrated step %d\n",i);
			return 0;
		}
	}
	DbgPrintf("\n R820T2 calibration failed\n");
	return -1;
}

/*
 * Initialize the R820T
 */
void R820T2_init(void)
{
    r_xtal = R820T_FREQ;
    r_ifreq = IFR820T;
    r_freq = 101800000;
	uint8_t data[R820T2_NUM_REGS];
	DbgPrintf("\n R820T init");
	memset(data, 0, sizeof(data));

	memcpy(r820t_regs, r82xx_init_array, R820T2_NUM_REGS);
   // initialize the device 
	R820T2_i2c_write(R820T2_WRITE_START,(uint8_t *) &r82xx_init_array[R820T2_WRITE_START], R820T2_NUM_REGS - R820T2_WRITE_START);  

    /* Calibrate */
    R820T2_calibrate();
    
    /* set freq after calibrate */
    R820T2_set_freq(r_freq);

}

void R820T2_set_stdby(void)
{
	uint8_t data[R820T2_NUM_REGS];
	memset(data, 0, sizeof(data));
	memcpy(r820t_regs, data, R820T2_NUM_REGS);
	DbgPrintf("\n R820T2_set_stdby");
}

