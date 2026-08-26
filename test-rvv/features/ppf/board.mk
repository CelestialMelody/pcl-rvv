# =============================================================================
# Board-side parameters for test-rvv/features/ppf.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_ppf_std
REMOTE_BENCH_RVV := bench_ppf_rvv
REMOTE_TEST      := test_ppf_rvv

REMOTE_DIR := /root/pcl-test/features/ppf

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
