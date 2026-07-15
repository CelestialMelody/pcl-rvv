# =============================================================================
# Board-side runner for RVV math helper/vectorization tests
# =============================================================================

SHELL := /bin/bash
.SHELLFLAGS := -o pipefail -c

REMOTE_DIR ?= /root/pcl-test/rvv/math
REMOTE_LIB_DIR ?= /root/pcl-test/lib
REMOTE_OUTPUT_DIR ?= $(REMOTE_DIR)/output

REMOTE_ACOS_TEST := acos_test
REMOTE_ATAN2_TEST := atan2_test
REMOTE_EXPF_TEST := expf_test
REMOTE_EXPF_REMEZ_VS_TAYLOR := expf_remez_vs_taylor
REMOTE_LOGF_TEST := logf_test
REMOTE_SINCOS_TEST := sincos_test
REMOTE_SINCOS_RANGE_SMOKE := sincos_range_smoke
REMOTE_SINCOS_RANGE_IMAGE_SPHERICAL_INTEGRATION_TEST := sincos_range_image_spherical_integration_test

define RUN_RULE
$(1): | $(REMOTE_OUTPUT_DIR)/$(2)
	@echo "[BOARD] $(3) -> $(REMOTE_OUTPUT_DIR)/$(2)/$(1).log"
	@LD_LIBRARY_PATH=$(REMOTE_LIB_DIR):$$LD_LIBRARY_PATH \
	$(REMOTE_DIR)/$(3) $(4) 2>&1 | tee $(REMOTE_OUTPUT_DIR)/$(2)/$(1).log
endef

$(eval $(call RUN_RULE,run_acos_test,acos,$(REMOTE_ACOS_TEST),))
$(eval $(call RUN_RULE,run_atan2_test,atan2,$(REMOTE_ATAN2_TEST),))
$(eval $(call RUN_RULE,run_expf_test,expf,$(REMOTE_EXPF_TEST),))
$(eval $(call RUN_RULE,run_expf_remez_vs_taylor,expf,$(REMOTE_EXPF_REMEZ_VS_TAYLOR),))
$(eval $(call RUN_RULE,run_logf_test,logf,$(REMOTE_LOGF_TEST),))
$(eval $(call RUN_RULE,run_sincos_test,sincos,$(REMOTE_SINCOS_TEST),))
$(eval $(call RUN_RULE,run_sincos_bench,sincos,$(REMOTE_SINCOS_TEST),--bench))
$(eval $(call RUN_RULE,run_sincos_range_smoke,sincos,$(REMOTE_SINCOS_RANGE_SMOKE),))
$(eval $(call RUN_RULE,run_sincos_range_image_spherical_integration_test,sincos,$(REMOTE_SINCOS_RANGE_IMAGE_SPHERICAL_INTEGRATION_TEST),))

run_math_all: run_acos_test run_atan2_test run_expf_test run_expf_remez_vs_taylor run_logf_test run_sincos_test run_sincos_range_smoke

$(REMOTE_OUTPUT_DIR)/acos $(REMOTE_OUTPUT_DIR)/atan2 $(REMOTE_OUTPUT_DIR)/expf $(REMOTE_OUTPUT_DIR)/logf $(REMOTE_OUTPUT_DIR)/sincos:
	mkdir -p $@

.PHONY: run_acos_test run_atan2_test run_expf_test run_expf_remez_vs_taylor run_logf_test \
	run_sincos_test run_sincos_bench run_sincos_range_smoke \
	run_sincos_range_image_spherical_integration_test run_math_all
