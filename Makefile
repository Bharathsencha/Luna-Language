ifeq (,$(filter -j%,$(MAKEFLAGS)))
MAKEFLAGS += -j$(shell nproc)
endif
CC = gcc
# Added -Igui to ensure the compiler can find headers in the gui/ folder
CFLAGS = -std=c11 -O3 -march=native -flto=auto -fopenmp -Iinclude -Igui -Wall -Wextra -Wno-unused-parameter
ASM = nasm
ASMFLAGS = -f elf64
TARGET = luna
OBJDIR = obj
BINDIR = bin

# Source files
SRCS = src/lexer.c src/token.c src/util.c src/ast.c src/parser.c \
       src/interpreter.c src/value.c src/main.c src/math_lib.c \
       src/string_lib.c src/error.c src/time_lib.c src/vec_lib.c \
       src/env.c src/library.c src/file_lib.c src/list_lib.c \
       src/sand_lib.c src/arena.c src/intern.c gui/gui_lib.c

# Object files
OBJS = $(OBJDIR)/lexer.o $(OBJDIR)/token.o $(OBJDIR)/util.o \
       $(OBJDIR)/ast.o $(OBJDIR)/parser.o $(OBJDIR)/interpreter.o \
       $(OBJDIR)/value.o $(OBJDIR)/main.o $(OBJDIR)/math_lib.o \
       $(OBJDIR)/string_lib.o $(OBJDIR)/error.o $(OBJDIR)/time_lib.o \
       $(OBJDIR)/time.o $(OBJDIR)/vec_lib.o \
       $(OBJDIR)/env.o $(OBJDIR)/library.o $(OBJDIR)/file_lib.o \
       $(OBJDIR)/list_lib.o $(OBJDIR)/sand_lib.o $(OBJDIR)/arena.o \
       $(OBJDIR)/intern.o $(OBJDIR)/gui_lib.o

all: $(BINDIR)/$(TARGET)

# Create directories
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Link the main interpreter
# Added -Llib to look for libraylib.a and system graphics dependencies
$(BINDIR)/$(TARGET): $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ -Llib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -z noexecstack -fopenmp

# Compile source files from src/
$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to compile source files from gui/
$(OBJDIR)/gui_lib.o: gui/gui_lib.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

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
	@if [ -f "lib/libraylib.a" ]; then echo "[OK] libraylib.a found"; else echo "[ERROR] libraylib.a missing in lib/"; fi
	@if [ -f "include/raylib.h" ]; then echo "[OK] raylib.h found"; else echo "[ERROR] raylib.h missing in include/"; fi

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

	@echo "==> Manual Check: test/balls.lu"
	@./$(BINDIR)/$(TARGET) test/balls.lu
	@echo ""
	@./test_runner.sh
	@echo "All tests passed!"

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(BINDIR)

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

# Run Luna vs Python Performance Comparison
run-comp:
	@chmod +x benchmark/compare.sh
	@+./benchmark/compare.sh

.PHONY: all clean run repl test ir preprocess install check-deps setup-mint run-comp