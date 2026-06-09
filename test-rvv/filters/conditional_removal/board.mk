# =============================================================================
# Board-side parameters for test-rvv/filters/conditional_removal.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_conditional_removal_std
REMOTE_BENCH_RVV := bench_conditional_removal_rvv
REMOTE_TEST      := test_conditional_removal_rvv

REMOTE_DIR := /root/pcl-test/filters/conditional_removal

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
