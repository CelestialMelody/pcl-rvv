# =============================================================================
# Board-side parameters for test-rvv/sample_consensus/sac_model_sphere.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_sac_model_sphere_std
REMOTE_BENCH_RVV := bench_sac_model_sphere_rvv
REMOTE_TEST      := test_sac_model_sphere_rvv

REMOTE_DIR := /root/pcl-test/sample_consensus/sac_model_sphere
REMOTE_TEST_ARGS ?=
REMOTE_BENCH_ARGS ?= 65536 200

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
