# =============================================================================
# Board-side parameters for test-rvv/segmentation/min_cut_segmentation.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_min_cut_segmentation_std
REMOTE_BENCH_RVV := bench_min_cut_segmentation_rvv
REMOTE_TEST      := test_min_cut_segmentation_rvv

REMOTE_DIR := /root/pcl-test/segmentation/min_cut_segmentation

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
