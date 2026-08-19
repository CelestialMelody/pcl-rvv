# =============================================================================
# Board-side parameters for test-rvv/surface/organized_fast_mesh.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_organized_fast_mesh_std
REMOTE_BENCH_RVV := bench_organized_fast_mesh_rvv
REMOTE_TEST      := test_organized_fast_mesh_rvv

REMOTE_DIR := /root/pcl-test/surface/organized_fast_mesh

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
