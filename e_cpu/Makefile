#
# assembler for the embedded processor
#
# Copyright (c) 2013-2026 John Seamons, ZL4VO/KF6VO

KIWI_SRC = ..

include $(KIWI_SRC)/Makefile.comp.inc

BUILD_DIR := ../../build
TOOLS_DIR := $(BUILD_DIR)/tools
GEN_DIR := $(BUILD_DIR)/gen

CFLAGS = -I. -I./asm -I.. -I$(KIWI_SRC) -D_GNU_SOURCE -g 
ifeq ($(ASAN),1)
    CFLAGS += -fsanitize=address
endif

ASM = $(TOOLS_DIR)/asm
ASM_SRC = 
ASM_INCLUDES = $(wildcard asm/*.h)
ASM_SOURCE = $(wildcard asm/*.cpp)

ASM_FLAGS = -2


all: $(ASM) no_gen

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(TOOLS_DIR): $(BUILD_DIR)
	@mkdir -p $(TOOLS_DIR)

$(GEN_DIR): $(BUILD_DIR)
	@mkdir -p $(GEN_DIR)

gen_includes debug2: $(ASM) $(ASM_SRC)
	-$(ASM) $(ASM_FLAGS) -o $(GEN_DIR) $(OTHER_DIR)
	sum $(GEN_DIR)/kiwi.aout

info: $(ASM) $(ASM_SRC)
	-$(ASM) -i $(ASM_FLAGS) -o $(GEN_DIR) $(OTHER_DIR)
	sum $(GEN_DIR)/kiwi.aout

gen_other: $(ASM) $(ASM_SRC)
	-$(ASM) $(ASM_FLAGS) -o $(GEN_DIR) -g $(OTHER_DIR)
	sum $(GEN_DIR)/kiwi.aout

$(ASM): $(TOOLS_DIR) $(GEN_DIR) $(ASM_INCLUDES) $(ASM_SOURCE) Makefile
	$(CPP) $(CFLAGS) $(ASM_SOURCE) -o $@

# don't regenerate the .h/.vh files
no_gen: $(ASM) $(ASM_SRC)
	-$(ASM) -n $(ASM_FLAGS) -o $(GEN_DIR) $(OTHER_DIR)
	sum $(GEN_DIR)/kiwi.aout

debug: $(ASM) $(ASM_SRC)
	-$(ASM) -n $(ASM_FLAGS) -d -o $(GEN_DIR) $(OTHER_DIR)
	sum $(GEN_DIR)/kiwi.aout

binary: $(ASM) $(ASM_SRC)
	-$(ASM) -n $(ASM_FLAGS) -b -o $(GEN_DIR) $(OTHER_DIR)
	sum $(GEN_DIR)/kiwi.aout

binary2: $(ASM) $(ASM_SRC)
	-$(ASM) -n $(ASM_FLAGS) -b -a -o $(GEN_DIR) $(OTHER_DIR)
	sum $(GEN_DIR)/kiwi.aout

stat stats: $(ASM) $(ASM_SRC)
	-$(ASM) -n $(ASM_FLAGS) -s -o $(GEN_DIR) $(OTHER_DIR)

gdb: $(ASM) $(ASM_SRC)
	@echo "gdb: r -o ../../build/gen"
	gdb $(ASM)

clean:
	-rm -rf $(TOOLS_DIR)/asm* $(GEN_DIR)
