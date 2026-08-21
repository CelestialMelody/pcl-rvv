# =============================================================================
# Board-side parameters for test-rvv/io/image_yuv422.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_image_yuv422_std
REMOTE_BENCH_RVV := bench_image_yuv422_rvv
REMOTE_TEST      := test_image_yuv422_rvv

REMOTE_DIR := /root/pcl-test/io/image_yuv422

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
