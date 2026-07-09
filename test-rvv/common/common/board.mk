# =============================================================================
# Board-side parameters for test-rvv/common/common.
# Shared rules are deployed as script/rvv-board-run.mk.
# =============================================================================

REMOTE_BENCH_STD := bench_common_std
REMOTE_BENCH_RVV := bench_common_rvv
REMOTE_TEST      := test_common
REMOTE_ATAN2_TEST := atan2_test
REMOTE_ACOS_TEST := acos_test
REMOTE_EXPF_TEST := expf_test
REMOTE_EXPF_REMEZ_VS_TAYLOR := expf_remez_vs_taylor
REMOTE_LOGF_TEST := logf_test

REMOTE_DIR := /root/pcl-test/common/common

REMOTE_ATAN2_OUTPUT_FILE ?= $(REMOTE_OUTPUT_DIR)/run_atan2_test.log
REMOTE_ACOS_OUTPUT_FILE ?= $(REMOTE_OUTPUT_DIR)/run_acos_test.log
REMOTE_EXPF_OUTPUT_FILE ?= $(REMOTE_OUTPUT_DIR)/run_expf_test.log
REMOTE_EXPF_REMEZ_OUTPUT_FILE ?= $(REMOTE_OUTPUT_DIR)/run_expf_remez_vs_taylor.log
REMOTE_LOGF_OUTPUT_FILE ?= $(REMOTE_OUTPUT_DIR)/run_logf_test.log

BOARD_MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BOARD_RUN_FRAGMENT := $(firstword $(wildcard script/rvv-board-run.mk $(BOARD_MAKEFILE_DIR)../../mk/rvv-board-run.mk))
include $(BOARD_RUN_FRAGMENT)

run_atan2_test: | $(REMOTE_OUTPUT_DIR)
	@echo "[BOARD] atan2_test -> $(REMOTE_ATAN2_OUTPUT_FILE)"
	@LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_ATAN2_TEST) 2>&1 | tee $(REMOTE_ATAN2_OUTPUT_FILE)

run_acos_test: | $(REMOTE_OUTPUT_DIR)
	@echo "[BOARD] acos_test -> $(REMOTE_ACOS_OUTPUT_FILE)"
	@LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_ACOS_TEST) 2>&1 | tee $(REMOTE_ACOS_OUTPUT_FILE)

run_expf_test: | $(REMOTE_OUTPUT_DIR)
	@echo "[BOARD] expf_test -> $(REMOTE_EXPF_OUTPUT_FILE)"
	@LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_EXPF_TEST) 2>&1 | tee $(REMOTE_EXPF_OUTPUT_FILE)

run_expf_remez_vs_taylor: | $(REMOTE_OUTPUT_DIR)
	@echo "[BOARD] expf_remez_vs_taylor -> $(REMOTE_EXPF_REMEZ_OUTPUT_FILE)"
	@LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_EXPF_REMEZ_VS_TAYLOR) 2>&1 | tee $(REMOTE_EXPF_REMEZ_OUTPUT_FILE)

run_logf_test: | $(REMOTE_OUTPUT_DIR)
	@echo "[BOARD] logf_test -> $(REMOTE_LOGF_OUTPUT_FILE)"
	@LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(REMOTE_LOGF_TEST) 2>&1 | tee $(REMOTE_LOGF_OUTPUT_FILE)

run_math: run_atan2_test run_acos_test run_expf_test run_logf_test

.PHONY: run_atan2_test run_acos_test run_expf_test run_expf_remez_vs_taylor run_logf_test run_math
