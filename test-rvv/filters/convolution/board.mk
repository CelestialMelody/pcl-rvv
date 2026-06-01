# =============================================================================
# 板卡侧运行 Makefile（配套 test-rvv/filters/convolution/Makefile）
# =============================================================================

REMOTE_BENCH_STD = bench_convolution_std
REMOTE_BENCH_RVV = bench_convolution_rvv
REMOTE_TEST      = test_convolution_rvv

REMOTE_DIR        = /root/pcl-test/filters/convolution
REMOTE_LIB_DIR    = /root/pcl-test/lib
REMOTE_OUTPUT_DIR = $(REMOTE_DIR)/output

SCRIPT_DIR           = $(REMOTE_DIR)/script
BENCH_COMPARE_SCRIPT = $(SCRIPT_DIR)/analyze_bench_compare.py

PYTHON ?= python3
BOARD_LABEL ?= Milkv-Jupiter
BENCH_COMPARE_SAVE ?=

REMOTE_BENCH_STD_OUTPUT_FILE = $(REMOTE_OUTPUT_DIR)/run_bench_std.log
REMOTE_BENCH_RVV_OUTPUT_FILE = $(REMOTE_OUTPUT_DIR)/run_bench_rvv.log
REMOTE_TEST_OUTPUT_FILE      = $(REMOTE_OUTPUT_DIR)/run_test.log

run_bench_std: | $(REMOTE_OUTPUT_DIR)
	@echo "[BOARD] Benchmark Std -> $(REMOTE_BENCH_STD_OUTPUT_FILE)"
	@LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_BENCH_STD) 20 full 2>&1 | tee $(REMOTE_BENCH_STD_OUTPUT_FILE)

run_bench_rvv: | $(REMOTE_OUTPUT_DIR)
	@echo "[BOARD] Benchmark RVV -> $(REMOTE_BENCH_RVV_OUTPUT_FILE)"
	@LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_BENCH_RVV) 20 full 2>&1 | tee $(REMOTE_BENCH_RVV_OUTPUT_FILE)

analyze_bench_compare: ensure_bench_logs
	@test -f '$(BENCH_COMPARE_SCRIPT)' || (echo "缺少 $(BENCH_COMPARE_SCRIPT)，请将开发机 test-rvv/script/analyze_bench_compare.py 同步到板卡 $(SCRIPT_DIR)/" >&2; exit 1)
	@$(PYTHON) '$(BENCH_COMPARE_SCRIPT)' \
		--std-log $(REMOTE_BENCH_STD_OUTPUT_FILE) \
		--rvv-log $(REMOTE_BENCH_RVV_OUTPUT_FILE) \
		--device "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)" \
		$(if $(BENCH_COMPARE_SAVE),| tee $(BENCH_COMPARE_SAVE),)

ensure_bench_logs: | $(REMOTE_OUTPUT_DIR)
	@if [ ! -f '$(REMOTE_BENCH_STD_OUTPUT_FILE)' ]; then \
		echo "[BOARD] Missing Std bench log, running run_bench_std first..."; \
		$(MAKE) run_bench_std; \
	fi
	@if [ ! -f '$(REMOTE_BENCH_RVV_OUTPUT_FILE)' ]; then \
		echo "[BOARD] Missing RVV bench log, running run_bench_rvv first..."; \
		$(MAKE) run_bench_rvv; \
	fi

run_bench_compare: run_bench_std run_bench_rvv analyze_bench_compare

run_test: | $(REMOTE_OUTPUT_DIR)
	@echo "[BOARD] Unit Test -> $(REMOTE_TEST_OUTPUT_FILE)"
	@LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_TEST) 2>&1 | tee $(REMOTE_TEST_OUTPUT_FILE)

$(REMOTE_OUTPUT_DIR):
	mkdir -p $(REMOTE_OUTPUT_DIR)

.PHONY: run_bench_std run_bench_rvv run_bench_compare analyze_bench_compare ensure_bench_logs run_test
