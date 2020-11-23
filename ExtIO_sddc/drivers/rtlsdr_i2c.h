#include "config.h"
#include "tuner_r82xx.h"
#include <stdio.h>

extern int I2cTransfer (
        uint8_t   byteAddress,
        uint8_t   devAddr,
        uint8_t   byteCount,
        uint8_t   *buffer,
        bool  isRead);

struct r82xx_priv priv;
struct r82xx_config config;

#define fprintf(e, s, ...) DbgPrintf(s, __VA_ARGS__)

int rtlsdr_i2c_write_fn(void* dev, int i2caddr, uint8_t * buf, uint8_t len){
    if (I2cTransfer(buf[0], i2caddr, len - 1, &buf[1], false) == 0)
    {
        return len;
    }
    return -1;

}

int rtlsdr_i2c_read_fn(void* dev, int i2caddr, uint8_t *buf, uint8_t len)
{

    if (I2cTransfer(buf[0], i2caddr, len - 1, &buf[1], true) == 0)
    {
        return len;
    }

    return -1;
}

int rt820init(void)
{
    priv.cfg = &config;

 	config.xtal = R820T_FREQ;
	config.max_i2c_msg_len = 30;
    config.verbose = true;
    config.vco_algo = 1;
    config.vco_curr_max = 0xff;
    config.vco_curr_min = 0xff;
    config.harmonic = 0;
	config.use_predetect = false;

    if (r82xx_init(&priv) < 0)
	{
		DbgPrintf("Failed to initialize\n"); 
	}

    return 0;
}

void set_all_gains(UINT8 gain_index)
{
  r82xx_set_gain(&priv, false, gain_index * 10, 0, 0, 0, 0, nullptr);
  DbgPrintf("\r\nset_all_gains %d", gain_index);
}

void set_freq(UINT32 freq)
{
    if (r82xx_set_freq(&priv, freq) < 0)
	{
		DbgPrintf("set_freq failed\n");
	}
}

void rt820shutdown(void)
{
    if (r82xx_standby(&priv) < 0)
	{
		DbgPrintf("Failed to standby\n");
	}
}

uint8_t rt820detect(void)
{
    uint8_t identity;
    memset(&config, 0, sizeof(config));
    memset(&priv, 0, sizeof(priv));

   // detect the hardware
    if (I2cTransfer(0, R820T_I2C_ADDR, 1, &identity, true) == 0)
    {
        config.i2c_addr = R820T_I2C_ADDR;
        config.rafael_chip = CHIP_R820T;
    }
    else if (I2cTransfer(0, R828D_I2C_ADDR, 1, &identity, true) == 0)
    {
        config.i2c_addr = R828D_I2C_ADDR;
        config.rafael_chip = CHIP_R828D;
    }
    else
        config.i2c_addr = 0;

    DbgPrintf("addr: %x id: %x\n", config.i2c_addr, identity);

    return identity;
}