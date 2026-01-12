BOARD_SET = 0
# Default to board nucleol4r5zi if the user did not choose one
BOARD ?= nucleol4r5zi

# MCU selection
ifneq ($(MCU),)
SOFT_FLOAT_FLAGS ?= -mfloat-abi=soft
HARD_FLOAT_FLAGS ?= -mfloat-abi=hard -mfpu=fpv4-sp-d16
FLOAT_FLAGS = $(SOFT_FLOAT_FLAGS)
else
# Default to Cortex-M4
MCU=cortex-m4
SOFT_FLOAT_FLAGS ?= -mfloat-abi=soft
HARD_FLOAT_FLAGS ?= -mfloat-abi=hard -mfpu=fpv4-sp-d16
FLOAT_FLAGS = $(HARD_FLOAT_FLAGS)
endif

####### STM32Discovery F407
ifeq ($(BOARD), stm32discovery)
OTHER_OPT = "-D__weak=__attribute__((weak))" "-D__packed=__attribute__((__packed__))"
CPU = -mthumb -mcpu=$(MCU) $(FLOAT_FLAGS)
SYSTEM = arm-none-eabi
CFLAGS  = $(CPU) $(OTHER_OPT) -DCURRENT_BOARD="stm32discovery" -Wall -fno-common -fno-strict-aliasing -Os $(INCLUDES) -g -Wfatal-errors -g -fdata-sections -ffunction-sections
BOARD_SET=1
CFLAGS += -DSTM32F407VG -DSTM32F4 -Ilibopencm3/include/ -Isrc/hal
LDSCRIPT += boards/stm32discoveryf407.ld
LDFLAGS += -Wl,--gc-sections,-Map=main.map,-cref -T $(LDSCRIPT) $(CPU)
LIBS = libopencm3/lib/libopencm3_stm32f4.a
LIBS_BUILD_CMD = cd libopencm3 && make FP_FLAGS="$(FLOAT_FLAGS)" TARGETS='stm32/f4'
BOARD_OCD_CFG=boards/stm32f4discovery.cfg
endif
####### NUCLEO-L4R5ZI
ifeq ($(BOARD), nucleol4r5zi)
OTHER_OPT = "-D__weak=__attribute__((weak))" "-D__packed=__attribute__((__packed__))"
CPU = -mthumb -mcpu=$(MCU) $(FLOAT_FLAGS)
SYSTEM = arm-none-eabi
CFLAGS  = $(CPU) $(OTHER_OPT) -DCURRENT_BOARD="nucleol4r5zi" -Wall -fno-common -fno-strict-aliasing -Os $(INCLUDES) -g -Wfatal-errors -g -fdata-sections -ffunction-sections
BOARD_SET=1
CFLAGS += -DSTM32L4R5ZI -DSTM32L4 -Ilibopencm3/include/ -Isrc/hal
LDSCRIPT += boards/nucleol4r5zi.ld
LDFLAGS += -Wl,--gc-sections,-Map=main.map,-cref -T $(LDSCRIPT) $(CPU)
LIBS = libopencm3/lib/libopencm3_stm32l4.a
LIBS_BUILD_CMD = cd libopencm3 && make FP_FLAGS="$(FLOAT_FLAGS)" TARGETS='stm32/l4'
BOARD_OCD_CFG=boards/nucleol4r5zi.cfg
endif
####### LEIA F437
ifeq ($(BOARD), leia)
OTHER_OPT = "-D__weak=__attribute__((weak))" "-D__packed=__attribute__((__packed__))"
CPU = -mthumb -mcpu=$(MCU) $(FLOAT_FLAGS)
SYSTEM = arm-none-eabi
CFLAGS  = $(CPU) $(OTHER_OPT) -DCURRENT_BOARD="leia" -Wall -fno-common -fno-strict-aliasing -Os $(INCLUDES) -g -Wfatal-errors -g -fdata-sections -ffunction-sections
BOARD_SET=1
CFLAGS += -DSTM32F437 -DSTM32F4 -Ilibopencm3/include/ -Isrc/hal
LDSCRIPT += boards/stm32discoveryf407.ld
LDFLAGS += -Wl,--gc-sections,-Map=main.map,-cref -T $(LDSCRIPT) $(CPU)
LIBS = libopencm3/lib/libopencm3_stm32f4.a
LIBS_BUILD_CMD = cd libopencm3 && make FP_FLAGS="$(FLOAT_FLAGS)" TARGETS='stm32/f4'
BOARD_OCD_CFG=boards/stm32f4discovery.cfg
endif

ifneq ($(BOARD), stm32discovery)
  ifneq ($(BOARD), nucleol4r5zi)
    ifneq ($(BOARD), leia)
      $(error Error, board $(BOARD) is not known ... Please select between nucleol4r5zi and stm32discovery!)
    endif
  endif
endif

ASFLAGS = $(CFLAGS) -x assembler-with-cpp
ARFLAGS = cr
OBJCOPYFLAGS = -Obinary
OBJDUMPFLAGS = -S
# Add no sys spec for the stdio wiring
#LDFLAGS += -static -specs=nano.specs -specs=nosys.specs
LDFLAGS += -static -specs=nosys.specs -lm -lgcc

# Force Link Time Optimizations
# XXX: warning, this can be agressive and yield wrong results
ifeq ($(FLTO),1)
CFLAGS += -flto
LDFLAGS += -flto
endif

ifeq ($(WNOERROR), 1)
  # Sometimes "-Werror" might be too much, this can be overriden
 CFLAGS := $(subst -Werror,,$(CFLAGS))
endif

CFLAGS += $(EXTRA_CFLAGS)

# Add user defined external libs
LIBS += $(USER_LIBS)

CC      = $(SYSTEM)-gcc
CCDEP   = $(SYSTEM)-gcc
LD      = $(SYSTEM)-gcc
AR      = $(SYSTEM)-ar
AS      = $(SYSTEM)-gcc
OBJCOPY = $(SYSTEM)-objcopy
OBJDUMP = $(SYSTEM)-objdump
GDB     = $(SYSTEM)-gdb
SIZE    = $(SYSTEM)-size
OCD     = openocd \
                -f $(BOARD_OCD_CFG)
STFLASH = st-flash

## Source directories: edit to add other folders
SRCDIR := src/ src/hal
# Add external USER sources
SRCDIR += $(USER_SRC)

OBJS = $(foreach DIR,$(SRCDIR),$(sort \
 $(patsubst %.c,%.o,$(wildcard $(DIR)/*.c)) \
 $(patsubst %.s,%.o,$(wildcard $(DIR)/*.s)) \
 $(patsubst %.S,%.o,$(wildcard $(DIR)/*.S))))

ifneq ($(BOARD_SET),1)
error:
	@echo "Error: please set the board with environment variable to either BOARD=stm32discovery or BOARD=nucleol4r5zi ..."

all: error

.PHONY: error
else
main.bin: main.elf
	$(OBJCOPY) $(OBJCOPYFLAGS) main.elf main.bin
	$(OBJDUMP) $(OBJDUMPFLAGS) main.elf > main.list
	$(SIZE) main.elf


main.elf: $(LIBS) $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBS) $(EXTERNAL_OBJS)

$(LIBS):
	@echo "==== Compiling libopencm3 for $(BOARD)"
	$(LIBS_BUILD_CMD)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.s.o:
	$(AS) $(ASFLAGS) -c -o $@ $<

ocd_reset:
	$(OCD) -c init -c "reset run" -c "adapter assert srst" -c shutdown
	#$(GDB) main.out <reset.gdb

ocd_flash: main.bin
	$(OCD) -c init -c "reset halt" \
		-c "flash write_image erase main.bin 0x08000000" \
		-c "reset run" \
		-c shutdown
reset:
	$(STFLASH) reset

flash: main.bin
	$(STFLASH) --reset write main.bin 0x08000000

clean:
	@rm -f $(OBJS) main.elf main.bin main.list main.map

cleanall:
	@cd libopencm3 && make clean
endif
