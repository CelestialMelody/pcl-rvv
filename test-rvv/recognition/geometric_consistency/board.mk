# =============================================================================
# Board-side parameters for test-rvv/recognition/geometric_consistency.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD ?= bench_gc_std
REMOTE_BENCH_RVV ?= bench_gc_rvv
REMOTE_TEST      ?= test_gc_rvv

REMOTE_DIR := /root/pcl-test/recognition/geometric_consistency
REMOTE_TEST_ARGS ?=
REMOTE_BENCH_ARGS ?= 4096 200 5 0.03

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
