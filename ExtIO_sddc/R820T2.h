#ifndef R820T2_H
#define R820T2_H
#include "license.txt" // Oscar Steila ik1xpv
#include <basetsd.h>

#define R820T2_ADDRESS	    (0x34) //  ((0x1A)<<1)
#define IFR820T				7000000
#define R820T2_NUM_REGS     0x20
#define R820T2_WRITE_START	5
#define R820T_FREQ (32000000)

class rt820tuner {
public:
    rt820tuner();
    bool init(void);
    void set_freq(UINT32 freq);
    void set_mixer_gain(UINT8 gain_index);
    void set_vga_gain(UINT8 gain_index);
    void set_stdby(void);
    void set_lna_gain(UINT8 gain_index);

    bool i2c_write(UINT8 reg, UINT8 *data, UINT8 sz);
private:
    void i2c_write_cache_mask(UINT8 reg, UINT8 data, UINT8 mask);
    void i2c_write_reg(UINT8 reg, UINT8 data);
    void i2c_read_raw(UINT8 *data, UINT8 sz);
    UINT8 i2c_read_reg_uncached(UINT8 reg);
    UINT8 i2c_read_reg_cached(UINT8 reg);
    void set_pll(UINT32 freq);
    void set_tf(UINT32 freq);
    void set_lna_agc(UINT8 value);
    void set_mixer_agc(UINT8 value);
    void set_if_bandwidth(UINT8 bw);
    INT32 calibrate(void);
    UINT8 m_vgagain_index; //
    UINT8 m_mixergain_index;
    UINT8 m_lnagain_index;
    UINT8 r820t_regs[R820T2_NUM_REGS];
    UINT32 r_freq;
    UINT32 r_xtal;
    UINT32 r_ifreq;

};

extern class rt820tuner R820T2;
extern const int r820t2_lna_gain_steps[16];
extern const UINT8 r82xx_init_array[R820T2_NUM_REGS];
#endif
