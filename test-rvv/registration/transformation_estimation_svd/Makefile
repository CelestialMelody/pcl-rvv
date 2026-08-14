# =============================================================================
#  PCL registration/transformation_estimation_svd RVV diagnostic Makefile
# =============================================================================

TOPIC  := transformation_estimation_svd
MODULE := registration

FOCUS_DIR      := pcl/registration/include/pcl/registration/impl/transformation_estimation_svd.hpp
FILTER_REPORT  := log/filtered_transformation_estimation_svd.log
ANALYZE_REPORT := log/analyze_transformation_estimation_svd.log

TARGET_BENCH     := bench_transformation_estimation_svd
TARGET_BENCH_STD := bench_transformation_estimation_svd_std
TARGET_BENCH_RVV := bench_transformation_estimation_svd_rvv
SRCS_BENCH       := src/bench_tesvd.cpp

TARGET_TEST     := test_transformation_estimation_svd
TARGET_TEST_STD := test_transformation_estimation_svd_std
TARGET_TEST_RVV := test_transformation_estimation_svd_rvv
SRCS_TEST       := src/test_tesvd.cpp

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
EVIDENCE_REGISTRY := $(LOG_DIR)/evidence_registry.json
TESVD_QEMU_CORRECTNESS_RUN_LABEL ?= qemu-tesvd-correctness-all-phase060
TESVD_QEMU_CORRECTNESS_CASE_FILTER ?= all-correctness
TESVD_QEMU_SMOKE_RUN_LABEL ?= qemu-tesvd-fused-ordered-cloud-pair-smoke-phase000
TESVD_QEMU_SMOKE_CASE_FILTER ?= ordered-cloud-pair
TESVD_QEMU_SMOKE_PHASE_DOC_REF ?= doc/phases/000-current-state-and-gaps/result.zh.md

TESVD_BOARD_REPEATED_RUNS ?= 5
TESVD_BOARD_BENCH_ITERATIONS ?= 20
TESVD_BOARD_BENCH_WARMUP_ITERATIONS ?= 5
TESVD_BOARD_RUN_LABEL ?= fused_full_cloud_repeated
TESVD_BOARD_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(TESVD_BOARD_RUN_LABEL)
TESVD_BOARD_SUMMARY := $(TESVD_BOARD_REPEATED_DIR)/summary.md
TESVD_BOARD_EVIDENCE_MANIFEST := $(TESVD_BOARD_REPEATED_DIR)/evidence_manifest.json
TESVD_BOARD_EVIDENCE_DOCTOR_MD := $(TESVD_BOARD_REPEATED_DIR)/evidence_doctor.md
TESVD_BOARD_SOURCE_INDEXED_RUN_LABEL ?= source_indexed_cloud_pair_repeated
TESVD_BOARD_SOURCE_INDEXED_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(TESVD_BOARD_SOURCE_INDEXED_RUN_LABEL)
TESVD_BOARD_SOURCE_INDEXED_SUMMARY := $(TESVD_BOARD_SOURCE_INDEXED_REPEATED_DIR)/summary.md
TESVD_BOARD_SOURCE_INDEXED_EVIDENCE_MANIFEST := $(TESVD_BOARD_SOURCE_INDEXED_REPEATED_DIR)/evidence_manifest.json
TESVD_BOARD_SOURCE_INDEXED_EVIDENCE_DOCTOR_MD := $(TESVD_BOARD_SOURCE_INDEXED_REPEATED_DIR)/evidence_doctor.md
TESVD_BOARD_PRODUCTION_RUN_LABEL ?= production_ordered_cloud_pair_repeated
TESVD_BOARD_PRODUCTION_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(TESVD_BOARD_PRODUCTION_RUN_LABEL)
TESVD_BOARD_PRODUCTION_SUMMARY := $(TESVD_BOARD_PRODUCTION_REPEATED_DIR)/summary.md
TESVD_BOARD_PRODUCTION_EVIDENCE_MANIFEST := $(TESVD_BOARD_PRODUCTION_REPEATED_DIR)/evidence_manifest.json
TESVD_BOARD_PRODUCTION_EVIDENCE_DOCTOR_MD := $(TESVD_BOARD_PRODUCTION_REPEATED_DIR)/evidence_doctor.md
TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_RUN_LABEL ?= production_source_indexed_cloud_pair_repeated
TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_RUN_LABEL)
TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_SUMMARY := $(TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_REPEATED_DIR)/summary.md
TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_EVIDENCE_MANIFEST := $(TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_REPEATED_DIR)/evidence_manifest.json
TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_EVIDENCE_DOCTOR_MD := $(TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_REPEATED_DIR)/evidence_doctor.md
TESVD_BOARD_PRODUCTION_DUAL_INDICES_RUN_LABEL ?= production_dual_indices_cloud_pair_repeated
TESVD_BOARD_PRODUCTION_DUAL_INDICES_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(TESVD_BOARD_PRODUCTION_DUAL_INDICES_RUN_LABEL)
TESVD_BOARD_PRODUCTION_DUAL_INDICES_SUMMARY := $(TESVD_BOARD_PRODUCTION_DUAL_INDICES_REPEATED_DIR)/summary.md
TESVD_BOARD_PRODUCTION_DUAL_INDICES_EVIDENCE_MANIFEST := $(TESVD_BOARD_PRODUCTION_DUAL_INDICES_REPEATED_DIR)/evidence_manifest.json
TESVD_BOARD_PRODUCTION_DUAL_INDICES_EVIDENCE_DOCTOR_MD := $(TESVD_BOARD_PRODUCTION_DUAL_INDICES_REPEATED_DIR)/evidence_doctor.md
TESVD_BOARD_PRODUCTION_CORRESPONDENCE_RUN_LABEL ?= production_correspondence_pair_repeated
TESVD_BOARD_PRODUCTION_CORRESPONDENCE_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(TESVD_BOARD_PRODUCTION_CORRESPONDENCE_RUN_LABEL)
TESVD_BOARD_PRODUCTION_CORRESPONDENCE_SUMMARY := $(TESVD_BOARD_PRODUCTION_CORRESPONDENCE_REPEATED_DIR)/summary.md
TESVD_BOARD_PRODUCTION_CORRESPONDENCE_EVIDENCE_MANIFEST := $(TESVD_BOARD_PRODUCTION_CORRESPONDENCE_REPEATED_DIR)/evidence_manifest.json
TESVD_BOARD_PRODUCTION_CORRESPONDENCE_EVIDENCE_DOCTOR_MD := $(TESVD_BOARD_PRODUCTION_CORRESPONDENCE_REPEATED_DIR)/evidence_doctor.md
TESVD_BOARD_DUAL_INDICES_RUN_LABEL ?= dual_indices_cloud_pair_repeated
TESVD_BOARD_DUAL_INDICES_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(TESVD_BOARD_DUAL_INDICES_RUN_LABEL)
TESVD_BOARD_DUAL_INDICES_SUMMARY := $(TESVD_BOARD_DUAL_INDICES_REPEATED_DIR)/summary.md
TESVD_BOARD_DUAL_INDICES_EVIDENCE_MANIFEST := $(TESVD_BOARD_DUAL_INDICES_REPEATED_DIR)/evidence_manifest.json
TESVD_BOARD_DUAL_INDICES_EVIDENCE_DOCTOR_MD := $(TESVD_BOARD_DUAL_INDICES_REPEATED_DIR)/evidence_doctor.md
TESVD_BOARD_CORRESPONDENCE_RUN_LABEL ?= correspondence_pair_repeated
TESVD_BOARD_CORRESPONDENCE_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(TESVD_BOARD_CORRESPONDENCE_RUN_LABEL)
TESVD_BOARD_CORRESPONDENCE_SUMMARY := $(TESVD_BOARD_CORRESPONDENCE_REPEATED_DIR)/summary.md
TESVD_BOARD_CORRESPONDENCE_EVIDENCE_MANIFEST := $(TESVD_BOARD_CORRESPONDENCE_REPEATED_DIR)/evidence_manifest.json
TESVD_BOARD_CORRESPONDENCE_EVIDENCE_DOCTOR_MD := $(TESVD_BOARD_CORRESPONDENCE_REPEATED_DIR)/evidence_doctor.md

$(TESVD_BOARD_REPEATED_DIR) $(TESVD_BOARD_SOURCE_INDEXED_REPEATED_DIR) $(TESVD_BOARD_PRODUCTION_REPEATED_DIR) $(TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_REPEATED_DIR) $(TESVD_BOARD_PRODUCTION_DUAL_INDICES_REPEATED_DIR) $(TESVD_BOARD_PRODUCTION_CORRESPONDENCE_REPEATED_DIR) $(TESVD_BOARD_DUAL_INDICES_REPEATED_DIR) $(TESVD_BOARD_CORRESPONDENCE_REPEATED_DIR) $(OUTPUT_DIR_BOARD)/test_smoke:
	mkdir -p $@

generate_qemu_smoke_evidence_manifest: $(BENCH_COMPARE_OUTPUT_FILE)
	$(PYTHON) script/generate_tesvd_qemu_evidence_manifest.py \
		--std-log $(BENCH_STD_OUTPUT_FILE) \
		--rvv-log $(BENCH_RVV_OUTPUT_FILE) \
		--summary-md $(BENCH_COMPARE_OUTPUT_FILE) \
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
		--run-label $(TESVD_QEMU_CORRECTNESS_RUN_LABEL) \
		--case-filter $(TESVD_QEMU_CORRECTNESS_CASE_FILTER) \
		--evidence-role qemu_correctness \
		--doc-ref README.zh.md \
		--doc-ref doc/transformation_estimation_svd-evaluation.zh.md \
		--doc-ref doc/phases/000-current-state-and-gaps/result.zh.md \
		--doc-ref doc/phases/040-source-indexed-production-integration/result.zh.md \
		--doc-ref doc/phases/060-dual-indices-correspondences-production-integration/result.zh.md

record_qemu_smoke_evidence_state: run_qemu_smoke_evidence_doctor
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(BENCH_STD_OUTPUT_FILE) \
		--path $(BENCH_RVV_OUTPUT_FILE) \
		--summary $(BENCH_COMPARE_OUTPUT_FILE) \
		--manifest $(QEMU_SMOKE_EVIDENCE_MANIFEST) \
		--doctor $(QEMU_SMOKE_EVIDENCE_DOCTOR_MD) \
		--target run_bench_compare \
		--backend qemu \
		--run-label $(TESVD_QEMU_SMOKE_RUN_LABEL) \
		--case-filter $(TESVD_QEMU_SMOKE_CASE_FILTER) \
		--evidence-role qemu_smoke_only \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref $(TESVD_QEMU_SMOKE_PHASE_DOC_REF)

run_board_test_smoke: deploy_board | $(OUTPUT_DIR_BOARD)/test_smoke
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_DIR)/log/board/test_smoke' REMOTE_TEST_ARGS='$(REMOTE_TEST_ARGS)'"
	@$(MAKE) -C $(CURDIR) fetch_board_logs OUTPUT_DIR_BOARD=$(OUTPUT_DIR_BOARD)/test_smoke REMOTE_BOARD_OUTPUT_DIR='$(REMOTE_DIR)/log/board/test_smoke'

collect_board_bench_fused_full_cloud_repeated: deploy_board | $(TESVD_BOARD_REPEATED_DIR)
	@for run in $$(seq 1 $(TESVD_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TESVD_BOARD_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(TESVD_BOARD_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] ordered-cloud-pair repeated run $$run_id/$(TESVD_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter all --iterations $(TESVD_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TESVD_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_fused_full_cloud:
	$(PYTHON) script/generate_tesvd_board_repeated_summary.py \
		--input-dir $(TESVD_BOARD_REPEATED_DIR) \
		--output-dir $(TESVD_BOARD_REPEATED_DIR) \
		--run-label $(TESVD_BOARD_RUN_LABEL) \
		--case-filter all \
		--row-source ordered-cloud-pair \
		--expected-runs $(TESVD_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV)

run_board_evidence_doctor_fused_full_cloud: generate_board_evidence_manifest_fused_full_cloud
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TESVD_BOARD_EVIDENCE_MANIFEST) \
		--output $(TESVD_BOARD_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_fused_full_cloud_state: run_board_evidence_doctor_fused_full_cloud
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TESVD_BOARD_SUMMARY) \
		--manifest $(TESVD_BOARD_EVIDENCE_MANIFEST) \
		--doctor $(TESVD_BOARD_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_fused_full_cloud_repeated \
		--backend board \
		--run-label board-tesvd-fused-ordered-cloud-pair-repeated-phase010 \
		--case-filter ordered-cloud-pair \
		--evidence-role pre_production_diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/phases/010-board-and-asm-diagnostic/result.zh.md

run_board_bench_fused_full_cloud_repeated: collect_board_bench_fused_full_cloud_repeated record_board_fused_full_cloud_state
run_board_bench_repeated: run_board_bench_fused_full_cloud_repeated
collect_board_bench_ordered_cloud_pair_repeated: collect_board_bench_fused_full_cloud_repeated
generate_board_evidence_manifest_ordered_cloud_pair: generate_board_evidence_manifest_fused_full_cloud
run_board_evidence_doctor_ordered_cloud_pair: run_board_evidence_doctor_fused_full_cloud
record_board_ordered_cloud_pair_state: record_board_fused_full_cloud_state
run_board_bench_ordered_cloud_pair_repeated: run_board_bench_fused_full_cloud_repeated
run_board_bench_ordered_cloud_pair: run_board_bench_ordered_cloud_pair_repeated

collect_board_bench_source_indexed_cloud_pair_repeated: deploy_board | $(TESVD_BOARD_SOURCE_INDEXED_REPEATED_DIR)
	@for run in $$(seq 1 $(TESVD_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TESVD_BOARD_SOURCE_INDEXED_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(TESVD_BOARD_SOURCE_INDEXED_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] source-indexed-cloud-pair repeated run $$run_id/$(TESVD_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter source-indexed-cloud-pair --iterations $(TESVD_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TESVD_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_source_indexed_cloud_pair:
	$(PYTHON) script/generate_tesvd_board_repeated_summary.py \
		--input-dir $(TESVD_BOARD_SOURCE_INDEXED_REPEATED_DIR) \
		--output-dir $(TESVD_BOARD_SOURCE_INDEXED_REPEATED_DIR) \
		--run-label $(TESVD_BOARD_SOURCE_INDEXED_RUN_LABEL) \
		--case-filter source-indexed-cloud-pair \
		--row-source source-indexed-cloud-pair \
		--expected-runs $(TESVD_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV)

run_board_evidence_doctor_source_indexed_cloud_pair: generate_board_evidence_manifest_source_indexed_cloud_pair
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TESVD_BOARD_SOURCE_INDEXED_EVIDENCE_MANIFEST) \
		--output $(TESVD_BOARD_SOURCE_INDEXED_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_source_indexed_cloud_pair_state: run_board_evidence_doctor_source_indexed_cloud_pair
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TESVD_BOARD_SOURCE_INDEXED_SUMMARY) \
		--manifest $(TESVD_BOARD_SOURCE_INDEXED_EVIDENCE_MANIFEST) \
		--doctor $(TESVD_BOARD_SOURCE_INDEXED_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_source_indexed_cloud_pair_repeated \
		--backend board \
		--run-label board-tesvd-source-indexed-cloud-pair-repeated-phase030 \
		--case-filter source-indexed-cloud-pair \
		--evidence-role row_source_diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/phases/030-row-source-audit/result.zh.md

run_board_bench_source_indexed_cloud_pair_repeated: collect_board_bench_source_indexed_cloud_pair_repeated record_board_source_indexed_cloud_pair_state
run_board_bench_source_indexed_cloud_pair: run_board_bench_source_indexed_cloud_pair_repeated

collect_board_bench_dual_indices_cloud_pair_repeated: deploy_board | $(TESVD_BOARD_DUAL_INDICES_REPEATED_DIR)
	@for run in $$(seq 1 $(TESVD_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TESVD_BOARD_DUAL_INDICES_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(TESVD_BOARD_DUAL_INDICES_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] dual-indices-cloud-pair repeated run $$run_id/$(TESVD_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter dual-indices-cloud-pair --iterations $(TESVD_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TESVD_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_dual_indices_cloud_pair:
	$(PYTHON) script/generate_tesvd_board_repeated_summary.py \
		--input-dir $(TESVD_BOARD_DUAL_INDICES_REPEATED_DIR) \
		--output-dir $(TESVD_BOARD_DUAL_INDICES_REPEATED_DIR) \
		--run-label $(TESVD_BOARD_DUAL_INDICES_RUN_LABEL) \
		--case-filter dual-indices-cloud-pair \
		--row-source dual-indices-cloud-pair \
		--expected-runs $(TESVD_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV)

run_board_evidence_doctor_dual_indices_cloud_pair: generate_board_evidence_manifest_dual_indices_cloud_pair
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TESVD_BOARD_DUAL_INDICES_EVIDENCE_MANIFEST) \
		--output $(TESVD_BOARD_DUAL_INDICES_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_dual_indices_cloud_pair_state: run_board_evidence_doctor_dual_indices_cloud_pair
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TESVD_BOARD_DUAL_INDICES_SUMMARY) \
		--manifest $(TESVD_BOARD_DUAL_INDICES_EVIDENCE_MANIFEST) \
		--doctor $(TESVD_BOARD_DUAL_INDICES_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_dual_indices_cloud_pair_repeated \
		--backend board \
		--run-label board-tesvd-dual-indices-cloud-pair-repeated-phase050 \
		--case-filter dual-indices-cloud-pair \
		--evidence-role row_source_diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/phases/050-dual-indices-correspondences-audit/result.zh.md

run_board_bench_dual_indices_cloud_pair_repeated: collect_board_bench_dual_indices_cloud_pair_repeated record_board_dual_indices_cloud_pair_state
run_board_bench_dual_indices_cloud_pair: run_board_bench_dual_indices_cloud_pair_repeated

collect_board_bench_correspondence_pair_repeated: deploy_board | $(TESVD_BOARD_CORRESPONDENCE_REPEATED_DIR)
	@for run in $$(seq 1 $(TESVD_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TESVD_BOARD_CORRESPONDENCE_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(TESVD_BOARD_CORRESPONDENCE_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] correspondence-pair repeated run $$run_id/$(TESVD_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter correspondence-pair --iterations $(TESVD_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TESVD_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_correspondence_pair:
	$(PYTHON) script/generate_tesvd_board_repeated_summary.py \
		--input-dir $(TESVD_BOARD_CORRESPONDENCE_REPEATED_DIR) \
		--output-dir $(TESVD_BOARD_CORRESPONDENCE_REPEATED_DIR) \
		--run-label $(TESVD_BOARD_CORRESPONDENCE_RUN_LABEL) \
		--case-filter correspondence-pair \
		--row-source correspondence-pair \
		--expected-runs $(TESVD_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV)

run_board_evidence_doctor_correspondence_pair: generate_board_evidence_manifest_correspondence_pair
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TESVD_BOARD_CORRESPONDENCE_EVIDENCE_MANIFEST) \
		--output $(TESVD_BOARD_CORRESPONDENCE_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_correspondence_pair_state: run_board_evidence_doctor_correspondence_pair
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TESVD_BOARD_CORRESPONDENCE_SUMMARY) \
		--manifest $(TESVD_BOARD_CORRESPONDENCE_EVIDENCE_MANIFEST) \
		--doctor $(TESVD_BOARD_CORRESPONDENCE_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_correspondence_pair_repeated \
		--backend board \
		--run-label board-tesvd-correspondence-pair-repeated-phase050 \
		--case-filter correspondence-pair \
		--evidence-role row_source_diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/phases/050-dual-indices-correspondences-audit/result.zh.md

run_board_bench_correspondence_pair_repeated: collect_board_bench_correspondence_pair_repeated record_board_correspondence_pair_state
run_board_bench_correspondence_pair: run_board_bench_correspondence_pair_repeated

collect_board_bench_production_ordered_cloud_pair_repeated: deploy_board | $(TESVD_BOARD_PRODUCTION_REPEATED_DIR)
	@for run in $$(seq 1 $(TESVD_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TESVD_BOARD_PRODUCTION_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(TESVD_BOARD_PRODUCTION_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] production ordered-cloud-pair repeated run $$run_id/$(TESVD_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter public-umeyama --iterations $(TESVD_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TESVD_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_production_ordered_cloud_pair:
	$(PYTHON) script/generate_tesvd_board_repeated_summary.py \
	--input-dir $(TESVD_BOARD_PRODUCTION_REPEATED_DIR) \
	--output-dir $(TESVD_BOARD_PRODUCTION_REPEATED_DIR) \
	--run-label $(TESVD_BOARD_PRODUCTION_RUN_LABEL) \
	--case-filter public-umeyama \
	--row-source ordered-cloud-pair \
	--expected-runs $(TESVD_BOARD_REPEATED_RUNS) \
	--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
	--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--evidence-role production_direct

run_board_evidence_doctor_production_ordered_cloud_pair: generate_board_evidence_manifest_production_ordered_cloud_pair
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TESVD_BOARD_PRODUCTION_EVIDENCE_MANIFEST) \
		--output $(TESVD_BOARD_PRODUCTION_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_production_ordered_cloud_pair_state: run_board_evidence_doctor_production_ordered_cloud_pair
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TESVD_BOARD_PRODUCTION_SUMMARY) \
		--manifest $(TESVD_BOARD_PRODUCTION_EVIDENCE_MANIFEST) \
		--doctor $(TESVD_BOARD_PRODUCTION_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_production_ordered_cloud_pair_repeated \
		--backend board \
		--run-label board-tesvd-production-ordered-cloud-pair-repeated-phase020 \
		--case-filter public-umeyama \
		--evidence-role production_direct \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/transformation_estimation_svd-evaluation.zh.md \
		--doc-ref doc/phases/020-pi1-production-integration-plan/result.zh.md

run_board_bench_production_ordered_cloud_pair_repeated: collect_board_bench_production_ordered_cloud_pair_repeated record_board_production_ordered_cloud_pair_state

collect_board_bench_production_source_indexed_cloud_pair_repeated: deploy_board | $(TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_REPEATED_DIR)
	@for run in $$(seq 1 $(TESVD_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] production source-indexed-cloud-pair repeated run $$run_id/$(TESVD_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter source-indexed-cloud-pair --iterations $(TESVD_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TESVD_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_production_source_indexed_cloud_pair:
	$(PYTHON) script/generate_tesvd_board_repeated_summary.py \
	--input-dir $(TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_REPEATED_DIR) \
	--output-dir $(TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_REPEATED_DIR) \
	--run-label $(TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_RUN_LABEL) \
	--case-filter source-indexed-cloud-pair \
	--row-source source-indexed-cloud-pair \
	--expected-runs $(TESVD_BOARD_REPEATED_RUNS) \
	--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
	--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--evidence-role production_direct

run_board_evidence_doctor_production_source_indexed_cloud_pair: generate_board_evidence_manifest_production_source_indexed_cloud_pair
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_EVIDENCE_MANIFEST) \
		--output $(TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_production_source_indexed_cloud_pair_state: run_board_evidence_doctor_production_source_indexed_cloud_pair
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_SUMMARY) \
		--manifest $(TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_EVIDENCE_MANIFEST) \
		--doctor $(TESVD_BOARD_PRODUCTION_SOURCE_INDEXED_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_production_source_indexed_cloud_pair_repeated \
		--backend board \
		--run-label board-tesvd-production-source-indexed-cloud-pair-repeated-phase040 \
		--case-filter source-indexed-cloud-pair \
		--evidence-role production_direct \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/transformation_estimation_svd-evaluation.zh.md \
		--doc-ref doc/phases/040-source-indexed-production-integration/result.zh.md

run_board_bench_production_source_indexed_cloud_pair_repeated: collect_board_bench_production_source_indexed_cloud_pair_repeated record_board_production_source_indexed_cloud_pair_state

collect_board_bench_production_dual_indices_cloud_pair_repeated: deploy_board | $(TESVD_BOARD_PRODUCTION_DUAL_INDICES_REPEATED_DIR)
	@for run in $$(seq 1 $(TESVD_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TESVD_BOARD_PRODUCTION_DUAL_INDICES_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(TESVD_BOARD_PRODUCTION_DUAL_INDICES_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] production dual-indices-cloud-pair repeated run $$run_id/$(TESVD_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter dual-indices-cloud-pair --iterations $(TESVD_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TESVD_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_production_dual_indices_cloud_pair:
	$(PYTHON) script/generate_tesvd_board_repeated_summary.py \
	--input-dir $(TESVD_BOARD_PRODUCTION_DUAL_INDICES_REPEATED_DIR) \
	--output-dir $(TESVD_BOARD_PRODUCTION_DUAL_INDICES_REPEATED_DIR) \
	--run-label $(TESVD_BOARD_PRODUCTION_DUAL_INDICES_RUN_LABEL) \
	--case-filter dual-indices-cloud-pair \
	--row-source dual-indices-cloud-pair \
	--expected-runs $(TESVD_BOARD_REPEATED_RUNS) \
	--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
	--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--evidence-role production_direct

run_board_evidence_doctor_production_dual_indices_cloud_pair: generate_board_evidence_manifest_production_dual_indices_cloud_pair
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TESVD_BOARD_PRODUCTION_DUAL_INDICES_EVIDENCE_MANIFEST) \
		--output $(TESVD_BOARD_PRODUCTION_DUAL_INDICES_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_production_dual_indices_cloud_pair_state: run_board_evidence_doctor_production_dual_indices_cloud_pair
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TESVD_BOARD_PRODUCTION_DUAL_INDICES_SUMMARY) \
		--manifest $(TESVD_BOARD_PRODUCTION_DUAL_INDICES_EVIDENCE_MANIFEST) \
		--doctor $(TESVD_BOARD_PRODUCTION_DUAL_INDICES_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_production_dual_indices_cloud_pair_repeated \
		--backend board \
		--run-label board-tesvd-production-dual-indices-cloud-pair-repeated-phase060 \
		--case-filter dual-indices-cloud-pair \
		--evidence-role production_direct \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/transformation_estimation_svd-evaluation.zh.md \
		--doc-ref doc/phases/060-dual-indices-correspondences-production-integration/result.zh.md

run_board_bench_production_dual_indices_cloud_pair_repeated: collect_board_bench_production_dual_indices_cloud_pair_repeated record_board_production_dual_indices_cloud_pair_state

collect_board_bench_production_correspondence_pair_repeated: deploy_board | $(TESVD_BOARD_PRODUCTION_CORRESPONDENCE_REPEATED_DIR)
	@for run in $$(seq 1 $(TESVD_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TESVD_BOARD_PRODUCTION_CORRESPONDENCE_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(TESVD_BOARD_PRODUCTION_CORRESPONDENCE_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] production correspondence-pair repeated run $$run_id/$(TESVD_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter correspondence-pair --iterations $(TESVD_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TESVD_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_production_correspondence_pair:
	$(PYTHON) script/generate_tesvd_board_repeated_summary.py \
	--input-dir $(TESVD_BOARD_PRODUCTION_CORRESPONDENCE_REPEATED_DIR) \
	--output-dir $(TESVD_BOARD_PRODUCTION_CORRESPONDENCE_REPEATED_DIR) \
	--run-label $(TESVD_BOARD_PRODUCTION_CORRESPONDENCE_RUN_LABEL) \
	--case-filter correspondence-pair \
	--row-source correspondence-pair \
	--expected-runs $(TESVD_BOARD_REPEATED_RUNS) \
	--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
	--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--evidence-role production_direct

run_board_evidence_doctor_production_correspondence_pair: generate_board_evidence_manifest_production_correspondence_pair
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TESVD_BOARD_PRODUCTION_CORRESPONDENCE_EVIDENCE_MANIFEST) \
		--output $(TESVD_BOARD_PRODUCTION_CORRESPONDENCE_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_production_correspondence_pair_state: run_board_evidence_doctor_production_correspondence_pair
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TESVD_BOARD_PRODUCTION_CORRESPONDENCE_SUMMARY) \
		--manifest $(TESVD_BOARD_PRODUCTION_CORRESPONDENCE_EVIDENCE_MANIFEST) \
		--doctor $(TESVD_BOARD_PRODUCTION_CORRESPONDENCE_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_production_correspondence_pair_repeated \
		--backend board \
		--run-label board-tesvd-production-correspondence-pair-repeated-phase060 \
		--case-filter correspondence-pair \
		--evidence-role production_direct \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/transformation_estimation_svd-evaluation.zh.md \
		--doc-ref doc/phases/060-dual-indices-correspondences-production-integration/result.zh.md

run_board_bench_production_correspondence_pair_repeated: collect_board_bench_production_correspondence_pair_repeated record_board_production_correspondence_pair_state

run_evidence_doctor: run_qemu_smoke_evidence_doctor
generate_evidence_manifest: generate_qemu_smoke_evidence_manifest

evidence_status:
	$(PYTHON) ../../script/evidence_registry.py check \
		--registry $(EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/registration/transformation_estimation_svd/log/qemu/*.log' \
		--scan-glob 'test-rvv/registration/transformation_estimation_svd/log/qemu/*.md' \
		--scan-glob 'test-rvv/registration/transformation_estimation_svd/log/qemu/*.json' \
		--scan-glob 'test-rvv/registration/transformation_estimation_svd/log/board/**/summary.md' \
		--scan-glob 'test-rvv/registration/transformation_estimation_svd/log/board/**/evidence_doctor.md' \
		--scan-glob 'test-rvv/registration/transformation_estimation_svd/log/board/**/evidence_manifest.json' \
		--doc test-rvv/registration/transformation_estimation_svd/README.zh.md \
		--doc test-rvv/registration/transformation_estimation_svd/doc/benchmark-and-evidence.zh.md \
		--doc test-rvv/registration/transformation_estimation_svd/doc/optimization-evidence.zh.md \
		--doc test-rvv/registration/transformation_estimation_svd/doc/phases/000-current-state-and-gaps/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_svd/doc/phases/010-board-and-asm-diagnostic/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_svd/doc/phases/030-row-source-audit/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_svd/doc/phases/020-pi1-production-integration-plan/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_svd/doc/phases/040-source-indexed-production-integration/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_svd/doc/phases/050-dual-indices-correspondences-audit/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_svd/doc/phases/060-dual-indices-correspondences-production-integration/result.zh.md \
		--require-doc-ref \
		--fail-on never

.PHONY: run_board_test_smoke collect_board_bench_fused_full_cloud_repeated generate_board_evidence_manifest_fused_full_cloud run_board_evidence_doctor_fused_full_cloud record_board_fused_full_cloud_state run_board_bench_fused_full_cloud_repeated run_board_bench_repeated collect_board_bench_production_ordered_cloud_pair_repeated generate_board_evidence_manifest_production_ordered_cloud_pair run_board_evidence_doctor_production_ordered_cloud_pair record_board_production_ordered_cloud_pair_state run_board_bench_production_ordered_cloud_pair_repeated collect_board_bench_source_indexed_cloud_pair_repeated generate_board_evidence_manifest_source_indexed_cloud_pair run_board_evidence_doctor_source_indexed_cloud_pair record_board_source_indexed_cloud_pair_state run_board_bench_source_indexed_cloud_pair_repeated run_board_bench_source_indexed_cloud_pair collect_board_bench_production_source_indexed_cloud_pair_repeated generate_board_evidence_manifest_production_source_indexed_cloud_pair run_board_evidence_doctor_production_source_indexed_cloud_pair record_board_production_source_indexed_cloud_pair_state run_board_bench_production_source_indexed_cloud_pair_repeated collect_board_bench_production_dual_indices_cloud_pair_repeated generate_board_evidence_manifest_production_dual_indices_cloud_pair run_board_evidence_doctor_production_dual_indices_cloud_pair record_board_production_dual_indices_cloud_pair_state run_board_bench_production_dual_indices_cloud_pair_repeated collect_board_bench_production_correspondence_pair_repeated generate_board_evidence_manifest_production_correspondence_pair run_board_evidence_doctor_production_correspondence_pair record_board_production_correspondence_pair_state run_board_bench_production_correspondence_pair_repeated
.PHONY: collect_board_bench_ordered_cloud_pair_repeated generate_board_evidence_manifest_ordered_cloud_pair run_board_evidence_doctor_ordered_cloud_pair record_board_ordered_cloud_pair_state run_board_bench_ordered_cloud_pair_repeated run_board_bench_ordered_cloud_pair
.PHONY: collect_board_bench_dual_indices_cloud_pair_repeated generate_board_evidence_manifest_dual_indices_cloud_pair run_board_evidence_doctor_dual_indices_cloud_pair record_board_dual_indices_cloud_pair_state run_board_bench_dual_indices_cloud_pair_repeated run_board_bench_dual_indices_cloud_pair collect_board_bench_correspondence_pair_repeated generate_board_evidence_manifest_correspondence_pair run_board_evidence_doctor_correspondence_pair record_board_correspondence_pair_state run_board_bench_correspondence_pair_repeated run_board_bench_correspondence_pair
