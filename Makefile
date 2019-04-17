OPENCM3DIR  = ./pqm4/libopencm3
OPENCM3NAME = opencm3_stm32f4
OPENCM3FILE = $(OPENCM3DIR)/lib/lib$(OPENCM3NAME).a
LDSCRIPT    = $(OPENCM3DIR)/lib/stm32/f4/stm32f405x6.ld

PREFIX     ?= arm-none-eabi
CC          = $(PREFIX)-gcc
LD          = $(PREFIX)-gcc
OBJCOPY     = $(PREFIX)-objcopy
OBJDUMP     = $(PREFIX)-objdump
GDB         = $(PREFIX)-gdb

ARCH_FLAGS  = -mthumb -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16
DEFINES     = -DSTM32F4
OBJS        = obj/stm32f4_wrapper.o obj/fips202.o obj/keccakf1600.o
RANDOMBYTES = obj/randombytes.o

CFLAGS     += -O3 \
              -Wall -Wextra -Wimplicit-function-declaration \
              -Wredundant-decls -Wmissing-prototypes -Wstrict-prototypes \
              -Wundef -Wshadow \
              -I$(OPENCM3DIR)/include \
              -fno-common $(ARCH_FLAGS) -MD $(DEFINES)
LDFLAGS    += --static -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group \
              -T$(LDSCRIPT) -nostartfiles -Wl,--gc-sections \
               $(ARCH_FLAGS) -L$(OPENCM3DIR)/lib

CC_HOST    = gcc
LD_HOST    = gcc

CFLAGS_HOST = -O3 -Wall -Wextra -Wpedantic
LDFLAGS_HOST =

OBJS_HOST  = obj-host/fips202.o obj-host/keccakf1600.o

KEMS=$(wildcard pqm4/crypto_kem/*/*)

# filter the targets that cannot be built on the M4
define filter_m4ignore
    $(filter-out $(patsubst %/.m4ignore,%,$(wildcard $(addsuffix /.m4ignore, $(1)))),$(1))
endef

# on the host, we are only interested in the reference implementations
KEMS_HOST=$(wildcard pqm4/crypto_kem/*/ref)
# on the M4, anything that compiles is a valid target
KEMS_M4=$(call filter_m4ignore, $(KEMS))


KEMSCPA=$(patsubst %,bin/%,$(patsubst pqm4_%,%_cpa.bin,$(subst /,_,$(KEMS_M4))))


KEMSCPA_HOST=$(patsubst %,bin-host/%,$(patsubst pqm4_%,%_cpa_host,$(subst /,_,$(KEMS_HOST))))

LIBS_M4=$(addsuffix /libpqm4.a,$(KEMS_M4))
LIBS_HOST=$(addsuffix /libpqhost.a,$(KEMS_HOST))

OWNDIR=$(shell pwd)
INCPATH=$(OWNDIR)/SCA/common


all: cpa cpahost



cpa: libs $(KEMSCPA)
cpahost: libs $(KEMSCPA_HOST)
libs: $(LIBS_M4) $(LIBS_HOST)

.PHONY: force
export INCPATH

# TODO: currently the libraries are not rebuilt when a file changes in a scheme
#  but specifying `libs` as as a dependency of the .elfs causes everything to
#  be constantly rebuilt. Suggestions welcome how to fix this nicely.
# Currently the workaround is to `make clean` after modifying schemes.

$(LIBS_M4): force
	make -C $(dir $@) libpqm4.a

$(LIBS_HOST): force
	make -C $(dir $@) libpqhost.a

bin-host/crypto_kem_%:  $(OBJS_HOST) obj-host/$(patsubst %,crypto_kem_%.o,%)
	mkdir -p bin-host
	$(LD_HOST) -o $@ \
	$(patsubst bin-host/%,obj-host/%.o,$@) \
	$(patsubst %cpa/host,pqm4/%libpqhost.a,$(patsubst bin-host/crypto/kem%,crypto_kem%,$(subst _,/,$@))) \
	$(OBJS_HOST) $(LDFLAGS_HOST) -lm

bin/%.bin: elf/%.elf
	mkdir -p bin
	$(OBJCOPY) -Obinary $^ $@

elf/crypto_kem_%_cpa.elf: $(OBJS) $(LDSCRIPT) obj/$(patsubst %,crypto_kem_%_cpa.o,%) $(OPENCM3FILE)
	mkdir -p elf
	$(LD) -o $@ \
	$(patsubst elf/%.elf,obj/%.o,$@) \
	$(patsubst %cpa.elf,pqm4/%libpqm4.a,$(patsubst elf/crypto/kem%,crypto_kem%,$(subst _,/,$@))) \
	$(OBJS) $(LDFLAGS) -l$(OPENCM3NAME) -lm



obj/crypto_kem_%_cpa.o: SCA/lac_cpa.c $(patsubst %,%/api.h,$(patsubst %,pqm4/crypto_kem/%,$(subst _,/,$%)))
	mkdir -p obj
	$(CC) $(CFLAGS) -o $@ -c $< \
	-I$(patsubst %cpa.o,pqm4/%,$(patsubst obj/%,%,$(subst crypto/kem,crypto_kem,$(subst _,/,$@)))) \
	-I./SCA/common/


obj-host/crypto_kem_%_cpa_host.o: SCA/lac_cpa_host.c $(patsubst %,%/api.h,$(patsubst %,pqm4/crypto_kem/%,$(subst _,/,$%)))
	mkdir -p obj-host
	$(CC_HOST) $(CFLAGS_HOST) -o $@ -c $< \
	-I$(patsubst %cpa/host.o,pqm4/%,$(patsubst obj-host/%,%,$(subst crypto/kem,crypto_kem,$(subst _,/,$@)))) \
	-I./SCA/common/





obj/randombytes.o: SCA/common/randombytes.c
	mkdir -p obj
	$(CC) $(CFLAGS) -o $@ -c $^

obj/stm32f4_wrapper.o:  SCA/common/stm32f4_wrapper.c
	mkdir -p obj
	$(CC) $(CFLAGS) -o $@ -c $^

obj/fips202.o:  SCA/common/fips202.c
	mkdir -p obj
	$(CC) $(CFLAGS) -o $@ -c $^

obj/keccakf1600.o:  SCA/common/keccakf1600.S
	mkdir -p obj
	$(CC) $(CFLAGS) -o $@ -c $^

obj-host/%.o: SCA/common/%.c
	mkdir -p obj-host
	$(CC_HOST) $(CFLAGS_HOST) -o $@ -c $^

$(OPENCM3FILE):
	@if [ ! "`ls -A $(OPENCM3_DIR)`" ] ; then \
		printf "######## ERROR ########\n"; \
		printf "\tlibopencm3 is not initialized.\n"; \
		printf "\tPlease run (in the root directory):\n"; \
		printf "\t$$ git submodule init\n"; \
		printf "\t$$ git submodule update\n"; \
		printf "\tbefore running make.\n"; \
		printf "######## ERROR ########\n"; \
		exit 1; \
		fi
	make -C $(OPENCM3DIR)



.PHONY: clean libclean

clean:
	find . -name \*.o -type f -exec rm -f {} \;
	find . -name \*.d -type f -exec rm -f {} \;
	find pqm4/crypto_kem -name \*.a -type f -exec rm -f {} \;
	find pqm4/crypto_sign -name \*.a -type f -exec rm -f {} \;
	rm -rf elf/
	rm -rf bin/
	rm -rf bin-host/
	rm -rf obj/
	rm -rf obj-host/

libclean:
	make -C $(OPENCM3DIR) clean
