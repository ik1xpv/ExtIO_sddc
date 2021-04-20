#pragma once


// bbrf103
void bbrf103_GpioSet(uint32_t mdata);
void bbrf103_GpioInitialize();

// hf103
void hf103_GpioSet(uint32_t mdata);
void hf103_GpioInitialize();
void hf103_SetAttenuator(uint8_t value);

// rx888
void rx888_GpioSet(uint32_t mdata);
void rx888_GpioInitialize();

// rx888r2
void rx888r2_GpioSet(uint32_t mdata);
void rx888r2_GpioInitialize();
void rx888r2_SetAttenuator(uint8_t value);
void rx888r2_SetGain(uint8_t value);

// rx888r3
void rx888r3_GpioSet(uint32_t mdata);
void rx888r3_GpioInitialize();
void rx888r3_SetAttenuator(uint8_t value);
void rx888r3_SetGain(uint8_t value);
int  rx888r3_preselect(uint32_t data);

// rx999
void rx999_GpioSet(uint32_t mdata);
void rx999_GpioInitialize();
void rx999_SetAttenuator(uint8_t value);
void rx999_SetGain(uint8_t value);
int rx999_preselect(uint32_t data);