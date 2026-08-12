# =============================================================================
#  PCL registration/transformation_estimation_point_to_plane_lls_weighted RVV diagnostic Makefile
# =============================================================================

TOPIC  := transformation_estimation_point_to_plane_lls_weighted
MODULE := registration

FOCUS_DIR      := pcl/registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp
FILTER_REPORT  := log/filtered_transformation_estimation_point_to_plane_lls_weighted.log
ANALYZE_REPORT := log/analyze_transformation_estimation_point_to_plane_lls_weighted.log

TARGET_BENCH     := bench_transformation_estimation_point_to_plane_lls_weighted
TARGET_BENCH_STD := bench_transformation_estimation_point_to_plane_lls_weighted_std
TARGET_BENCH_RVV := bench_transformation_estimation_point_to_plane_lls_weighted_rvv
SRCS_BENCH       := src/bench_teptplw.cpp

TARGET_TEST     := test_transformation_estimation_point_to_plane_lls_weighted
TARGET_TEST_STD := test_transformation_estimation_point_to_plane_lls_weighted_std
TARGET_TEST_RVV := test_transformation_estimation_point_to_plane_lls_weighted_rvv
SRCS_TEST       := \
	src/test_teptplw_public_semantics.cpp \
	src/test_teptplw_input_semantics.cpp \
	src/test_teptplw_candidates.cpp \
	src/test_teptplw_row_sources.cpp \
	src/test_teptplw_production_direct.cpp

TEPTPLW_GTEST_FILTER_PUBLIC_SEMANTICS ?= TransformationEstimationPointToPlaneLLSWeighted.Std*MatchesPublicEstimator
TEPTPLW_GTEST_FILTER_INPUT_SEMANTICS ?= TransformationEstimationPointToPlaneLLSWeighted.Public*KeepsOutputMatrix:TransformationEstimationPointToPlaneLLSWeighted.ProductionFullCloudZeroWeightsMatchStdWithinBudget:TransformationEstimationPointToPlaneLLSWeighted.ProductionFullCloudNegativeWeightsMatchStdWithinBudget
TEPTPLW_GTEST_FILTER_ROW_SOURCES ?= TransformationEstimationPointToPlaneLLSWeighted.SourceIndexedCandidateMatchesStd:TransformationEstimationPointToPlaneLLSWeighted.DualIndicesCandidateMatchesStd:TransformationEstimationPointToPlaneLLSWeighted.CorrespondenceCandidateMatchesStd:TransformationEstimationPointToPlaneLLSWeighted.SmallInputFallsBackForIsolatedSizeGate:TransformationEstimationPointToPlaneLLSWeighted.InvalidLaneMaskMatchesStd:TransformationEstimationPointToPlaneLLSWeighted.NonFiniteWeightsAreNotMaskedWhenPointsAreFinite
TEPTPLW_GTEST_FILTER_SOURCE_INDEXED_FAMILY ?= TransformationEstimationPointToPlaneLLSWeighted.SourceIndexedBlock*
TEPTPLW_GTEST_FILTER_DUAL_CORRESPONDENCE_FAMILY ?= TransformationEstimationPointToPlaneLLSWeighted.DualIndicesBlock*:TransformationEstimationPointToPlaneLLSWeighted.CorrespondenceBlock*
TEPTPLW_GTEST_FILTER_CANDIDATES ?= TransformationEstimationPointToPlaneLLSWeighted.FullCloud*:TransformationEstimationPointToPlaneLLSWeighted.ProductionDefaultFusedAbcdIlpRepresentativePointTypesMatchStd
TEPTPLW_GTEST_FILTER_PRODUCTION_DIRECT ?= TransformationEstimationPointToPlaneLLSWeighted.ProductionFullCloud*:TransformationEstimationPointToPlaneLLSWeighted.ProductionSourceIndexed*
TEPTPLW_GTEST_FILTER_PRODUCTION_SOURCE_INDICES ?= TransformationEstimationPointToPlaneLLSWeighted.ProductionSourceIndexed*

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/registration/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include

LIBS_CORE := \
	-lpcl_registration \
	-lpcl_common \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread

TEPTPLW_SCRIPT_DIR := script
TEPTPLW_BOARD_COLLECT_SCRIPT := $(TEPTPLW_SCRIPT_DIR)/collect_teptplw_board_rvv_ba.py
TEPTPLW_TRACE_SUMMARY_SCRIPT := $(TEPTPLW_SCRIPT_DIR)/summarize_teptplw_trace.py
TEPTPLW_ASM_ATTRIBUTION_SCRIPT := $(TEPTPLW_SCRIPT_DIR)/generate_teptplw_asm_attribution.py
TEPTPLW_COMPARE_SCRIPT := $(TEPTPLW_SCRIPT_DIR)/collect_teptplw_board_compare_repeated.py
TEPTPLW_EVIDENCE_MANIFEST_SCRIPT := $(TEPTPLW_SCRIPT_DIR)/generate_teptplw_evidence_manifest.py
TEPTPLW_EVIDENCE_DOCTOR_SCRIPT := ../../script/evidence_doctor.py
TEPTPLW_BOARD_EVIDENCE_DIR ?= $(OUTPUT_DIR_BOARD)/production_default_fused_abcd_ilp
TEPTPLW_COMPARE_EVIDENCE_DIR ?= $(OUTPUT_DIR_BOARD)/production_dispatch_fused_abcd_ilp
TEPTPLW_SOURCE_INDICES_EVIDENCE_DIR ?= $(OUTPUT_DIR_BOARD)/production_source_indices_staged_gather
TEPTPLW_ROW_SOURCE_EVIDENCE_DIR ?= $(OUTPUT_DIR_BOARD)/row_sources_diagnostic
TEPTPLW_BOARD_SIZE ?= 262144
TEPTPLW_BOARD_RUNS ?= 5
TEPTPLW_BOARD_ITERATIONS ?= 20
TEPTPLW_BOARD_WARMUP_ITERATIONS ?= 5
TEPTPLW_BENCH_WARMUP_ITERATIONS ?= 5
TEPTPLW_COMPARE_CASE_FILTER ?= production-dispatch
TEPTPLW_COMPARE_SIZE ?= 262144
TEPTPLW_COMPARE_RUNS ?= 5
TEPTPLW_COMPARE_ITERATIONS ?= 20
TEPTPLW_COMPARE_WARMUP_ITERATIONS ?= 5
TEPTPLW_SOURCE_INDICES_CASE_FILTER ?= production-source-indices
TEPTPLW_SOURCE_INDICES_SIZE ?= 65536,262144
TEPTPLW_SOURCE_INDICES_RUNS ?= 5
TEPTPLW_SOURCE_INDICES_ITERATIONS ?= 20
TEPTPLW_SOURCE_INDICES_WARMUP_ITERATIONS ?= 5
TEPTPLW_SOURCE_INDICES_PROBE_EVIDENCE_DIR ?= $(OUTPUT_DIR_BOARD)/production_source_indices_block_fused_abcd_ilp_probe
TEPTPLW_SOURCE_INDICES_PROBE_MANIFEST ?= $(TEPTPLW_SOURCE_INDICES_PROBE_EVIDENCE_DIR)/evidence_manifest.json
TEPTPLW_SOURCE_INDICES_PROBE_DOCTOR_MD ?= $(TEPTPLW_SOURCE_INDICES_PROBE_EVIDENCE_DIR)/evidence_doctor.md
TEPTPLW_SOURCE_INDICES_PROBE_DOCTOR_JSON ?= $(TEPTPLW_SOURCE_INDICES_PROBE_EVIDENCE_DIR)/evidence_doctor.json
TEPTPLW_SOURCE_INDEXED_FAMILY_EVIDENCE_DIR ?= $(OUTPUT_DIR_BOARD)/source_indexed_family_repeated
TEPTPLW_SOURCE_INDEXED_FAMILY_CASE_FILTER ?= source-indexed-family
TEPTPLW_SOURCE_INDEXED_FAMILY_SIZE ?= 262144
TEPTPLW_SOURCE_INDEXED_FAMILY_RUNS ?= 5
TEPTPLW_SOURCE_INDEXED_FAMILY_ITERATIONS ?= 20
TEPTPLW_SOURCE_INDEXED_FAMILY_WARMUP_ITERATIONS ?= 5
TEPTPLW_SOURCE_INDEXED_FAMILY_MANIFEST ?= $(TEPTPLW_SOURCE_INDEXED_FAMILY_EVIDENCE_DIR)/evidence_manifest.json
TEPTPLW_SOURCE_INDEXED_FAMILY_DOCTOR_MD ?= $(TEPTPLW_SOURCE_INDEXED_FAMILY_EVIDENCE_DIR)/evidence_doctor.md
TEPTPLW_SOURCE_INDEXED_FAMILY_DOCTOR_JSON ?= $(TEPTPLW_SOURCE_INDEXED_FAMILY_EVIDENCE_DIR)/evidence_doctor.json
TEPTPLW_ROW_SOURCE_CASE_FILTER ?= row-sources
TEPTPLW_ROW_SOURCE_SIZE ?= 262144
TEPTPLW_ROW_SOURCE_RUNS ?= 5
TEPTPLW_ROW_SOURCE_ITERATIONS ?= 20
TEPTPLW_ROW_SOURCE_WARMUP_ITERATIONS ?= 5
TEPTPLW_TRACE_LOGS ?= $(wildcard $(TEPTPLW_BOARD_EVIDENCE_DIR)/run*_rvv.log)
TEPTPLW_ASM_EVIDENCE ?= $(TEPTPLW_BOARD_EVIDENCE_DIR)/asm_production_symbol_attribution.md
TEPTPLW_BENCH_CASE_FILTER_PRODUCTION_DISPATCH ?= production-dispatch
TEPTPLW_BENCH_CASE_FILTER_PRODUCTION_SOURCE_INDICES ?= production-source-indices
TEPTPLW_BENCH_CASE_FILTER_FUSED_FORMULA ?= fused-formula
TEPTPLW_BENCH_CASE_FILTER_ROW_SOURCES ?= row-sources
TEPTPLW_BENCH_CASE_FILTER_SOURCE_INDEXED_FAMILY ?= source-indexed-family
TEPTPLW_BENCH_CASE_FILTER_DUAL_CORRESPONDENCE_FAMILY ?= dual-correspondence-family
TEPTPLW_BENCH_CASE_FILTER_PRODUCTION_SHAPED_FUSED_FORMULA ?= production-shaped-fused-formula
TEPTPLW_BENCH_CASE_FILTER_GENERIC_FUSED_ABC ?= generic-fused-abc
TEPTPLW_BENCH_CASE_FILTER_GENERIC_FUSED_FORMULA ?= generic-fused-formula
TEPTPLW_BENCH_CASE_FILTER_GENERIC_FUSED_ABC_TRACE ?= generic-fused-abc-trace
TEPTPLW_BENCH_CASE_FILTER_PRODUCTION_DEFAULT_FUSED_ABCD_ILP ?= production-default-fused-abcd-ilp

include ../../mk/rvv-topic.mk

define TEPTPLW_DEFINE_FILTERED_TEST_TARGETS
run_test_$(1): run_test_$(1)_compare
run_test_$(1)_std: clean_test_std | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_test USE_PCL_RVV10=0 TARGET_TEST=$(TARGET_TEST_STD) TEST_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_test_$(1)_std.log TEST_ARGS="--gtest_filter=$(2)"
run_test_$(1)_rvv: clean_test_rvv | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_test USE_PCL_RVV10=1 TARGET_TEST=$(TARGET_TEST_RVV) TEST_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_test_$(1)_rvv.log TEST_ARGS="--gtest_filter=$(2)"
run_test_$(1)_compare: run_test_$(1)_std run_test_$(1)_rvv
endef

$(eval $(call TEPTPLW_DEFINE_FILTERED_TEST_TARGETS,public_semantics,$(TEPTPLW_GTEST_FILTER_PUBLIC_SEMANTICS)))
$(eval $(call TEPTPLW_DEFINE_FILTERED_TEST_TARGETS,input_semantics,$(TEPTPLW_GTEST_FILTER_INPUT_SEMANTICS)))
$(eval $(call TEPTPLW_DEFINE_FILTERED_TEST_TARGETS,row_sources,$(TEPTPLW_GTEST_FILTER_ROW_SOURCES)))
$(eval $(call TEPTPLW_DEFINE_FILTERED_TEST_TARGETS,source_indexed_family,$(TEPTPLW_GTEST_FILTER_SOURCE_INDEXED_FAMILY)))
$(eval $(call TEPTPLW_DEFINE_FILTERED_TEST_TARGETS,dual_correspondence_family,$(TEPTPLW_GTEST_FILTER_DUAL_CORRESPONDENCE_FAMILY)))
$(eval $(call TEPTPLW_DEFINE_FILTERED_TEST_TARGETS,candidates,$(TEPTPLW_GTEST_FILTER_CANDIDATES)))
$(eval $(call TEPTPLW_DEFINE_FILTERED_TEST_TARGETS,production_direct,$(TEPTPLW_GTEST_FILTER_PRODUCTION_DIRECT)))
$(eval $(call TEPTPLW_DEFINE_FILTERED_TEST_TARGETS,source_indices,$(TEPTPLW_GTEST_FILTER_PRODUCTION_SOURCE_INDICES)))

run_bench_default_diagnostic: run_bench_default_diagnostic_compare
run_bench_default_diagnostic_compare: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_compare BENCH_STD_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_default_diagnostic_std.log BENCH_RVV_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_default_diagnostic_rvv.log BENCH_COMPARE_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/analyze_bench_compare_default_diagnostic.log

define TEPTPLW_DEFINE_FILTERED_BENCH_TARGETS
run_bench_$(1): run_bench_$(1)_compare
run_bench_$(1)_compare: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_compare BENCH_ARGS="--case-filter $(2) --warmup-iterations $(TEPTPLW_BENCH_WARMUP_ITERATIONS)" BENCH_STD_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_$(1)_std.log BENCH_RVV_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_$(1)_rvv.log BENCH_COMPARE_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/analyze_bench_compare_$(1).log
endef

$(eval $(call TEPTPLW_DEFINE_FILTERED_BENCH_TARGETS,production_dispatch,$(TEPTPLW_BENCH_CASE_FILTER_PRODUCTION_DISPATCH)))
$(eval $(call TEPTPLW_DEFINE_FILTERED_BENCH_TARGETS,production_source_indices,$(TEPTPLW_BENCH_CASE_FILTER_PRODUCTION_SOURCE_INDICES)))
$(eval $(call TEPTPLW_DEFINE_FILTERED_BENCH_TARGETS,fused_formula,$(TEPTPLW_BENCH_CASE_FILTER_FUSED_FORMULA)))
$(eval $(call TEPTPLW_DEFINE_FILTERED_BENCH_TARGETS,row_sources,$(TEPTPLW_BENCH_CASE_FILTER_ROW_SOURCES)))
$(eval $(call TEPTPLW_DEFINE_FILTERED_BENCH_TARGETS,source_indexed_family,$(TEPTPLW_BENCH_CASE_FILTER_SOURCE_INDEXED_FAMILY)))
$(eval $(call TEPTPLW_DEFINE_FILTERED_BENCH_TARGETS,dual_correspondence_family,$(TEPTPLW_BENCH_CASE_FILTER_DUAL_CORRESPONDENCE_FAMILY)))
$(eval $(call TEPTPLW_DEFINE_FILTERED_BENCH_TARGETS,production_shaped_fused_formula,$(TEPTPLW_BENCH_CASE_FILTER_PRODUCTION_SHAPED_FUSED_FORMULA)))
$(eval $(call TEPTPLW_DEFINE_FILTERED_BENCH_TARGETS,generic_fused_abc,$(TEPTPLW_BENCH_CASE_FILTER_GENERIC_FUSED_ABC)))
$(eval $(call TEPTPLW_DEFINE_FILTERED_BENCH_TARGETS,generic_fused_formula,$(TEPTPLW_BENCH_CASE_FILTER_GENERIC_FUSED_FORMULA)))
$(eval $(call TEPTPLW_DEFINE_FILTERED_BENCH_TARGETS,generic_fused_abc_trace,$(TEPTPLW_BENCH_CASE_FILTER_GENERIC_FUSED_ABC_TRACE)))
$(eval $(call TEPTPLW_DEFINE_FILTERED_BENCH_TARGETS,production_default_fused_abcd_ilp,$(TEPTPLW_BENCH_CASE_FILTER_PRODUCTION_DEFAULT_FUSED_ABCD_ILP)))

define TEPTPLW_DEFINE_BOARD_TEST_TARGETS
run_board_test_$(1): | $(OUTPUT_DIR_BOARD)
	@$(MAKE) -C $(CURDIR) run_board_test \
		REMOTE_BOARD_OUTPUT_DIR=$(REMOTE_DIR)/log/board/run_board_test_$(1) \
		REMOTE_TEST_ARGS="--gtest_filter=$(2)" \
		OUTPUT_DIR_BOARD=$(OUTPUT_DIR_BOARD)/run_board_test_$(1)
	@$(MAKE) -C $(CURDIR) fetch_board_logs \
		REMOTE_BOARD_OUTPUT_DIR=$(REMOTE_DIR)/log/board/run_board_test_$(1) \
		OUTPUT_DIR_BOARD=$(OUTPUT_DIR_BOARD)/run_board_test_$(1)
endef

define TEPTPLW_DEFINE_BOARD_BENCH_TARGETS
run_board_bench_$(1): | $(OUTPUT_DIR_BOARD)
	@$(MAKE) -C $(CURDIR) run_board_bench_compare \
		REMOTE_BOARD_OUTPUT_DIR=$(REMOTE_DIR)/log/board/run_board_bench_$(1) \
		BOARD_LABEL=TEPTPLW-$(1) \
		BENCH_ARGS="$(2) --warmup-iterations $(TEPTPLW_BOARD_WARMUP_ITERATIONS)" \
		OUTPUT_DIR_BOARD=$(OUTPUT_DIR_BOARD)/run_board_bench_$(1)
	@$(MAKE) -C $(CURDIR) fetch_board_logs \
		REMOTE_BOARD_OUTPUT_DIR=$(REMOTE_DIR)/log/board/run_board_bench_$(1) \
		OUTPUT_DIR_BOARD=$(OUTPUT_DIR_BOARD)/run_board_bench_$(1)
endef

$(eval $(call TEPTPLW_DEFINE_BOARD_TEST_TARGETS,public_semantics,$(TEPTPLW_GTEST_FILTER_PUBLIC_SEMANTICS)))
$(eval $(call TEPTPLW_DEFINE_BOARD_TEST_TARGETS,input_semantics,$(TEPTPLW_GTEST_FILTER_INPUT_SEMANTICS)))
$(eval $(call TEPTPLW_DEFINE_BOARD_TEST_TARGETS,row_sources,$(TEPTPLW_GTEST_FILTER_ROW_SOURCES)))
$(eval $(call TEPTPLW_DEFINE_BOARD_TEST_TARGETS,source_indexed_family,$(TEPTPLW_GTEST_FILTER_SOURCE_INDEXED_FAMILY)))
$(eval $(call TEPTPLW_DEFINE_BOARD_TEST_TARGETS,dual_correspondence_family,$(TEPTPLW_GTEST_FILTER_DUAL_CORRESPONDENCE_FAMILY)))
$(eval $(call TEPTPLW_DEFINE_BOARD_TEST_TARGETS,candidates,$(TEPTPLW_GTEST_FILTER_CANDIDATES)))
$(eval $(call TEPTPLW_DEFINE_BOARD_TEST_TARGETS,production_direct,$(TEPTPLW_GTEST_FILTER_PRODUCTION_DIRECT)))
$(eval $(call TEPTPLW_DEFINE_BOARD_TEST_TARGETS,source_indices,$(TEPTPLW_GTEST_FILTER_PRODUCTION_SOURCE_INDICES)))

$(eval $(call TEPTPLW_DEFINE_BOARD_BENCH_TARGETS,default_diagnostic,))
$(eval $(call TEPTPLW_DEFINE_BOARD_BENCH_TARGETS,fused_formula,--case-filter $(TEPTPLW_BENCH_CASE_FILTER_FUSED_FORMULA)))
$(eval $(call TEPTPLW_DEFINE_BOARD_BENCH_TARGETS,row_sources,--case-filter $(TEPTPLW_BENCH_CASE_FILTER_ROW_SOURCES)))
$(eval $(call TEPTPLW_DEFINE_BOARD_BENCH_TARGETS,source_indexed_family,--case-filter $(TEPTPLW_BENCH_CASE_FILTER_SOURCE_INDEXED_FAMILY)))
$(eval $(call TEPTPLW_DEFINE_BOARD_BENCH_TARGETS,dual_correspondence_family,--case-filter $(TEPTPLW_BENCH_CASE_FILTER_DUAL_CORRESPONDENCE_FAMILY)))
$(eval $(call TEPTPLW_DEFINE_BOARD_BENCH_TARGETS,production_dispatch,--case-filter $(TEPTPLW_BENCH_CASE_FILTER_PRODUCTION_DISPATCH)))
$(eval $(call TEPTPLW_DEFINE_BOARD_BENCH_TARGETS,production_source_indices,--case-filter $(TEPTPLW_BENCH_CASE_FILTER_PRODUCTION_SOURCE_INDICES)))
$(eval $(call TEPTPLW_DEFINE_BOARD_BENCH_TARGETS,production_shaped_fused_formula,--case-filter $(TEPTPLW_BENCH_CASE_FILTER_PRODUCTION_SHAPED_FUSED_FORMULA)))
$(eval $(call TEPTPLW_DEFINE_BOARD_BENCH_TARGETS,generic_fused_abc,--case-filter $(TEPTPLW_BENCH_CASE_FILTER_GENERIC_FUSED_ABC)))
$(eval $(call TEPTPLW_DEFINE_BOARD_BENCH_TARGETS,generic_fused_formula,--case-filter $(TEPTPLW_BENCH_CASE_FILTER_GENERIC_FUSED_FORMULA)))
$(eval $(call TEPTPLW_DEFINE_BOARD_BENCH_TARGETS,generic_fused_abc_trace,--case-filter $(TEPTPLW_BENCH_CASE_FILTER_GENERIC_FUSED_ABC_TRACE)))
$(eval $(call TEPTPLW_DEFINE_BOARD_BENCH_TARGETS,production_default_fused_abcd_ilp,--case-filter $(TEPTPLW_BENCH_CASE_FILTER_PRODUCTION_DEFAULT_FUSED_ABCD_ILP)))

run_bench_production_default_fused_abcd_ilp_rvv: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_rvv BENCH_ARGS="--case-filter $(TEPTPLW_BENCH_CASE_FILTER_PRODUCTION_DEFAULT_FUSED_ABCD_ILP)" BENCH_RVV_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_production_default_fused_abcd_ilp_rvv.log

collect_board_production_default_fused_abcd_ilp:
	$(PYTHON_RUN) $(TEPTPLW_BOARD_COLLECT_SCRIPT) \
		--case-filter production-default-fused-abcd-ilp \
		--size $(TEPTPLW_BOARD_SIZE) \
		--runs $(TEPTPLW_BOARD_RUNS) \
		--iterations $(TEPTPLW_BOARD_ITERATIONS) \
		--warmup-iterations $(TEPTPLW_BOARD_WARMUP_ITERATIONS) \
		--output-dir $(TEPTPLW_BOARD_EVIDENCE_DIR)

clean_board_production_default_fused_abcd_ilp:
	rm -rf $(TEPTPLW_BOARD_EVIDENCE_DIR)

refresh_board_production_default_fused_abcd_ilp: clean_board_production_default_fused_abcd_ilp collect_board_production_default_fused_abcd_ilp

summarize_board_production_default_fused_abcd_ilp:
	@test -n "$(strip $(TEPTPLW_TRACE_LOGS))" || \
		(echo "[trace] No run*_rvv.log under $(TEPTPLW_BOARD_EVIDENCE_DIR)." >&2; exit 1)
	$(PYTHON_RUN) $(TEPTPLW_TRACE_SUMMARY_SCRIPT) \
		$(TEPTPLW_TRACE_LOGS) \
		--output $(TEPTPLW_BOARD_EVIDENCE_DIR)/trace_summary.md

asm_production_default_fused_abcd_ilp:
	$(MAKE) dump_bench_rvv
	$(PYTHON_RUN) $(TEPTPLW_ASM_ATTRIBUTION_SCRIPT) \
		--profile production-default-fused-abcd-ilp \
		--asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm \
		--output $(TEPTPLW_ASM_EVIDENCE)

collect_board_production_dispatch_repeated:
	$(PYTHON_RUN) $(TEPTPLW_COMPARE_SCRIPT) \
		--case-filter $(TEPTPLW_COMPARE_CASE_FILTER) \
		--size $(TEPTPLW_COMPARE_SIZE) \
		--runs $(TEPTPLW_COMPARE_RUNS) \
		--iterations $(TEPTPLW_COMPARE_ITERATIONS) \
		--warmup-iterations $(TEPTPLW_COMPARE_WARMUP_ITERATIONS) \
		--output-dir $(TEPTPLW_COMPARE_EVIDENCE_DIR)

clean_board_production_dispatch_repeated:
	rm -rf $(TEPTPLW_COMPARE_EVIDENCE_DIR)

refresh_board_production_dispatch_repeated: clean_board_production_dispatch_repeated collect_board_production_dispatch_repeated

collect_board_production_source_indices_repeated:
	$(PYTHON_RUN) $(TEPTPLW_COMPARE_SCRIPT) \
		--case-filter $(TEPTPLW_SOURCE_INDICES_CASE_FILTER) \
		--size $(TEPTPLW_SOURCE_INDICES_SIZE) \
		--runs $(TEPTPLW_SOURCE_INDICES_RUNS) \
		--iterations $(TEPTPLW_SOURCE_INDICES_ITERATIONS) \
		--warmup-iterations $(TEPTPLW_SOURCE_INDICES_WARMUP_ITERATIONS) \
		--output-dir $(TEPTPLW_SOURCE_INDICES_EVIDENCE_DIR)

clean_board_production_source_indices_repeated:
	rm -rf $(TEPTPLW_SOURCE_INDICES_EVIDENCE_DIR)

refresh_board_production_source_indices_repeated: clean_board_production_source_indices_repeated collect_board_production_source_indices_repeated

collect_board_production_source_indices_probe_repeated:
	$(PYTHON_RUN) $(TEPTPLW_COMPARE_SCRIPT) \
		--case-filter $(TEPTPLW_SOURCE_INDICES_CASE_FILTER) \
		--size $(TEPTPLW_SOURCE_INDICES_SIZE) \
		--runs $(TEPTPLW_SOURCE_INDICES_RUNS) \
		--iterations $(TEPTPLW_SOURCE_INDICES_ITERATIONS) \
		--warmup-iterations $(TEPTPLW_SOURCE_INDICES_WARMUP_ITERATIONS) \
		--rerun-target production-source-indices-block-fused-probe \
		--collection-profile source-indexed-block-fused-abcd-ilp-production-probe \
		--output-dir $(TEPTPLW_SOURCE_INDICES_PROBE_EVIDENCE_DIR)

doctor_board_production_source_indices_probe_repeated:
	@test -f $(TEPTPLW_SOURCE_INDICES_PROBE_EVIDENCE_DIR)/summary.md || \
		(echo "[evidence] missing $(TEPTPLW_SOURCE_INDICES_PROBE_EVIDENCE_DIR)/summary.md; run collect_board_production_source_indices_probe_repeated on board first." >&2; exit 1)
	$(PYTHON_RUN) $(TEPTPLW_EVIDENCE_MANIFEST_SCRIPT) \
		--kind production-source-indices-block-fused-probe \
		--summary $(TEPTPLW_SOURCE_INDICES_PROBE_EVIDENCE_DIR)/summary.md \
		--output $(TEPTPLW_SOURCE_INDICES_PROBE_MANIFEST)
	$(PYTHON_RUN) $(TEPTPLW_EVIDENCE_DOCTOR_SCRIPT) \
		--manifest $(TEPTPLW_SOURCE_INDICES_PROBE_MANIFEST) \
		--output $(TEPTPLW_SOURCE_INDICES_PROBE_DOCTOR_MD) \
		--json-output $(TEPTPLW_SOURCE_INDICES_PROBE_DOCTOR_JSON) \
		--fail-on never

clean_board_production_source_indices_probe_repeated:
	rm -rf $(TEPTPLW_SOURCE_INDICES_PROBE_EVIDENCE_DIR)

refresh_board_production_source_indices_probe_repeated: clean_board_production_source_indices_probe_repeated collect_board_production_source_indices_probe_repeated

collect_board_source_indexed_family_repeated:
	$(PYTHON_RUN) $(TEPTPLW_COMPARE_SCRIPT) \
		--case-filter $(TEPTPLW_SOURCE_INDEXED_FAMILY_CASE_FILTER) \
		--size $(TEPTPLW_SOURCE_INDEXED_FAMILY_SIZE) \
		--runs $(TEPTPLW_SOURCE_INDEXED_FAMILY_RUNS) \
		--iterations $(TEPTPLW_SOURCE_INDEXED_FAMILY_ITERATIONS) \
		--warmup-iterations $(TEPTPLW_SOURCE_INDEXED_FAMILY_WARMUP_ITERATIONS) \
		--output-dir $(TEPTPLW_SOURCE_INDEXED_FAMILY_EVIDENCE_DIR)

doctor_board_source_indexed_family_repeated:
	@test -f $(TEPTPLW_SOURCE_INDEXED_FAMILY_EVIDENCE_DIR)/summary.md || \
		(echo "[evidence] missing $(TEPTPLW_SOURCE_INDEXED_FAMILY_EVIDENCE_DIR)/summary.md; run collect_board_source_indexed_family_repeated on board first." >&2; exit 1)
	$(PYTHON_RUN) $(TEPTPLW_EVIDENCE_MANIFEST_SCRIPT) \
		--kind source-indexed-family-repeated \
		--summary $(TEPTPLW_SOURCE_INDEXED_FAMILY_EVIDENCE_DIR)/summary.md \
		--output $(TEPTPLW_SOURCE_INDEXED_FAMILY_MANIFEST)
	$(PYTHON_RUN) $(TEPTPLW_EVIDENCE_DOCTOR_SCRIPT) \
		--manifest $(TEPTPLW_SOURCE_INDEXED_FAMILY_MANIFEST) \
		--output $(TEPTPLW_SOURCE_INDEXED_FAMILY_DOCTOR_MD) \
		--json-output $(TEPTPLW_SOURCE_INDEXED_FAMILY_DOCTOR_JSON) \
		--fail-on never

clean_board_source_indexed_family_repeated:
	rm -rf $(TEPTPLW_SOURCE_INDEXED_FAMILY_EVIDENCE_DIR)

refresh_board_source_indexed_family_repeated: clean_board_source_indexed_family_repeated collect_board_source_indexed_family_repeated

collect_board_row_sources_repeated:
	$(PYTHON_RUN) $(TEPTPLW_COMPARE_SCRIPT) \
		--case-filter $(TEPTPLW_ROW_SOURCE_CASE_FILTER) \
		--size $(TEPTPLW_ROW_SOURCE_SIZE) \
		--runs $(TEPTPLW_ROW_SOURCE_RUNS) \
		--iterations $(TEPTPLW_ROW_SOURCE_ITERATIONS) \
		--warmup-iterations $(TEPTPLW_ROW_SOURCE_WARMUP_ITERATIONS) \
		--output-dir $(TEPTPLW_ROW_SOURCE_EVIDENCE_DIR)

clean_board_row_sources_repeated:
	rm -rf $(TEPTPLW_ROW_SOURCE_EVIDENCE_DIR)

refresh_board_row_sources_repeated: clean_board_row_sources_repeated collect_board_row_sources_repeated

.PHONY: run_test_public_semantics run_test_public_semantics_std run_test_public_semantics_rvv run_test_public_semantics_compare run_test_input_semantics run_test_input_semantics_std run_test_input_semantics_rvv run_test_input_semantics_compare run_test_row_sources run_test_row_sources_std run_test_row_sources_rvv run_test_row_sources_compare run_test_source_indexed_family run_test_source_indexed_family_std run_test_source_indexed_family_rvv run_test_source_indexed_family_compare run_test_dual_correspondence_family run_test_dual_correspondence_family_std run_test_dual_correspondence_family_rvv run_test_dual_correspondence_family_compare run_test_candidates run_test_candidates_std run_test_candidates_rvv run_test_candidates_compare run_test_production_direct run_test_production_direct_std run_test_production_direct_rvv run_test_production_direct_compare run_test_source_indices run_test_source_indices_std run_test_source_indices_rvv run_test_source_indices_compare run_board_test_public_semantics run_board_test_input_semantics run_board_test_row_sources run_board_test_source_indexed_family run_board_test_dual_correspondence_family run_board_test_candidates run_board_test_production_direct run_board_test_source_indices run_bench_default_diagnostic run_bench_default_diagnostic_compare run_bench_fused_formula run_bench_fused_formula_compare run_bench_row_sources run_bench_row_sources_compare run_bench_source_indexed_family run_bench_source_indexed_family_compare run_bench_dual_correspondence_family run_bench_dual_correspondence_family_compare run_bench_production_dispatch run_bench_production_dispatch_compare run_bench_production_source_indices run_bench_production_source_indices_compare run_bench_production_shaped_fused_formula run_bench_production_shaped_fused_formula_compare run_bench_generic_fused_abc run_bench_generic_fused_abc_compare run_bench_generic_fused_formula run_bench_generic_fused_formula_compare run_bench_generic_fused_abc_trace run_bench_generic_fused_abc_trace_compare run_bench_production_default_fused_abcd_ilp run_bench_production_default_fused_abcd_ilp_compare run_bench_production_default_fused_abcd_ilp_rvv run_board_bench_default_diagnostic run_board_bench_fused_formula run_board_bench_row_sources run_board_bench_source_indexed_family run_board_bench_dual_correspondence_family run_board_bench_production_dispatch run_board_bench_production_source_indices run_board_bench_production_shaped_fused_formula run_board_bench_generic_fused_abc run_board_bench_generic_fused_formula run_board_bench_generic_fused_abc_trace run_board_bench_production_default_fused_abcd_ilp collect_board_production_default_fused_abcd_ilp clean_board_production_default_fused_abcd_ilp refresh_board_production_default_fused_abcd_ilp summarize_board_production_default_fused_abcd_ilp asm_production_default_fused_abcd_ilp collect_board_production_dispatch_repeated clean_board_production_dispatch_repeated refresh_board_production_dispatch_repeated collect_board_production_source_indices_repeated clean_board_production_source_indices_repeated refresh_board_production_source_indices_repeated collect_board_production_source_indices_probe_repeated doctor_board_production_source_indices_probe_repeated clean_board_production_source_indices_probe_repeated refresh_board_production_source_indices_probe_repeated collect_board_source_indexed_family_repeated doctor_board_source_indexed_family_repeated clean_board_source_indexed_family_repeated refresh_board_source_indexed_family_repeated collect_board_row_sources_repeated clean_board_row_sources_repeated refresh_board_row_sources_repeated
