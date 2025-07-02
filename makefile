# ==========================
# 🛠️  ForgeWatch Makefile
# --------------------------
# Language: C99
# Purpose : Rebuild automation tool
# ==========================

# ==== Tools ====
CC      = clang

# ==== Directories ====
SRC     = src
BUILD   = build
BIN     = /usr/local/bin
TARGET  = $(BUILD)/forgewatch

# ==== Flags ====
CFLAGS  = -std=c99 -Wall -Wextra -O2 -Wimplicit-function-declaration
LDFLAGS = -pthread -lm

# ==== Source Files ====
C_SOURCES   := $(shell find $(SRC) -name '*.c')

# ==== Object Files ====
C_OBJS   := $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(C_SOURCES))
OBJS     := $(C_OBJS)

# ==== Phony Targets ====
.PHONY: all clean run install debug help windows

# ==== Default Target ====
all: $(TARGET)
	@echo "✅ Build complete."

# ==== Compile C Files ====
$(BUILD)/%.o: $(SRC)/%.c
	@mkdir -p $(dir $@)
	@echo "🔧 Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# ==== Link All Object Files ====
$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	@echo "🔗 Linking to create binary..."
	@$(CC) -o $@ $^ $(LDFLAGS)



windows: $(SRC)/main.c
	x86_64-w64-mingw32-gcc -o forgewatch.exe src/main.c -luser32 -lkernel32
# ==== Run the Built Binary ====
run: $(TARGET)
	@echo "🚀 Running forgewatch..."
	@./$(TARGET)

# ==== Debug Build ====
debug: CFLAGS += -g -DDEBUG
debug: clean all
	@echo "🐞 Debug build complete."

# ==== Install to System PATH ====
install: $(TARGET)
	@echo "📦 Installing to $(BIN)..."
	@sudo install -m 755 $(TARGET) $(BIN)/forgewatch
	@echo "✅ Installed as 'forgewatch'."

# ==== Clean All Build Artifacts ====
clean:
	@echo "🧹 Cleaning build directory..."
	@rm -rf $(BUILD)

# ==== Help ====
help:
	@echo "🛠️  ForgeWatch Build System"
	@echo "----------------------------"
	@echo "make           - Build release version"
	@echo "make run       - Build and run the binary"
	@echo "make debug     - Build with debug info"
	@echo "make install   - Install to system bin directory ($(BIN))"
	@echo "make clean     - Remove build files"
