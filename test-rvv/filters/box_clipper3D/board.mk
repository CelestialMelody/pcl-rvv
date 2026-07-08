# =============================================================================
# Board-side parameters for test-rvv/filters/box_clipper3D.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_box_clipper3D_std
REMOTE_BENCH_RVV := bench_box_clipper3D_rvv
REMOTE_TEST      := test_box_clipper3D_rvv

REMOTE_DIR := /root/pcl-test/filters/box_clipper3D

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
