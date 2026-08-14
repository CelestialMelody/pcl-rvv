# =============================================================================
# Board-side parameters for test-rvv/registration/transformation_estimation_2D.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_transformation_estimation_2D_std
REMOTE_BENCH_RVV := bench_transformation_estimation_2D_rvv
REMOTE_TEST      := test_transformation_estimation_2D_rvv

REMOTE_DIR := /root/pcl-test/registration/transformation_estimation_2D

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
