# =============================================================================
# Board-side parameters for test-rvv/features/organized_edge_detection.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_TEST := test_organized_edge_detection_rvv
REMOTE_BENCH_STD := bench_organized_edge_detection_std
REMOTE_BENCH_RVV := bench_organized_edge_detection_rvv
REMOTE_DIR := /root/pcl-test/features/organized_edge_detection

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
