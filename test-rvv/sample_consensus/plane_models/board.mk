# =============================================================================
# Board-side parameters for test-rvv/sample_consensus/plane_models.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_sac_normal_plane_std
REMOTE_BENCH_RVV := bench_sac_normal_plane_rvv
REMOTE_TEST      := rvv_sac_plane_test
REMOTE_BENCH_LOAD := bench_sac_normal_plane_load_compare

REMOTE_DIR := /root/pcl-test/sample_consensus/plane_models
REMOTE_PCD_FILE := $(REMOTE_DIR)/pcd/sac_plane_test.pcd
REMOTE_TEST_ARGS := $(REMOTE_PCD_FILE)
REMOTE_BENCH_ARGS := $(REMOTE_PCD_FILE)
REMOTE_BENCH_LOAD_OUTPUT_FILE ?= $(REMOTE_OUTPUT_DIR)/run_bench_load_compare.log

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)

run_bench: run_bench_rvv

run_bench_load_compare: | $(REMOTE_OUTPUT_DIR)
	@echo "[BOARD] NormalPlane load-compare -> $(REMOTE_BENCH_LOAD_OUTPUT_FILE)"
	@LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_BENCH_LOAD) $(REMOTE_PCD_FILE) 200 2>&1 | tee $(REMOTE_BENCH_LOAD_OUTPUT_FILE)

.PHONY: run_bench run_bench_load_compare
