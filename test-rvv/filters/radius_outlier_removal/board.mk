# =============================================================================
# Board-side parameters for test-rvv/filters/radius_outlier_removal.
# =============================================================================

REMOTE_BENCH_STD := bench_radius_outlier_removal_std
REMOTE_BENCH_RVV := bench_radius_outlier_removal_rvv
REMOTE_TEST      := test_radius_outlier_removal_rvv

REMOTE_DIR := /root/pcl-test/filters/radius_outlier_removal

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
