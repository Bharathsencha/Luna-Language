# Luna is an ahead-of-time compiler: Luna source -> LLVM IR -> native binary (clang).
#
#   make            build bin/luna from the seeds in bootstrap/ (needs clang 18)
#   make test       compile and run every test, compare with test/expected/
#   make seed       refresh the seeds after changing compiler/ or compiler/runtime/
#   make clean      remove build output
#
# The compiler and its runtime are written in Luna. bootstrap/lunac.ll and
# bootstrap/luna_rt_native.ll are their LLVM IR, kept in the repository so the first compiler
# can be built with clang alone.

ifeq (,$(filter -j%,$(MAKEFLAGS)))
MAKEFLAGS += -j$(shell nproc)
endif

CLANG = clang
BINDIR = bin
OUT = C

LIBS = -lm -lpthread -ldl

SEED = bootstrap/lunac.ll
SEED_RT = bootstrap/luna_rt_native.ll
COMPILER_LU = $(wildcard compiler/*.lu)
RT_LU = $(sort $(wildcard compiler/runtime/*.lu))

all: $(BINDIR)/luna

# seeds -> seed compiler -> runtime and compiler IR of the current sources -> compiler ->
# IR again. The two compiler IR files must match.
$(BINDIR)/luna: $(SEED) $(SEED_RT) $(COMPILER_LU) $(RT_LU)
	@mkdir -p $(BINDIR) $(OUT)
	@$(CLANG) -O1 -w $(SEED) $(SEED_RT) $(LIBS) -o $(OUT)/luna_seed
	@$(OUT)/luna_seed --native $(RT_LU) -o $(OUT)/boot_rt.ll
	@$(OUT)/luna_seed compiler/lunac.lu $(OUT)/boot1.ll $(CURDIR)
	@$(CLANG) -O1 -w $(OUT)/boot1.ll $(OUT)/boot_rt.ll $(LIBS) -o $(OUT)/luna_stage1
	@$(OUT)/luna_stage1 compiler/lunac.lu $(OUT)/boot2.ll $(CURDIR)
	@cmp $(OUT)/boot1.ll $(OUT)/boot2.ll
	@cp $(OUT)/luna_stage1 $@
	@echo "[OK] built $@ (stage 1 and stage 2 emit identical IR)"
	@cmp -s $(SEED) $(OUT)/boot1.ll || echo "[WARN] the compiler seed is older than compiler/*.lu; run make seed"
	@cmp -s $(SEED_RT) $(OUT)/boot_rt.ll || echo "[WARN] the runtime seed is older than compiler/runtime/*.lu; run make seed"

# Rewrites the seeds from the current sources. Commit them with the change that needs them.
seed: $(BINDIR)/luna
	@cp $(OUT)/boot1.ll $(SEED)
	@cp $(OUT)/boot_rt.ll $(SEED_RT)
	@echo "[OK] wrote $(SEED) and $(SEED_RT)"

test: $(BINDIR)/luna
	@compiler/run_tests.sh
	@sh test/native/run.sh

clean:
	rm -rf $(BINDIR) $(OUT)

.PHONY: all seed test clean
