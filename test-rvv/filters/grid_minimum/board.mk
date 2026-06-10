# =============================================================================
# Board-side parameters for test-rvv/filters/grid_minimum.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_grid_minimum_std
REMOTE_BENCH_RVV := bench_grid_minimum_rvv
REMOTE_TEST      := test_grid_minimum_rvv

REMOTE_DIR := /root/pcl-test/filters/grid_minimum

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
