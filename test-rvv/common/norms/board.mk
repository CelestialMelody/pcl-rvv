# =============================================================================
# Board-side parameters for test-rvv/common/norms.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_norms_std
REMOTE_BENCH_RVV := bench_norms_rvv
REMOTE_TEST      := test_norms
REMOTE_TEST_STD_VS_RVV_COMPARE := test_norms_std_vs_rvv_compare

REMOTE_DIR := /root/pcl-test/common/norms
REMOTE_TEST_STD_VS_RVV_COMPARE_OUTPUT_FILE ?= $(REMOTE_OUTPUT_DIR)/run_test_std_vs_rvv_compare.log

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)

run_test_std_vs_rvv_compare: | $(REMOTE_OUTPUT_DIR)
	@echo "[BOARD] Std vs RVV compare -> $(REMOTE_TEST_STD_VS_RVV_COMPARE_OUTPUT_FILE)"
	@LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_TEST_STD_VS_RVV_COMPARE) 2>&1 | tee $(REMOTE_TEST_STD_VS_RVV_COMPARE_OUTPUT_FILE)

.PHONY: run_test_std_vs_rvv_compare
