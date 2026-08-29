# =============================================================================
# Board-side parameters for test-rvv/keypoints/trajkovic_3d.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD ?= bench_trajkovic_3d_std
REMOTE_BENCH_RVV ?= bench_trajkovic_3d_rvv
REMOTE_TEST      ?= test_trajkovic_3d_rvv

REMOTE_DIR := /root/pcl-test/keypoints/trajkovic_3d
REMOTE_TEST_ARGS ?=
REMOTE_BENCH_ARGS ?= --case-filter all --iterations 20 --warmup 3 --width 320 --height 240

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
