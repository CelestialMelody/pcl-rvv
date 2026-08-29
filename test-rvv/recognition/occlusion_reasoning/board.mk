# =============================================================================
# Board-side parameters for test-rvv/recognition/occlusion_reasoning.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD ?= bench_occlusion_reasoning_std
REMOTE_BENCH_RVV ?= bench_occlusion_reasoning_rvv
REMOTE_TEST      ?= test_occlusion_reasoning_rvv

REMOTE_DIR := /root/pcl-test/recognition/occlusion_reasoning
REMOTE_TEST_ARGS ?=
REMOTE_BENCH_ARGS ?= 65536 150 150 200 5

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
