# =============================================================================
# Board-side parameters for test-rvv/common/common.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_common_std
REMOTE_BENCH_RVV := bench_common_rvv
REMOTE_TEST      := test_common

REMOTE_DIR := /root/pcl-test/common/common

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
