ifeq (,$(filter -j%,$(MAKEFLAGS)))
MAKEFLAGS += -j$(shell nproc)
endif
CC = gcc
# Added -Igui to ensure the compiler can find headers in the gui/ folder
CFLAGS = -std=c11 -O3 -march=native -flto=auto -fopenmp -funroll-loops -fomit-frame-pointer -DNDEBUG -Iinclude -Igui -Wall -Wextra -Wno-unused-parameter
DEPFLAGS = -MMD -MP
ASM = nasm
ASMFLAGS = -f elf64
TARGET = luna
OBJDIR = obj
BINDIR = bin
LIBDIR = lib
UNSAFE_RT_DIR = rust/unsafe_rt
UNSAFE_RT_SRCS := $(shell find $(UNSAFE_RT_DIR)/src -type f) $(UNSAFE_RT_DIR)/Cargo.toml
UNSAFE_RT_BUILD_LIB = $(UNSAFE_RT_DIR)/target/release/libluna_memory_rt.a
UNSAFE_RT_LIB = $(LIBDIR)/libluna_memory_rt.a
DATA_RT_DIR = rust/data_rt
DATA_RT_SRCS := $(shell find $(DATA_RT_DIR)/src -type f) $(DATA_RT_DIR)/Cargo.toml
DATA_RT_BUILD_LIB = $(DATA_RT_DIR)/target/release/libluna_data_rt.a
DATA_RT_LIB = $(LIBDIR)/libluna_data_rt.a
CARGO = env -u MAKEFLAGS -u MFLAGS cargo
ZIG = zig

# Source files
SRCS = src/lexer.c src/token.c src/util.c src/ast.c src/parser.c \
       src/interpreter.c src/value.c src/main.c src/math_lib.c \
       src/gc.c src/gc_visit.c \
       src/string_lib.c src/error.c src/time_lib.c src/vec_lib.c \
       src/env.c src/library.c src/file_lib.c src/list_lib.c \
       src/unsafe_runtime.c src/luna_runtime.c src/luna_test.c \
       src/sand_lib.c src/arena.c src/intern.c src/data_runtime.c \
       gui/gui_lib.c gui/gl_backend.c gui/audio_backend.c \
       gui/gl_backend_3d.c gui/gui_lib_3d.c

# Object files
OBJS = $(OBJDIR)/lexer.o $(OBJDIR)/token.o $(OBJDIR)/util.o \
       $(OBJDIR)/ast.o $(OBJDIR)/parser.o $(OBJDIR)/interpreter.o \
       $(OBJDIR)/value.o $(OBJDIR)/main.o $(OBJDIR)/math_lib.o \
       $(OBJDIR)/gc.o $(OBJDIR)/gc_visit.o \
       $(OBJDIR)/string_lib.o $(OBJDIR)/error.o $(OBJDIR)/time_lib.o \
       $(OBJDIR)/time.o $(OBJDIR)/vec_lib.o \
       $(OBJDIR)/env.o $(OBJDIR)/library.o $(OBJDIR)/file_lib.o \
       $(OBJDIR)/unsafe_runtime.o $(OBJDIR)/luna_runtime.o \
       $(OBJDIR)/luna_test.o \
       $(OBJDIR)/list_lib.o $(OBJDIR)/sand_lib.o $(OBJDIR)/arena.o \
       $(OBJDIR)/intern.o $(OBJDIR)/data_runtime.o $(OBJDIR)/gui_lib.o \
       $(OBJDIR)/gl_backend.o $(OBJDIR)/audio_backend.o \
       $(OBJDIR)/gl_backend_3d.o $(OBJDIR)/gui_lib_3d.o
DEPS = $(OBJS:.o=.d)

all: $(BINDIR)/$(TARGET)

# Create directories
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

$(LIBDIR):
	mkdir -p $(LIBDIR)

# Build the Rust unsafe runtime static library
$(UNSAFE_RT_BUILD_LIB): $(UNSAFE_RT_SRCS)
	@echo "==> Building Rust unsafe runtime..."
	cd $(UNSAFE_RT_DIR) && $(CARGO) build --release
	@echo "==> libluna_memory_rt.a ready"

$(UNSAFE_RT_LIB): $(UNSAFE_RT_BUILD_LIB) | $(LIBDIR)
	cp $(UNSAFE_RT_BUILD_LIB) $(UNSAFE_RT_LIB)
	@echo "==> Copied libluna_memory_rt.a to $(LIBDIR)/"

# Build the Rust structured-data runtime static library
$(DATA_RT_BUILD_LIB): $(DATA_RT_SRCS)
	@echo "==> Building Rust data runtime..."
	cd $(DATA_RT_DIR) && $(CARGO) build --release
	@echo "==> libluna_data_rt.a ready"

$(DATA_RT_LIB): $(DATA_RT_BUILD_LIB) | $(LIBDIR)
	cp $(DATA_RT_BUILD_LIB) $(DATA_RT_LIB)
	@echo "==> Copied libluna_data_rt.a to $(LIBDIR)/"

# Link the main interpreter
# Uses GLFW (static) + OpenGL + miniaudio + Rust runtimes
$(BINDIR)/$(TARGET): $(OBJS) $(UNSAFE_RT_LIB) $(DATA_RT_LIB) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ -L$(LIBDIR) \
		-lglfw3 -lGL -lm -lpthread -ldl -lrt -lX11 \
		-z noexecstack -fopenmp

# Compile source files from src/
$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

# Rules to compile source files from gui/
$(OBJDIR)/gui_lib.o: gui/gui_lib.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(OBJDIR)/gl_backend.o: gui/gl_backend.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(OBJDIR)/audio_backend.o: gui/audio_backend.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(OBJDIR)/gl_backend_3d.o: gui/gl_backend_3d.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(OBJDIR)/gui_lib_3d.o: gui/gui_lib_3d.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

# Compile assembly files
$(OBJDIR)/%.o: asm/%.asm | $(OBJDIR)
	$(ASM) $(ASMFLAGS) $< -o $@

# # nstall Luna to the system path
# install: $(BINDIR)/$(TARGET)
# 	@echo "Installing Luna to /usr/local/bin..."
# 	sudo cp $(BINDIR)/$(TARGET) /usr/local/bin/$(TARGET)
# 	@echo "Luna is now installed! Run it anywhere by typing 'luna'."

# Check for required system dependencies
check-deps:
	@echo "Checking dependencies for Luna..."
	@command -v gcc >/dev/null 2>&1 || echo "[ERROR] gcc not found"
	@command -v nasm >/dev/null 2>&1 || echo "[ERROR] nasm not found"
	@command -v cargo >/dev/null 2>&1 && echo "[OK] cargo found" || echo "[ERROR] cargo not found"
	@command -v $(ZIG) >/dev/null 2>&1 && echo "[OK] zig found" || echo "[ERROR] zig not found"
	@if [ -f "lib/libglfw3.a" ]; then echo "[OK] libglfw3.a found"; else echo "[ERROR] libglfw3.a missing in lib/"; fi
	@if [ -f "include/glfw3.h" ]; then echo "[OK] glfw3.h found"; else echo "[ERROR] glfw3.h missing in include/"; fi

# Helper for Linux Mint/Ubuntu to install system graphics libs
setup-ubuntu:
	@echo "Installing graphics dependencies..."
	sudo apt update
	sudo apt install -y build-essential nasm libx11-dev libxcursor-dev \
		libxinerama-dev libxrandr-dev libxi-dev libasound2-dev \
		mesa-common-dev libgl1-mesa-dev

test: $(BINDIR)/$(TARGET)
	@echo "==> Manual Check: test/test_core.lu"
	@./$(BINDIR)/$(TARGET) test/test_core.lu
	@echo ""
	@echo "==> Manual Check: test/test_math.lu"
	@./$(BINDIR)/$(TARGET) test/test_math.lu
	@echo ""
	@echo "==> Manual Check: test/test_functions.lu"
	@./$(BINDIR)/$(TARGET) test/test_functions.lu
	@echo ""
	@echo "==> Manual Check: test/test_vectors.lu"
	@./$(BINDIR)/$(TARGET) test/test_vectors.lu
	@echo ""
	@echo "==> Manual Check: test/test_strings.lu"
	@./$(BINDIR)/$(TARGET) test/test_strings.lu
	@echo ""
	@echo "==> Manual Check: test/test_file_io.lu"
	@./$(BINDIR)/$(TARGET) test/test_file_io.lu
	@echo ""
	@echo "==> Manual Check: test/test_bloc.lu"
	@./$(BINDIR)/$(TARGET) test/test_bloc.lu
	@echo ""
	@echo "==> Manual Check: test/test_box.lu"
	@./$(BINDIR)/$(TARGET) test/test_box.lu
	@echo ""
	@echo "==> Manual Check: test/test_template.lu"
	@./$(BINDIR)/$(TARGET) test/test_template.lu
	@echo ""
	@echo "==> Manual Check: test/test_unsafe.lu"
	@./$(BINDIR)/$(TARGET) test/test_unsafe.lu
	@echo ""

	@echo "==> Manual Check: test/balls.lu"
	@./$(BINDIR)/$(TARGET) test/balls.lu
	@echo ""
	@./test_runner.sh
	@$(CARGO) test --manifest-path rust/unsafe_rt/Cargo.toml
	@$(CARGO) test --manifest-path rust/data_rt/Cargo.toml
	@echo "All tests passed!"

test-rust-data:
	@$(CARGO) test --manifest-path rust/data_rt/Cargo.toml
	@echo "Rust data runtime tests passed."

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(BINDIR)
	cd $(UNSAFE_RT_DIR) && $(CARGO) clean
	cd $(DATA_RT_DIR) && $(CARGO) clean

clean-c:
	rm -rf $(OBJDIR) $(BINDIR)

-include $(DEPS)

# Run the interpreter on the main file
run: $(BINDIR)/$(TARGET)
	./$(BINDIR)/$(TARGET) main.lu   #Modify this with your file name to run it.

# Shortcut to run the REPL
repl: $(BINDIR)/$(TARGET)
	./$(BINDIR)/$(TARGET)

# Run the Car Game
run-cargame: $(BINDIR)/$(TARGET)
	./$(BINDIR)/$(TARGET) cargame/cargame.lu

run-music: $(BINDIR)/$(TARGET)
	./$(BINDIR)/$(TARGET) musicplayer/music.lu

# Generate IR (GIMPLE) representation
ir:
	@mkdir -p ir
	@for src in $(SRCS); do \
		base=$$(basename $$src .c); \
		echo "Generating GIMPLE IR for $$src..."; \
		$(CC) $(CFLAGS) -fdump-tree-gimple -c $$src -o /dev/null; \
	done
	@find . -name "*.gimple" -exec mv {} ir/ \;
	@echo "IR files generated in ir/ directory"

# Generate preprocessed source files
preprocess:
	@mkdir -p preprocessed
	@for src in $(SRCS); do \
		base=$$(basename $$src .c); \
		echo "Preprocessing $$src..."; \
		$(CC) $(CFLAGS) -E $$src -o preprocessed/$$base.i; \
	done
	@echo "Preprocessed files generated in preprocessed/ directory"

# Run Luna vs Zig Performance Comparison
run-comp: $(BINDIR)/$(TARGET)
	@chmod +x benchmark/compare.sh
	@+ZIG=$(ZIG) ./benchmark/compare.sh

test-gc: $(BINDIR)/$(TARGET)
	@chmod +x test_gc/run_gc_bench.sh
	@./test_gc/run_gc_bench.sh

test-gc-safety: $(BINDIR)/$(TARGET)
	@echo "==> GC stress+verify: test/test_gc_safety.lu"
	@env LUNA_GC_STRESS=1 LUNA_GC_VERIFY=1 ./$(BINDIR)/$(TARGET) test/test_gc_safety.lu
	@echo ""
	@echo "==> GC stress+verify: test/test_gc_large_safety.lu"
	@env LUNA_GC_STRESS=1 LUNA_GC_VERIFY=1 ./$(BINDIR)/$(TARGET) test/test_gc_large_safety.lu
	@echo ""
	@echo "GC safety tests passed under stress+verify."

zig-test:
	cd zig-test && ZIG_GLOBAL_CACHE_DIR=.zig-global-cache zig build test --cache-dir .zig-cache
	@echo "Zig test passed. No errors found."

vm:
	$(MAKE) -C vm

vm-run: vm
	./vm/luna_vm vm/sample.luvm

.PHONY: all clean clean-c run repl test test-rust-data test-gc test-gc-safety zig-test ir preprocess install check-deps setup-mint run-comp vm vm-run