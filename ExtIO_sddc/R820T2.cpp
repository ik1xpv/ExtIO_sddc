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


#define _R820T2DEBUG_ // if debugtrace required

/* registers */
extern volatile int64_t	glLOfreq;

using namespace std;

rt820tuner R820T2; // the class

#define VGA_BASE_GAIN	-47
static const int r82xx_vga_gain_steps[]  = {
	0, 26, 26, 30, 42, 35, 24, 13, 14, 32, 36, 34, 35, 37, 35, 36
};

static const int r82xx_lna_gain_steps[]  = {
	0, 9, 13, 40, 38, 13, 31, 22, 26, 31, 26, 14, 19, 5, 35, 13
};

static const int r82xx_mixer_gain_steps[]  = {
	0, 5, 10, 10, 19, 9, 10, 25, 17, 10, 8, 16, 13, 6, 3, -8
};

static const uint8_t vga_gains[GAIN_STEPS] =   { 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8};

static const uint8_t mixer_gains[GAIN_STEPS] = { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,10,10,11,11,12,12,13,13,14 };
static const uint8_t lna_gains[GAIN_STEPS] =   { 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,10,10,11,11,12,12,13,13,14,15 };
// total gain dB x 10
static const uint16_t total_gain[GAIN_STEPS] =  { 0, 9, 14, 27, 37, 77, 87, 125, 144, 157,
                            166, 197, 207, 229, 254, 280, 297, 328,
                            338, 364, 372, 386, 402, 421, 434, 439,
                            445, 480, 496 };

/* initial freq @ 128MHz -> ~5MHz IF due to xtal mismatch */
const UINT8 r82xx_init_array[R820T2_NUM_REGS] =
{
	0x00, 0x00, 0x00, 0x00,	0x00,		/* 00 to 04 */
	/* 05 */ 0x90, // 0x90 LNA manual gain mode, init to 0
	/* 06 */ 0x80, // 0x80
	/* 07 */ 0x60,
	/* 08 */ 0xc0, // Image Gain Adjustment
	/* 09 */ 0x40, //  40 Image Phase Adjustment
	/* 0A */ 0x80, // 0xa0  A8 Channel filter [0..3]: 0 = widest, f = narrowest - Optimal. Don't touch!
	/* 0B */ 0x6f, // 6F High pass filter - Optimal. Don't touch!
	/* 0C */ 0x40, // 0x480x40 VGA control by code, init at 0
	/* 0D */ 0x63, // 0x63  LNA AGC settings: [0..3]: Lower threshold; [4..7]: High threshold
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
	/* 1C */ 0x54,  //0x54
	/* 1D */ 0xAE,  //0xAE
	/* 1E */ 0x0A,
	/* 1F */ 0xC0
};

/* initial freq @ 128MHz -> ~5MHz IF due to xtal mismatch */
const UINT8 r82xx_stdby[R820T2_NUM_REGS] =
{
	0x00, 0x00, 0x00, 0x00,	0x00,		/* 00 to 04 */
	/* 05 */ 0xA0, //
	/* 06 */ 0x80, //
	/* 07 */ 0x20,
	/* 08 */ 0x40, // Image Gain Adjustment
	/* 09 */ 0xc0, //  40 Image Phase Adjustment
	/* 0A */ 0xe0, // 0xa0  A8 Channel filter [0..3]: 0 = widest, f = narrowest - Optimal. Don't touch!
	/* 0B */ 0x6f, // 6F High pass filter - Optimal. Don't touch!
	/* 0C */ 0x40, // 0x480x40 VGA control by code, init at 0
	/* 0D */ 0x63, // 0x63  LNA AGC settings: [0..3]: Lower threshold; [4..7]: High threshold
	/* 0E */ 0x75,
	/* 0F */ 0x3A, // LDO_5V OFF, clk out OFF,
	/* 10 */ 0x7C,
	/* 11 */ 0x83,
	/* 12 */ 0x80,
	/* 13 */ 0x00,
	/* 14 */ 0x0F,
	/* 15 */ 0x00,
	/* 16 */ 0xC0,
	/* 17 */ 0xC8,
	/* 18 */ 0x48,
	/* 19 */ 0x4C, //0xCC
	/* 1A */ 0x62, //0x60
	/* 1B */ 0x00,
	/* 1C */ 0x54,  //0x54
	/* 1D */ 0xAE,  //0xAE
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
  /* 0 MHz */              0,
  /* .open_d = */       0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */         0xdf, /* R27[7:0]  band2,band0 */
  }, {
  /* 50 MHz */            50,
  /* .open_d = */       0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */         0xbe, /* R27[7:0]  band4,band1  */
  }, {
  /* 55 MHz */            55,
  /* .open_d = */       0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */         0x8b, /* R27[7:0]  band7,band4 */
  }, {
  /* 60 MHz */            60,
  /* .open_d = */       0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */         0x7b, /* R27[7:0]  band8,band4 */
  }, {
  /* 65 MHz */            65,
  /* .open_d = */       0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */         0x69, /* R27[7:0]  band9,band6 */
  }, {
  /* 70 MHz */            70,
  /* .open_d = */       0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */         0x58, /* R27[7:0]  band10,band7 */
  }, {
  /* 75 MHz */            75,
  /* .open_d = */       0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */         0x44, /* R27[7:0]  band11,band11 */
  }, {
  /* 80 MHz */            80,
  /* .open_d = */       0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */         0x44, /* R27[7:0]  band11,band11 */
  }, {
  /* 90 MHz */           90,
  /* .open_d = */      0x00, /* high */
  /* .rf_mux_ploy = */ 0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */        0x34, /* R27[7:0]  band12,band11 */
  }, {
  /* 100 MHz */         100,
  /* .open_d = */      0x00, /* high */
  /* .rf_mux_ploy = */ 0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */        0x34, /* R27[7:0]  band12,band11 */
  }, {
  /* 110 MHz */         110,
  /* .open_d = */      0x00, /* high */
  /* .rf_mux_ploy = */ 0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */        0x24, /* R27[7:0]  band13,band11 */
  }, {
  /* 120 MHz */         120,
  /* .open_d = */      0x00, /* high */
  /* .rf_mux_ploy = */ 0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */        0x24, /* R27[7:0]  band13,band11 */
  }, {
  /* 140 MHz */         140,
  /* .open_d = */      0x00, /* high */
  /* .rf_mux_ploy = */ 0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */        0x14, /* R27[7:0]  band14,band11 */
  }, {
  /* 180 MHz */         180,
  /* .open_d = */      0x00, /* high */
  /* .rf_mux_ploy = */ 0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */        0x13, /* R27[7:0]  band14,band12 */
  }, {
  /* 220 MHz */         220,
  /* .open_d = */      0x00, /* high */
  /* .rf_mux_ploy = */ 0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */        0x13, /* R27[7:0]  band14,band12 */
  }, {
  /* 250 MHz */         250,
  /* .open_d = */      0x00, /* high */
  /* .rf_mux_ploy = */ 0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */        0x11, /* R27[7:0]  highest,highest */
  }, {
  /* 280 MHz */         280,
  /* .open_d = */      0x00, /* high */
  /* .rf_mux_ploy = */ 0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */        0x00, /* R27[7:0]  highest,highest */
  }, {
  /* 310 MHz */         310,
  /* .open_d = */      0x00, /* high */
  /* .rf_mux_ploy = */ 0x41, /* R26[7:6]=1 (bypass)  R26[1:0]=1 (middle) */
  /* .tf_c = */        0x00, /* R27[7:0]  highest,highest */
  }, {
  /* 450 MHz */         450,
  /* .open_d = */      0x00, /* high */
  /* .rf_mux_ploy = */ 0x41, /* R26[7:6]=1 (bypass)  R26[1:0]=1 (middle) */
  /* .tf_c = */        0x00, /* R27[7:0]  highest,highest */
  }, {
  /* 588 MHz */         588,
  /* .open_d = */      0x00, /* high */
  /* .rf_mux_ploy = */ 0x40, /* R26[7:6]=1 (bypass)  R26[1:0]=0 (highest) */
  /* .tf_c = */        0x00, /* R27[7:0]  highest,highest */
  }, {
  /* 650 MHz */         650,
  /* .open_d = */      0x00, /* high */
  /* .rf_mux_ploy = */ 0x40, /* R26[7:6]=1 (bypass)  R26[1:0]=0 (highest) */
  /* .tf_c = */        0x00, /* R27[7:0]  highest,highest */
  }
};


rt820tuner::rt820tuner()
{
    r_xtal = R820T2_FREQ;
    r_ifreq = R820T2_IF_CARRIER;
    r_freq = 0;
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
    if (calibrate() !=0)
        DbgPrintf("R820T2 calibrate() failed\n");

    /* set freq after calibrate */
    r_freq = glLOfreq;
    set_freq(r_freq);
    m_lnagain_index  = 0;
    m_mixergain_index  = 0;
    m_vgagain_index  = 0;
    return r;
}
bool rt820tuner::stdby(void)
{
    bool r= false;
	DbgPrintf("R820T2 stdby\n");
    r = i2c_write(R820T2_WRITE_START,(UINT8 *) &r82xx_stdby[R820T2_WRITE_START], R820T2_NUM_REGS - R820T2_WRITE_START);
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
    i2c_write_cache_mask(0x10, 0x08, 0x0b);
	i2c_write_cache_mask(0x08, 0x00, 0x3f);  // Mixer buffer power on, high current, Image Gain Adjustment min
	i2c_write_cache_mask(0x09, 0x00, 0x3f);  // IF Filter power on, high current, Image Gain Adjustment min

}

/*
 * Update LO PLL
 */

void rt820tuner::set_pll(UINT32 freq)
{
	const UINT32 vco_min = 1770000000;
	const UINT32 vco_max = 3900000000LL;  // max 1.850.000.000
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
	UINT8 nintx;
	UINT8 tmp;

	/* Calculate vco output divider
	  example:
	  freq = 106.800.000
	  div = 4
	  vco_exact = 3.417.600.000
	*/
	for (div_num = 0; div_num < 5; div_num++)
	{
		vco_exact = (UINT32) ((unsigned long long int) freq << (div_num + 1));  // https://stackoverflow.com/questions/7401888/why-doesnt-left-bit-shift-for-32-bit-integers-work-as-expected-when-used
		if (vco_exact >= vco_min && vco_exact <= vco_max)
		{
			break;
		}
	}

	/* Calculate the integer PLL feedback divider
      example:
	  freq = 106.800.000
	  vco_exact = 3.417.600.000
      nint = 93
      vco_frac = 25.600.000
      nint = 75
      ni = 18
      si = 3
	*/
	vco_exact = (UINT32) ((unsigned long long int)freq << (div_num + 1));
	nint = (UINT8)((vco_exact + (pll_ref >> 16)) / pll_ref_2x);
	vco_frac = vco_exact - pll_ref_2x * nint;

	nintx = nint - 13;
	ni = (nintx >> 2);
	si = nintx - (ni << 2);

	// DbgPrintf((char *) "R820T2 nint %d ni %d si %d\n", nint, ni, si);

	i2c_write_cache_mask( 0x10, 0x10, 0x10);  // REF /2?????
	/* Set the vco output divider */
	i2c_write_cache_mask(0x10, (UINT8)(div_num << 5), 0xe0);

	/* Set the PLL Feedback integer divider */
	tmp = (UINT8)(ni + (si << 6));
	i2c_write_reg(0x14, tmp);

	/* Update Fractional PLL */
	if (vco_frac == 0)
	{
		/* Disable frac pll */
		i2c_write_cache_mask(0x12, 0x08, 0x08);
	}
	else
	{
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

		/* Update Sigma-Delta Modulator */
		i2c_write_reg(0x15, (UINT8)(sdm & 0xff));
		i2c_write_reg(0x16, (UINT8)(sdm >> 8));
		/* Enable frac pll */
		i2c_write_cache_mask(0x12, 0x00, 0x08);

	}

#ifdef _R820T2DEBUG_
 		UINT32 actual_freq = ((((unsigned long long int) (nint) << 16) + sdm) * (unsigned long long int) pll_ref_2x) >> (div_num + 1 + 16);
        UINT32 delta = freq - actual_freq;
        unsigned long long int vco = ((unsigned long long int) actual_freq << (div_num + 1));
        DbgPrintf((char *) "R820T2 TUNE freq %d Hz err: %d Hz divnum %d nint %d vco %lu Hz\n", actual_freq, delta, div_num, nint,(long unsigned int)  vco );
#endif

	return;
}

/*
 * Update Tracking Filter and LO to frequency
 */
void rt820tuner::set_freq(UINT32 freq)
{
  UINT32 lo_freq = freq + r_ifreq;
#ifdef _R820T2DEBUG_
		DbgPrintf((char *) "R820T2   LO freq %d Hz\n",lo_freq );
#endif
  set_tf(freq);
  set_pll(lo_freq);
  r_freq = freq;
}


void rt820tuner::set_all_gains(UINT8 gain_index)
{
  uint8_t lna, mix, vga;
  lna = lna_gains [ gain_index ];
  mix = mixer_gains [ gain_index ];
  vga =  vga_gains [ gain_index ];
  totalgain = total_gain[ gain_index ];
  vga = 9;
  set_lna_gain(lna);
  set_mixer_gain(mix);
  set_vga_gain(vga);

}


/*
 * Update LNA Gain
 */
void rt820tuner::set_lna_gain(UINT8 gain_index)
{
  if (m_lnagain_index != gain_index)
  {
      i2c_write_cache_mask(0x05, gain_index, 0x0f);
      m_lnagain_index = gain_index;
  }
}

/*
 * Update Mixer Gain
 */
void rt820tuner::set_mixer_gain(UINT8 gain_index)
{
  if (m_mixergain_index != gain_index)
  {
      i2c_write_cache_mask(0x07, gain_index, 0x0f);
      m_mixergain_index = gain_index;
  }
}

/*
 * Update VGA Gain
 */
void rt820tuner::set_vga_gain(UINT8 gain_index)
{
  if (m_vgagain_index != gain_index)
  {
      i2c_write_cache_mask(0x0c, gain_index, 0x0f);
      m_vgagain_index = gain_index;
  }
}

/*
 * Enable/Disable LNA AGC (not used)
 */
void rt820tuner::set_lna_agc(UINT8 value)
{
  value = value != 0 ? 0x00 : 0x10;
  i2c_write_cache_mask(0x05, value, 0x10);
}

/*
 * Enable/Disable Mixer AGC (not used)
 */
void rt820tuner::set_mixer_agc(UINT8 value)
{
  value = value != 0 ? 0x10 : 0x00;
  i2c_write_cache_mask(0x07, value, 0x10);
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
			return 0;
		}
	}
	return -1;
}




 float rt820tuner::get_allgain(void)
 {
     float gdB, glin;
     gdB =  (float) totalgain / 10.0;
     glin = 331.0/ pow(10.0,(gdB/20.0));
     return glin;
 }

float rt820tuner::get_gain_dB(UINT idx)
 {
     float gdB= 0.0;
     if (idx < GAIN_STEPS)
        gdB = (float) total_gain[idx]/10.0;
     return gdB;
 }
