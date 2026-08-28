# =============================================================================
# Board-side parameters for test-rvv/recognition/linemod_template_scoring.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD ?= bench_linemod_template_scoring_std
REMOTE_BENCH_RVV ?= bench_linemod_template_scoring_rvv
REMOTE_TEST      ?= test_linemod_template_scoring_rvv

REMOTE_DIR := /root/pcl-test/recognition/linemod_template_scoring
REMOTE_TEST_ARGS ?=
REMOTE_BENCH_ARGS ?= 4096 96 200 5

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
