# =============================================================================
# Board-side parameters for test-rvv/keypoints/trajkovic_2d_response_grid.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_trajkovic_2d_response_grid_std
REMOTE_BENCH_RVV := bench_trajkovic_2d_response_grid_rvv
REMOTE_TEST      := test_trajkovic_2d_response_grid_rvv
REMOTE_DIR := /root/pcl-test/keypoints/trajkovic_2d_response_grid

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
