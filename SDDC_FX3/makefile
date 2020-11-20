## Copyright Cypress Semiconductor Corporation, 2010-2011,
## All Rights Reserved
## UNPUBLISHED, LICENSED SOFTWARE.
##
## CONFIDENTIAL AND PROPRIETARY INFORMATION
## WHICH IS THE PROPERTY OF CYPRESS.
##
## Use of this file is governed
## by the license agreement included in the file
##
##      <install>/license/license.txt
##
## where <install> is the Cypress software
## installation root directory path.
##

FX3FWROOT=../SDK
FX3PFWROOT=../SDK/u3p_firmware

MODULE = SDDC_FX3
CYCONFOPT=fx3_release

all:compile $(MODULE).img

include $(FX3FWROOT)/common/fx3_build_config.mak

SOURCE +=  DebugConsole.c  i2cmodule.c  R820T2.c  RunApplication.c  Si5351.c  StartStopApplication.c  StartUP.c  Support.c  USBdescriptor.c  USBhandler.c hf103.c bbrf103.c rx888.c

C_OBJECT=$(SOURCE:%.c=./%.o)
A_OBJECT=$(SOURCE_ASM:%.S=./%.o)

EXES = $(MODULE).$(EXEEXT)

$(MODULE).$(EXEEXT): $(A_OBJECT) $(C_OBJECT)
	$(LINK)

$(C_OBJECT) : %.o : %.c
	$(COMPILE)

$(A_OBJECT) : %.o : %.S
	$(ASSEMBLE)

./elf2img : $(FX3FWROOT)/util/elf2img.c
	gcc -o elf2img $<

clean:
	rm -f ./$(MODULE).$(EXEEXT)
	rm -f ./elf2img
	rm -f ./$(MODULE).img
	rm -f ./$(MODULE).map
	rm -f ./*.o

compile: $(C_OBJECT) $(A_OBJECT) $(EXES)

$(MODULE).img : $(MODULE).$(EXEEXT) ./elf2img
	./elf2img -i $< -o $@

#[]#
