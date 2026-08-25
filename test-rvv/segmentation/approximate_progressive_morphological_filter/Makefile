# =============================================================================
#  PCL segmentation/approximate_progressive_morphological_filter RVV diagnostic
# =============================================================================

TOPIC  := approximate_progressive_morphological_filter
MODULE := segmentation

FOCUS_DIR      := pcl/segmentation/include/pcl/segmentation/impl/approximate_progressive_morphological_filter.hpp
FILTER_REPORT  := log/filtered_apmf.log
ANALYZE_REPORT := log/analyze_apmf.log

TARGET_BENCH     := bench_apmf
TARGET_BENCH_STD := bench_apmf_std
TARGET_BENCH_RVV := bench_apmf_rvv
SRCS_BENCH       := src/bench_apmf.cpp

TARGET_TEST     := test_apmf
TARGET_TEST_STD := test_apmf_std
TARGET_TEST_RVV := test_apmf_rvv
SRCS_TEST       := src/test_apmf.cpp

TARGET_PRODUCTION_BENCH_STD := bench_apmf_production_std
TARGET_PRODUCTION_BENCH_RVV := bench_apmf_production_rvv
SRCS_PRODUCTION_BENCH       := src/bench_apmf_production.cpp

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/filters/include \
	-I$(PCL_SOURCE_ROOT)/segmentation/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include

LIBS_CORE := -lpcl_segmentation -lpcl_filters -lpcl_common -lboost_system -lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread

include ../../mk/rvv-topic.mk

APMF_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/repeated
APMF_REPEATED_SUMMARY := $(APMF_REPEATED_DIR)/summary.md
APMF_EVIDENCE_MANIFEST := $(APMF_REPEATED_DIR)/evidence_manifest.json
APMF_EVIDENCE_DOCTOR_MD := $(APMF_REPEATED_DIR)/evidence_doctor.md
APMF_EVIDENCE_INCLUDE_REGEX ?=
APMF_EVIDENCE_BINARY_STD := $(if $(findstring production public,$(APMF_EVIDENCE_INCLUDE_REGEX)),$(TARGET_PRODUCTION_BENCH_STD),$(TARGET_BENCH_STD))
APMF_EVIDENCE_BINARY_RVV := $(if $(findstring production public,$(APMF_EVIDENCE_INCLUDE_REGEX)),$(TARGET_PRODUCTION_BENCH_RVV),$(TARGET_BENCH_RVV))
APMF_BINARY_HASH ?= $(shell if [ -f build/riscv/$(APMF_EVIDENCE_BINARY_STD) ] && [ -f build/riscv/$(APMF_EVIDENCE_BINARY_RVV) ]; then sha256sum build/riscv/$(APMF_EVIDENCE_BINARY_STD) build/riscv/$(APMF_EVIDENCE_BINARY_RVV) | sha256sum | awk '{print $$1}'; fi)

generate_board_evidence_manifest:
	$(PYTHON) script/generate_apmf_board_evidence_manifest.py \
		--repeated-dir $(APMF_REPEATED_DIR) \
		--summary $(APMF_REPEATED_SUMMARY) \
		--output $(APMF_EVIDENCE_MANIFEST) \
		--device "$(BOARD_LABEL)" \
		--iterations 8 \
		--warmup-iterations 2 \
		--binary-hash "$(APMF_BINARY_HASH)" \
		$(if $(APMF_EVIDENCE_INCLUDE_REGEX),--include-regex "$(APMF_EVIDENCE_INCLUDE_REGEX)",)

run_board_evidence_doctor: generate_board_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(APMF_EVIDENCE_MANIFEST) \
		--output $(APMF_EVIDENCE_DOCTOR_MD) \
		--fail-on never

run_evidence_doctor: run_board_evidence_doctor

build_production_bench_rvv:
	rm -f $(BUILD_DIR)/$(ARCH)/$(TARGET_PRODUCTION_BENCH_RVV)
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_BENCH=$(TARGET_PRODUCTION_BENCH_RVV) SRCS_BENCH="$(SRCS_PRODUCTION_BENCH)" $(BUILD_DIR)/$(ARCH)/$(TARGET_PRODUCTION_BENCH_RVV)

dump_production_bench_rvv: build_production_bench_rvv | $(ASM_DIR)/$(ARCH)
	$(OBJDUMP) -d -C $(BUILD_DIR)/$(ARCH)/$(TARGET_PRODUCTION_BENCH_RVV) > $(ASM_DIR)/$(ARCH)/$(TARGET_PRODUCTION_BENCH_RVV).full.asm
	grep -E $(VEC_REGEX_STR) $(ASM_DIR)/$(ARCH)/$(TARGET_PRODUCTION_BENCH_RVV).full.asm > $(ASM_DIR)/$(ARCH)/$(TARGET_PRODUCTION_BENCH_RVV).asm || true
	@echo "[DONE] RVV asm dump: $(ASM_DIR)/$(ARCH)/$(TARGET_PRODUCTION_BENCH_RVV).asm"

check_production_rvv_asm: dump_production_bench_rvv
	@grep -q "apmfExtractRVV" $(ASM_DIR)/$(ARCH)/$(TARGET_PRODUCTION_BENCH_RVV).full.asm
	@grep -Eq "v(fcvt\\.rtz\\.x\\.f\\.v|compress\\.vm|cpop\\.m|luxseg3ei32\\.v|lse32\\.v)" $(ASM_DIR)/$(ARCH)/$(TARGET_PRODUCTION_BENCH_RVV).asm

run_production_bench_std:
	@$(MAKE) -C $(CURDIR) run_bench USE_PCL_RVV10=0 TARGET_BENCH=$(TARGET_PRODUCTION_BENCH_STD) SRCS_BENCH="$(SRCS_PRODUCTION_BENCH)" BENCH_STD_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_production_std.log BENCH_ARGS="$(BENCH_ARGS)"

run_production_bench_rvv:
	@$(MAKE) -C $(CURDIR) run_bench USE_PCL_RVV10=1 TARGET_BENCH=$(TARGET_PRODUCTION_BENCH_RVV) SRCS_BENCH="$(SRCS_PRODUCTION_BENCH)" BENCH_RVV_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_production_rvv.log BENCH_ARGS="$(BENCH_ARGS)"

run_board_production_bench_compare:
	@$(MAKE) -C $(CURDIR) deploy_bench_std \
		SRCS_BENCH="$(SRCS_PRODUCTION_BENCH)" \
		TARGET_BENCH_STD=$(TARGET_PRODUCTION_BENCH_STD) \
		REMOTE_BENCH_STD=$(TARGET_PRODUCTION_BENCH_STD)
	@$(MAKE) -C $(CURDIR) deploy_bench_rvv \
		SRCS_BENCH="$(SRCS_PRODUCTION_BENCH)" \
		TARGET_BENCH_RVV=$(TARGET_PRODUCTION_BENCH_RVV) \
		REMOTE_BENCH_RVV=$(TARGET_PRODUCTION_BENCH_RVV)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare \
		REMOTE_BENCH_STD=$(TARGET_PRODUCTION_BENCH_STD) \
		REMOTE_BENCH_RVV=$(TARGET_PRODUCTION_BENCH_RVV) \
		REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' \
		BENCH_COMPARE_SAVE='$(REMOTE_BOARD_OUTPUT_DIR)/analyze_bench_compare.log' \
		BOARD_LABEL='$(BOARD_LABEL)' \
		REMOTE_BENCH_ARGS='$(BENCH_ARGS)'"

.PHONY: generate_board_evidence_manifest run_board_evidence_doctor run_evidence_doctor build_production_bench_rvv dump_production_bench_rvv check_production_rvv_asm run_production_bench_std run_production_bench_rvv run_board_production_bench_compare
