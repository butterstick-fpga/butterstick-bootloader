include ../include/generated/variables.mak
include $(SOC_DIRECTORY)/software/common.mak

ifeq ($(CPU),blackparrot)
BP_LIBS =  -L$(BP_LITEX_SOFTWARE)
BP_FLAGS = -lgcc
endif

CFLAGS += -I$(FW_DIRECTORY)/include
CFLAGS += -DI2C_FREQ_HZ=400

TINYUSB_DIR := $(FW_DIRECTORY)/deps/tinyusb

# $(TINYUSB_DIR)/hw/bsp/luna_eptri/luna_eptri.c 
TINYUSB_SRC := 	$(TINYUSB_DIR)/src/portable/luna/eptri/dcd_eptri.c \
				$(TINYUSB_DIR)/src/tusb.c \
				$(TINYUSB_DIR)/src/device/usbd.c \
				$(TINYUSB_DIR)/src/device/usbd_control.c \
				$(TINYUSB_DIR)/src/common/tusb_fifo.c \
				$(TINYUSB_DIR)/src/class/dfu/dfu_device.c 

TINYUSB_OBJ := $(notdir $(TINYUSB_SRC:.c=.o))

CFLAGS += 	-I$(TINYUSB_DIR)/hw/bsp/luna_eptri/include \
			-I$(TINYUSB_DIR)/hw/bsp/luna_eptri \
			-Isrc -I$(TINYUSB_DIR)/hw \
			-I$(TINYUSB_DIR)/src \
			-DCFG_TUSB_MCU=OPT_MCU_LUNA_EPTRI \
			-fdata-sections -ffunction-sections -fsingle-precision-constant -fno-strict-aliasing \
			-DCFG_TUSB_DEBUG=1

vpath %.c $(dir $(TINYUSB_SRC)) $(FW_DIRECTORY)

OBJECTS =  	crt0.o \
			main.o	\
			sleep.o \
			flash.o \
			usb_descriptors.o 	
	   		

OBJECTS += $(TINYUSB_OBJ)




all: oc-fw.bin
	$(PYTHON) -m litex.soc.software.memusage oc-fw.elf $(CURDIR)/../include/generated/regions.ld $(TRIPLE)

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@
ifneq ($(OS),Windows_NT)
	chmod -x $@
endif
ifeq ($(CPUENDIANNESS),little)
	$(PYTHON) -m litex.soc.software.mkmscimg $@ --little
else
	$(PYTHON) -m litex.soc.software.mkmscimg $@
endif

oc-fw.elf: ../libcompiler_rt/libcompiler_rt.a \
	../libbase/libbase-nofloat.a \
	../liblitedram/liblitedram.a \
	../liblitespi/liblitespi.a \
	$(OBJECTS)
	$(LD) $(LDFLAGS) -L$(BUILDINC_DIRECTORY)/generated -T $(FW_DIRECTORY)/linker.ld -N -o $@ \
		$(OBJECTS) \
		-L../libcompiler_rt \
		-L../liblitedram \
		-L../libbase \
		-L../liblitespi \
		-llitedram -lbase-nofloat -lcompiler_rt \
		-Map=fw.map, -gc-sections
		
ifneq ($(OS),Windows_NT)
	chmod -x $@
endif

# pull in dependency info for *existing* .o files
-include $(OBJECTS:.o=.d)

%.o: %.c
	$(compile)

%.o:  $(FW_DIRECTORY)/%.S
	$(assemble)


clean:
	$(RM) $(OBJECTS) oc-fw.elf oc-fw.bin .*~ *~

.PHONY: all clean