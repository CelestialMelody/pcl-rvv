# =============================================================================
# Board-side parameters for test-rvv/io/point_cloud_image_extractors.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_pcie_std
REMOTE_BENCH_RVV := bench_pcie_rvv
REMOTE_TEST      := test_pcie_rvv

REMOTE_DIR := /root/pcl-test/io/point_cloud_image_extractors
REMOTE_BENCH_ARGS ?= --iterations 20 --warmup-iterations 3 --case-filter all

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
