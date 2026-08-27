# =============================================================================
# PCL features/range_image_border_extractor RVV diagnostic Makefile
#
# 本 topic 先评估 RangeImageBorderExtractor（距离图边界提取器）的
# score-update（分数传播）候选。QEMU（仿真器）只用于 correctness（正确性）
# 和日志形状；性能结论必须来自 board（板卡）或目标硬件。
# =============================================================================

TOPIC  := range_image_border_extractor
MODULE := features

FOCUS_DIR      := pcl/features/include/pcl/features/impl/range_image_border_extractor.hpp
FILTER_REPORT  := log/filtered_range_image_border_extractor.log
ANALYZE_REPORT := log/analyze_range_image_border_extractor.log

TARGET_TEST     := test_range_image_border_extractor
TARGET_TEST_STD := test_range_image_border_extractor_std
TARGET_TEST_RVV := test_range_image_border_extractor_rvv
BOARD_TARGET_TEST := test_range_image_border_extractor_rvv
PRODUCTION_SRCS := ../../../features/src/range_image_border_extractor.cpp
SRCS_TEST       := src/test_range_image_border_extractor.cpp $(PRODUCTION_SRCS)

TARGET_BENCH     := bench_range_image_border_extractor
TARGET_BENCH_STD := bench_range_image_border_extractor_std
TARGET_BENCH_RVV := bench_range_image_border_extractor_rvv
SRCS_BENCH       := src/bench_range_image_border_extractor.cpp $(PRODUCTION_SRCS)

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/features/include \
	-I$(PCL_SOURCE_ROOT)/range_image/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include \
	-I$(FLANN_ROOT)/include \
	-I$(LZ4_ROOT)/include \
	-I$(HDF5_ROOT)/include \
	-I$(ZLIB_ROOT)/include \
	-I$(LIBPNG_ROOT)/include

LIBS_TEST := -lpcl_common -lflann -llz4 -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lpcl_common -lflann -llz4 -lm

REMOTE_DIR ?= /root/pcl-test/features/range_image_border_extractor

REPEATED_BOARD_RUNS ?= 5
REPEATED_BOARD_TAG ?= phase010_four_score_pipeline_clean
REPEATED_BOARD_TITLE ?= RangeImageBorderExtractor four-score-pipeline diagnostic repeated board summary
REPEATED_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated_$(REPEATED_BOARD_TAG)
REPEATED_BOARD_REMOTE_TAG ?= $(REPEATED_BOARD_TAG)
REPEATED_BOARD_RUN_LABEL ?= range_image_border_extractor_phase010_four_score_pipeline_repeated_clean
REPEATED_BOARD_CASE_NAME ?= score_pipeline_641x481_four_images
REPEATED_BOARD_EVIDENCE_ROLE ?= diagnostic
EVIDENCE_MANIFEST_REPEATED ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_manifest.json
EVIDENCE_DOCTOR_REPEATED_MD ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.md
EVIDENCE_DOCTOR_REPEATED_JSON ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.json
RIBE_REPEATED_BENCH_ARGS ?= --case-filter $(REPEATED_BOARD_CASE_NAME) --repeat 8 --iterations 10 --warmup 2
RIBE_EVIDENCE_REGISTRY ?= log/evidence_registry.json

include ../../mk/rvv-topic.mk

dump_test_rvv: | $(ASM_DIR)/$(ARCH)
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_TEST=$(TARGET_TEST_RVV) $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_RVV)
	$(OBJDUMP) -d -C $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_RVV) > $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).full.asm
	grep -E $(VEC_REGEX_STR) $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).full.asm > $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm || true
	@echo "[DONE] range_image_border_extractor RVV test asm dump: $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm"

run_qemu_smoke: run_test_compare

board_repeated:
	@set -eu; \
	for i in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
			BENCH_ARGS="$(RIBE_REPEATED_BENCH_ARGS)" \
			OUTPUT_DIR_BOARD="$(REPEATED_BOARD_OUTPUT_DIR)/$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated_$(REPEATED_BOARD_REMOTE_TAG)/$$run"; \
	done

evidence_manifest_repeated:
	python3 script/generate_range_image_border_extractor_evidence_manifest.py \
		--summary-dir "$(REPEATED_BOARD_OUTPUT_DIR)" \
		--output "$(EVIDENCE_MANIFEST_REPEATED)" \
		--summary-output "$(REPEATED_BOARD_OUTPUT_DIR)/summary.md" \
		--title "$(REPEATED_BOARD_TITLE)" \
		--run-label "$(REPEATED_BOARD_RUN_LABEL)" \
		--case-name "$(REPEATED_BOARD_CASE_NAME)" \
		--expected-runs "$(REPEATED_BOARD_RUNS)" \
		--asm "$(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm" \
		--std-binary "$(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD)" \
		--rvv-binary "$(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV)"

evidence_doctor_repeated: evidence_manifest_repeated
	python3 ../../script/evidence_doctor.py \
		--manifest "$(EVIDENCE_MANIFEST_REPEATED)" \
		--output "$(EVIDENCE_DOCTOR_REPEATED_MD)" \
		--json-output "$(EVIDENCE_DOCTOR_REPEATED_JSON)" \
		--fail-on never

record_evidence_repeated:
	python3 ../../script/evidence_registry.py record \
		--registry "$(RIBE_EVIDENCE_REGISTRY)" \
		--summary "$(REPEATED_BOARD_OUTPUT_DIR)/summary.md" \
		--manifest "$(EVIDENCE_MANIFEST_REPEATED)" \
		--doctor "$(EVIDENCE_DOCTOR_REPEATED_MD)" \
		--target evidence_doctor_repeated \
		--backend board \
		--run-label "$(REPEATED_BOARD_RUN_LABEL)" \
		--case-filter "$(REPEATED_BOARD_CASE_NAME)" \
		--evidence-role "$(REPEATED_BOARD_EVIDENCE_ROLE)" \
		--doc-ref test-rvv/features/range_image_border_extractor/doc/benchmark-and-evidence.zh.md \
		--doc-ref test-rvv/features/range_image_border_extractor/doc/range_image_border_extractor-evaluation.zh.md \
		--doc-ref test-rvv/features/range_image_border_extractor/doc/phases/000-current-state-and-score-update/result.zh.md \
		--doc-ref test-rvv/features/range_image_border_extractor/doc/phases/010-production-shaped-score-pipeline/result.zh.md \
		--doc-ref test-rvv/features/range_image_border_extractor/doc/phases/020-range-image-fixture-and-neighbor-score/result.zh.md \
		--doc-ref test-rvv/features/range_image_border_extractor/doc/phases/030-score-generation-component-split/result.zh.md \
		--freshness-state fresh

record_evidence_phase000:
	@$(MAKE) -C $(CURDIR) record_evidence_repeated \
		REPEATED_BOARD_TAG=phase000_score_update \
		REPEATED_BOARD_RUN_LABEL=range_image_border_extractor_phase000_score_update_repeated \
		REPEATED_BOARD_CASE_NAME=score_update_641x481_tail

record_evidence_phase010:
	@$(MAKE) -C $(CURDIR) record_evidence_repeated \
		REPEATED_BOARD_TAG=phase010_four_score_pipeline_clean \
		REPEATED_BOARD_RUN_LABEL=range_image_border_extractor_phase010_four_score_pipeline_repeated_clean \
		REPEATED_BOARD_CASE_NAME=score_pipeline_641x481_four_images

record_evidence_phase020:
	@$(MAKE) -C $(CURDIR) record_evidence_repeated \
		REPEATED_BOARD_TAG=phase020_range_image_generation_score_update \
		REPEATED_BOARD_RUN_LABEL=range_image_border_extractor_phase020_range_image_generation_score_update_repeated \
		REPEATED_BOARD_CASE_NAME=range_image_score_generation_160x120,range_image_generation_plus_update_160x120 \
		REPEATED_BOARD_EVIDENCE_ROLE='production-shaped diagnostic'

record_evidence_phase030:
	@$(MAKE) -C $(CURDIR) record_evidence_repeated \
		REPEATED_BOARD_TAG=phase030_score_generation_component_split \
		REPEATED_BOARD_RUN_LABEL=range_image_border_extractor_phase030_score_generation_component_split_repeated \
		REPEATED_BOARD_CASE_NAME=range_image_local_surface_160x120,range_image_border_scores_after_surface_160x120 \
		REPEATED_BOARD_EVIDENCE_ROLE='production-shaped diagnostic'

refresh_evidence_registry:
	rm -f "$(RIBE_EVIDENCE_REGISTRY)"
	@$(MAKE) -C $(CURDIR) record_evidence_phase000
	@$(MAKE) -C $(CURDIR) record_evidence_phase010
	@$(MAKE) -C $(CURDIR) record_evidence_phase020
	@$(MAKE) -C $(CURDIR) record_evidence_phase030

evidence_status:
	@python3 ../../script/evidence_registry.py check \
		--registry "$(RIBE_EVIDENCE_REGISTRY)" \
		--scan-glob 'test-rvv/features/range_image_border_extractor/log/board/repeated_phase000_score_update/summary.md' \
		--scan-glob 'test-rvv/features/range_image_border_extractor/log/board/repeated_phase000_score_update/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/range_image_border_extractor/log/board/repeated_phase000_score_update/evidence_doctor.md' \
		--scan-glob 'test-rvv/features/range_image_border_extractor/log/board/repeated_phase010_four_score_pipeline_clean/summary.md' \
		--scan-glob 'test-rvv/features/range_image_border_extractor/log/board/repeated_phase010_four_score_pipeline_clean/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/range_image_border_extractor/log/board/repeated_phase010_four_score_pipeline_clean/evidence_doctor.md' \
		--scan-glob 'test-rvv/features/range_image_border_extractor/log/board/repeated_phase020_range_image_generation_score_update/summary.md' \
		--scan-glob 'test-rvv/features/range_image_border_extractor/log/board/repeated_phase020_range_image_generation_score_update/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/range_image_border_extractor/log/board/repeated_phase020_range_image_generation_score_update/evidence_doctor.md' \
		--scan-glob 'test-rvv/features/range_image_border_extractor/log/board/repeated_phase030_score_generation_component_split/summary.md' \
		--scan-glob 'test-rvv/features/range_image_border_extractor/log/board/repeated_phase030_score_generation_component_split/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/range_image_border_extractor/log/board/repeated_phase030_score_generation_component_split/evidence_doctor.md' \
		--doc test-rvv/features/range_image_border_extractor/doc/benchmark-and-evidence.zh.md \
		--doc test-rvv/features/range_image_border_extractor/doc/range_image_border_extractor-evaluation.zh.md \
		--doc test-rvv/features/range_image_border_extractor/doc/phases/000-current-state-and-score-update/result.zh.md \
		--doc test-rvv/features/range_image_border_extractor/doc/phases/010-production-shaped-score-pipeline/result.zh.md \
		--doc test-rvv/features/range_image_border_extractor/doc/phases/020-range-image-fixture-and-neighbor-score/result.zh.md \
		--doc test-rvv/features/range_image_border_extractor/doc/phases/030-score-generation-component-split/result.zh.md \
		$(EXTRA_EVIDENCE_STATUS_ARGS) \
		--fail-on never

check_evidence_doc_refs:
	@$(MAKE) -C $(CURDIR) evidence_status EXTRA_EVIDENCE_STATUS_ARGS=--require-doc-ref

.PHONY: dump_test_rvv run_qemu_smoke board_repeated evidence_manifest_repeated evidence_doctor_repeated record_evidence_repeated record_evidence_phase000 record_evidence_phase010 record_evidence_phase020 record_evidence_phase030 refresh_evidence_registry evidence_status check_evidence_doc_refs
