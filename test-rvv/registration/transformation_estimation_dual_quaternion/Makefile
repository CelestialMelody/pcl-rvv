# =============================================================================
#  PCL registration/transformation_estimation_dual_quaternion RVV diagnostic Makefile
# =============================================================================

TOPIC  := transformation_estimation_dual_quaternion
MODULE := registration

FOCUS_DIR      := pcl/registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp
FILTER_REPORT  := log/filtered_transformation_estimation_dual_quaternion.log
ANALYZE_REPORT := log/analyze_transformation_estimation_dual_quaternion.log

TARGET_BENCH     := bench_transformation_estimation_dual_quaternion
TARGET_BENCH_STD := bench_transformation_estimation_dual_quaternion_std
TARGET_BENCH_RVV := bench_transformation_estimation_dual_quaternion_rvv
SRCS_BENCH       := src/bench_tedq.cpp

TARGET_TEST     := test_transformation_estimation_dual_quaternion
TARGET_TEST_STD := test_transformation_estimation_dual_quaternion_std
TARGET_TEST_RVV := test_transformation_estimation_dual_quaternion_rvv
SRCS_TEST       := src/test_tedq.cpp

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

include ../../mk/rvv-topic.mk

QEMU_SMOKE_EVIDENCE_MANIFEST := $(OUTPUT_DIR_QEMU)/evidence_manifest.json
QEMU_SMOKE_EVIDENCE_DOCTOR_MD := $(OUTPUT_DIR_QEMU)/evidence_doctor.md
QEMU_SMOKE_BENCH_ARGS ?= --case-filter production-public-retained-row-sources --iterations 1 --warmup-iterations 0
EVIDENCE_REGISTRY := $(LOG_DIR)/evidence_registry.json

TEDQ_BOARD_REPEATED_RUNS ?= 5
TEDQ_BOARD_BENCH_ITERATIONS ?= 20
TEDQ_BOARD_BENCH_WARMUP_ITERATIONS ?= 5
TEDQ_BOARD_RUN_LABEL ?= ordered_cloud_pair_repeated
TEDQ_BOARD_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(TEDQ_BOARD_RUN_LABEL)
TEDQ_BOARD_SUMMARY := $(TEDQ_BOARD_REPEATED_DIR)/summary.md
TEDQ_BOARD_EVIDENCE_MANIFEST := $(TEDQ_BOARD_REPEATED_DIR)/evidence_manifest.json
TEDQ_BOARD_EVIDENCE_DOCTOR_MD := $(TEDQ_BOARD_REPEATED_DIR)/evidence_doctor.md

TEDQ_BOARD_PRODUCTION_RUN_LABEL ?= production_public_retained_row_sources_repeated
TEDQ_BOARD_PRODUCTION_CASE_FILTER ?= production-public-retained-row-sources
TEDQ_BOARD_PRODUCTION_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(TEDQ_BOARD_PRODUCTION_RUN_LABEL)
TEDQ_BOARD_PRODUCTION_SUMMARY := $(TEDQ_BOARD_PRODUCTION_REPEATED_DIR)/summary.md
TEDQ_BOARD_PRODUCTION_EVIDENCE_MANIFEST := $(TEDQ_BOARD_PRODUCTION_REPEATED_DIR)/evidence_manifest.json
TEDQ_BOARD_PRODUCTION_EVIDENCE_DOCTOR_MD := $(TEDQ_BOARD_PRODUCTION_REPEATED_DIR)/evidence_doctor.md
TEDQ_BOARD_PRODUCTION_SOURCE_INDEXED_DIR ?= $(OUTPUT_DIR_BOARD)/production_public_source_indexed_cloud_pair_repeated
TEDQ_BOARD_PRODUCTION_SOURCE_INDEXED_SUMMARY := $(TEDQ_BOARD_PRODUCTION_SOURCE_INDEXED_DIR)/summary.md
TEDQ_BOARD_PRODUCTION_SOURCE_INDEXED_EVIDENCE_MANIFEST := $(TEDQ_BOARD_PRODUCTION_SOURCE_INDEXED_DIR)/evidence_manifest.json
TEDQ_BOARD_PRODUCTION_SOURCE_INDEXED_EVIDENCE_DOCTOR_MD := $(TEDQ_BOARD_PRODUCTION_SOURCE_INDEXED_DIR)/evidence_doctor.md
TEDQ_BOARD_PRODUCTION_DUAL_INDEXED_DIR ?= $(OUTPUT_DIR_BOARD)/production_public_dual_indexed_cloud_pair_repeated
TEDQ_BOARD_PRODUCTION_DUAL_INDEXED_SUMMARY := $(TEDQ_BOARD_PRODUCTION_DUAL_INDEXED_DIR)/summary.md
TEDQ_BOARD_PRODUCTION_DUAL_INDEXED_EVIDENCE_MANIFEST := $(TEDQ_BOARD_PRODUCTION_DUAL_INDEXED_DIR)/evidence_manifest.json
TEDQ_BOARD_PRODUCTION_DUAL_INDEXED_EVIDENCE_DOCTOR_MD := $(TEDQ_BOARD_PRODUCTION_DUAL_INDEXED_DIR)/evidence_doctor.md

TEDQ_BOARD_COMPONENT_RUN_LABEL ?= component_ablation_repeated
TEDQ_BOARD_COMPONENT_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(TEDQ_BOARD_COMPONENT_RUN_LABEL)
TEDQ_BOARD_COMPONENT_SUMMARY := $(TEDQ_BOARD_COMPONENT_REPEATED_DIR)/summary.md
TEDQ_BOARD_COMPONENT_EVIDENCE_MANIFEST := $(TEDQ_BOARD_COMPONENT_REPEATED_DIR)/evidence_manifest.json
TEDQ_BOARD_COMPONENT_EVIDENCE_DOCTOR_MD := $(TEDQ_BOARD_COMPONENT_REPEATED_DIR)/evidence_doctor.md

TEDQ_BOARD_ROW_SOURCE_RUN_LABEL ?= row_source_expansion_repeated
TEDQ_BOARD_ROW_SOURCE_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(TEDQ_BOARD_ROW_SOURCE_RUN_LABEL)
TEDQ_BOARD_ROW_SOURCE_SUMMARY := $(TEDQ_BOARD_ROW_SOURCE_REPEATED_DIR)/summary.md
TEDQ_BOARD_ROW_SOURCE_EVIDENCE_MANIFEST := $(TEDQ_BOARD_ROW_SOURCE_REPEATED_DIR)/evidence_manifest.json
TEDQ_BOARD_ROW_SOURCE_EVIDENCE_DOCTOR_MD := $(TEDQ_BOARD_ROW_SOURCE_REPEATED_DIR)/evidence_doctor.md
TEDQ_BOARD_ROW_SOURCE_DUAL_DIR ?= $(OUTPUT_DIR_BOARD)/row_source_expansion_dual_indexed_repeated
TEDQ_BOARD_ROW_SOURCE_DUAL_SUMMARY := $(TEDQ_BOARD_ROW_SOURCE_DUAL_DIR)/summary.md
TEDQ_BOARD_ROW_SOURCE_DUAL_EVIDENCE_MANIFEST := $(TEDQ_BOARD_ROW_SOURCE_DUAL_DIR)/evidence_manifest.json
TEDQ_BOARD_ROW_SOURCE_DUAL_EVIDENCE_DOCTOR_MD := $(TEDQ_BOARD_ROW_SOURCE_DUAL_DIR)/evidence_doctor.md
TEDQ_BOARD_ROW_SOURCE_CORRESPONDENCE_DIR ?= $(OUTPUT_DIR_BOARD)/row_source_expansion_correspondence_repeated
TEDQ_BOARD_ROW_SOURCE_CORRESPONDENCE_SUMMARY := $(TEDQ_BOARD_ROW_SOURCE_CORRESPONDENCE_DIR)/summary.md
TEDQ_BOARD_ROW_SOURCE_CORRESPONDENCE_EVIDENCE_MANIFEST := $(TEDQ_BOARD_ROW_SOURCE_CORRESPONDENCE_DIR)/evidence_manifest.json
TEDQ_BOARD_ROW_SOURCE_CORRESPONDENCE_EVIDENCE_DOCTOR_MD := $(TEDQ_BOARD_ROW_SOURCE_CORRESPONDENCE_DIR)/evidence_doctor.md
TEDQ_BOARD_ROW_SOURCE_FAMILY_LABEL ?= row_source_family_comparison_repeated
TEDQ_BOARD_ROW_SOURCE_FAMILY_DIR ?= $(OUTPUT_DIR_BOARD)/$(TEDQ_BOARD_ROW_SOURCE_FAMILY_LABEL)
TEDQ_BOARD_ROW_SOURCE_FAMILY_SUMMARY := $(TEDQ_BOARD_ROW_SOURCE_FAMILY_DIR)/summary.md
TEDQ_BOARD_ROW_SOURCE_FAMILY_EVIDENCE_MANIFEST := $(TEDQ_BOARD_ROW_SOURCE_FAMILY_DIR)/evidence_manifest.json
TEDQ_BOARD_ROW_SOURCE_FAMILY_EVIDENCE_DOCTOR_MD := $(TEDQ_BOARD_ROW_SOURCE_FAMILY_DIR)/evidence_doctor.md
TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_LABEL ?= indexed_direct_gather_family_comparison_repeated
TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_DIR ?= $(OUTPUT_DIR_BOARD)/$(TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_LABEL)
TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_SUMMARY := $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_DIR)/summary.md
TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_EVIDENCE_MANIFEST := $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_DIR)/evidence_manifest.json
TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_EVIDENCE_DOCTOR_MD := $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_DIR)/evidence_doctor.md
TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_LABEL ?= correspondence_direct_index_stream_repeated
TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_DIR ?= $(OUTPUT_DIR_BOARD)/$(TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_LABEL)
TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_SUMMARY := $(TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_DIR)/summary.md
TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_EVIDENCE_MANIFEST := $(TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_DIR)/evidence_manifest.json
TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_EVIDENCE_DOCTOR_MD := $(TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_DIR)/evidence_doctor.md
TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_LABEL ?= correspondence_segment_load_repeated
TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_DIR ?= $(OUTPUT_DIR_BOARD)/$(TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_LABEL)
TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_SUMMARY := $(TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_DIR)/summary.md
TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_EVIDENCE_MANIFEST := $(TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_DIR)/evidence_manifest.json
TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_EVIDENCE_DOCTOR_MD := $(TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_DIR)/evidence_doctor.md
TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_LABEL ?= correspondence_index_locality_ablation_repeated
TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_DIR ?= $(OUTPUT_DIR_BOARD)/$(TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_LABEL)
TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_SUMMARY := $(TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_DIR)/summary.md
TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_EVIDENCE_MANIFEST := $(TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_DIR)/evidence_manifest.json
TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_EVIDENCE_DOCTOR_MD := $(TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_DIR)/evidence_doctor.md
TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_LABEL ?= correspondence_point_type_layout_repeated
TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_DIR ?= $(OUTPUT_DIR_BOARD)/$(TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_LABEL)
TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_SUMMARY := $(TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_DIR)/summary.md
TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_EVIDENCE_MANIFEST := $(TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_DIR)/evidence_manifest.json
TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_EVIDENCE_DOCTOR_MD := $(TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_DIR)/evidence_doctor.md
TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_LABEL ?= indexed_direct_gather_point_type_layout_repeated
TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_DIR ?= $(OUTPUT_DIR_BOARD)/$(TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_LABEL)
TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_SUMMARY := $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_DIR)/summary.md
TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_EVIDENCE_MANIFEST := $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_DIR)/evidence_manifest.json
TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_EVIDENCE_DOCTOR_MD := $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_DIR)/evidence_doctor.md

$(TEDQ_BOARD_REPEATED_DIR) $(TEDQ_BOARD_PRODUCTION_REPEATED_DIR) $(TEDQ_BOARD_COMPONENT_REPEATED_DIR) $(TEDQ_BOARD_ROW_SOURCE_REPEATED_DIR) $(TEDQ_BOARD_ROW_SOURCE_DUAL_DIR) $(TEDQ_BOARD_ROW_SOURCE_CORRESPONDENCE_DIR) $(TEDQ_BOARD_ROW_SOURCE_FAMILY_DIR) $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_DIR) $(TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_DIR) $(TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_DIR) $(TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_DIR) $(TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_DIR) $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_DIR) $(OUTPUT_DIR_BOARD)/test_smoke:
	mkdir -p $@

run_qemu_bench_smoke:
	@$(MAKE) -C $(CURDIR) run_bench_std BENCH_ARGS="$(QEMU_SMOKE_BENCH_ARGS)"
	@$(MAKE) -C $(CURDIR) run_bench_rvv BENCH_ARGS="$(QEMU_SMOKE_BENCH_ARGS)"

generate_qemu_smoke_evidence_manifest: run_qemu_bench_smoke
	$(PYTHON) script/generate_tedq_qemu_evidence_manifest.py \
		--std-log $(BENCH_STD_OUTPUT_FILE) \
		--rvv-log $(BENCH_RVV_OUTPUT_FILE) \
		--asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm \
		--output $(QEMU_SMOKE_EVIDENCE_MANIFEST)

run_qemu_smoke_evidence_doctor: generate_qemu_smoke_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(QEMU_SMOKE_EVIDENCE_MANIFEST) \
		--output $(QEMU_SMOKE_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_qemu_correctness_state:
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(TEST_STD_OUTPUT_FILE) \
		--path $(TEST_RVV_OUTPUT_FILE) \
		--target run_test_compare \
		--backend qemu \
		--run-label qemu-tedq-correctness-phase013 \
		--case-filter indexed-direct-gather-point-type-layout-correctness \
		--evidence-role qemu_correctness \
		--doc-ref README.zh.md \
		--doc-ref doc/transformation_estimation_dual_quaternion-evaluation.zh.md \
		--doc-ref doc/correctness-tests.zh.md \
		--doc-ref doc-rvv/registration/transformation_estimation_dual_quaternion-RVV.zh.md \
		--doc-ref doc/phases/005-row-source-expansion/result.zh.md \
		--doc-ref doc/phases/007-indexed-direct-gather-family-expansion/result.zh.md \
		--doc-ref doc/phases/009-correspondence-segment-load/result.zh.md \
		--doc-ref doc/phases/010-correspondence-index-locality/plan.zh.md \
		--doc-ref doc/phases/010-correspondence-index-locality/result.zh.md \
		--doc-ref doc/phases/011-row-source-taxonomy-and-pattern-boundary/result.zh.md \
		--doc-ref doc/phases/012-point-type-layout-expansion/plan.zh.md \
		--doc-ref doc/phases/012-point-type-layout-expansion/result.zh.md \
		--doc-ref doc/phases/013-indexed-direct-gather-point-type-layout/plan.zh.md \
		--doc-ref doc/phases/013-indexed-direct-gather-point-type-layout/result.zh.md

record_qemu_smoke_evidence_state: run_qemu_smoke_evidence_doctor
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(BENCH_STD_OUTPUT_FILE) \
		--path $(BENCH_RVV_OUTPUT_FILE) \
		--manifest $(QEMU_SMOKE_EVIDENCE_MANIFEST) \
		--doctor $(QEMU_SMOKE_EVIDENCE_DOCTOR_MD) \
		--target run_qemu_bench_smoke \
		--backend qemu \
		--run-label qemu-tedq-production-public-retained-row-sources-smoke-phase016 \
		--case-filter production-public-retained-row-sources \
		--evidence-role qemu_smoke_only \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc-rvv/registration/transformation_estimation_dual_quaternion-RVV.zh.md \
		--doc-ref doc/phases/009-correspondence-segment-load/result.zh.md \
		--doc-ref doc/phases/010-correspondence-index-locality/plan.zh.md \
		--doc-ref doc/phases/010-correspondence-index-locality/result.zh.md \
		--doc-ref doc/phases/011-row-source-taxonomy-and-pattern-boundary/result.zh.md \
		--doc-ref doc/phases/012-point-type-layout-expansion/plan.zh.md \
		--doc-ref doc/phases/012-point-type-layout-expansion/result.zh.md \
		--doc-ref doc/phases/016-production-adoption-scope-finalization/plan.zh.md \
		--doc-ref doc/phases/016-production-adoption-scope-finalization/result.zh.md

run_board_test_smoke: deploy_board | $(OUTPUT_DIR_BOARD)/test_smoke
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_DIR)/log/board/test_smoke' REMOTE_TEST_ARGS='$(REMOTE_TEST_ARGS)'"
	@$(MAKE) -C $(CURDIR) fetch_board_logs OUTPUT_DIR_BOARD=$(OUTPUT_DIR_BOARD)/test_smoke REMOTE_BOARD_OUTPUT_DIR='$(REMOTE_DIR)/log/board/test_smoke'

collect_board_bench_rvv_accum_repeated: deploy_board | $(TEDQ_BOARD_REPEATED_DIR)
	@rm -rf "$(TEDQ_BOARD_REPEATED_DIR)"/run-*
	@for run in $$(seq 1 $(TEDQ_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TEDQ_BOARD_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(TEDQ_BOARD_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] ordered_cloud_pair repeated run $$run_id/$(TEDQ_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter ordered-cloud-pair --iterations $(TEDQ_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TEDQ_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_rvv_accum:
	$(PYTHON) script/generate_tedq_board_repeated_summary.py \
		--input-dir $(TEDQ_BOARD_REPEATED_DIR) \
		--output-dir $(TEDQ_BOARD_REPEATED_DIR) \
		--run-label $(TEDQ_BOARD_RUN_LABEL) \
		--case-filter ordered-cloud-pair \
		--expected-runs $(TEDQ_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"

run_board_evidence_doctor_rvv_accum: generate_board_evidence_manifest_rvv_accum
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TEDQ_BOARD_EVIDENCE_MANIFEST) \
		--output $(TEDQ_BOARD_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_rvv_accum_state: run_board_evidence_doctor_rvv_accum
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TEDQ_BOARD_SUMMARY) \
		--manifest $(TEDQ_BOARD_EVIDENCE_MANIFEST) \
		--doctor $(TEDQ_BOARD_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_rvv_accum_repeated \
		--backend board \
		--run-label board-tedq-ordered-cloud-pair-repeated-phase001 \
		--case-filter ordered-cloud-pair \
		--evidence-role pre_production_diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/phases/001-board-diagnostic/result.zh.md

run_board_bench_rvv_accum_repeated: collect_board_bench_rvv_accum_repeated record_board_rvv_accum_state

collect_board_bench_production_public_repeated: deploy_board | $(TEDQ_BOARD_PRODUCTION_REPEATED_DIR)
	@rm -rf "$(TEDQ_BOARD_PRODUCTION_REPEATED_DIR)"/run-*
	@for run in $$(seq 1 $(TEDQ_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TEDQ_BOARD_PRODUCTION_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(TEDQ_BOARD_PRODUCTION_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] production_public_retained_row_sources repeated run $$run_id/$(TEDQ_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter $(TEDQ_BOARD_PRODUCTION_CASE_FILTER) --iterations $(TEDQ_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TEDQ_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_production_public:
	$(PYTHON) script/generate_tedq_board_repeated_summary.py \
		--input-dir $(TEDQ_BOARD_PRODUCTION_REPEATED_DIR) \
		--output-dir $(TEDQ_BOARD_PRODUCTION_REPEATED_DIR) \
		--run-label $(TEDQ_BOARD_PRODUCTION_RUN_LABEL) \
		--case-filter $(TEDQ_BOARD_PRODUCTION_CASE_FILTER) \
		--row-source-policy ordered-cloud-pair \
		--production-public \
		--expected-runs $(TEDQ_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"
	$(PYTHON) script/generate_tedq_board_repeated_summary.py \
		--input-dir $(TEDQ_BOARD_PRODUCTION_REPEATED_DIR) \
		--output-dir $(TEDQ_BOARD_PRODUCTION_SOURCE_INDEXED_DIR) \
		--run-label production_public_source_indexed_cloud_pair_repeated \
		--case-filter $(TEDQ_BOARD_PRODUCTION_CASE_FILTER) \
		--row-source-policy source-indexed-cloud-pair \
		--production-public \
		--expected-runs $(TEDQ_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"
	$(PYTHON) script/generate_tedq_board_repeated_summary.py \
		--input-dir $(TEDQ_BOARD_PRODUCTION_REPEATED_DIR) \
		--output-dir $(TEDQ_BOARD_PRODUCTION_DUAL_INDEXED_DIR) \
		--run-label production_public_dual_indexed_cloud_pair_repeated \
		--case-filter $(TEDQ_BOARD_PRODUCTION_CASE_FILTER) \
		--row-source-policy dual-indexed-cloud-pair \
		--production-public \
		--expected-runs $(TEDQ_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"

run_board_evidence_doctor_production_public: generate_board_evidence_manifest_production_public
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TEDQ_BOARD_PRODUCTION_EVIDENCE_MANIFEST) \
		--output $(TEDQ_BOARD_PRODUCTION_EVIDENCE_DOCTOR_MD) \
		--fail-on never
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TEDQ_BOARD_PRODUCTION_SOURCE_INDEXED_EVIDENCE_MANIFEST) \
		--output $(TEDQ_BOARD_PRODUCTION_SOURCE_INDEXED_EVIDENCE_DOCTOR_MD) \
		--fail-on never
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TEDQ_BOARD_PRODUCTION_DUAL_INDEXED_EVIDENCE_MANIFEST) \
		--output $(TEDQ_BOARD_PRODUCTION_DUAL_INDEXED_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_production_public_state: run_board_evidence_doctor_production_public
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TEDQ_BOARD_PRODUCTION_SUMMARY) \
		--manifest $(TEDQ_BOARD_PRODUCTION_EVIDENCE_MANIFEST) \
		--doctor $(TEDQ_BOARD_PRODUCTION_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_production_public_repeated \
		--backend board \
		--run-label board-tedq-production-public-retained-row-sources-repeated-phase016 \
		--case-filter $(TEDQ_BOARD_PRODUCTION_CASE_FILTER) \
		--evidence-role production_direct \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc-rvv/registration/transformation_estimation_dual_quaternion-RVV.zh.md \
		--doc-ref doc/phases/016-production-adoption-scope-finalization/plan.zh.md \
		--doc-ref doc/phases/016-production-adoption-scope-finalization/result.zh.md \
		--path $(TEDQ_BOARD_PRODUCTION_SOURCE_INDEXED_SUMMARY) \
		--path $(TEDQ_BOARD_PRODUCTION_SOURCE_INDEXED_EVIDENCE_MANIFEST) \
		--path $(TEDQ_BOARD_PRODUCTION_SOURCE_INDEXED_EVIDENCE_DOCTOR_MD) \
		--path $(TEDQ_BOARD_PRODUCTION_DUAL_INDEXED_SUMMARY) \
		--path $(TEDQ_BOARD_PRODUCTION_DUAL_INDEXED_EVIDENCE_MANIFEST) \
		--path $(TEDQ_BOARD_PRODUCTION_DUAL_INDEXED_EVIDENCE_DOCTOR_MD)

run_board_bench_production_public_repeated: collect_board_bench_production_public_repeated record_board_production_public_state

collect_board_bench_component_ablation_repeated: deploy_board | $(TEDQ_BOARD_COMPONENT_REPEATED_DIR)
	@rm -rf "$(TEDQ_BOARD_COMPONENT_REPEATED_DIR)"/run-*
	@for run in $$(seq 1 $(TEDQ_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TEDQ_BOARD_COMPONENT_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(TEDQ_BOARD_COMPONENT_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] component_ablation repeated run $$run_id/$(TEDQ_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter component-ablation --iterations $(TEDQ_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TEDQ_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_component_ablation:
	$(PYTHON) script/generate_tedq_board_repeated_summary.py \
		--input-dir $(TEDQ_BOARD_COMPONENT_REPEATED_DIR) \
		--output-dir $(TEDQ_BOARD_COMPONENT_REPEATED_DIR) \
		--run-label $(TEDQ_BOARD_COMPONENT_RUN_LABEL) \
		--case-filter component-ablation \
		--expected-runs $(TEDQ_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)" \
		--component-ablation

run_board_evidence_doctor_component_ablation: generate_board_evidence_manifest_component_ablation
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TEDQ_BOARD_COMPONENT_EVIDENCE_MANIFEST) \
		--output $(TEDQ_BOARD_COMPONENT_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_component_ablation_state: run_board_evidence_doctor_component_ablation
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TEDQ_BOARD_COMPONENT_SUMMARY) \
		--manifest $(TEDQ_BOARD_COMPONENT_EVIDENCE_MANIFEST) \
		--doctor $(TEDQ_BOARD_COMPONENT_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_component_ablation_repeated \
		--backend board \
		--run-label board-tedq-component-ablation-repeated-phase003 \
		--case-filter component-ablation \
		--evidence-role component_ablation \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/phases/003-component-ablation/result.zh.md

run_board_bench_component_ablation_repeated: collect_board_bench_component_ablation_repeated record_board_component_ablation_state
run_board_bench_repeated: run_board_bench_component_ablation_repeated

collect_board_bench_row_source_expansion_repeated: deploy_board | $(TEDQ_BOARD_ROW_SOURCE_REPEATED_DIR)
	@rm -rf "$(TEDQ_BOARD_ROW_SOURCE_REPEATED_DIR)"/run-*
	@for run in $$(seq 1 $(TEDQ_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TEDQ_BOARD_ROW_SOURCE_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(TEDQ_BOARD_ROW_SOURCE_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] row_source_expansion repeated run $$run_id/$(TEDQ_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter row-source-expansion --iterations $(TEDQ_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TEDQ_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_row_source_expansion:
	$(PYTHON) script/generate_tedq_board_repeated_summary.py \
		--input-dir $(TEDQ_BOARD_ROW_SOURCE_REPEATED_DIR) \
		--output-dir $(TEDQ_BOARD_ROW_SOURCE_REPEATED_DIR) \
		--run-label $(TEDQ_BOARD_ROW_SOURCE_RUN_LABEL) \
		--case-filter row-source-expansion \
		--row-source-policy source-indexed-cloud-pair \
		--expected-runs $(TEDQ_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"
	$(PYTHON) script/generate_tedq_board_repeated_summary.py \
		--input-dir $(TEDQ_BOARD_ROW_SOURCE_REPEATED_DIR) \
		--output-dir $(TEDQ_BOARD_ROW_SOURCE_DUAL_DIR) \
		--run-label row_source_expansion_dual_indexed_repeated \
		--case-filter row-source-expansion \
		--row-source-policy dual-indexed-cloud-pair \
		--expected-runs $(TEDQ_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"
	$(PYTHON) script/generate_tedq_board_repeated_summary.py \
		--input-dir $(TEDQ_BOARD_ROW_SOURCE_REPEATED_DIR) \
		--output-dir $(TEDQ_BOARD_ROW_SOURCE_CORRESPONDENCE_DIR) \
		--run-label row_source_expansion_correspondence_repeated \
		--case-filter row-source-expansion \
		--row-source-policy correspondence-pair \
		--expected-runs $(TEDQ_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"

run_board_evidence_doctor_row_source_expansion: generate_board_evidence_manifest_row_source_expansion
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TEDQ_BOARD_ROW_SOURCE_EVIDENCE_MANIFEST) \
		--output $(TEDQ_BOARD_ROW_SOURCE_EVIDENCE_DOCTOR_MD) \
		--fail-on never
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TEDQ_BOARD_ROW_SOURCE_DUAL_EVIDENCE_MANIFEST) \
		--output $(TEDQ_BOARD_ROW_SOURCE_DUAL_EVIDENCE_DOCTOR_MD) \
		--fail-on never
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TEDQ_BOARD_ROW_SOURCE_CORRESPONDENCE_EVIDENCE_MANIFEST) \
		--output $(TEDQ_BOARD_ROW_SOURCE_CORRESPONDENCE_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_row_source_expansion_state: run_board_evidence_doctor_row_source_expansion
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TEDQ_BOARD_ROW_SOURCE_SUMMARY) \
		--manifest $(TEDQ_BOARD_ROW_SOURCE_EVIDENCE_MANIFEST) \
		--doctor $(TEDQ_BOARD_ROW_SOURCE_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_row_source_expansion_repeated \
		--backend board \
		--run-label board-tedq-row-source-expansion-repeated-phase005 \
		--case-filter row-source-expansion \
		--evidence-role diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/phases/005-row-source-expansion/result.zh.md \
		--path $(TEDQ_BOARD_ROW_SOURCE_DUAL_SUMMARY) \
		--path $(TEDQ_BOARD_ROW_SOURCE_DUAL_EVIDENCE_MANIFEST) \
		--path $(TEDQ_BOARD_ROW_SOURCE_DUAL_EVIDENCE_DOCTOR_MD) \
		--path $(TEDQ_BOARD_ROW_SOURCE_CORRESPONDENCE_SUMMARY) \
		--path $(TEDQ_BOARD_ROW_SOURCE_CORRESPONDENCE_EVIDENCE_MANIFEST) \
		--path $(TEDQ_BOARD_ROW_SOURCE_CORRESPONDENCE_EVIDENCE_DOCTOR_MD)

run_board_bench_row_source_expansion_repeated: collect_board_bench_row_source_expansion_repeated record_board_row_source_expansion_state

collect_board_bench_row_source_family_comparison_repeated: deploy_board | $(TEDQ_BOARD_ROW_SOURCE_FAMILY_DIR)
	@rm -rf "$(TEDQ_BOARD_ROW_SOURCE_FAMILY_DIR)"/run-*
	@for run in $$(seq 1 $(TEDQ_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TEDQ_BOARD_ROW_SOURCE_FAMILY_LABEL)/run-$$run_id"; \
		local_dir="$(TEDQ_BOARD_ROW_SOURCE_FAMILY_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] row_source_family_comparison repeated run $$run_id/$(TEDQ_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter row-source-family-comparison --iterations $(TEDQ_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TEDQ_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_row_source_family_comparison:
	$(PYTHON) script/generate_tedq_row_source_family_summary.py \
		--input-dir $(TEDQ_BOARD_ROW_SOURCE_FAMILY_DIR) \
		--output-dir $(TEDQ_BOARD_ROW_SOURCE_FAMILY_DIR) \
		--run-label $(TEDQ_BOARD_ROW_SOURCE_FAMILY_LABEL) \
		--case-filter row-source-family-comparison \
		--expected-runs $(TEDQ_BOARD_REPEATED_RUNS) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"

run_board_evidence_doctor_row_source_family_comparison: generate_board_evidence_manifest_row_source_family_comparison
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TEDQ_BOARD_ROW_SOURCE_FAMILY_EVIDENCE_MANIFEST) \
		--output $(TEDQ_BOARD_ROW_SOURCE_FAMILY_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_row_source_family_comparison_state: run_board_evidence_doctor_row_source_family_comparison
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TEDQ_BOARD_ROW_SOURCE_FAMILY_SUMMARY) \
		--manifest $(TEDQ_BOARD_ROW_SOURCE_FAMILY_EVIDENCE_MANIFEST) \
		--doctor $(TEDQ_BOARD_ROW_SOURCE_FAMILY_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_row_source_family_comparison_repeated \
		--backend board \
		--run-label board-tedq-row-source-family-comparison-phase006 \
		--case-filter row-source-family-comparison \
		--evidence-role diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/phases/006-test-support-split-and-row-source-family-comparison/result.zh.md

run_board_bench_row_source_family_comparison_repeated: collect_board_bench_row_source_family_comparison_repeated record_board_row_source_family_comparison_state

collect_board_bench_indexed_direct_gather_family_comparison_repeated: deploy_board | $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_DIR)
	@rm -rf "$(TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_DIR)"/run-*
	@for run in $$(seq 1 $(TEDQ_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_LABEL)/run-$$run_id"; \
		local_dir="$(TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] indexed_direct_gather_family_comparison repeated run $$run_id/$(TEDQ_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter indexed-direct-gather-family-comparison --iterations $(TEDQ_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TEDQ_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_indexed_direct_gather_family_comparison:
	$(PYTHON) script/generate_tedq_indexed_direct_gather_family_summary.py \
		--input-dir $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_DIR) \
		--output-dir $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_DIR) \
		--run-label $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_LABEL) \
		--case-filter indexed-direct-gather-family-comparison \
		--expected-runs $(TEDQ_BOARD_REPEATED_RUNS) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"

run_board_evidence_doctor_indexed_direct_gather_family_comparison: generate_board_evidence_manifest_indexed_direct_gather_family_comparison
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_EVIDENCE_MANIFEST) \
		--output $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_indexed_direct_gather_family_comparison_state: run_board_evidence_doctor_indexed_direct_gather_family_comparison
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_SUMMARY) \
		--manifest $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_EVIDENCE_MANIFEST) \
		--doctor $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_FAMILY_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_indexed_direct_gather_family_comparison_repeated \
		--backend board \
		--run-label board-tedq-indexed-direct-gather-family-comparison-phase007 \
		--case-filter indexed-direct-gather-family-comparison \
		--evidence-role diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/phases/007-indexed-direct-gather-family-expansion/result.zh.md

run_board_bench_indexed_direct_gather_family_comparison_repeated: collect_board_bench_indexed_direct_gather_family_comparison_repeated record_board_indexed_direct_gather_family_comparison_state

collect_board_bench_correspondence_direct_index_stream_repeated: deploy_board | $(TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_DIR)
	@rm -rf "$(TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_DIR)"/run-*
	@for run in $$(seq 1 $(TEDQ_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_LABEL)/run-$$run_id"; \
		local_dir="$(TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] correspondence_direct_index_stream repeated run $$run_id/$(TEDQ_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter correspondence-direct-index-stream-comparison --iterations $(TEDQ_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TEDQ_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_correspondence_direct_index_stream:
	$(PYTHON) script/generate_tedq_correspondence_direct_index_stream_summary.py \
		--input-dir $(TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_DIR) \
		--output-dir $(TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_DIR) \
		--run-label $(TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_LABEL) \
		--case-filter correspondence-direct-index-stream-comparison \
		--expected-runs $(TEDQ_BOARD_REPEATED_RUNS) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"

run_board_evidence_doctor_correspondence_direct_index_stream: generate_board_evidence_manifest_correspondence_direct_index_stream
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_EVIDENCE_MANIFEST) \
		--output $(TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_correspondence_direct_index_stream_state: run_board_evidence_doctor_correspondence_direct_index_stream
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_SUMMARY) \
		--manifest $(TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_EVIDENCE_MANIFEST) \
		--doctor $(TEDQ_BOARD_CORRESPONDENCE_DIRECT_INDEX_STREAM_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_correspondence_direct_index_stream_repeated \
		--backend board \
		--run-label board-tedq-correspondence-direct-index-stream-phase008 \
		--case-filter correspondence-direct-index-stream-comparison \
		--evidence-role diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/phases/008-correspondence-direct-index-stream/result.zh.md

run_board_bench_correspondence_direct_index_stream_repeated: collect_board_bench_correspondence_direct_index_stream_repeated record_board_correspondence_direct_index_stream_state

collect_board_bench_correspondence_segment_load_repeated: deploy_board | $(TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_DIR)
	@rm -rf "$(TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_DIR)"/run-*
	@for run in $$(seq 1 $(TEDQ_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_LABEL)/run-$$run_id"; \
		local_dir="$(TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] correspondence_segment_load repeated run $$run_id/$(TEDQ_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter correspondence-segment-load-comparison --iterations $(TEDQ_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TEDQ_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_correspondence_segment_load:
	$(PYTHON) script/generate_tedq_correspondence_segment_load_summary.py \
		--input-dir $(TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_DIR) \
		--output-dir $(TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_DIR) \
		--run-label $(TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_LABEL) \
		--case-filter correspondence-segment-load-comparison \
		--expected-runs $(TEDQ_BOARD_REPEATED_RUNS) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"

run_board_evidence_doctor_correspondence_segment_load: generate_board_evidence_manifest_correspondence_segment_load
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_EVIDENCE_MANIFEST) \
		--output $(TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_correspondence_segment_load_state: run_board_evidence_doctor_correspondence_segment_load
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_SUMMARY) \
		--manifest $(TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_EVIDENCE_MANIFEST) \
		--doctor $(TEDQ_BOARD_CORRESPONDENCE_SEGMENT_LOAD_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_correspondence_segment_load_repeated \
		--backend board \
		--run-label board-tedq-correspondence-segment-load-phase009 \
		--case-filter correspondence-segment-load-comparison \
		--evidence-role diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/phases/009-correspondence-segment-load/result.zh.md

run_board_bench_correspondence_segment_load_repeated: collect_board_bench_correspondence_segment_load_repeated record_board_correspondence_segment_load_state

collect_board_bench_correspondence_index_locality_repeated: deploy_board | $(TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_DIR)
	@rm -rf "$(TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_DIR)"/run-*
	@for run in $$(seq 1 $(TEDQ_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_LABEL)/run-$$run_id"; \
		local_dir="$(TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] correspondence_index_locality repeated run $$run_id/$(TEDQ_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter correspondence-index-locality-ablation --iterations $(TEDQ_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TEDQ_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_correspondence_index_locality:
	$(PYTHON) script/generate_tedq_correspondence_index_locality_summary.py \
		--input-dir $(TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_DIR) \
		--output-dir $(TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_DIR) \
		--run-label $(TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_LABEL) \
		--case-filter correspondence-index-locality-ablation \
		--expected-runs $(TEDQ_BOARD_REPEATED_RUNS) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"

run_board_evidence_doctor_correspondence_index_locality: generate_board_evidence_manifest_correspondence_index_locality
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_EVIDENCE_MANIFEST) \
		--output $(TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_correspondence_index_locality_state: run_board_evidence_doctor_correspondence_index_locality
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_SUMMARY) \
		--manifest $(TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_EVIDENCE_MANIFEST) \
		--doctor $(TEDQ_BOARD_CORRESPONDENCE_INDEX_LOCALITY_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_correspondence_index_locality_repeated \
		--backend board \
		--run-label board-tedq-correspondence-index-locality-phase010 \
		--case-filter correspondence-index-locality-ablation \
		--evidence-role diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/phases/010-correspondence-index-locality/plan.zh.md \
		--doc-ref doc/phases/010-correspondence-index-locality/result.zh.md

run_board_bench_correspondence_index_locality_repeated: collect_board_bench_correspondence_index_locality_repeated record_board_correspondence_index_locality_state

collect_board_bench_correspondence_point_type_layout_repeated: deploy_board | $(TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_DIR)
	@rm -rf "$(TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_DIR)"/run-*
	@for run in $$(seq 1 $(TEDQ_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_LABEL)/run-$$run_id"; \
		local_dir="$(TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] correspondence_point_type_layout repeated run $$run_id/$(TEDQ_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter correspondence-point-type-layout --iterations $(TEDQ_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TEDQ_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_correspondence_point_type_layout:
	$(PYTHON) script/generate_tedq_correspondence_point_type_layout_summary.py \
		--input-dir $(TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_DIR) \
		--output-dir $(TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_DIR) \
		--run-label $(TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_LABEL) \
		--case-filter correspondence-point-type-layout \
		--expected-runs $(TEDQ_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"

run_board_evidence_doctor_correspondence_point_type_layout: generate_board_evidence_manifest_correspondence_point_type_layout
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_EVIDENCE_MANIFEST) \
		--output $(TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_correspondence_point_type_layout_state: run_board_evidence_doctor_correspondence_point_type_layout
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_SUMMARY) \
		--manifest $(TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_EVIDENCE_MANIFEST) \
		--doctor $(TEDQ_BOARD_CORRESPONDENCE_POINT_TYPE_LAYOUT_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_correspondence_point_type_layout_repeated \
		--backend board \
		--run-label board-tedq-correspondence-point-type-layout-phase012 \
		--case-filter correspondence-point-type-layout \
		--evidence-role diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/phases/012-point-type-layout-expansion/result.zh.md

run_board_bench_correspondence_point_type_layout_repeated: collect_board_bench_correspondence_point_type_layout_repeated record_board_correspondence_point_type_layout_state

collect_board_bench_indexed_direct_gather_point_type_layout_repeated: deploy_board | $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_DIR)
	@rm -rf "$(TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_DIR)"/run-*
	@for run in $$(seq 1 $(TEDQ_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_LABEL)/run-$$run_id"; \
		local_dir="$(TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] indexed_direct_gather_point_type_layout repeated run $$run_id/$(TEDQ_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter indexed-direct-gather-point-type-layout --iterations $(TEDQ_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TEDQ_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_indexed_direct_gather_point_type_layout:
	$(PYTHON) script/generate_tedq_indexed_direct_gather_point_type_layout_summary.py \
		--input-dir $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_DIR) \
		--output-dir $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_DIR) \
		--run-label $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_LABEL) \
		--case-filter indexed-direct-gather-point-type-layout \
		--expected-runs $(TEDQ_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"

run_board_evidence_doctor_indexed_direct_gather_point_type_layout: generate_board_evidence_manifest_indexed_direct_gather_point_type_layout
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_EVIDENCE_MANIFEST) \
		--output $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_indexed_direct_gather_point_type_layout_state: run_board_evidence_doctor_indexed_direct_gather_point_type_layout
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_SUMMARY) \
		--manifest $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_EVIDENCE_MANIFEST) \
		--doctor $(TEDQ_BOARD_INDEXED_DIRECT_GATHER_POINT_TYPE_LAYOUT_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_indexed_direct_gather_point_type_layout_repeated \
		--backend board \
		--run-label board-tedq-indexed-direct-gather-point-type-layout-phase013 \
		--case-filter indexed-direct-gather-point-type-layout \
		--evidence-role diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/phases/013-indexed-direct-gather-point-type-layout/result.zh.md

run_board_bench_indexed_direct_gather_point_type_layout_repeated: collect_board_bench_indexed_direct_gather_point_type_layout_repeated record_board_indexed_direct_gather_point_type_layout_state

run_evidence_doctor: run_qemu_smoke_evidence_doctor
generate_evidence_manifest: generate_qemu_smoke_evidence_manifest

evidence_status:
	$(PYTHON) ../../script/evidence_registry.py check \
		--registry $(EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/*.log' \
		--scan-glob 'test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/*.md' \
		--scan-glob 'test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/*.json' \
		--scan-glob 'test-rvv/registration/transformation_estimation_dual_quaternion/log/board/**/summary.md' \
		--scan-glob 'test-rvv/registration/transformation_estimation_dual_quaternion/log/board/**/evidence_doctor.md' \
		--scan-glob 'test-rvv/registration/transformation_estimation_dual_quaternion/log/board/**/evidence_manifest.json' \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/README.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/benchmark-and-evidence.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/optimization-evidence.zh.md \
		--doc doc-rvv/registration/transformation_estimation_dual_quaternion-RVV.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/000-current-state-and-gaps/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/001-board-diagnostic/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/002-production-integration-plan/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/003-component-ablation/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/004-production-boundary-probe/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/005-row-source-expansion/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/006-test-support-split-and-row-source-family-comparison/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/007-indexed-direct-gather-family-expansion/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/008-correspondence-direct-index-stream/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/009-correspondence-segment-load/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/010-correspondence-index-locality/plan.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/010-correspondence-index-locality/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/011-row-source-taxonomy-and-pattern-boundary/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/012-point-type-layout-expansion/plan.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/012-point-type-layout-expansion/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/013-indexed-direct-gather-point-type-layout/plan.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/013-indexed-direct-gather-point-type-layout/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/015-production-integration-all-row-sources/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/016-production-adoption-scope-finalization/plan.zh.md \
		--doc test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/016-production-adoption-scope-finalization/result.zh.md \
		--require-doc-ref \
		--fail-on never

.PHONY: run_board_test_smoke collect_board_bench_rvv_accum_repeated generate_board_evidence_manifest_rvv_accum run_board_evidence_doctor_rvv_accum record_board_rvv_accum_state run_board_bench_rvv_accum_repeated collect_board_bench_production_public_repeated generate_board_evidence_manifest_production_public run_board_evidence_doctor_production_public record_board_production_public_state run_board_bench_production_public_repeated collect_board_bench_component_ablation_repeated generate_board_evidence_manifest_component_ablation run_board_evidence_doctor_component_ablation record_board_component_ablation_state run_board_bench_component_ablation_repeated collect_board_bench_row_source_expansion_repeated generate_board_evidence_manifest_row_source_expansion run_board_evidence_doctor_row_source_expansion record_board_row_source_expansion_state run_board_bench_row_source_expansion_repeated collect_board_bench_row_source_family_comparison_repeated generate_board_evidence_manifest_row_source_family_comparison run_board_evidence_doctor_row_source_family_comparison record_board_row_source_family_comparison_state run_board_bench_row_source_family_comparison_repeated collect_board_bench_indexed_direct_gather_family_comparison_repeated generate_board_evidence_manifest_indexed_direct_gather_family_comparison run_board_evidence_doctor_indexed_direct_gather_family_comparison record_board_indexed_direct_gather_family_comparison_state run_board_bench_indexed_direct_gather_family_comparison_repeated collect_board_bench_correspondence_direct_index_stream_repeated generate_board_evidence_manifest_correspondence_direct_index_stream run_board_evidence_doctor_correspondence_direct_index_stream record_board_correspondence_direct_index_stream_state run_board_bench_correspondence_direct_index_stream_repeated collect_board_bench_correspondence_segment_load_repeated generate_board_evidence_manifest_correspondence_segment_load run_board_evidence_doctor_correspondence_segment_load record_board_correspondence_segment_load_state run_board_bench_correspondence_segment_load_repeated collect_board_bench_correspondence_index_locality_repeated generate_board_evidence_manifest_correspondence_index_locality run_board_evidence_doctor_correspondence_index_locality record_board_correspondence_index_locality_state run_board_bench_correspondence_index_locality_repeated collect_board_bench_correspondence_point_type_layout_repeated generate_board_evidence_manifest_correspondence_point_type_layout run_board_evidence_doctor_correspondence_point_type_layout record_board_correspondence_point_type_layout_state run_board_bench_correspondence_point_type_layout_repeated collect_board_bench_indexed_direct_gather_point_type_layout_repeated generate_board_evidence_manifest_indexed_direct_gather_point_type_layout run_board_evidence_doctor_indexed_direct_gather_point_type_layout record_board_indexed_direct_gather_point_type_layout_state run_board_bench_indexed_direct_gather_point_type_layout_repeated run_board_bench_repeated
