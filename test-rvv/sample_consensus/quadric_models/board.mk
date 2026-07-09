# =============================================================================
# Board-side parameters for test-rvv/sample_consensus/quadric_models.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_TEST := rvv_sac_quadric_test
REMOTE_DIR := /root/pcl-test/sample_consensus/quadric_models

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
