# =============================================================================
# Board-side parameters for test-rvv/common/distances.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_distances_std
REMOTE_BENCH_RVV := bench_distances_rvv
REMOTE_TEST      := test_distances
REMOTE_BENCH_LOAD_COMPARE := bench_getmaxsegment_load_compare

REMOTE_DIR := /root/pcl-test/common/distances
REMOTE_BENCH_LOAD_COMPARE_OUTPUT_FILE ?= $(REMOTE_OUTPUT_DIR)/run_bench_load_compare.log

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)

run_bench_load_compare: | $(REMOTE_OUTPUT_DIR)
	@echo "[BOARD] getMaxSegment load-compare -> $(REMOTE_BENCH_LOAD_COMPARE_OUTPUT_FILE)"
	@LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_BENCH_LOAD_COMPARE) 2>&1 | tee $(REMOTE_BENCH_LOAD_COMPARE_OUTPUT_FILE)

.PHONY: run_bench_load_compare
