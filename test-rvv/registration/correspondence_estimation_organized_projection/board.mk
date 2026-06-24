# =============================================================================
# Board-side parameters for test-rvv/registration/correspondence_estimation_organized_projection.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_correspondence_estimation_organized_projection_std
REMOTE_BENCH_RVV := bench_correspondence_estimation_organized_projection_rvv
REMOTE_TEST      := test_correspondence_estimation_organized_projection_rvv

REMOTE_DIR := /root/pcl-test/registration/correspondence_estimation_organized_projection

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
