#include "Application.h"
#include "radio.h"

#include "adf4351.h"

#define GPIO_ATT_LE 17
#define GPIO_BIAS_VHF 18
#define GPIO_BIAS_HF 19
#define GPIO_RANDO 20
#define GPIO_LED_RED 21

#define GPIO_ATT_DATA  26
#define GPIO_ATT_CLK   27
#define GPIO_SHDWN 28
#define GPIO_DITH 29
#define GPIO_VHF_EN 35
#define GPIO_VGA_LE 38

#define GPIO_4351_CLK 22
#define GPIO_4351_DATA 23
#define GPIO_4351_LE 34

#define GPIO_ADC_PGA 24

#define GPIO_PRESEL_0 55
#define GPIO_PRESEL_1 54
#define GPIO_PRESEL_2 50

adf4350_init_param adf4351_init_params = {
    .clkin = 25000000,
    .channel_spacing = 5000,
    .power_up_frequency = 100000000,
    .reference_div_factor = 0,
    .reference_doubler_enable = 0,
    .reference_div2_enable = 0,

    // r2_user_settings
    .phase_detector_polarity_positive_enable = 1,
    .lock_detect_precision_6ns_enable = 0,
    .lock_detect_function_integer_n_enable = 0,
    .charge_pump_current = 7, // FixMe
    .muxout_select = 0,
    .low_spur_mode_enable = 1,

    // r3_user_settings
    .cycle_slip_reduction_enable = 1,
    .charge_cancellation_enable = 0,
    .anti_backlash_3ns_enable = 0,
    .band_select_clock_mode_high_enable = 1,
    .clk_divider_12bit = 0,
    .clk_divider_mode = 0,

    // r4_user_settings
    .aux_output_enable = 1,
    .aux_output_fundamental_enable = 1,
    .mute_till_lock_enable = 0,
    .output_power = 0, //-4dbm
    .aux_output_power = 0
};
uint32_t registers[6] = {0x4580A8, 0x80080C9, 0x4E42, 0x4B3, 0xBC803C, 0x580005};

void rx999_GpioSet(uint32_t mdata)
{
    CyU3PGpioSetValue (GPIO_SHDWN, (mdata & SHDWN) == SHDWN ); 		 // SHDN
    CyU3PGpioSetValue (GPIO_DITH, (mdata & DITH ) == DITH  ); 		 // DITH
    CyU3PGpioSetValue (GPIO_RANDO, (mdata & RANDO) == RANDO ); 		 // RAND
    CyU3PGpioSetValue (GPIO_BIAS_HF, (mdata & BIAS_HF) == BIAS_HF);
    CyU3PGpioSetValue (GPIO_BIAS_VHF, (mdata & BIAS_VHF) == BIAS_VHF);
    CyU3PGpioSetValue (GPIO_LED_RED, (mdata & LED_RED) == LED_RED);
    CyU3PGpioSetValue (GPIO_VHF_EN, (mdata & VHF_EN) == VHF_EN ); // VHF_ENß
}

void rx999_GpioInitialize()
{
    ConfGPIOsimpleout (GPIO_SHDWN);
    ConfGPIOsimpleout (GPIO_DITH);
    ConfGPIOsimpleout (GPIO_RANDO);
    ConfGPIOsimpleout (GPIO_BIAS_HF);
    ConfGPIOsimpleout (GPIO_BIAS_VHF);
    ConfGPIOsimpleout (GPIO_LED_RED);
    ConfGPIOsimpleout (GPIO_VHF_EN);

    ConfGPIOsimpleout (GPIO_ATT_LE);
    ConfGPIOsimpleout (GPIO_ATT_DATA);
    ConfGPIOsimpleout (GPIO_ATT_CLK);
    ConfGPIOsimpleout (GPIO_VGA_LE);

    ConfGPIOsimpleout (GPIO_ADC_PGA);

	// Set all nominated pins to outputs

	ConfGPIOsimpleout (GPIO_4351_LE);
	ConfGPIOsimpleout (GPIO_4351_CLK);
	ConfGPIOsimpleout (GPIO_4351_DATA);

	// preselector
	ConfGPIOsimpleout (GPIO_PRESEL_0);
	ConfGPIOsimpleout (GPIO_PRESEL_1);
	ConfGPIOsimpleout (GPIO_PRESEL_2);

    rx999_GpioSet(LED_RED);

    CyU3PGpioSetValue (GPIO_ATT_LE, 0);  // ATT_LE latched
    CyU3PGpioSetValue (GPIO_ATT_CLK, 0);  // test version
    CyU3PGpioSetValue (GPIO_ATT_DATA, 0);  // ATT_DATA
    CyU3PGpioSetValue (GPIO_VGA_LE, 1);

	CyU3PGpioSetValue (GPIO_PRESEL_0, 0);
	CyU3PGpioSetValue (GPIO_PRESEL_1, 0);
	CyU3PGpioSetValue (GPIO_PRESEL_2, 0);
}

/*
    PE4304
	direct GPIO output implementation  64 step 0.5 dB
*/
void rx999_SetAttenuator(uint8_t value)
{
	uint8_t bits = 6;
	const uint8_t mask = 0x20;
	uint8_t i,b;
	CyU3PGpioSetValue (GPIO_ATT_LE, 0);    // ATT_LE latched
	CyU3PGpioSetValue (GPIO_ATT_CLK, 0);   // ATT_CLK
	for( i = bits ; i >0; i--)
	{
		b = (value & mask) != 0;
		CyU3PGpioSetValue (GPIO_ATT_DATA, b); // ATT_DATA
		CyU3PGpioSetValue (GPIO_ATT_CLK, 1);
		value = value << 1;
		CyU3PGpioSetValue (GPIO_ATT_CLK, 0);
	}
	CyU3PGpioSetValue (GPIO_ATT_LE, 1);    // ATT_LE latched
	CyU3PGpioSetValue (GPIO_ATT_LE, 0);    // ATT_LE latched
}

/*
    AD8370
	direct GPIO output implementation 128 step 0.5 dB
*/
void rx999_SetGain(uint8_t value)
{
	uint8_t bits = 8;
	const uint8_t mask = 0x80;
	uint8_t i,b;
	CyU3PGpioSetValue (GPIO_VGA_LE, 0);    // ATT_LE latched
	CyU3PGpioSetValue (GPIO_ATT_CLK, 0);   // ATT_CLK
	for( i = bits ; i >0; i--)
	{
		b = (value & mask) != 0;
		CyU3PGpioSetValue (GPIO_ATT_DATA, b); // ATT_DATA
		CyU3PGpioSetValue (GPIO_ATT_CLK, 1);
		value = value << 1;
		CyU3PGpioSetValue (GPIO_ATT_CLK, 0);
	}
	CyU3PGpioSetValue (GPIO_VGA_LE, 1);    // ATT_LE latched
	CyU3PGpioSetValue (GPIO_ATT_DATA, 0); // ATT_DATA
}

/***************************************************************************//**
 * @brief Writes 4 bytes (32 bits) of data to ADF4350.
 *
 * @param data - Data value to write.
 *
 * @return Always Returns 0
*******************************************************************************/
int32_t adf4350_write(uint32_t data)
{
	#define HIGH 1
	#define LOW 0
	// Set idle conditions
	CyU3PGpioSetValue(GPIO_4351_LE, HIGH);
	CyU3PGpioSetValue(GPIO_4351_CLK, LOW);
	CyU3PGpioSetValue(GPIO_4351_DATA, LOW);

	//Select device LE low
	CyU3PGpioSetValue(GPIO_4351_LE, LOW);

	DebugPrint(4, " ADF4351 Register (one of the five) Updated\n");

	// Send all 32 bits
	for (uint16_t i = 0; i <32; i++)
	{
		// Test left-most bit

		if (data & 0x80000000)
			CyU3PGpioSetValue(GPIO_4351_DATA, HIGH);
		else
			CyU3PGpioSetValue(GPIO_4351_DATA, LOW);

		// Pulse clock
		CyU3PGpioSetValue(GPIO_4351_CLK, HIGH);
		CyU3PThreadSleep(1);
		CyU3PGpioSetValue(GPIO_4351_CLK, LOW);
		CyU3PThreadSleep(1);

		// shift data left so next bit will be leftmost
		data <<= 1;
	}

	//Set ADF4351 LE high

	CyU3PGpioSetValue(GPIO_4351_LE, HIGH);

	#undef HIGH
	#undef LOW

	return 0;
}

int rx999_preselect(uint32_t data)
{
	CyU3PGpioSetValue (GPIO_PRESEL_0, ((data >> 0) & 0x01));
	CyU3PGpioSetValue (GPIO_PRESEL_1, ((data >> 1) & 0x01));
	CyU3PGpioSetValue (GPIO_PRESEL_2, ((data >> 2) & 0x01));

	return 0;
}