# =============================================================================
# Board-side parameters for test-rvv/io/organized_pointcloud_conversion.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_organized_pointcloud_conversion_std
REMOTE_BENCH_RVV := bench_organized_pointcloud_conversion_rvv
REMOTE_TEST      := test_organized_pointcloud_conversion_rvv

REMOTE_DIR := /root/pcl-test/io/organized_pointcloud_conversion
REMOTE_BENCH_ARGS ?= --iterations 30 --warmup-iterations 5 --case-filter all

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
