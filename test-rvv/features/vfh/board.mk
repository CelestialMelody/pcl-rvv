# =============================================================================
# Board-side parameters for test-rvv/features/vfh.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_vfh_std
REMOTE_BENCH_RVV := bench_vfh_rvv
REMOTE_TEST      := test_vfh_rvv

REMOTE_DIR := /root/pcl-test/features/vfh

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
