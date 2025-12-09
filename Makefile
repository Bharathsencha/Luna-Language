CC = gcc
CFLAGS = -std=c11 -O2 -Iinclude
ASM = nasm
ASMFLAGS = -f elf64
TARGET = luna
OBJDIR = obj
BINDIR = bin

# Source files
SRCS = src/lexer.c src/token.c src/util.c src/ast.c src/parser.c src/interpreter.c src/value.c src/main.c src/math_lib.c src/string_lib.c src/error.c src/time_lib.c src/vec_lib.c src/env.c src/library.c

# Object files (Updated: vec.o -> vec_math.o)
OBJS = $(OBJDIR)/lexer.o $(OBJDIR)/token.o $(OBJDIR)/util.o $(OBJDIR)/ast.o $(OBJDIR)/parser.o $(OBJDIR)/interpreter.o $(OBJDIR)/value.o $(OBJDIR)/main.o $(OBJDIR)/math_lib.o $(OBJDIR)/string_lib.o $(OBJDIR)/error.o $(OBJDIR)/time_lib.o $(OBJDIR)/time.o $(OBJDIR)/vec_lib.o $(OBJDIR)/vec_math.o $(OBJDIR)/env.o $(OBJDIR)/library.o

all: $(BINDIR)/$(TARGET)

# Create directories
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Link the main interpreter
$(BINDIR)/$(TARGET): $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ -lm

# Compile source files
$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile assembly files
$(OBJDIR)/%.o: asm/%.asm | $(OBJDIR)
	$(ASM) $(ASMFLAGS) $< -o $@


test: $(BINDIR)/$(TARGET)
	@echo "==> Manual Check: test/math.lu"
	@./$(BINDIR)/$(TARGET) test/math.lu
	@echo ""

	@echo "==> Manual Check: test/flow.lu"
	@./$(BINDIR)/$(TARGET) test/flow.lu
	@echo ""

	@echo "==> Manual Check: test/func.lu"
	@./$(BINDIR)/$(TARGET) test/func.lu
	@echo ""

	@echo "==> Manual Check: test/types.lu"
	@./$(BINDIR)/$(TARGET) test/types.lu
	@echo ""

	@echo "==> Manual Check: test/arrays.lu"
	@./$(BINDIR)/$(TARGET) test/arrays.lu
	@echo ""

	@echo "==> Manual Check: test/balls.lu"
	@./$(BINDIR)/$(TARGET) test/balls.lu
	@echo ""

	@echo "==> Manual Check: test/fail_check.lu"
	@./$(BINDIR)/$(TARGET) test/fail_check.lu || true
	@echo ""

	@./test_runner.sh
	@echo "All tests passed!"


# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(BINDIR)

# Run the interpreter on the main file
run: $(BINDIR)/$(TARGET)
	./$(BINDIR)/$(TARGET) main.lu

# Shortcut to run the REPL
repl: $(BINDIR)/$(TARGET)
	./$(BINDIR)/$(TARGET)

.PHONY: all clean run repl test