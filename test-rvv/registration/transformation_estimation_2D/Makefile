# =============================================================================
#  PCL registration/transformation_estimation_2D RVV diagnostic Makefile
# =============================================================================

TOPIC  := transformation_estimation_2D
MODULE := registration

FOCUS_DIR      := pcl/registration/include/pcl/registration/impl/transformation_estimation_2D.hpp
FILTER_REPORT  := log/filtered_transformation_estimation_2D.log
ANALYZE_REPORT := log/analyze_transformation_estimation_2D.log

TARGET_BENCH     := bench_transformation_estimation_2D
TARGET_BENCH_STD := bench_transformation_estimation_2D_std
TARGET_BENCH_RVV := bench_transformation_estimation_2D_rvv
SRCS_BENCH       := src/bench_te2d.cpp

TARGET_TEST     := test_transformation_estimation_2D
TARGET_TEST_STD := test_transformation_estimation_2D_std
TARGET_TEST_RVV := test_transformation_estimation_2D_rvv
SRCS_TEST       := src/test_te2d.cpp

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

TE2D_QEMU_SMOKE_ARGS ?= --case-filter ordered-cloud-pair-fused --iterations 2 --warmup-iterations 1
TE2D_QEMU_PUBLIC_SMOKE_ARGS ?= --case-filter ordered-cloud-pair-public --iterations 2 --warmup-iterations 1
TE2D_QEMU_ROW_SOURCE_SMOKE_ARGS ?= --case-filter row-source-fused --iterations 2 --warmup-iterations 1

include ../../mk/rvv-topic.mk

QEMU_SMOKE_EVIDENCE_MANIFEST := $(OUTPUT_DIR_QEMU)/evidence_manifest.json
QEMU_SMOKE_EVIDENCE_DOCTOR_MD := $(OUTPUT_DIR_QEMU)/evidence_doctor.md
QEMU_ASM_ATTRIBUTION_MD := $(OUTPUT_DIR_QEMU)/asm_attribution.md
QEMU_ASM_ATTRIBUTION_JSON := $(OUTPUT_DIR_QEMU)/asm_attribution.json
QEMU_PRODUCTION_PUBLIC_DIR := $(OUTPUT_DIR_QEMU)/production_public
QEMU_PRODUCTION_PUBLIC_BENCH_LOG := $(QEMU_PRODUCTION_PUBLIC_DIR)/run_bench_ordered_cloud_pair_public_rvv.log
QEMU_PRODUCTION_PUBLIC_EVIDENCE_MANIFEST := $(QEMU_PRODUCTION_PUBLIC_DIR)/evidence_manifest.json
QEMU_PRODUCTION_PUBLIC_EVIDENCE_DOCTOR_MD := $(QEMU_PRODUCTION_PUBLIC_DIR)/evidence_doctor.md
QEMU_PRODUCTION_PUBLIC_ASM_ATTRIBUTION_MD := $(QEMU_PRODUCTION_PUBLIC_DIR)/asm_attribution.md
QEMU_PRODUCTION_PUBLIC_ASM_ATTRIBUTION_JSON := $(QEMU_PRODUCTION_PUBLIC_DIR)/asm_attribution.json
QEMU_ROW_SOURCE_DIR := $(OUTPUT_DIR_QEMU)/row_source
QEMU_ROW_SOURCE_BENCH_LOG := $(QEMU_ROW_SOURCE_DIR)/run_bench_row_source_fused_rvv.log
QEMU_ROW_SOURCE_EVIDENCE_MANIFEST := $(QEMU_ROW_SOURCE_DIR)/evidence_manifest.json
QEMU_ROW_SOURCE_EVIDENCE_DOCTOR_MD := $(QEMU_ROW_SOURCE_DIR)/evidence_doctor.md
QEMU_ROW_SOURCE_ASM_ATTRIBUTION_MD := $(QEMU_ROW_SOURCE_DIR)/asm_attribution.md
QEMU_ROW_SOURCE_ASM_ATTRIBUTION_JSON := $(QEMU_ROW_SOURCE_DIR)/asm_attribution.json
EVIDENCE_REGISTRY := $(LOG_DIR)/evidence_registry.json

TE2D_BOARD_REPEATED_RUNS ?= 5
TE2D_BOARD_BENCH_ITERATIONS ?= 20
TE2D_BOARD_BENCH_WARMUP_ITERATIONS ?= 5
TE2D_BOARD_RUN_LABEL ?= ordered_cloud_pair_repeated
TE2D_BOARD_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(TE2D_BOARD_RUN_LABEL)
TE2D_BOARD_SUMMARY := $(TE2D_BOARD_REPEATED_DIR)/summary.md
TE2D_BOARD_EVIDENCE_MANIFEST := $(TE2D_BOARD_REPEATED_DIR)/evidence_manifest.json
TE2D_BOARD_EVIDENCE_DOCTOR_MD := $(TE2D_BOARD_REPEATED_DIR)/evidence_doctor.md

TE2D_BOARD_PUBLIC_RUN_LABEL ?= ordered_cloud_pair_public_repeated
TE2D_BOARD_PUBLIC_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(TE2D_BOARD_PUBLIC_RUN_LABEL)
TE2D_BOARD_PUBLIC_SUMMARY := $(TE2D_BOARD_PUBLIC_REPEATED_DIR)/summary.md
TE2D_BOARD_PUBLIC_EVIDENCE_MANIFEST := $(TE2D_BOARD_PUBLIC_REPEATED_DIR)/evidence_manifest.json
TE2D_BOARD_PUBLIC_EVIDENCE_DOCTOR_MD := $(TE2D_BOARD_PUBLIC_REPEATED_DIR)/evidence_doctor.md

TE2D_BOARD_ROW_SOURCE_RUN_LABEL ?= row_source_fused_repeated
TE2D_BOARD_ROW_SOURCE_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(TE2D_BOARD_ROW_SOURCE_RUN_LABEL)
TE2D_BOARD_ROW_SOURCE_SUMMARY := $(TE2D_BOARD_ROW_SOURCE_REPEATED_DIR)/summary.md
TE2D_BOARD_ROW_SOURCE_EVIDENCE_MANIFEST := $(TE2D_BOARD_ROW_SOURCE_REPEATED_DIR)/evidence_manifest.json
TE2D_BOARD_ROW_SOURCE_EVIDENCE_DOCTOR_MD := $(TE2D_BOARD_ROW_SOURCE_REPEATED_DIR)/evidence_doctor.md

$(TE2D_BOARD_REPEATED_DIR) $(TE2D_BOARD_PUBLIC_REPEATED_DIR) $(TE2D_BOARD_ROW_SOURCE_REPEATED_DIR) $(OUTPUT_DIR_BOARD)/test_smoke $(QEMU_PRODUCTION_PUBLIC_DIR) $(QEMU_ROW_SOURCE_DIR):
	mkdir -p $@

run_test_public_semantics:
	@$(MAKE) -C $(CURDIR) run_test_compare TEST_ARGS="--gtest_filter=TransformationEstimation2D.Public*"

run_test_candidates:
	@$(MAKE) -C $(CURDIR) run_test_compare TEST_ARGS="--gtest_filter=TransformationEstimation2D.Fused*"

run_bench_ordered_cloud_pair_smoke: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_rvv BENCH_ARGS="$(TE2D_QEMU_SMOKE_ARGS)" BENCH_RVV_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_ordered_cloud_pair_smoke_rvv.log

run_bench_ordered_cloud_pair_public_smoke: | $(QEMU_PRODUCTION_PUBLIC_DIR)
	@$(MAKE) -C $(CURDIR) run_bench_rvv BENCH_ARGS="$(TE2D_QEMU_PUBLIC_SMOKE_ARGS)" BENCH_RVV_OUTPUT_FILE=$(QEMU_PRODUCTION_PUBLIC_BENCH_LOG)

run_bench_row_source_smoke: | $(QEMU_ROW_SOURCE_DIR)
	@$(MAKE) -C $(CURDIR) run_bench_rvv BENCH_ARGS="$(TE2D_QEMU_ROW_SOURCE_SMOKE_ARGS)" BENCH_RVV_OUTPUT_FILE=$(QEMU_ROW_SOURCE_BENCH_LOG)

dump_bench_std: | $(ASM_DIR)/$(ARCH)
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=0 TARGET_BENCH=$(TARGET_BENCH_STD) $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD)
	$(OBJDUMP) -d -C $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) > $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_STD).full.asm
	grep -E $(VEC_REGEX_STR) $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_STD).full.asm > $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_STD).asm || true
	@echo "[DONE] Std asm dump: $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_STD).asm"

generate_asm_attribution_summary: dump_bench_std dump_bench_rvv
	$(PYTHON) script/generate_te2d_asm_summary.py \
		--std-full-asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_STD).full.asm \
		--rvv-full-asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm \
		--rvv-filtered-asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm \
		--output $(QEMU_ASM_ATTRIBUTION_MD) \
		--json-output $(QEMU_ASM_ATTRIBUTION_JSON)

generate_production_public_asm_attribution_summary: dump_bench_std dump_bench_rvv | $(QEMU_PRODUCTION_PUBLIC_DIR)
	$(PYTHON) script/generate_te2d_asm_summary.py \
		--std-full-asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_STD).full.asm \
		--rvv-full-asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm \
		--rvv-filtered-asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm \
		--output $(QEMU_PRODUCTION_PUBLIC_ASM_ATTRIBUTION_MD) \
		--json-output $(QEMU_PRODUCTION_PUBLIC_ASM_ATTRIBUTION_JSON)

generate_row_source_asm_attribution_summary: dump_bench_std dump_bench_rvv | $(QEMU_ROW_SOURCE_DIR)
	$(PYTHON) script/generate_te2d_asm_summary.py \
		--std-full-asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_STD).full.asm \
		--rvv-full-asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm \
		--rvv-filtered-asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm \
		--output $(QEMU_ROW_SOURCE_ASM_ATTRIBUTION_MD) \
		--json-output $(QEMU_ROW_SOURCE_ASM_ATTRIBUTION_JSON) \
		--focus-category row_source_lambda_boundary

generate_qemu_smoke_evidence_manifest: run_bench_ordered_cloud_pair_smoke generate_asm_attribution_summary
	$(PYTHON) script/generate_te2d_qemu_evidence_manifest.py \
		--rvv-log $(OUTPUT_DIR_QEMU)/run_bench_ordered_cloud_pair_smoke_rvv.log \
		--asm-summary $(QEMU_ASM_ATTRIBUTION_JSON) \
		--output $(QEMU_SMOKE_EVIDENCE_MANIFEST)

run_qemu_smoke_evidence_doctor: generate_qemu_smoke_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(QEMU_SMOKE_EVIDENCE_MANIFEST) \
		--output $(QEMU_SMOKE_EVIDENCE_DOCTOR_MD) \
		--fail-on never

generate_qemu_production_public_evidence_manifest: run_bench_ordered_cloud_pair_public_smoke generate_production_public_asm_attribution_summary
	$(PYTHON) script/generate_te2d_qemu_evidence_manifest.py \
		--rvv-log $(QEMU_PRODUCTION_PUBLIC_BENCH_LOG) \
		--asm-summary $(QEMU_PRODUCTION_PUBLIC_ASM_ATTRIBUTION_JSON) \
		--output $(QEMU_PRODUCTION_PUBLIC_EVIDENCE_MANIFEST) \
		--title "transformation_estimation_2D production public QEMU smoke" \
		--label "production public QEMU smoke; no performance conclusion" \
		--summary-evidence-role qemu_smoke_only \
		--comparison-evidence-role qemu_smoke_only \
		--case-kind qemu_smoke_only \
		--group te2d_qemu_ordered_cloud_pair_public_smoke \
		--asm-category production_public_combined

generate_qemu_row_source_evidence_manifest: run_bench_row_source_smoke generate_row_source_asm_attribution_summary
	$(PYTHON) script/generate_te2d_qemu_evidence_manifest.py \
		--rvv-log $(QEMU_ROW_SOURCE_BENCH_LOG) \
		--asm-summary $(QEMU_ROW_SOURCE_ASM_ATTRIBUTION_JSON) \
		--output $(QEMU_ROW_SOURCE_EVIDENCE_MANIFEST) \
		--title "transformation_estimation_2D row-source QEMU smoke" \
		--label "row-source diagnostic QEMU smoke; no performance conclusion" \
		--summary-evidence-role qemu_smoke_only \
		--comparison-evidence-role qemu_smoke_only \
		--case-kind qemu_smoke_only \
		--row-source auto \
		--group te2d_qemu_row_source_fused \
		--asm-category row_source_lambda_boundary

run_qemu_production_public_evidence_doctor: generate_qemu_production_public_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(QEMU_PRODUCTION_PUBLIC_EVIDENCE_MANIFEST) \
		--output $(QEMU_PRODUCTION_PUBLIC_EVIDENCE_DOCTOR_MD) \
		--fail-on never

run_qemu_row_source_evidence_doctor: generate_qemu_row_source_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(QEMU_ROW_SOURCE_EVIDENCE_MANIFEST) \
		--output $(QEMU_ROW_SOURCE_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_qemu_correctness_state: run_test_compare
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(TEST_STD_OUTPUT_FILE) \
		--path $(TEST_RVV_OUTPUT_FILE) \
		--target run_test_compare \
		--backend qemu \
		--run-label qemu-te2d-correctness-phase010 \
		--case-filter all \
		--evidence-role qemu_correctness \
		--doc-ref README.zh.md \
		--doc-ref doc/testing-overview.zh.md \
		--doc-ref doc/correctness-tests.zh.md \
		--doc-ref doc/phases/010-scaffold-and-ordered-cloud-pair-correlation-diagnostic/result.zh.md \
		--doc-ref doc/phases/050-pi2-production-patch-and-direct-evidence/result.zh.md

record_qemu_smoke_evidence_state: run_qemu_smoke_evidence_doctor
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(OUTPUT_DIR_QEMU)/run_bench_ordered_cloud_pair_smoke_rvv.log \
		--path $(QEMU_ASM_ATTRIBUTION_JSON) \
		--summary $(QEMU_ASM_ATTRIBUTION_MD) \
		--manifest $(QEMU_SMOKE_EVIDENCE_MANIFEST) \
		--doctor $(QEMU_SMOKE_EVIDENCE_DOCTOR_MD) \
		--target run_bench_ordered_cloud_pair_smoke \
		--backend qemu \
		--run-label qemu-te2d-ordered-cloud-pair-smoke-phase020 \
		--case-filter ordered-cloud-pair-fused \
		--evidence-role qemu_smoke_only \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/test-support-code-map.zh.md \
		--doc-ref doc/transformation_estimation_2D-evaluation.zh.md \
		--doc-ref doc/phases/020-board-and-asm-evidence/result.zh.md

record_qemu_bench_smoke_state: record_qemu_smoke_evidence_state

record_qemu_production_public_state: run_qemu_production_public_evidence_doctor
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(QEMU_PRODUCTION_PUBLIC_BENCH_LOG) \
		--path $(QEMU_PRODUCTION_PUBLIC_ASM_ATTRIBUTION_JSON) \
		--summary $(QEMU_PRODUCTION_PUBLIC_ASM_ATTRIBUTION_MD) \
		--manifest $(QEMU_PRODUCTION_PUBLIC_EVIDENCE_MANIFEST) \
		--doctor $(QEMU_PRODUCTION_PUBLIC_EVIDENCE_DOCTOR_MD) \
		--target run_bench_ordered_cloud_pair_public_smoke \
		--backend qemu \
		--run-label qemu-te2d-production-public-smoke-pi4 \
		--case-filter ordered-cloud-pair-public \
		--evidence-role qemu_smoke_only \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/test-support-code-map.zh.md \
		--doc-ref doc/transformation_estimation_2D-evaluation.zh.md \
		--doc-ref doc/phases/050-pi2-production-patch-and-direct-evidence/result.zh.md

record_qemu_row_source_state: run_qemu_row_source_evidence_doctor
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(QEMU_ROW_SOURCE_BENCH_LOG) \
		--path $(QEMU_ROW_SOURCE_ASM_ATTRIBUTION_JSON) \
		--summary $(QEMU_ROW_SOURCE_ASM_ATTRIBUTION_MD) \
		--manifest $(QEMU_ROW_SOURCE_EVIDENCE_MANIFEST) \
		--doctor $(QEMU_ROW_SOURCE_EVIDENCE_DOCTOR_MD) \
		--target run_bench_row_source_smoke \
		--backend qemu \
		--run-label qemu-te2d-row-source-fused-smoke-phase030 \
		--case-filter row-source-fused \
		--evidence-role qemu_smoke_only \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/test-support-code-map.zh.md \
		--doc-ref doc/transformation_estimation_2D-evaluation.zh.md \
		--doc-ref doc/phases/030-row-source-family-carryover/result.zh.md

run_board_test_smoke: deploy_board | $(OUTPUT_DIR_BOARD)/test_smoke
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_DIR)/log/board/test_smoke' REMOTE_TEST_ARGS='$(REMOTE_TEST_ARGS)'"
	@$(MAKE) -C $(CURDIR) fetch_board_logs OUTPUT_DIR_BOARD=$(OUTPUT_DIR_BOARD)/test_smoke REMOTE_BOARD_OUTPUT_DIR='$(REMOTE_DIR)/log/board/test_smoke'

collect_board_bench_ordered_cloud_pair_repeated: deploy_board | $(TE2D_BOARD_REPEATED_DIR)
	@rm -rf "$(TE2D_BOARD_REPEATED_DIR)"/run-*
	@for run in $$(seq 1 $(TE2D_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TE2D_BOARD_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(TE2D_BOARD_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] ordered_cloud_pair repeated run $$run_id/$(TE2D_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter ordered-cloud-pair-fused --iterations $(TE2D_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TE2D_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

collect_board_bench_ordered_cloud_pair_public_repeated: deploy_board | $(TE2D_BOARD_PUBLIC_REPEATED_DIR)
	@rm -rf "$(TE2D_BOARD_PUBLIC_REPEATED_DIR)"/run-*
	@for run in $$(seq 1 $(TE2D_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TE2D_BOARD_PUBLIC_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(TE2D_BOARD_PUBLIC_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] ordered_cloud_pair public repeated run $$run_id/$(TE2D_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter ordered-cloud-pair-public --iterations $(TE2D_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TE2D_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

collect_board_bench_row_source_repeated: deploy_board | $(TE2D_BOARD_ROW_SOURCE_REPEATED_DIR)
	@rm -rf "$(TE2D_BOARD_ROW_SOURCE_REPEATED_DIR)"/run-*
	@for run in $$(seq 1 $(TE2D_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(TE2D_BOARD_ROW_SOURCE_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(TE2D_BOARD_ROW_SOURCE_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] row_source_fused repeated run $$run_id/$(TE2D_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter row-source-fused --iterations $(TE2D_BOARD_BENCH_ITERATIONS) --warmup-iterations $(TE2D_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_ordered_cloud_pair:
	$(PYTHON) script/generate_te2d_board_repeated_summary.py \
		--input-dir $(TE2D_BOARD_REPEATED_DIR) \
		--output-dir $(TE2D_BOARD_REPEATED_DIR) \
		--run-label $(TE2D_BOARD_RUN_LABEL) \
		--case-filter ordered-cloud-pair-fused \
		--expected-runs $(TE2D_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"

generate_board_evidence_manifest_ordered_cloud_pair_public:
	$(PYTHON) script/generate_te2d_board_repeated_summary.py \
		--input-dir $(TE2D_BOARD_PUBLIC_REPEATED_DIR) \
		--output-dir $(TE2D_BOARD_PUBLIC_REPEATED_DIR) \
		--run-label $(TE2D_BOARD_PUBLIC_RUN_LABEL) \
		--case-filter ordered-cloud-pair-public \
		--expected-runs $(TE2D_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"

generate_board_evidence_manifest_row_source:
	$(PYTHON) script/generate_te2d_board_repeated_summary.py \
		--input-dir $(TE2D_BOARD_ROW_SOURCE_REPEATED_DIR) \
		--output-dir $(TE2D_BOARD_ROW_SOURCE_REPEATED_DIR) \
		--run-label $(TE2D_BOARD_ROW_SOURCE_RUN_LABEL) \
		--case-filter row-source-fused \
		--expected-runs $(TE2D_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"

run_board_evidence_doctor_ordered_cloud_pair: generate_board_evidence_manifest_ordered_cloud_pair
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TE2D_BOARD_EVIDENCE_MANIFEST) \
		--output $(TE2D_BOARD_EVIDENCE_DOCTOR_MD) \
		--fail-on never

run_board_evidence_doctor_ordered_cloud_pair_public: generate_board_evidence_manifest_ordered_cloud_pair_public
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TE2D_BOARD_PUBLIC_EVIDENCE_MANIFEST) \
		--output $(TE2D_BOARD_PUBLIC_EVIDENCE_DOCTOR_MD) \
		--fail-on never

run_board_evidence_doctor_row_source: generate_board_evidence_manifest_row_source
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(TE2D_BOARD_ROW_SOURCE_EVIDENCE_MANIFEST) \
		--output $(TE2D_BOARD_ROW_SOURCE_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_ordered_cloud_pair_state: run_board_evidence_doctor_ordered_cloud_pair
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TE2D_BOARD_SUMMARY) \
		--manifest $(TE2D_BOARD_EVIDENCE_MANIFEST) \
		--doctor $(TE2D_BOARD_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_ordered_cloud_pair_repeated \
		--backend board \
		--run-label board-te2d-ordered-cloud-pair-repeated-phase020 \
		--case-filter ordered-cloud-pair-fused \
		--evidence-role pre_production_diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/test-support-code-map.zh.md \
		--doc-ref doc/transformation_estimation_2D-evaluation.zh.md \
		--doc-ref doc/phases/020-board-and-asm-evidence/result.zh.md

record_board_ordered_cloud_pair_public_state: run_board_evidence_doctor_ordered_cloud_pair_public
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TE2D_BOARD_PUBLIC_SUMMARY) \
		--manifest $(TE2D_BOARD_PUBLIC_EVIDENCE_MANIFEST) \
		--doctor $(TE2D_BOARD_PUBLIC_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_ordered_cloud_pair_public_repeated \
		--backend board \
		--run-label board-te2d-production-public-repeated-pi4 \
		--case-filter ordered-cloud-pair-public \
		--evidence-role post_production_performance \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/test-support-code-map.zh.md \
		--doc-ref doc/transformation_estimation_2D-evaluation.zh.md \
		--doc-ref doc/phases/050-pi2-production-patch-and-direct-evidence/result.zh.md

record_board_row_source_state: run_board_evidence_doctor_row_source
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(TE2D_BOARD_ROW_SOURCE_SUMMARY) \
		--manifest $(TE2D_BOARD_ROW_SOURCE_EVIDENCE_MANIFEST) \
		--doctor $(TE2D_BOARD_ROW_SOURCE_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bench_row_source_repeated \
		--backend board \
		--run-label board-te2d-row-source-fused-repeated-phase030 \
		--case-filter row-source-fused \
		--evidence-role pre_production_diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/test-support-code-map.zh.md \
		--doc-ref doc/transformation_estimation_2D-evaluation.zh.md \
		--doc-ref doc/phases/030-row-source-family-carryover/result.zh.md

run_board_bench_ordered_cloud_pair_repeated: collect_board_bench_ordered_cloud_pair_repeated record_board_ordered_cloud_pair_state
run_board_bench_ordered_cloud_pair_public_repeated: collect_board_bench_ordered_cloud_pair_public_repeated record_board_ordered_cloud_pair_public_state
run_board_bench_row_source_repeated: collect_board_bench_row_source_repeated record_board_row_source_state
run_board_bench_repeated: run_board_bench_ordered_cloud_pair_repeated

run_evidence_doctor: run_qemu_smoke_evidence_doctor run_qemu_production_public_evidence_doctor run_qemu_row_source_evidence_doctor
generate_evidence_manifest: generate_qemu_smoke_evidence_manifest generate_qemu_production_public_evidence_manifest generate_qemu_row_source_evidence_manifest

evidence_status:
	$(PYTHON) ../../script/evidence_registry.py check \
		--registry $(EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/registration/transformation_estimation_2D/log/qemu/*.log' \
		--scan-glob 'test-rvv/registration/transformation_estimation_2D/log/qemu/*.md' \
		--scan-glob 'test-rvv/registration/transformation_estimation_2D/log/qemu/*.json' \
		--scan-glob 'test-rvv/registration/transformation_estimation_2D/log/qemu/**/*.log' \
		--scan-glob 'test-rvv/registration/transformation_estimation_2D/log/qemu/**/*.md' \
		--scan-glob 'test-rvv/registration/transformation_estimation_2D/log/qemu/**/*.json' \
		--scan-glob 'test-rvv/registration/transformation_estimation_2D/log/board/**/summary.md' \
		--scan-glob 'test-rvv/registration/transformation_estimation_2D/log/board/**/evidence_doctor.md' \
		--scan-glob 'test-rvv/registration/transformation_estimation_2D/log/board/**/evidence_manifest.json' \
		--doc test-rvv/registration/transformation_estimation_2D/README.zh.md \
		--doc test-rvv/registration/transformation_estimation_2D/doc/testing-overview.zh.md \
		--doc test-rvv/registration/transformation_estimation_2D/doc/correctness-tests.zh.md \
		--doc test-rvv/registration/transformation_estimation_2D/doc/benchmark-and-evidence.zh.md \
		--doc test-rvv/registration/transformation_estimation_2D/doc/optimization-evidence.zh.md \
		--doc test-rvv/registration/transformation_estimation_2D/doc/test-support-code-map.zh.md \
		--doc test-rvv/registration/transformation_estimation_2D/doc/transformation_estimation_2D-evaluation.zh.md \
		--doc test-rvv/registration/transformation_estimation_2D/doc/phases/010-scaffold-and-ordered-cloud-pair-correlation-diagnostic/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_2D/doc/phases/020-board-and-asm-evidence/result.zh.md \
		--doc test-rvv/registration/transformation_estimation_2D/doc/phases/040-production-integration-plan/plan.zh.md \
		--doc test-rvv/registration/transformation_estimation_2D/doc/phases/040-production-integration-plan/result.zh.md \
			--doc test-rvv/registration/transformation_estimation_2D/doc/phases/050-pi2-production-patch-and-direct-evidence/plan.zh.md \
			--doc test-rvv/registration/transformation_estimation_2D/doc/phases/050-pi2-production-patch-and-direct-evidence/result.zh.md \
			--doc test-rvv/registration/transformation_estimation_2D/doc/phases/030-row-source-family-carryover/plan.zh.md \
			--doc test-rvv/registration/transformation_estimation_2D/doc/phases/030-row-source-family-carryover/result.zh.md \
			--require-doc-ref \
			--fail-on never

.PHONY: run_test_public_semantics run_test_candidates run_bench_ordered_cloud_pair_smoke run_bench_ordered_cloud_pair_public_smoke run_bench_row_source_smoke dump_bench_std generate_asm_attribution_summary generate_production_public_asm_attribution_summary generate_row_source_asm_attribution_summary generate_qemu_smoke_evidence_manifest run_qemu_smoke_evidence_doctor generate_qemu_production_public_evidence_manifest run_qemu_production_public_evidence_doctor generate_qemu_row_source_evidence_manifest run_qemu_row_source_evidence_doctor record_qemu_correctness_state record_qemu_smoke_evidence_state record_qemu_bench_smoke_state record_qemu_production_public_state record_qemu_row_source_state run_board_test_smoke collect_board_bench_ordered_cloud_pair_repeated collect_board_bench_ordered_cloud_pair_public_repeated collect_board_bench_row_source_repeated generate_board_evidence_manifest_ordered_cloud_pair generate_board_evidence_manifest_ordered_cloud_pair_public generate_board_evidence_manifest_row_source run_board_evidence_doctor_ordered_cloud_pair run_board_evidence_doctor_ordered_cloud_pair_public run_board_evidence_doctor_row_source record_board_ordered_cloud_pair_state record_board_ordered_cloud_pair_public_state record_board_row_source_state run_board_bench_ordered_cloud_pair_repeated run_board_bench_ordered_cloud_pair_public_repeated run_board_bench_row_source_repeated run_board_bench_repeated run_evidence_doctor generate_evidence_manifest evidence_status
