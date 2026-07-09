# =============================================================================
# Board-side parameters for test-rvv/2d.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_TEST      := test_2d
REMOTE_BENCH_STD := bench_2d_std
REMOTE_BENCH_RVV := bench_2d_rvv
REMOTE_EDGE_STORE_TEST := test_edge_store_bench

REMOTE_DIR     := /root/pcl-test/2d
REMOTE_PCD_DIR := $(REMOTE_DIR)/pcds

REMOTE_TEST_ARGS := \
	$(REMOTE_PCD_DIR)/lena.pcd \
	$(REMOTE_PCD_DIR)/gauss_smooth.pcd \
	$(REMOTE_PCD_DIR)/erosion.pcd \
	$(REMOTE_PCD_DIR)/dilation.pcd \
	$(REMOTE_PCD_DIR)/opening.pcd \
	$(REMOTE_PCD_DIR)/closing.pcd \
	$(REMOTE_PCD_DIR)/erosion_binary.pcd \
	$(REMOTE_PCD_DIR)/dilation_binary.pcd \
	$(REMOTE_PCD_DIR)/opening_binary.pcd \
	$(REMOTE_PCD_DIR)/closing_binary.pcd \
	$(REMOTE_PCD_DIR)/canny.pcd

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)

run_bench: run_bench_rvv

run_bench_store_compare:
	@LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_EDGE_STORE_TEST)

.PHONY: run_bench run_bench_store_compare
