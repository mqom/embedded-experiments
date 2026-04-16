### MQOM source related elements
MQOM2_FOLDER=mqom_base
ifeq ($(VERIFY_STREAM_TEST),1)
MQOM2_FOLDER=mqom_verifstream_presign
endif
ifeq ($(PRESIGN_TEST),1)
MQOM2_FOLDER=mqom_verifstream_presign
endif
ifeq ($(ONETREE_TEST),1)
MQOM2_FOLDER=mqom_onetree
endif
MQOM2_RELATIVE_FOLDER=../

MQOM2_OPTIONS?=MQOM2_VARIANT=cat1-gf16-fast-r5

### Embedded related elements
CROSS_CC ?= arm-none-eabi-gcc
CROSS_AR ?= arm-none-eabi-ar
CROSS_RANLIB ?= arm-none-eabi-ranlib

ifeq ($(MAT_MULT_TEST),1)
  EXTRA_SRC=benchmark/timing.c tests/matmul/bitsliced.c tests/matmul/bitsliced_cond.c tests/matmul/bitsliced_composite.c tests/matmul/bitsliced_composite_cond.c tests/matmul/test_matmul.c $(MQOM2_RELATIVE_FOLDER)/embedded_CM4/platform.c
else
  ifeq ($(RIJNDAEL_TEST),1)
    EXTRA_CFLAGS += -DEXTERNAL_HAL_GET_CYCLES -DNUM_TESTS_CYCLES=100
    EXTRA_SRC=benchmark/timing.c  rijndael/tests/test_rijndael.c $(MQOM2_RELATIVE_FOLDER)/embedded_CM4/platform.c $(MQOM2_RELATIVE_FOLDER)/embedded_CM4/mqom2_embedded_aes_hw_override.c
  else
    ifeq ($(VERIFY_STREAM_TEST),1)
      EXTRA_SRC=benchmark/timing.c $(MQOM2_RELATIVE_FOLDER)/embedded_CM4/mqom2_embedded_tests_streaming_verif.c $(MQOM2_RELATIVE_FOLDER)/embedded_CM4/platform.c $(MQOM2_RELATIVE_FOLDER)/embedded_CM4/mqom2_embedded_aes_hw_override.c
    else
      ifeq ($(PRESIGN_TEST),1)
        EXTRA_SRC=benchmark/timing.c $(MQOM2_RELATIVE_FOLDER)/embedded_CM4/mqom2_embedded_tests_presign.c $(MQOM2_RELATIVE_FOLDER)/embedded_CM4/platform.c $(MQOM2_RELATIVE_FOLDER)/embedded_CM4/mqom2_embedded_aes_hw_override.c
      else
        EXTRA_SRC=benchmark/timing.c $(MQOM2_RELATIVE_FOLDER)/embedded_CM4/mqom2_embedded_tests.c $(MQOM2_RELATIVE_FOLDER)/embedded_CM4/platform.c $(MQOM2_RELATIVE_FOLDER)/embedded_CM4/mqom2_embedded_aes_hw_override.c
      endif
    endif
  endif
endif

# Embedded related stuff
EMBEDDED = embedded_CM4
MCU_CFLAGS=-march=armv7e-m -mtune=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16
MCU_CFLAGS+=-DCURRENT_BOARD=$(BOARD)
# For garbage collecting unused stuff
MCU_CFLAGS+=-fdata-sections -ffunction-sections
EXTRA_CFLAGS += $(MCU_CFLAGS) -DBARE_METAL_KAT_TESTS -I.
EXTERNAL_OBJS_EMBEDDED_=$(shell cd $(MQOM2_FOLDER) && CC="$(CROSS_CC)" AR="$(CROSS_AR)" RANLIB="$(CROSS_RANLIB)" EXTRA_SRC="$(EXTRA_SRC)" EXTRA_CFLAGS="$(EXTRA_CFLAGS)" $(MQOM2_OPTIONS) make --no-print-directory print_objects)
# Generate the object
EXTERNAL_OBJS_EMBEDDED=$(foreach OBJ, $(EXTERNAL_OBJS_EMBEDDED_),../$(MQOM2_FOLDER)/$(OBJ))
# Add bare metal KAT self-tests
EXTRA_SRC += $(MQOM2_RELATIVE_FOLDER)/KAT_ref_tests/KAT_tests.c

ifeq ($(GEN_STATIC_STACK_ANALYSIS),1)
MCU_CFLAGS += -fstack-usage -fdump-ipa-cgraph
endif

# Default to board nucleol4r5zi if the user did not choose one
BOARD ?= nucleol4r5zi

# Adapt stack spraying depending on the board (and SRAM size)
ifeq ($(BOARD),nucleol4r5zi)
  ifeq ($(VERIFY_STREAM_TEST),1)
    # Because of high heap usage, limit stack usage measurement when testing verify streaming
    MCU_CFLAGS += -DMAX_SPRAY_STACK=100000
  else
    MCU_CFLAGS += -DMAX_SPRAY_STACK=450000
  endif
else
  MCU_CFLAGS += -DMAX_SPRAY_STACK=50000
endif

# The board has a hardware AES
ifeq ($(BOARD),leia)
  MCU_CFLAGS += -DBOARD_HAS_HW_AES
endif

# Allow to override the weak symbols for MQOM low-level API
MQOM2_OPTIONS += USE_WEAK_LOW_LEVEL_API=1

# Compile the MQOM objects
# We must have fetched MQOM upstream sources first
objects:
	cd $(MQOM2_FOLDER) && CC="$(CROSS_CC)" AR="$(CROSS_AR)" RANLIB="$(CROSS_RANLIB)" EXTRA_CFLAGS="$(EXTRA_CFLAGS)" BOARD=$(BOARD) EXTRA_SRC="$(EXTRA_SRC)" $(MQOM2_OPTIONS) make

# Compile the minimal firmware for CM4
firmware: objects
	cd $(EMBEDDED) && make clean && CC="$(CROSS_CC)" AR="$(CROSS_AR)" RANLIB="$(CROSS_RANLIB)" BOARD=$(BOARD) EXTERNAL_OBJS="$(EXTERNAL_OBJS_EMBEDDED)" make

ocd_reset:
ifneq ($(BOARD),)
	cd $(EMBEDDED) && make ocd_reset
else
	@echo "[-] Error: target not available for local compilation ..."
endif

ocd_flash:
ifneq ($(BOARD),)
	cd $(EMBEDDED) && make ocd_flash
else
	@echo "[-] Error: target not available for local compilation ..."
endif

reset:
ifneq ($(BOARD),)
	cd $(EMBEDDED) && make reset
else
	@echo "[-] Error: target not available for local compilation ..."
endif

flash:
ifneq ($(BOARD),)
	cd $(EMBEDDED) && make flash
else
	@echo "[-] Error: target not available for local compilation ..."
endif

all: objects

IMAGE_NAME = mqom-arm-dev-env
.PHONY: dev-shell dev-shell-usb
dev-shell:
	@echo "Building Docker image..."
	@docker build -t $(IMAGE_NAME) --build-arg HOST_UID=$$(id -u) --build-arg HOST_GID=$$(id -g) -f Docker/Dockerfile .
	@echo "Launching dev environment"
	@docker run -it --rm -v $(PWD):/home/app $(IMAGE_NAME)

dev-shell-usb:
	@echo "Checking host OS compatibility wit USB sharing..."
	@if [ "$$(uname -s)" = "Darwin" ]; then \
		echo "Error: macOS detected, it does not support Docker USB sharing" ; \
		echo "  Flash from macOS with 'brew install stlink'." ; \
		exit 1 ; \
	fi
	@if uname -r | grep -qiE "microsoft|wsl"; then \
		echo "Error: Windows (WSL) detected, it does not support Docker USB sharing" ; \
		echo "  Flash from Windows or use 'usbipd-win'." ; \
		exit 1 ; \
	fi
	@printf "WARNING: to share USB, docker will run with '--privileged', use at your own risks.\nDo you still want to continue? [y/N] " ; \
	read ans ; \
	if [ "$$ans" != "y" ] && [ "$$ans" != "Y" ]; then \
		echo "Aborting..." ; \
		exit 1 ; \
	fi
	@echo "Building Docker image..."
	@docker build -t $(IMAGE_NAME) --build-arg HOST_UID=$$(id -u) --build-arg HOST_GID=$$(id -g) -f Docker/Dockerfile .
	@echo "Launching dev environment with USB support"
	@docker run -it --rm \
		--privileged \
		-v /dev/bus/usb:/dev/bus/usb \
		-v $(PWD):/home/app \
		$(IMAGE_NAME)

clean:
	cd mqom_base && make clean
	cd mqom_verifstream_presign && make clean
	cd mqom_onetree && make clean
	rm -f embedded_CM4/mqom2_embedded_tests.o embedded_CM4/mqom2_embedded_tests_streaming_verif.o embedded_CM4/mqom2_embedded_tests_presign.o KAT_ref_tests/KAT_tests.o ./embedded_CM4/mqom2_embedded_aes_hw_override.o ./embedded_CM4/platform.o
	find . -name "*.su" -type f -delete
	find . -name "*.cgraph" -type f -delete
	cd $(EMBEDDED) && make clean

distclean: clean
	cd $(EMBEDDED) && make cleanall
	find . -name "*.o" -type f -delete
