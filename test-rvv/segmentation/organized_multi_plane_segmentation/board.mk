# =============================================================================
# Board-side parameters for test-rvv/segmentation/organized_multi_plane_segmentation.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_omps_std
REMOTE_BENCH_RVV := bench_omps_rvv
REMOTE_TEST      := test_omps_rvv

REMOTE_DIR := /root/pcl-test/segmentation/organized_multi_plane_segmentation

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
