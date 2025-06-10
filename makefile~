# CodeVoid basic (C99 + ASM) Makefile

# Tools
ASM     = nasm
CC      = clang

# Project Structure
SRC     = src
BUILD   = build
TARGET  = $(BUILD)/main

# Flags
CFLAGS  = -std=c99 -Wall -Wextra -O2 -Wimplicit-function-declaration
LDFLAGS = -pthread -lm

# Sources
C_SOURCES   := $(shell find $(SRC) -name '*.c')
ASM_SOURCES := $(shell find $(SRC) -name '*.asm')

# Objects
C_OBJS      := $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(C_SOURCES))
ASM_OBJS    := $(patsubst $(SRC)/%.asm,$(BUILD)/%.asm.o,$(ASM_SOURCES))
OBJS        := $(C_OBJS) $(ASM_OBJS)

.PHONY: all clean run

# Default target
all: $(TARGET)

# Compile C files
$(BUILD)/%.o: $(SRC)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble ASM files
$(BUILD)/%.asm.o: $(SRC)/%.asm
	@mkdir -p $(dir $@)
	$(ASM) -f elf64 $< -o $@

# Link all object files
$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LDFLAGS)

# Run the output binary
run: $(TARGET)
	./$(TARGET)

# Clean build artifacts
clean:
	rm -rf $(BUILD)
