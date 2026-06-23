# =============================================================================
# Board-side parameters for test-rvv/filters/covariance_sampling.
# =============================================================================

REMOTE_BENCH_STD := bench_covariance_sampling_std
REMOTE_BENCH_RVV := bench_covariance_sampling_rvv
REMOTE_TEST      := test_covariance_sampling_rvv

REMOTE_DIR := /root/pcl-test/filters/covariance_sampling

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)
