# =============================================================================
# Board-side parameters for test-rvv/surface/marching_cubes_rbf.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_marching_cubes_rbf_std
REMOTE_BENCH_RVV := bench_marching_cubes_rbf_rvv
REMOTE_TEST      := test_marching_cubes_rbf_rvv

REMOTE_DIR := /root/pcl-test/surface/marching_cubes_rbf

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
