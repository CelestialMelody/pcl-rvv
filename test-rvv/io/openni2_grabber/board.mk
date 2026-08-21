# =============================================================================
# Board-side parameters for test-rvv/io/openni2_grabber.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_openni2_grabber_std
REMOTE_BENCH_RVV := bench_openni2_grabber_rvv
REMOTE_TEST      := test_openni2_grabber_rvv

REMOTE_DIR := /root/pcl-test/io/openni2_grabber

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
