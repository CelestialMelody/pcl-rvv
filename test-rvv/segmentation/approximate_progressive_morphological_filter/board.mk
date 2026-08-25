# =============================================================================
# Board-side parameters for test-rvv/segmentation/approximate_progressive_morphological_filter.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_apmf_std
REMOTE_BENCH_RVV := bench_apmf_rvv
REMOTE_TEST      := test_apmf_rvv

REMOTE_DIR := /root/pcl-test/segmentation/approximate_progressive_morphological_filter

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
