# =============================================================================
# Board-side parameters for test-rvv/keypoints/harris_2d.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_harris_2d_std
REMOTE_BENCH_RVV := bench_harris_2d_rvv
REMOTE_TEST      := test_harris_2d_rvv

REMOTE_DIR := /root/pcl-test/keypoints/harris_2d

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
