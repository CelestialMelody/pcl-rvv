# =============================================================================
# 板卡侧运行 Makefile（配套 test-rvv/common/norms/Makefile 的 deploy_* 目标）
#
# 用法（板卡上，在含本文件的目录）：make -f board.mk run_bench_compare
# 依赖：已将 bench_norms_std / bench_norms_rvv、可选 test_norms / test_norms_std_vs_rvv_compare、
#       analyze_bench_compare.py 部署到 REMOTE_DIR，且 REMOTE_LIB_DIR 下有所需 .so。
# =============================================================================

REMOTE_BENCH_STD = bench_norms_std
REMOTE_BENCH_RVV = bench_norms_rvv
REMOTE_TEST      = test_norms
REMOTE_TEST_STD_VS_RVV_COMPARE = test_norms_std_vs_rvv_compare

REMOTE_DIR        = /root/pcl-test/common/norms
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
REMOTE_TEST_STD_VS_RVV_COMPARE_OUTPUT_FILE = $(REMOTE_OUTPUT_DIR)/run_test_std_vs_rvv_compare.log

run_bench_std: | $(REMOTE_OUTPUT_DIR)
	@echo "[BOARD] Benchmark Std -> $(REMOTE_BENCH_STD_OUTPUT_FILE)"
	LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_BENCH_STD) 2>&1 | tee $(REMOTE_BENCH_STD_OUTPUT_FILE)

run_bench_rvv: | $(REMOTE_OUTPUT_DIR)
	@echo "[BOARD] Benchmark RVV -> $(REMOTE_BENCH_RVV_OUTPUT_FILE)"
	LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_BENCH_RVV) 2>&1 | tee $(REMOTE_BENCH_RVV_OUTPUT_FILE)

analyze_bench_compare:
	@test -f '$(BENCH_COMPARE_SCRIPT)' || (echo "缺少 $(BENCH_COMPARE_SCRIPT)，请将开发机 test-rvv/script/analyze_bench_compare.py 同步到板卡 $(SCRIPT_DIR)/" >&2; exit 1)
	$(PYTHON) '$(BENCH_COMPARE_SCRIPT)' \
		--std-log $(REMOTE_BENCH_STD_OUTPUT_FILE) \
		--rvv-log $(REMOTE_BENCH_RVV_OUTPUT_FILE) \
		--device "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)" \
		$(if $(BENCH_COMPARE_SAVE),| tee $(BENCH_COMPARE_SAVE),)

run_bench_compare: run_bench_std run_bench_rvv analyze_bench_compare

# gtest 单测，无额外命令行参数；若仅部署了 test_norms_std / test_norms_rvv，可覆盖：make -f board.mk run_test REMOTE_TEST=test_norms_rvv
run_test: | $(REMOTE_OUTPUT_DIR)
	@echo "[BOARD] Unit Test -> $(REMOTE_TEST_OUTPUT_FILE)"
	LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_TEST) 2>&1 | tee $(REMOTE_TEST_OUTPUT_FILE)

# 开发机：\c make deploy_test_std_vs_rvv_compare；需与 \c test_norms 相同动态库（\c libpcl_common 等）
run_test_std_vs_rvv_compare: | $(REMOTE_OUTPUT_DIR)
	@echo "[BOARD] Std vs RVV compare -> $(REMOTE_TEST_STD_VS_RVV_COMPARE_OUTPUT_FILE)"
	LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_TEST_STD_VS_RVV_COMPARE) 2>&1 | tee $(REMOTE_TEST_STD_VS_RVV_COMPARE_OUTPUT_FILE)

$(REMOTE_OUTPUT_DIR):
	mkdir -p $(REMOTE_OUTPUT_DIR)

.PHONY: run_bench_std run_bench_rvv run_bench_compare analyze_bench_compare run_test \
	run_test_std_vs_rvv_compare
