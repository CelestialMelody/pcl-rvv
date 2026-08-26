# =============================================================================
# Board-side parameters for test-rvv/segmentation/extract_polygonal_prism_data.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_eppd_std
REMOTE_BENCH_RVV := bench_eppd_rvv
REMOTE_TEST      := test_eppd_rvv

REMOTE_DIR := /root/pcl-test/segmentation/extract_polygonal_prism_data

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
