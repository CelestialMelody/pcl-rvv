# =============================================================================
# Board-side parameters for test-rvv/recognition/implicit_shape_model.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD ?= bench_ism_std
REMOTE_BENCH_RVV ?= bench_ism_rvv
REMOTE_TEST      ?= test_ism_rvv

REMOTE_DIR := /root/pcl-test/recognition/implicit_shape_model
REMOTE_TEST_ARGS ?=
REMOTE_BENCH_ARGS ?= --case-filter all --clusters 184 --points 768 --votes 8192 --iterations 100 --warmup-iterations 5

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
