CFLAGS ?= -g -O3 -march=native -mtune=native -Wall -Wextra -Werror

# Source files for Rindael
SRC_FILES = rijndael_ref.c rijndael_table.c rijndael_aes_ni.c rijndael_ct64.c

# Source files for the tests
TEST_SRC_FILES = tests/test_rijndael.c

# All the base objects to compile
OBJS   = $(patsubst %.c,%.o, $(filter %.c,$(SRC_FILES)))
OBJS  += $(patsubst %.s,%.o, $(filter %.s,$(SRC_FILES)))
OBJS  += $(patsubst %.S,%.o, $(filter %.S,$(SRC_FILES)))

# All the test objects to compile
TEST_OBJS   = $(patsubst %.c,%.o, $(filter %.c,$(TEST_SRC_FILES)))
TEST_OBJS  += $(patsubst %.s,%.o, $(filter %.s,$(TEST_SRC_FILES)))
TEST_OBJS  += $(patsubst %.S,%.o, $(filter %.S,$(TEST_SRC_FILES)))

ifeq ($(RIJNDAEL_OPT_ARMV7M),1)
  # Force RIJNDAEL optimized assembly usage where possible
  CFLAGS += -DRIJNDAEL_OPT_ARMV7M
  ASMFLAGS += -x assembler-with-cpp
  SRC_FILES += aes128_table_arvmv7m.s
  SRC_FILES += aes128_fixsliced_arvmv7m.s
  OBJS  += aes128_table_arvmv7m.o
  OBJS  += aes128_fixsliced_arvmv7m.o
endif

ifeq ($(RIJNDAEL_CT64_CT_KEYSCHED),1)
  # Force constant time key schedule for the bitsliced ct64 variant
  CFLAGS += -DRIJNDAEL_CT64_CT_KEYSCHED
endif

CFLAGS += $(EXTRA_CFLAGS)

# By default, compile with constant time bitslice Rijndael
# If asked to, compile to slow constant time ref, non-constant time table based
# or to AES-NI accelerated based
CFLAGS += -DRIJNDAEL_BITSLICE
ifeq ($(RIJNDAEL_CONSTANT_TIME_REF),1)
  CFLAGS := $(subst -DRIJNDAEL_BITSLICE,-DRIJNDAEL_CONSTANT_TIME_REF,$(CFLAGS))
endif
ifeq ($(RIJNDAEL_TABLE),1)
  CFLAGS := $(subst -DRIJNDAEL_BITSLICE,-DRIJNDAEL_TABLE,$(CFLAGS))
  CFLAGS += -Wno-array-bounds -Wno-stringop-overflow
endif
ifeq ($(RIJNDAEL_AES_NI),1)
  CFLAGS := $(subst -DRIJNDAEL_BITSLICE,-DRIJNDAEL_AES_NI,$(CFLAGS))
endif

ifeq ($(NO_CYCLES_COUNT), 1)
  CFLAGS += -DNO_CYCLES_COUNT
endif

ifeq ($(WNOERROR), 1)
  # Sometimes "-Werror" might be too much, this can be overriden
  CFLAGS := $(subst -Werror,,$(CFLAGS))
endif

# Use the sanitizers
# XXX: warning, this *does not* work with the boards, only with local
# compilation 
ifeq ($(USE_SANITIZERS),1)
CFLAGS += -fsanitize=undefined -fsanitize=address -fsanitize=leak
# XXX: NOTE: we must remove the signed integers overflows from the
# sanitizers since we "abuse" them in the arithmetic library
CFLAGS += -fno-sanitize=signed-integer-overflow
endif

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.s.o:
	$(CC) $(CFLAGS) $(ASMFLAGS) -c -o $@ $<

all: $(OBJS)

test: $(OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(TEST_OBJS) -o test_rijndael

clean:
	@rm -f *.o tests/*.o test_rijndael
