# Kitty DOOM Makefile
# Requires GNU make 3.81+

# Set default goal before including other makefiles
.DEFAULT_GOAL := all

# Include build utilities and external download rules
include mk/common.mk
include mk/external.mk

# Directories and output
OUT := build
TARGET := $(OUT)/kitty-doom

# Source files
SRCS := src/input.c src/main.c src/render.c

# Object files (placed in build directory)
OBJS := $(patsubst src/%.c,$(OUT)/%.o,$(SRCS))

# Dependency files
DEPS := $(OBJS:.o=.d)

# Compiler and flags
CC := cc
CFLAGS := -std=gnu99 -Wall -Wextra -O2 -g -Isrc -MMD -MP
LDLIBS := -lpthread

# Check if compiler supports a specific flag
check_flag = $(shell $(CC) $(1) -E -xc /dev/null > /dev/null 2>&1 && echo $(1))

# Warning suppression candidates for PureDOOM (third-party code in main.c)
PUREDOOM_CFLAGS_TO_CHECK := \
    -Wno-parentheses \
    -Wno-enum-compare \
    -Wno-deprecated-non-prototype \
    -Wno-unused-parameter \
    -Wno-unused-variable \
    -Wno-unused-but-set-variable \
    -Wno-unused-but-set-parameter \
    -Wno-sign-compare \
    -Wno-missing-field-initializers \
    -Wno-unknown-pragmas \
    -Wno-sometimes-uninitialized \
    -Wno-unknown-warning-option \
    -Wno-string-concatenation \
    -Wno-enum-conversion \
    -Wno-implicit-fallthrough \
    -Wno-dangling-pointer \
    -Wno-maybe-uninitialized

# Only add flags that the compiler supports
PUREDOOM_CFLAGS := $(foreach flag,$(PUREDOOM_CFLAGS_TO_CHECK),$(call check_flag,$(flag)))

# Default target
.PHONY: all
all: $(TARGET) $(DOOM1_WAD)

# Download game assets (DOOM1.WAD and PureDOOM.h)
.PHONY: download-assets
download-assets: $(DOOM1_WAD) $(PUREDOOM_HEADER)

# Run the game
.PHONY: run
run: $(TARGET) $(DOOM1_WAD)
	@$(TARGET)

# Link binary
$(TARGET): $(OBJS) | $(OUT)
	$(VECHO) "  LD\t$@\n"
	$(Q)$(CC) -o $@ $^ $(LDLIBS)

# Compile source files (depends on PureDOOM.h)
$(OUT)/%.o: src/%.c $(PUREDOOM_HEADER) | $(OUT)
	$(VECHO) "  CC\t$@\n"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

# Special rule for main.c with PureDOOM warning suppression
$(OUT)/main.o: src/main.c $(PUREDOOM_HEADER) | $(OUT)
	$(VECHO) "  CC\t$@\n"
	$(Q)$(CC) $(CFLAGS) $(PUREDOOM_CFLAGS) -c -o $@ $<

# Create build directory
$(OUT):
	$(Q)mkdir -p $(OUT)

# Clean build artifacts
.PHONY: clean
clean:
	$(VECHO) "  CLEAN\t\t$(OUT)\n"
	$(Q)rm -rf $(OUT)

# Clean everything including downloaded files
.PHONY: distclean
distclean: clean
	$(Q)$(MAKE) -s clean-external

# Include dependency files
-include $(DEPS)
