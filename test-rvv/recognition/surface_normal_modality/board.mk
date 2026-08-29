# =============================================================================
# Board-side parameters for test-rvv/recognition/surface_normal_modality.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_snm_std
REMOTE_BENCH_RVV := bench_snm_rvv
REMOTE_TEST      := test_snm_rvv

REMOTE_DIR := /root/pcl-test/recognition/surface_normal_modality

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
