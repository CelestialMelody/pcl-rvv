# =============================================================================
# Board-side parameters for test-rvv/keypoints/iss_3d.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD ?= bench_iss_3d_std
REMOTE_BENCH_RVV ?= bench_iss_3d_rvv
REMOTE_TEST      ?= test_iss_3d_rvv

REMOTE_DIR := /root/pcl-test/keypoints/iss_3d
REMOTE_TEST_ARGS ?=
REMOTE_BENCH_ARGS ?= --case-filter all --iterations 20 --warmup 3 --points 8192 --neighbors 256

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
