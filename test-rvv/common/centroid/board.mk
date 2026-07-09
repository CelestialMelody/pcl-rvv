# =============================================================================
# Board-side parameters for test-rvv/common/centroid.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_centroid_std
REMOTE_BENCH_RVV := bench_centroid_rvv
REMOTE_TEST      := test_centroid

REMOTE_DIR := /root/pcl-test/common/centroid
REMOTE_TEST_ARGS := $(REMOTE_DIR)/pcd/bun0.pcd

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
