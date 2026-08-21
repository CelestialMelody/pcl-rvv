# =============================================================================
# Board-side parameters for test-rvv/io/lzf_image_io.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_lzf_image_io_std
REMOTE_BENCH_RVV := bench_lzf_image_io_rvv
REMOTE_TEST      := test_lzf_image_io_rvv

REMOTE_DIR := /root/pcl-test/io/lzf_image_io

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
