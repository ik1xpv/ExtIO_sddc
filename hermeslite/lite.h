#pragma once

#define MAX_RECEIVERS 16
#define SERVICE_PORT 1024
#define MAX_DATA_SIZE 2048
#define HERMESLITE 0x02
#define FIRMWARE_VERSION 0x28 //firmware version 4.0 (=0x28)
#define TIMEOUT_MS 100
#define HANDLER_STEADY_TIME_US 5000

// port definitions from host
#define GENERAL_REGISTERS_FROM_HOST_PORT 1024
#define RECEIVER_SPECIFIC_REGISTERS_FROM_HOST_PORT 1025
#define TRANSMITTER_SPECIFIC_REGISTERS_FROM_HOST_PORT 1026
#define HIGH_PRIORITY_FROM_HOST_PORT 1027
#define AUDIO_FROM_HOST_PORT 1028
#define TX_IQ_FROM_HOST_PORT 1029

// port definitions to host
#define COMMAND_RESPONCE_TO_HOST_PORT 1024
#define HIGH_PRIORITY_TO_HOST_PORT 1025
#define MIC_LINE_TO_HOST_PORT 1026
#define WIDE_BAND_TO_HOST_PORT 1027
#define RX_IQ_TO_HOST_PORT_0 1035