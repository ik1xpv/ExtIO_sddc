#include "Application.h"
#include "rd5815.h"

static uint32_t refclk;

void RDA5815Initial(uint32_t freq)
{
	refclk = freq;
	//I2cTransferW1( RD5812_I2C_ADDR, register address,register data);
	DebugPrint(4, "RDA5815Initial\n");

	CyU3PThreadSleep(1); //Wait 1ms.

	// Chip register soft reset
	I2cTransferW1(0x04, RD5812_I2C_ADDR, 0x04);
	I2cTransferW1(0x04, RD5812_I2C_ADDR, 0x05);

	// Initial configuration start

	//pll setting
	I2cTransferW1(0x1a, RD5812_I2C_ADDR, 0x13);
	I2cTransferW1(0x41, RD5812_I2C_ADDR, 0x53);
	I2cTransferW1(0x38, RD5812_I2C_ADDR, 0x9B);
	I2cTransferW1(0x39, RD5812_I2C_ADDR, 0x15);
	I2cTransferW1(0x3A, RD5812_I2C_ADDR, 0x00);
	I2cTransferW1(0x3B, RD5812_I2C_ADDR, 0x00);
	I2cTransferW1(0x3C, RD5812_I2C_ADDR, 0x0c);
	I2cTransferW1(0x0c, RD5812_I2C_ADDR, 0xE2);
	I2cTransferW1(0x2e, RD5812_I2C_ADDR, 0x6F);

	switch (refclk)
	{
	case 27000000:
		I2cTransferW1(0x72, RD5812_I2C_ADDR, 0x07); // v1.1, 1538~1539
		I2cTransferW1(0x73, RD5812_I2C_ADDR, 0x10);
		I2cTransferW1(0x74, RD5812_I2C_ADDR, 0x71);
		I2cTransferW1(0x75, RD5812_I2C_ADDR, 0x06); // v1.1, 1363~1364, 1862~1863
		I2cTransferW1(0x76, RD5812_I2C_ADDR, 0x40);
		I2cTransferW1(0x77, RD5812_I2C_ADDR, 0x89);
		I2cTransferW1(0x79, RD5812_I2C_ADDR, 0x04); // v1.1, 900
		I2cTransferW1(0x7A, RD5812_I2C_ADDR, 0x2A);
		I2cTransferW1(0x7B, RD5812_I2C_ADDR, 0xAA);
		I2cTransferW1(0x7C, RD5812_I2C_ADDR, 0xAB);
		break;
	case 30000000:
		I2cTransferW1(0x72, RD5812_I2C_ADDR, 0x06); // v1.2, 1544~1545
		I2cTransferW1(0x73, RD5812_I2C_ADDR, 0x60);
		I2cTransferW1(0x74, RD5812_I2C_ADDR, 0x66);
		I2cTransferW1(0x75, RD5812_I2C_ADDR, 0x05); // v1.2, 1364~1365, 1859~1860
		I2cTransferW1(0x76, RD5812_I2C_ADDR, 0xA0);
		I2cTransferW1(0x77, RD5812_I2C_ADDR, 0x7B);
		I2cTransferW1(0x79, RD5812_I2C_ADDR, 0x03); // v1.2, 901
		I2cTransferW1(0x7A, RD5812_I2C_ADDR, 0xC0);
		I2cTransferW1(0x7B, RD5812_I2C_ADDR, 0x00);
		I2cTransferW1(0x7C, RD5812_I2C_ADDR, 0x00);
		break;
	case 24000000:
		I2cTransferW1(0x72, RD5812_I2C_ADDR, 0x08); // v1.3, 1547~1548
		I2cTransferW1(0x73, RD5812_I2C_ADDR, 0x00);
		I2cTransferW1(0x74, RD5812_I2C_ADDR, 0x80);
		I2cTransferW1(0x75, RD5812_I2C_ADDR, 0x07); // v1.3, 1367~1368, 1859~1860
		I2cTransferW1(0x76, RD5812_I2C_ADDR, 0x10);
		I2cTransferW1(0x77, RD5812_I2C_ADDR, 0x9A);
		I2cTransferW1(0x79, RD5812_I2C_ADDR, 0x04); // v1.3, 901
		I2cTransferW1(0x7A, RD5812_I2C_ADDR, 0xB0);
		I2cTransferW1(0x7B, RD5812_I2C_ADDR, 0x00);
		I2cTransferW1(0x7C, RD5812_I2C_ADDR, 0x00);
		break;

	default:
		DebugPrint(4, "refclk is out of rage");
		break;
	}

	I2cTransferW1(0x2f, RD5812_I2C_ADDR, 0x57);
	I2cTransferW1(0x0d, RD5812_I2C_ADDR, 0x70);
	I2cTransferW1(0x18, RD5812_I2C_ADDR, 0x4B);
	I2cTransferW1(0x30, RD5812_I2C_ADDR, 0xFF);
	I2cTransferW1(0x5c, RD5812_I2C_ADDR, 0xFF);
	I2cTransferW1(0x65, RD5812_I2C_ADDR, 0x00);
	I2cTransferW1(0x70, RD5812_I2C_ADDR, 0x3F);
	I2cTransferW1(0x71, RD5812_I2C_ADDR, 0x3F);
	I2cTransferW1(0x53, RD5812_I2C_ADDR, 0xA8);
	I2cTransferW1(0x46, RD5812_I2C_ADDR, 0x21);
	I2cTransferW1(0x47, RD5812_I2C_ADDR, 0x84);
	I2cTransferW1(0x48, RD5812_I2C_ADDR, 0x10);
	I2cTransferW1(0x49, RD5812_I2C_ADDR, 0x08);
	I2cTransferW1(0x60, RD5812_I2C_ADDR, 0x80);
	I2cTransferW1(0x61, RD5812_I2C_ADDR, 0x80);
	I2cTransferW1(0x6A, RD5812_I2C_ADDR, 0x08);
	I2cTransferW1(0x6B, RD5812_I2C_ADDR, 0x63);
	I2cTransferW1(0x69, RD5812_I2C_ADDR, 0xF8);
	I2cTransferW1(0x57, RD5812_I2C_ADDR, 0x64);
	I2cTransferW1(0x05, RD5812_I2C_ADDR, 0xaa);
	I2cTransferW1(0x06, RD5812_I2C_ADDR, 0xaa);
	I2cTransferW1(0x15, RD5812_I2C_ADDR, 0xAE);
	I2cTransferW1(0x4a, RD5812_I2C_ADDR, 0x67);
	I2cTransferW1(0x4b, RD5812_I2C_ADDR, 0x77);

	//agc setting

	I2cTransferW1(0x4f, RD5812_I2C_ADDR, 0x40);
	I2cTransferW1(0x5b, RD5812_I2C_ADDR, 0x20);

	I2cTransferW1(0x16, RD5812_I2C_ADDR, 0x0C);
	I2cTransferW1(0x18, RD5812_I2C_ADDR, 0x0C);
	I2cTransferW1(0x30, RD5812_I2C_ADDR, 0x1C);
	I2cTransferW1(0x5c, RD5812_I2C_ADDR, 0x2C);
	I2cTransferW1(0x6c, RD5812_I2C_ADDR, 0x3C);
	I2cTransferW1(0x6e, RD5812_I2C_ADDR, 0x3C);
	I2cTransferW1(0x1b, RD5812_I2C_ADDR, 0x7C);
	I2cTransferW1(0x1d, RD5812_I2C_ADDR, 0xBD);
	I2cTransferW1(0x1f, RD5812_I2C_ADDR, 0xBD);
	I2cTransferW1(0x21, RD5812_I2C_ADDR, 0xBE);
	I2cTransferW1(0x23, RD5812_I2C_ADDR, 0xBE);
	I2cTransferW1(0x25, RD5812_I2C_ADDR, 0xFE);
	I2cTransferW1(0x27, RD5812_I2C_ADDR, 0xFF);
	I2cTransferW1(0x29, RD5812_I2C_ADDR, 0xFF);
	I2cTransferW1(0xb3, RD5812_I2C_ADDR, 0xFF);
	I2cTransferW1(0xb5, RD5812_I2C_ADDR, 0xFF);

	I2cTransferW1(0x17, RD5812_I2C_ADDR, 0xF0);
	I2cTransferW1(0x19, RD5812_I2C_ADDR, 0xF0);
	I2cTransferW1(0x31, RD5812_I2C_ADDR, 0xF0);
	I2cTransferW1(0x5d, RD5812_I2C_ADDR, 0xF0);
	I2cTransferW1(0x6d, RD5812_I2C_ADDR, 0xF0);
	I2cTransferW1(0x6f, RD5812_I2C_ADDR, 0xF1);
	I2cTransferW1(0x1c, RD5812_I2C_ADDR, 0xF5);
	I2cTransferW1(0x1e, RD5812_I2C_ADDR, 0x35);
	I2cTransferW1(0x20, RD5812_I2C_ADDR, 0x79);
	I2cTransferW1(0x22, RD5812_I2C_ADDR, 0x9D);
	I2cTransferW1(0x24, RD5812_I2C_ADDR, 0xBE);
	I2cTransferW1(0x26, RD5812_I2C_ADDR, 0xBE);
	I2cTransferW1(0x28, RD5812_I2C_ADDR, 0xBE);
	I2cTransferW1(0x2a, RD5812_I2C_ADDR, 0xCF);
	I2cTransferW1(0xb4, RD5812_I2C_ADDR, 0xDF);
	I2cTransferW1(0xb6, RD5812_I2C_ADDR, 0x0F);

	I2cTransferW1(0xb7, RD5812_I2C_ADDR, 0x15); //start
	I2cTransferW1(0xb9, RD5812_I2C_ADDR, 0x6c);
	I2cTransferW1(0xbb, RD5812_I2C_ADDR, 0x63);
	I2cTransferW1(0xbd, RD5812_I2C_ADDR, 0x5a);
	I2cTransferW1(0xbf, RD5812_I2C_ADDR, 0x5a);
	I2cTransferW1(0xc1, RD5812_I2C_ADDR, 0x55);
	I2cTransferW1(0xc3, RD5812_I2C_ADDR, 0x55);
	I2cTransferW1(0xc5, RD5812_I2C_ADDR, 0x47);
	I2cTransferW1(0xa3, RD5812_I2C_ADDR, 0x53);
	I2cTransferW1(0xa5, RD5812_I2C_ADDR, 0x4f);
	I2cTransferW1(0xa7, RD5812_I2C_ADDR, 0x4e);
	I2cTransferW1(0xa9, RD5812_I2C_ADDR, 0x4e);
	I2cTransferW1(0xab, RD5812_I2C_ADDR, 0x54);
	I2cTransferW1(0xad, RD5812_I2C_ADDR, 0x31);
	I2cTransferW1(0xaf, RD5812_I2C_ADDR, 0x43);
	I2cTransferW1(0xb1, RD5812_I2C_ADDR, 0x9f);

	I2cTransferW1(0xb8, RD5812_I2C_ADDR, 0x6c); //end
	I2cTransferW1(0xba, RD5812_I2C_ADDR, 0x92);
	I2cTransferW1(0xbc, RD5812_I2C_ADDR, 0x8a);
	I2cTransferW1(0xbe, RD5812_I2C_ADDR, 0x8a);
	I2cTransferW1(0xc0, RD5812_I2C_ADDR, 0x82);
	I2cTransferW1(0xc2, RD5812_I2C_ADDR, 0x93);
	I2cTransferW1(0xc4, RD5812_I2C_ADDR, 0x85);
	I2cTransferW1(0xc6, RD5812_I2C_ADDR, 0x77);
	I2cTransferW1(0xa4, RD5812_I2C_ADDR, 0x82);
	I2cTransferW1(0xa6, RD5812_I2C_ADDR, 0x7e);
	I2cTransferW1(0xa8, RD5812_I2C_ADDR, 0x7d);
	I2cTransferW1(0xaa, RD5812_I2C_ADDR, 0x6f);
	I2cTransferW1(0xac, RD5812_I2C_ADDR, 0x65);
	I2cTransferW1(0xae, RD5812_I2C_ADDR, 0x43);
	I2cTransferW1(0xb0, RD5812_I2C_ADDR, 0x9f);
	I2cTransferW1(0xb2, RD5812_I2C_ADDR, 0xf0);

	I2cTransferW1(0x81, RD5812_I2C_ADDR, 0x92); //rise
	I2cTransferW1(0x82, RD5812_I2C_ADDR, 0xb4);
	I2cTransferW1(0x83, RD5812_I2C_ADDR, 0xb3);
	I2cTransferW1(0x84, RD5812_I2C_ADDR, 0xac);
	I2cTransferW1(0x85, RD5812_I2C_ADDR, 0xba);
	I2cTransferW1(0x86, RD5812_I2C_ADDR, 0xbc);
	I2cTransferW1(0x87, RD5812_I2C_ADDR, 0xaf);
	I2cTransferW1(0x88, RD5812_I2C_ADDR, 0xa2);
	I2cTransferW1(0x89, RD5812_I2C_ADDR, 0xac);
	I2cTransferW1(0x8a, RD5812_I2C_ADDR, 0xa9);
	I2cTransferW1(0x8b, RD5812_I2C_ADDR, 0x9b);
	I2cTransferW1(0x8c, RD5812_I2C_ADDR, 0x7d);
	I2cTransferW1(0x8d, RD5812_I2C_ADDR, 0x74);
	I2cTransferW1(0x8e, RD5812_I2C_ADDR, 0x9f);
	I2cTransferW1(0x8f, RD5812_I2C_ADDR, 0xf0);

	I2cTransferW1(0x90, RD5812_I2C_ADDR, 0x15); //fall
	I2cTransferW1(0x91, RD5812_I2C_ADDR, 0x39);
	I2cTransferW1(0x92, RD5812_I2C_ADDR, 0x30);
	I2cTransferW1(0x93, RD5812_I2C_ADDR, 0x27);
	I2cTransferW1(0x94, RD5812_I2C_ADDR, 0x29);
	I2cTransferW1(0x95, RD5812_I2C_ADDR, 0x0d);
	I2cTransferW1(0x96, RD5812_I2C_ADDR, 0x10);
	I2cTransferW1(0x97, RD5812_I2C_ADDR, 0x1e);
	I2cTransferW1(0x98, RD5812_I2C_ADDR, 0x1a);
	I2cTransferW1(0x99, RD5812_I2C_ADDR, 0x19);
	I2cTransferW1(0x9a, RD5812_I2C_ADDR, 0x19);
	I2cTransferW1(0x9b, RD5812_I2C_ADDR, 0x32);
	I2cTransferW1(0x9c, RD5812_I2C_ADDR, 0x1f);
	I2cTransferW1(0x9d, RD5812_I2C_ADDR, 0x31);
	I2cTransferW1(0x9e, RD5812_I2C_ADDR, 0x43);

	CyU3PThreadSleep(10); //Wait 10ms;

	// Initial configuration end
}

/********************************************************************************/
//	Function to Set the RDA5815
//	fPLL:		Frequency			unit: MHz from 250 to 2300
//	fSym:	SymbolRate			unit: KSps from 1000 to 60000
/********************************************************************************/
int32_t RDA5815Set(unsigned long fPLL, unsigned long fSym)
{
	unsigned char buffer;
	unsigned long temp_value = 0;
	unsigned long bw; /*,temp_value1 = 0,temp_value2=0 ;*/
	unsigned char Filter_bw_control_bit;

	DebugPrint(4, "RDA5815Set %d, %d\n", fPLL, fSym);

	I2cTransferW1(0x04, RD5812_I2C_ADDR, 0xc1); //add by rda 2011.8.9,RXON = 0 , change normal working state to idle state
	I2cTransferW1(0x2b, RD5812_I2C_ADDR, 0x95); //clk_interface_27m=0  add by rda 2012.1.12

	//set frequency start
	switch (refclk)
	{
	case 27000000:
		temp_value = (unsigned long)fPLL * 77672; //((2^21) / RDA5815_XTALFREQ);
		break;
	case 30000000:								  // v1.2
		temp_value = (unsigned long)fPLL * 69905; //((2^21) / RDA5815_XTALFREQ);
		break;
	case 24000000:								  // v1.3
		temp_value = (unsigned long)fPLL * 87381; //((2^21) / RDA5815_XTALFREQ);
		break;
	default:
		DebugPrint(4, "refclk is out of rage\n");
	}

	DebugPrint(4, "set value to 0x%x\n", temp_value);

	buffer = ((unsigned char)((temp_value >> 24) & 0xff));
	I2cTransferW1(0x07, RD5812_I2C_ADDR, buffer);
	buffer = ((unsigned char)((temp_value >> 16) & 0xff));
	I2cTransferW1(0x08, RD5812_I2C_ADDR, buffer);
	buffer = ((unsigned char)((temp_value >> 8) & 0xff));
	I2cTransferW1(0x09, RD5812_I2C_ADDR, buffer);
	buffer = ((unsigned char)(temp_value & 0xff));
	I2cTransferW1(0x0a, RD5812_I2C_ADDR, buffer);
	//set frequency end

	// set Filter bandwidth start
	bw = fSym; //kHz

	Filter_bw_control_bit = (unsigned char)((bw * 135 / 200 + 4000) / 1000);

	if (Filter_bw_control_bit < 4)
		Filter_bw_control_bit = 4; // MHz
	else if (Filter_bw_control_bit > 40)
		Filter_bw_control_bit = 40; // MHz

	Filter_bw_control_bit &= 0x3f;
	Filter_bw_control_bit |= 0x40; //v1.5

	DebugPrint(4, "set bw valye to 0x%x\n", Filter_bw_control_bit);

	I2cTransferW1(0x0b, RD5812_I2C_ADDR, Filter_bw_control_bit);
	// set Filter bandwidth end

	I2cTransferW1(0x04, RD5812_I2C_ADDR, 0xc3); //add by rda 2011.8.9,RXON = 0 ,rxon=1,normal working
	I2cTransferW1(0x2b, RD5812_I2C_ADDR, 0x97); //clk_interface_27m=1  add by rda 2012.1.12
	CyU3PThreadSleep(5);						//Wait 5ms;

	return 1;
}

void RDA5815Shutdown()
{
	// Chip register soft reset
	DebugPrint(4, "RDA5815Shutdown\n");
	I2cTransferW1(0x04, RD5812_I2C_ADDR, 0x00);
	refclk = 0;
}
