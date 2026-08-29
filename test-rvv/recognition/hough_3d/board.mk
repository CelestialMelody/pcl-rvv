# =============================================================================
# Board-side parameters for test-rvv/recognition/hough_3d.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD ?= bench_hough_3d_std
REMOTE_BENCH_RVV ?= bench_hough_3d_rvv
REMOTE_TEST      ?= test_hough_3d_rvv

REMOTE_DIR := /root/pcl-test/recognition/hough_3d
REMOTE_TEST_ARGS ?=
REMOTE_BENCH_ARGS ?= 65536 200 5 0.03 10 1

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
