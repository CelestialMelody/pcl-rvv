# =============================================================================
# PCL recognition/linemod_template_scoring RVV topic Makefile
#
# QEMU 只用于 correctness（正确性）和日志形状；性能结论必须来自
# board（板卡）或目标硬件。
# =============================================================================

TOPIC  := linemod_template_scoring
MODULE := recognition
TOPIC_DOC_PREFIX := test-rvv/recognition/linemod_template_scoring

FOCUS_DIR      := pcl/recognition/src/linemod.cpp
FILTER_REPORT  := log/filtered_linemod_template_scoring.log
ANALYZE_REPORT := log/analyze_linemod_template_scoring.log

TARGET_TEST     := test_linemod_template_scoring
TARGET_TEST_STD := test_linemod_template_scoring_std
TARGET_TEST_RVV := test_linemod_template_scoring_rvv
SRCS_TEST       := src/test_linemod_template_scoring.cpp

TARGET_BENCH     := bench_linemod_template_scoring
TARGET_BENCH_STD := bench_linemod_template_scoring_std
TARGET_BENCH_RVV := bench_linemod_template_scoring_rvv
SRCS_BENCH       := src/bench_linemod_template_scoring.cpp
BENCH_ARGS       ?= 4096 96 200 5

REMOTE_DIR ?= /root/pcl-test/recognition/linemod_template_scoring
REMOTE_BENCH_ARGS ?= $(BENCH_ARGS)
REMOTE_TEST_ARGS ?=
BENCH_ITERATIONS := $(word 3,$(BENCH_ARGS))

EVIDENCE_REGISTRY := log/evidence_registry.json
PHASE000_DIR := doc/phases/000-current-state-and-score-accumulation
PHASE000_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase000-score-accumulation-warmup5
PHASE000_REPEATED_REMOTE_DIR ?= $(REMOTE_DIR)/log/board/repeated-20260828-phase000-score-accumulation-warmup5
PHASE000_REPEATED_RUNS ?= 01 02 03 04 05
PHASE000_SUMMARY := $(PHASE000_DIR)/score-accumulation-repeated-summary.md
PHASE000_EVIDENCE_MANIFEST := $(PHASE000_DIR)/score-accumulation-repeated-evidence-manifest.json
PHASE000_EVIDENCE_DOCTOR_MD := $(PHASE000_DIR)/score-accumulation-repeated-evidence-doctor.md
PHASE000_EVIDENCE_DOCTOR_JSON := $(PHASE000_DIR)/score-accumulation-repeated-evidence-doctor.json
PHASE000_RVV_BINARY := build/riscv/bench_linemod_template_scoring_rvv
PHASE000_ASM := build/asm/riscv/bench_linemod_template_scoring_rvv.full.asm
PHASE010_DIR := doc/phases/010-threshold-scan-and-detection-order
PHASE010_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase010-score-scan-warmup5
PHASE010_REPEATED_REMOTE_DIR ?= $(REMOTE_DIR)/log/board/repeated-20260828-phase010-score-scan-warmup5
PHASE010_REPEATED_RUNS ?= 01 02 03 04 05
PHASE010_SUMMARY := $(PHASE010_DIR)/score-scan-repeated-summary.md
PHASE010_EVIDENCE_MANIFEST := $(PHASE010_DIR)/score-scan-repeated-evidence-manifest.json
PHASE010_EVIDENCE_DOCTOR_MD := $(PHASE010_DIR)/score-scan-repeated-evidence-doctor.md
PHASE010_EVIDENCE_DOCTOR_JSON := $(PHASE010_DIR)/score-scan-repeated-evidence-doctor.json
PHASE010_RVV_BINARY := build/riscv/bench_linemod_template_scoring_rvv
PHASE010_ASM := build/asm/riscv/bench_linemod_template_scoring_rvv.full.asm
PHASE020_DIR := doc/phases/020-accumulation-bench-boundary-ablation
PHASE020_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase020-accumulation-ablation-warmup5
PHASE020_REPEATED_REMOTE_DIR ?= $(REMOTE_DIR)/log/board/repeated-20260828-phase020-accumulation-ablation-warmup5
PHASE020_REPEATED_RUNS ?= 01 02 03 04 05
PHASE020_SUMMARY := $(PHASE020_DIR)/accumulation-ablation-repeated-summary.md
PHASE020_EVIDENCE_MANIFEST := $(PHASE020_DIR)/accumulation-ablation-repeated-evidence-manifest.json
PHASE020_EVIDENCE_DOCTOR_MD := $(PHASE020_DIR)/accumulation-ablation-repeated-evidence-doctor.md
PHASE020_EVIDENCE_DOCTOR_JSON := $(PHASE020_DIR)/accumulation-ablation-repeated-evidence-doctor.json
PHASE020_TARGET_BENCH_STD := bench_linemod_template_scoring_accumulation_only_std
PHASE020_TARGET_BENCH_RVV := bench_linemod_template_scoring_accumulation_only_rvv
PHASE020_RVV_BINARY := build/riscv/$(PHASE020_TARGET_BENCH_RVV)
PHASE020_ASM := build/asm/riscv/$(PHASE020_TARGET_BENCH_RVV).full.asm
PHASE030_DIR := doc/phases/030-energy-map-generation
PHASE030_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase030-energy-map-generation-warmup5
PHASE030_REPEATED_REMOTE_DIR ?= $(REMOTE_DIR)/log/board/repeated-20260828-phase030-energy-map-generation-warmup5
PHASE030_REPEATED_RUNS ?= 01 02 03 04 05
PHASE030_SUMMARY := $(PHASE030_DIR)/energy-map-repeated-summary.md
PHASE030_EVIDENCE_MANIFEST := $(PHASE030_DIR)/energy-map-repeated-evidence-manifest.json
PHASE030_EVIDENCE_DOCTOR_MD := $(PHASE030_DIR)/energy-map-repeated-evidence-doctor.md
PHASE030_EVIDENCE_DOCTOR_JSON := $(PHASE030_DIR)/energy-map-repeated-evidence-doctor.json
PHASE030_TARGET_BENCH_STD := bench_linemod_template_scoring_energy_map_only_std
PHASE030_TARGET_BENCH_RVV := bench_linemod_template_scoring_energy_map_only_rvv
PHASE030_RVV_BINARY := build/riscv/$(PHASE030_TARGET_BENCH_RVV)
PHASE030_ASM := build/asm/riscv/$(PHASE030_TARGET_BENCH_RVV).full.asm
PHASE040_DIR := doc/phases/040-linearized-map-copy-ablation
PHASE040_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase040-linearized-map-copy-warmup5
PHASE040_REPEATED_REMOTE_DIR ?= $(REMOTE_DIR)/log/board/repeated-20260828-phase040-linearized-map-copy-warmup5
PHASE040_REPEATED_RUNS ?= 01 02 03 04 05
PHASE040_SUMMARY := $(PHASE040_DIR)/linearized-map-copy-repeated-summary.md
PHASE040_EVIDENCE_MANIFEST := $(PHASE040_DIR)/linearized-map-copy-repeated-evidence-manifest.json
PHASE040_EVIDENCE_DOCTOR_MD := $(PHASE040_DIR)/linearized-map-copy-repeated-evidence-doctor.md
PHASE040_EVIDENCE_DOCTOR_JSON := $(PHASE040_DIR)/linearized-map-copy-repeated-evidence-doctor.json
PHASE040_TARGET_BENCH_STD := bench_linemod_template_scoring_linearized_map_copy_only_std
PHASE040_TARGET_BENCH_RVV := bench_linemod_template_scoring_linearized_map_copy_only_rvv
PHASE040_RVV_BINARY := build/riscv/$(PHASE040_TARGET_BENCH_RVV)
PHASE040_ASM := build/asm/riscv/$(PHASE040_TARGET_BENCH_RVV).full.asm
PHASE050_DIR := doc/phases/050-full-chain-timing-and-production-eligibility
PHASE050_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase050-full-chain-split-warmup5
PHASE050_REPEATED_REMOTE_DIR ?= $(REMOTE_DIR)/log/board/repeated-20260828-phase050-full-chain-split-warmup5
PHASE050_REPEATED_RUNS ?= 01 02 03 04 05
PHASE050_SUMMARY := $(PHASE050_DIR)/full-chain-split-repeated-summary.md
PHASE050_EVIDENCE_MANIFEST := $(PHASE050_DIR)/full-chain-split-repeated-evidence-manifest.json
PHASE050_EVIDENCE_DOCTOR_MD := $(PHASE050_DIR)/full-chain-split-repeated-evidence-doctor.md
PHASE050_EVIDENCE_DOCTOR_JSON := $(PHASE050_DIR)/full-chain-split-repeated-evidence-doctor.json
PHASE050_TARGET_BENCH_STD := bench_linemod_template_scoring_full_chain_split_std
PHASE050_TARGET_BENCH_RVV := bench_linemod_template_scoring_full_chain_split_rvv
PHASE050_RVV_BINARY := build/riscv/$(PHASE050_TARGET_BENCH_RVV)
PHASE050_ASM := build/asm/riscv/$(PHASE050_TARGET_BENCH_RVV).full.asm
PHASE070_DIR := doc/phases/070-production-integration-loop
PHASE070_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase070-production-direct-warmup5
PHASE070_REPEATED_REMOTE_DIR ?= $(REMOTE_DIR)/log/board/repeated-20260828-phase070-production-direct-warmup5
PHASE070_REPEATED_RUNS ?= 01 02 03 04 05
PHASE070_SUMMARY := $(PHASE070_DIR)/production-direct-repeated-summary.md
PHASE070_EVIDENCE_MANIFEST := $(PHASE070_DIR)/production-direct-repeated-evidence-manifest.json
PHASE070_EVIDENCE_DOCTOR_MD := $(PHASE070_DIR)/production-direct-repeated-evidence-doctor.md
PHASE070_EVIDENCE_DOCTOR_JSON := $(PHASE070_DIR)/production-direct-repeated-evidence-doctor.json
PHASE070_TARGET_TEST_STD := test_linemod_template_scoring_production_direct_std
PHASE070_TARGET_TEST_RVV := test_linemod_template_scoring_production_direct_rvv
PHASE070_TARGET_BENCH_STD := bench_linemod_template_scoring_production_direct_std
PHASE070_TARGET_BENCH_RVV := bench_linemod_template_scoring_production_direct_rvv
PHASE070_TEST_STD_BINARY := build/riscv/$(PHASE070_TARGET_TEST_STD)
PHASE070_TEST_RVV_BINARY := build/riscv/$(PHASE070_TARGET_TEST_RVV)
PHASE070_BENCH_STD_BINARY := build/riscv/$(PHASE070_TARGET_BENCH_STD)
PHASE070_BENCH_RVV_BINARY := build/riscv/$(PHASE070_TARGET_BENCH_RVV)
PHASE070_ASM := build/asm/riscv/$(PHASE070_TARGET_BENCH_RVV).full.asm
PHASE070_PRODUCTION_SRCS = \
	$(PCL_SOURCE_ROOT)/recognition/src/linemod.cpp \
	$(PCL_SOURCE_ROOT)/recognition/src/quantizable_modality.cpp \
	$(PCL_SOURCE_ROOT)/recognition/src/mask_map.cpp
PHASE070_TEST_SRCS = src/test_linemod_template_scoring_production_direct.cpp $(PHASE070_PRODUCTION_SRCS)
PHASE070_BENCH_SRCS = src/bench_linemod_template_scoring_production_direct.cpp $(PHASE070_PRODUCTION_SRCS)
PHASE070_LIBS_TEST := -lpcl_common -lboost_system -llz4 -lgtest -lgtest_main -lpthread -lm
PHASE070_LIBS_BENCH := -lpcl_common -lboost_system -llz4 -lm
PHASE080_DIR := doc/phases/080-semi-scale-production-direct-probe
PHASE080_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase080-semi-scale-production-direct-warmup5
PHASE080_REPEATED_REMOTE_DIR ?= $(REMOTE_DIR)/log/board/repeated-20260828-phase080-semi-scale-production-direct-warmup5
PHASE080_REPEATED_RUNS ?= 01 02 03 04 05
PHASE080_SUMMARY := $(PHASE080_DIR)/semi-scale-production-direct-repeated-summary.md
PHASE080_EVIDENCE_MANIFEST := $(PHASE080_DIR)/semi-scale-production-direct-repeated-evidence-manifest.json
PHASE080_EVIDENCE_DOCTOR_MD := $(PHASE080_DIR)/semi-scale-production-direct-repeated-evidence-doctor.md
PHASE080_EVIDENCE_DOCTOR_JSON := $(PHASE080_DIR)/semi-scale-production-direct-repeated-evidence-doctor.json
PHASE080_TARGET_TEST_STD := test_linemod_template_scoring_semiscale_production_direct_std
PHASE080_TARGET_TEST_RVV := test_linemod_template_scoring_semiscale_production_direct_rvv
PHASE080_TARGET_BENCH_STD := bench_linemod_template_scoring_semiscale_production_direct_std
PHASE080_TARGET_BENCH_RVV := bench_linemod_template_scoring_semiscale_production_direct_rvv
PHASE080_TEST_STD_BINARY := build/riscv/$(PHASE080_TARGET_TEST_STD)
PHASE080_TEST_RVV_BINARY := build/riscv/$(PHASE080_TARGET_TEST_RVV)
PHASE080_BENCH_STD_BINARY := build/riscv/$(PHASE080_TARGET_BENCH_STD)
PHASE080_BENCH_RVV_BINARY := build/riscv/$(PHASE080_TARGET_BENCH_RVV)
PHASE080_ASM := build/asm/riscv/$(PHASE080_TARGET_BENCH_RVV).full.asm
PHASE080_TEST_SRCS = $(PHASE070_TEST_SRCS)
PHASE080_BENCH_SRCS = $(PHASE070_BENCH_SRCS)
PHASE080_LIBS_TEST := $(PHASE070_LIBS_TEST)
PHASE080_LIBS_BENCH := $(PHASE070_LIBS_BENCH)

EVIDENCE_DOC_REFS := \
	$(TOPIC_DOC_PREFIX)/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/linemod_template_scoring-evaluation.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/testing-overview.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/correctness-tests.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/benchmark-and-evidence.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-evidence.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/test-support-code-map.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-roadmap.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/optimization-matrix.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/000-current-state-and-score-accumulation/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/000-current-state-and-score-accumulation/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/010-threshold-scan-and-detection-order/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/010-threshold-scan-and-detection-order/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-accumulation-bench-boundary-ablation/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-accumulation-bench-boundary-ablation/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/030-energy-map-generation/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/030-energy-map-generation/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/040-linearized-map-copy-ablation/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/040-linearized-map-copy-ablation/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/050-full-chain-timing-and-production-eligibility/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/050-full-chain-timing-and-production-eligibility/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/060-pi1-production-integration-plan/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/060-pi1-production-integration-plan/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/070-production-integration-loop/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/070-production-integration-loop/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/080-semi-scale-production-direct-probe/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/080-semi-scale-production-direct-probe/result.zh.md \
	doc-rvv/recognition/linemod_template_scoring-RVV.zh.md \
	doc-rvv/library-screening/recognition/recognition-function-evaluation-queue.zh.md
EVIDENCE_DOC_ARGS := $(foreach doc,$(EVIDENCE_DOC_REFS),--doc $(doc))
EVIDENCE_DOC_REF_ARGS := $(foreach doc,$(EVIDENCE_DOC_REFS),--doc-ref $(doc))

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/recognition/include \
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

LIBS_CORE := -lpcl_recognition -lpcl_common -lboost_system -llz4 -lm
LIBS_TEST := $(LIBS_CORE) -lgtest -lgtest_main -lpthread
LIBS_BENCH := $(LIBS_CORE)

include ../../mk/rvv-topic.mk

.PHONY: run_linemod_score_correctness dump_linemod_score_bench_rvv collect_score_accumulation_repeated_board generate_score_accumulation_evidence_manifest run_score_accumulation_evidence_doctor record_score_accumulation_evidence_state check_score_accumulation_evidence_freshness collect_score_scan_repeated_board generate_score_scan_evidence_manifest run_score_scan_evidence_doctor record_score_scan_evidence_state check_score_scan_evidence_freshness dump_accumulation_ablation_bench_rvv collect_accumulation_ablation_repeated_board generate_accumulation_ablation_evidence_manifest run_accumulation_ablation_evidence_doctor record_accumulation_ablation_evidence_state check_accumulation_ablation_evidence_freshness dump_energy_map_bench_rvv collect_energy_map_repeated_board generate_energy_map_evidence_manifest run_energy_map_evidence_doctor record_energy_map_evidence_state check_energy_map_evidence_freshness dump_linearized_map_copy_bench_rvv collect_linearized_map_copy_repeated_board generate_linearized_map_copy_evidence_manifest run_linearized_map_copy_evidence_doctor record_linearized_map_copy_evidence_state check_linearized_map_copy_evidence_freshness dump_full_chain_split_bench_rvv collect_full_chain_split_repeated_board generate_full_chain_split_evidence_manifest run_full_chain_split_evidence_doctor record_full_chain_split_evidence_state check_full_chain_split_evidence_freshness run_production_direct_test_std run_production_direct_test_rvv run_production_direct_test_compare dump_production_direct_bench_rvv collect_production_direct_repeated_board generate_production_direct_evidence_manifest run_production_direct_evidence_doctor record_production_direct_evidence_state check_production_direct_evidence_freshness run_semiscale_production_direct_test_std run_semiscale_production_direct_test_rvv run_semiscale_production_direct_test_compare dump_semiscale_production_direct_bench_rvv collect_semiscale_production_direct_repeated_board generate_semiscale_production_direct_evidence_manifest run_semiscale_production_direct_evidence_doctor record_semiscale_production_direct_evidence_state check_semiscale_production_direct_evidence_freshness

run_linemod_score_correctness:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=LINEMODTemplateScoring.ScoreAccumulation*"

dump_linemod_score_bench_rvv: | $(ASM_DIR)/$(ARCH)
	@$(MAKE) -C $(CURDIR) clean_bench_rvv
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_BENCH=$(TARGET_BENCH_RVV) $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV)
	$(OBJDUMP) -d -C $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) > $(PHASE010_ASM)
	grep -E $(VEC_REGEX_STR) $(PHASE010_ASM) > $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm || true
	@echo "[DONE] LINEMOD score bench RVV asm dump: $(PHASE010_ASM)"

collect_score_accumulation_repeated_board: dump_linemod_score_bench_rvv
	rm -rf $(PHASE000_REPEATED_BOARD_DIR)
	@for run in $(PHASE000_REPEATED_RUNS); do \
		echo "[BOARD] Phase 000 score accumulation repeated run $$run"; \
		$(MAKE) -C $(CURDIR) board_smoke \
			OUTPUT_DIR_BOARD="$(PHASE000_REPEATED_BOARD_DIR)/run-$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(PHASE000_REPEATED_REMOTE_DIR)/run-$$run" \
			BENCH_ARGS="$(BENCH_ARGS)" \
			REMOTE_BENCH_ARGS="$(REMOTE_BENCH_ARGS)" \
			REMOTE_TEST_ARGS="--gtest_filter=LINEMODTemplateScoring.*"; \
	done

generate_score_accumulation_evidence_manifest: dump_linemod_score_bench_rvv
	$(PYTHON_RUN) script/generate_linemod_template_scoring_evidence_manifest.py \
		--repeated-dir "$(PHASE000_REPEATED_BOARD_DIR)" \
		--asm "$(PHASE000_ASM)" \
		--rvv-binary "$(PHASE000_RVV_BINARY)" \
		--summary "$(PHASE000_SUMMARY)" \
		--output "$(PHASE000_EVIDENCE_MANIFEST)"

run_score_accumulation_evidence_doctor: generate_score_accumulation_evidence_manifest
	$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest "$(PHASE000_EVIDENCE_MANIFEST)" \
		--output "$(PHASE000_EVIDENCE_DOCTOR_MD)" \
		--json-output "$(PHASE000_EVIDENCE_DOCTOR_JSON)" \
		--fail-on never

record_score_accumulation_evidence_state: run_score_accumulation_evidence_doctor
	$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(PHASE000_SUMMARY)" \
		--manifest "$(PHASE000_EVIDENCE_MANIFEST)" \
		--doctor "$(PHASE000_EVIDENCE_DOCTOR_MD)" \
		--path "$(PHASE000_EVIDENCE_DOCTOR_JSON)" \
		--target "$@" \
		--backend board \
		--run-label "phase000-score-accumulation-repeated-20260828" \
		--case-filter "mem_size=4096,nr_maps=96,iterations=$(BENCH_ITERATIONS)" \
		--evidence-role "production_shaped_diagnostic" \
		$(EVIDENCE_DOC_REF_ARGS)

check_score_accumulation_evidence_freshness:
	$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob "$(TOPIC_DOC_PREFIX)/doc/phases/000-current-state-and-score-accumulation/score-accumulation-repeated-*.md" \
		--scan-glob "$(TOPIC_DOC_PREFIX)/doc/phases/000-current-state-and-score-accumulation/score-accumulation-repeated-*.json" \
		$(EVIDENCE_DOC_ARGS) \
		--require-doc-ref \
		--fail-on never

collect_score_scan_repeated_board: dump_linemod_score_bench_rvv
	rm -rf $(PHASE010_REPEATED_BOARD_DIR)
	@for run in $(PHASE010_REPEATED_RUNS); do \
		echo "[BOARD] Phase 010 score scan repeated run $$run"; \
		$(MAKE) -C $(CURDIR) board_smoke \
			OUTPUT_DIR_BOARD="$(PHASE010_REPEATED_BOARD_DIR)/run-$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(PHASE010_REPEATED_REMOTE_DIR)/run-$$run" \
			BENCH_ARGS="$(BENCH_ARGS)" \
			REMOTE_BENCH_ARGS="$(REMOTE_BENCH_ARGS)" \
			REMOTE_TEST_ARGS="--gtest_filter=LINEMODTemplateScoring.*"; \
	done

generate_score_scan_evidence_manifest: dump_linemod_score_bench_rvv
	$(PYTHON_RUN) script/generate_linemod_template_scoring_evidence_manifest.py \
		--repeated-dir "$(PHASE010_REPEATED_BOARD_DIR)" \
		--asm "$(PHASE010_ASM)" \
		--rvv-binary "$(PHASE010_RVV_BINARY)" \
		--summary "$(PHASE010_SUMMARY)" \
		--output "$(PHASE010_EVIDENCE_MANIFEST)"

run_score_scan_evidence_doctor: generate_score_scan_evidence_manifest
	$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest "$(PHASE010_EVIDENCE_MANIFEST)" \
		--output "$(PHASE010_EVIDENCE_DOCTOR_MD)" \
		--json-output "$(PHASE010_EVIDENCE_DOCTOR_JSON)" \
		--fail-on never

record_score_scan_evidence_state: run_score_scan_evidence_doctor
	$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(PHASE010_SUMMARY)" \
		--manifest "$(PHASE010_EVIDENCE_MANIFEST)" \
		--doctor "$(PHASE010_EVIDENCE_DOCTOR_MD)" \
		--path "$(PHASE010_EVIDENCE_DOCTOR_JSON)" \
		--target "$@" \
		--backend board \
		--run-label "phase010-score-scan-repeated-20260828" \
		--case-filter "mem_size=4096,nr_maps=96,iterations=$(BENCH_ITERATIONS)" \
		--evidence-role "production_shaped_diagnostic" \
		$(EVIDENCE_DOC_REF_ARGS)

check_score_scan_evidence_freshness:
	$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob "$(TOPIC_DOC_PREFIX)/doc/phases/010-threshold-scan-and-detection-order/score-scan-repeated-*.md" \
		--scan-glob "$(TOPIC_DOC_PREFIX)/doc/phases/010-threshold-scan-and-detection-order/score-scan-repeated-*.json" \
		$(EVIDENCE_DOC_ARGS) \
		--require-doc-ref \
		--fail-on never

dump_accumulation_ablation_bench_rvv: | $(ASM_DIR)/$(ARCH)
	@rm -f $(PHASE020_RVV_BINARY)
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_BENCH=$(PHASE020_TARGET_BENCH_RVV) EXTRA_CXXFLAGS="-DLINEMOD_SCORE_BENCH_ACCUMULATION_ONLY" $(PHASE020_RVV_BINARY)
	$(OBJDUMP) -d -C $(PHASE020_RVV_BINARY) > $(PHASE020_ASM)
	grep -E $(VEC_REGEX_STR) $(PHASE020_ASM) > $(ASM_DIR)/$(ARCH)/$(PHASE020_TARGET_BENCH_RVV).asm || true
	@echo "[DONE] Phase 020 RVV asm dump: $(PHASE020_ASM)"

collect_accumulation_ablation_repeated_board: dump_accumulation_ablation_bench_rvv
	rm -rf $(PHASE020_REPEATED_BOARD_DIR)
	@for run in $(PHASE020_REPEATED_RUNS); do \
		echo "[BOARD] Phase 020 accumulation-only ablation repeated run $$run"; \
		$(MAKE) -C $(CURDIR) board_smoke \
			OUTPUT_DIR_BOARD="$(PHASE020_REPEATED_BOARD_DIR)/run-$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(PHASE020_REPEATED_REMOTE_DIR)/run-$$run" \
			BENCH_ARGS="$(BENCH_ARGS)" \
			REMOTE_BENCH_ARGS="$(REMOTE_BENCH_ARGS)" \
			REMOTE_TEST_ARGS="--gtest_filter=LINEMODTemplateScoring.*" \
			EXTRA_CXXFLAGS="-DLINEMOD_SCORE_BENCH_ACCUMULATION_ONLY"; \
	done

generate_accumulation_ablation_evidence_manifest: dump_accumulation_ablation_bench_rvv
	$(PYTHON_RUN) script/generate_linemod_template_scoring_evidence_manifest.py \
		--repeated-dir "$(PHASE020_REPEATED_BOARD_DIR)" \
		--asm "$(PHASE020_ASM)" \
		--rvv-binary "$(PHASE020_RVV_BINARY)" \
		--summary "$(PHASE020_SUMMARY)" \
		--output "$(PHASE020_EVIDENCE_MANIFEST)"

run_accumulation_ablation_evidence_doctor: generate_accumulation_ablation_evidence_manifest
	$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest "$(PHASE020_EVIDENCE_MANIFEST)" \
		--output "$(PHASE020_EVIDENCE_DOCTOR_MD)" \
		--json-output "$(PHASE020_EVIDENCE_DOCTOR_JSON)" \
		--fail-on never

record_accumulation_ablation_evidence_state: run_accumulation_ablation_evidence_doctor
	$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(PHASE020_SUMMARY)" \
		--manifest "$(PHASE020_EVIDENCE_MANIFEST)" \
		--doctor "$(PHASE020_EVIDENCE_DOCTOR_MD)" \
		--path "$(PHASE020_EVIDENCE_DOCTOR_JSON)" \
		--target "$@" \
		--backend board \
		--run-label "phase020-accumulation-ablation-repeated-20260828" \
		--case-filter "mem_size=4096,nr_maps=96,iterations=$(BENCH_ITERATIONS),accumulation_only=1" \
		--evidence-role "production_shaped_diagnostic" \
		$(EVIDENCE_DOC_REF_ARGS)

check_accumulation_ablation_evidence_freshness:
	$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob "$(TOPIC_DOC_PREFIX)/doc/phases/020-accumulation-bench-boundary-ablation/accumulation-ablation-repeated-*.md" \
		--scan-glob "$(TOPIC_DOC_PREFIX)/doc/phases/020-accumulation-bench-boundary-ablation/accumulation-ablation-repeated-*.json" \
		$(EVIDENCE_DOC_ARGS) \
		--require-doc-ref \
		--fail-on never

dump_energy_map_bench_rvv: | $(ASM_DIR)/$(ARCH)
	@rm -f $(PHASE030_RVV_BINARY)
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_BENCH=$(PHASE030_TARGET_BENCH_RVV) EXTRA_CXXFLAGS="-DLINEMOD_SCORE_BENCH_ENERGY_MAP_ONLY" $(PHASE030_RVV_BINARY)
	$(OBJDUMP) -d -C $(PHASE030_RVV_BINARY) > $(PHASE030_ASM)
	grep -E $(VEC_REGEX_STR) $(PHASE030_ASM) > $(ASM_DIR)/$(ARCH)/$(PHASE030_TARGET_BENCH_RVV).asm || true
	@echo "[DONE] Phase 030 RVV asm dump: $(PHASE030_ASM)"

collect_energy_map_repeated_board: dump_energy_map_bench_rvv
	rm -rf $(PHASE030_REPEATED_BOARD_DIR)
	@for run in $(PHASE030_REPEATED_RUNS); do \
		echo "[BOARD] Phase 030 energy map generation repeated run $$run"; \
		$(MAKE) -C $(CURDIR) board_smoke \
			OUTPUT_DIR_BOARD="$(PHASE030_REPEATED_BOARD_DIR)/run-$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(PHASE030_REPEATED_REMOTE_DIR)/run-$$run" \
			BENCH_ARGS="$(BENCH_ARGS)" \
			REMOTE_BENCH_ARGS="$(REMOTE_BENCH_ARGS)" \
			REMOTE_TEST_ARGS="--gtest_filter=LINEMODTemplateScoring.EnergyMapGeneration*" \
			EXTRA_CXXFLAGS="-DLINEMOD_SCORE_BENCH_ENERGY_MAP_ONLY"; \
	done

generate_energy_map_evidence_manifest: dump_energy_map_bench_rvv
	$(PYTHON_RUN) script/generate_linemod_template_scoring_evidence_manifest.py \
		--repeated-dir "$(PHASE030_REPEATED_BOARD_DIR)" \
		--asm "$(PHASE030_ASM)" \
		--rvv-binary "$(PHASE030_RVV_BINARY)" \
		--summary "$(PHASE030_SUMMARY)" \
		--output "$(PHASE030_EVIDENCE_MANIFEST)" \
		--include-label "LINEMOD energy map generation"

run_energy_map_evidence_doctor: generate_energy_map_evidence_manifest
	$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest "$(PHASE030_EVIDENCE_MANIFEST)" \
		--output "$(PHASE030_EVIDENCE_DOCTOR_MD)" \
		--json-output "$(PHASE030_EVIDENCE_DOCTOR_JSON)" \
		--fail-on never

record_energy_map_evidence_state: run_energy_map_evidence_doctor
	$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(PHASE030_SUMMARY)" \
		--manifest "$(PHASE030_EVIDENCE_MANIFEST)" \
		--doctor "$(PHASE030_EVIDENCE_DOCTOR_MD)" \
		--path "$(PHASE030_EVIDENCE_DOCTOR_JSON)" \
		--target "$@" \
		--backend board \
		--run-label "phase030-energy-map-repeated-20260828" \
		--case-filter "mem_size=4096,nr_maps=96,iterations=$(BENCH_ITERATIONS),energy_map_only=1" \
		--evidence-role "production_shaped_diagnostic" \
		$(EVIDENCE_DOC_REF_ARGS)

check_energy_map_evidence_freshness:
	$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob "$(TOPIC_DOC_PREFIX)/doc/phases/030-energy-map-generation/energy-map-repeated-*.md" \
		--scan-glob "$(TOPIC_DOC_PREFIX)/doc/phases/030-energy-map-generation/energy-map-repeated-*.json" \
		$(EVIDENCE_DOC_ARGS) \
		--require-doc-ref \
		--fail-on never

dump_linearized_map_copy_bench_rvv: | $(ASM_DIR)/$(ARCH)
	@rm -f $(PHASE040_RVV_BINARY)
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_BENCH=$(PHASE040_TARGET_BENCH_RVV) EXTRA_CXXFLAGS="-DLINEMOD_SCORE_BENCH_LINEARIZED_COPY_ONLY" $(PHASE040_RVV_BINARY)
	$(OBJDUMP) -d -C $(PHASE040_RVV_BINARY) > $(PHASE040_ASM)
	grep -E $(VEC_REGEX_STR) $(PHASE040_ASM) > $(ASM_DIR)/$(ARCH)/$(PHASE040_TARGET_BENCH_RVV).asm || true
	@echo "[DONE] Phase 040 RVV asm dump: $(PHASE040_ASM)"

collect_linearized_map_copy_repeated_board: dump_linearized_map_copy_bench_rvv
	rm -rf $(PHASE040_REPEATED_BOARD_DIR)
	@for run in $(PHASE040_REPEATED_RUNS); do \
		echo "[BOARD] Phase 040 linearized map copy repeated run $$run"; \
		$(MAKE) -C $(CURDIR) board_smoke \
			OUTPUT_DIR_BOARD="$(PHASE040_REPEATED_BOARD_DIR)/run-$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(PHASE040_REPEATED_REMOTE_DIR)/run-$$run" \
			BENCH_ARGS="$(BENCH_ARGS)" \
			REMOTE_BENCH_ARGS="$(REMOTE_BENCH_ARGS)" \
			REMOTE_TEST_ARGS="--gtest_filter=LINEMODTemplateScoring.LinearizedMapCopy*" \
			EXTRA_CXXFLAGS="-DLINEMOD_SCORE_BENCH_LINEARIZED_COPY_ONLY"; \
	done

generate_linearized_map_copy_evidence_manifest: dump_linearized_map_copy_bench_rvv
	$(PYTHON_RUN) script/generate_linemod_template_scoring_evidence_manifest.py \
		--repeated-dir "$(PHASE040_REPEATED_BOARD_DIR)" \
		--asm "$(PHASE040_ASM)" \
		--rvv-binary "$(PHASE040_RVV_BINARY)" \
		--summary "$(PHASE040_SUMMARY)" \
		--output "$(PHASE040_EVIDENCE_MANIFEST)" \
		--include-label "LINEMOD linearized map copy"

run_linearized_map_copy_evidence_doctor: generate_linearized_map_copy_evidence_manifest
	$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest "$(PHASE040_EVIDENCE_MANIFEST)" \
		--output "$(PHASE040_EVIDENCE_DOCTOR_MD)" \
		--json-output "$(PHASE040_EVIDENCE_DOCTOR_JSON)" \
		--fail-on never

record_linearized_map_copy_evidence_state: run_linearized_map_copy_evidence_doctor
	$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(PHASE040_SUMMARY)" \
		--manifest "$(PHASE040_EVIDENCE_MANIFEST)" \
		--doctor "$(PHASE040_EVIDENCE_DOCTOR_MD)" \
		--path "$(PHASE040_EVIDENCE_DOCTOR_JSON)" \
		--target "$@" \
		--backend board \
		--run-label "phase040-linearized-map-copy-repeated-20260828" \
		--case-filter "mem_size=4096,nr_maps=96,iterations=$(BENCH_ITERATIONS),linearized_map_copy_only=1" \
		--evidence-role "production_shaped_diagnostic" \
		$(EVIDENCE_DOC_REF_ARGS)

check_linearized_map_copy_evidence_freshness:
	$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob "$(TOPIC_DOC_PREFIX)/doc/phases/040-linearized-map-copy-ablation/linearized-map-copy-repeated-*.md" \
		--scan-glob "$(TOPIC_DOC_PREFIX)/doc/phases/040-linearized-map-copy-ablation/linearized-map-copy-repeated-*.json" \
		$(EVIDENCE_DOC_ARGS) \
		--require-doc-ref \
		--fail-on never

dump_full_chain_split_bench_rvv: | $(ASM_DIR)/$(ARCH)
	@rm -f $(PHASE050_RVV_BINARY)
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_BENCH=$(PHASE050_TARGET_BENCH_RVV) EXTRA_CXXFLAGS="-DLINEMOD_SCORE_BENCH_FULL_CHAIN_LINEARIZED_COPY_ONLY" $(PHASE050_RVV_BINARY)
	$(OBJDUMP) -d -C $(PHASE050_RVV_BINARY) > $(PHASE050_ASM)
	grep -E $(VEC_REGEX_STR) $(PHASE050_ASM) > $(ASM_DIR)/$(ARCH)/$(PHASE050_TARGET_BENCH_RVV).asm || true
	@echo "[DONE] Phase 050 RVV asm dump: $(PHASE050_ASM)"

collect_full_chain_split_repeated_board: dump_full_chain_split_bench_rvv
	rm -rf $(PHASE050_REPEATED_BOARD_DIR)
	@for run in $(PHASE050_REPEATED_RUNS); do \
		echo "[BOARD] Phase 050 full-chain split repeated run $$run"; \
		$(MAKE) -C $(CURDIR) board_smoke \
			OUTPUT_DIR_BOARD="$(PHASE050_REPEATED_BOARD_DIR)/run-$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(PHASE050_REPEATED_REMOTE_DIR)/run-$$run" \
			BENCH_ARGS="$(BENCH_ARGS)" \
			REMOTE_BENCH_ARGS="$(REMOTE_BENCH_ARGS)" \
			REMOTE_TEST_ARGS="--gtest_filter=LINEMODTemplateScoring.*" \
			EXTRA_CXXFLAGS="-DLINEMOD_SCORE_BENCH_FULL_CHAIN_LINEARIZED_COPY_ONLY"; \
	done

generate_full_chain_split_evidence_manifest: dump_full_chain_split_bench_rvv
	$(PYTHON_RUN) script/generate_linemod_template_scoring_evidence_manifest.py \
		--repeated-dir "$(PHASE050_REPEATED_BOARD_DIR)" \
		--asm "$(PHASE050_ASM)" \
		--rvv-binary "$(PHASE050_RVV_BINARY)" \
		--summary "$(PHASE050_SUMMARY)" \
		--output "$(PHASE050_EVIDENCE_MANIFEST)" \
		--include-label "LINEMOD full-chain total" \
		--include-label "LINEMOD full-chain linearized map copy"

run_full_chain_split_evidence_doctor: generate_full_chain_split_evidence_manifest
	$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest "$(PHASE050_EVIDENCE_MANIFEST)" \
		--output "$(PHASE050_EVIDENCE_DOCTOR_MD)" \
		--json-output "$(PHASE050_EVIDENCE_DOCTOR_JSON)" \
		--fail-on never

record_full_chain_split_evidence_state: run_full_chain_split_evidence_doctor
	$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(PHASE050_SUMMARY)" \
		--manifest "$(PHASE050_EVIDENCE_MANIFEST)" \
		--doctor "$(PHASE050_EVIDENCE_DOCTOR_MD)" \
		--path "$(PHASE050_EVIDENCE_DOCTOR_JSON)" \
		--target "$@" \
		--backend board \
		--run-label "phase050-full-chain-split-repeated-20260828" \
		--case-filter "mem_size=4096,nr_maps=96,iterations=$(BENCH_ITERATIONS),full_chain_linearized_copy_only=1" \
		--evidence-role "production_shaped_diagnostic" \
		$(EVIDENCE_DOC_REF_ARGS)

check_full_chain_split_evidence_freshness:
	$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob "$(TOPIC_DOC_PREFIX)/doc/phases/050-full-chain-timing-and-production-eligibility/full-chain-split-repeated-*.md" \
		--scan-glob "$(TOPIC_DOC_PREFIX)/doc/phases/050-full-chain-timing-and-production-eligibility/full-chain-split-repeated-*.json" \
		$(EVIDENCE_DOC_ARGS) \
		--require-doc-ref \
		--fail-on never

$(PHASE070_TEST_STD_BINARY): $(PHASE070_TEST_SRCS) | $(LOG_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Phase 070 production-direct Unit Test Std ($(ARCH))..."
	$(CXX) $(filter-out -D__RVV10__,$(CXXFLAGS)) -DPCLAPI_EXPORTS $(PHASE070_TEST_SRCS) $(LDFLAGS) $(EXTRA_LDFLAGS) $(PHASE070_LIBS_TEST) -o $@

$(PHASE070_TEST_RVV_BINARY): $(PHASE070_TEST_SRCS) | $(LOG_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Phase 070 production-direct Unit Test RVV ($(ARCH))..."
	$(CXX) $(CXXFLAGS) -DPCLAPI_EXPORTS $(PHASE070_TEST_SRCS) $(LDFLAGS) $(EXTRA_LDFLAGS) $(PHASE070_LIBS_TEST) -o $@

$(PHASE070_BENCH_STD_BINARY): $(PHASE070_BENCH_SRCS) | $(LOG_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Phase 070 production-direct Benchmark Std ($(ARCH))..."
	$(CXX) $(filter-out -D__RVV10__,$(CXXFLAGS)) -DPCLAPI_EXPORTS $(PHASE070_BENCH_SRCS) $(LDFLAGS) $(EXTRA_LDFLAGS) $(PHASE070_LIBS_BENCH) -o $@

$(PHASE070_BENCH_RVV_BINARY): $(PHASE070_BENCH_SRCS) | $(LOG_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Phase 070 production-direct Benchmark RVV ($(ARCH))..."
	$(CXX) $(CXXFLAGS) -DPCLAPI_EXPORTS $(PHASE070_BENCH_SRCS) $(LDFLAGS) $(EXTRA_LDFLAGS) $(PHASE070_LIBS_BENCH) -o $@

run_production_direct_test_std: $(PHASE070_TEST_STD_BINARY) | $(OUTPUT_DIR_QEMU)
	$(RUN_CMD) ./$(PHASE070_TEST_STD_BINARY) $(TEST_ARGS) 2>&1 | tee $(OUTPUT_DIR_QEMU)/phase070-production-direct-test-std.log

run_production_direct_test_rvv: $(PHASE070_TEST_RVV_BINARY) | $(OUTPUT_DIR_QEMU)
	$(RUN_CMD) ./$(PHASE070_TEST_RVV_BINARY) $(TEST_ARGS) 2>&1 | tee $(OUTPUT_DIR_QEMU)/phase070-production-direct-test-rvv.log

run_production_direct_test_compare: run_production_direct_test_std run_production_direct_test_rvv

dump_production_direct_bench_rvv: $(PHASE070_BENCH_RVV_BINARY) | $(ASM_DIR)/$(ARCH)
	$(OBJDUMP) -d -C $(PHASE070_BENCH_RVV_BINARY) > $(PHASE070_ASM)
	grep -E $(VEC_REGEX_STR) $(PHASE070_ASM) > $(ASM_DIR)/$(ARCH)/$(PHASE070_TARGET_BENCH_RVV).asm
	grep -E 'linearizeEnergyMapRVV' $(PHASE070_ASM)
	grep -E '\bvlse8\.v\b' $(PHASE070_ASM)
	grep -E '\bvse8\.v\b' $(PHASE070_ASM)
	@echo "[DONE] Phase 070 production-direct RVV asm dump: $(PHASE070_ASM)"

collect_production_direct_repeated_board: dump_production_direct_bench_rvv
	rm -rf $(PHASE070_REPEATED_BOARD_DIR)
	@for run in $(PHASE070_REPEATED_RUNS); do \
		echo "[BOARD] Phase 070 production-direct repeated run $$run"; \
		$(MAKE) -C $(CURDIR) deploy_board \
			TARGET_BENCH_STD="$(PHASE070_TARGET_BENCH_STD)" \
			TARGET_BENCH_RVV="$(PHASE070_TARGET_BENCH_RVV)" \
			SRCS_BENCH="$(PHASE070_BENCH_SRCS)" \
			LIBS_BENCH="$(PHASE070_LIBS_BENCH)" \
			TARGET_TEST="$(PHASE070_TARGET_TEST_RVV)" \
			BOARD_TARGET_TEST="$(PHASE070_TARGET_TEST_RVV)" \
			SRCS_TEST="$(PHASE070_TEST_SRCS)" \
			LIBS_TEST="$(PHASE070_LIBS_TEST)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(PHASE070_REPEATED_REMOTE_DIR)/run-$$run' REMOTE_TEST='$(PHASE070_TARGET_TEST_RVV)' REMOTE_TEST_ARGS='--gtest_filter=LINEMODTemplateScoringProductionDirect.*'"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$(PHASE070_REPEATED_REMOTE_DIR)/run-$$run' REMOTE_BENCH_STD='$(PHASE070_TARGET_BENCH_STD)' REMOTE_BENCH_RVV='$(PHASE070_TARGET_BENCH_RVV)' BENCH_COMPARE_SAVE='$(PHASE070_REPEATED_REMOTE_DIR)/run-$$run/analyze_bench_compare.log' BOARD_LABEL='Milkv-Jupiter Phase 070 production-direct' REMOTE_BENCH_ARGS='$(REMOTE_BENCH_ARGS)'"; \
		mkdir -p "$(PHASE070_REPEATED_BOARD_DIR)/run-$$run"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$(PHASE070_REPEATED_REMOTE_DIR)/run-$$run/ $(PHASE070_REPEATED_BOARD_DIR)/run-$$run/; \
	done

generate_production_direct_evidence_manifest: dump_production_direct_bench_rvv
	$(PYTHON_RUN) script/generate_linemod_template_scoring_evidence_manifest.py \
		--repeated-dir "$(PHASE070_REPEATED_BOARD_DIR)" \
		--asm "$(PHASE070_ASM)" \
		--rvv-binary "$(PHASE070_BENCH_RVV_BINARY)" \
		--summary "$(PHASE070_SUMMARY)" \
		--output "$(PHASE070_EVIDENCE_MANIFEST)" \
		--evidence-role "production-public" \
		--include-label "LINEMOD production matchTemplates total" \
		--include-label "LINEMOD production detectTemplates total"

run_production_direct_evidence_doctor: generate_production_direct_evidence_manifest
	$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest "$(PHASE070_EVIDENCE_MANIFEST)" \
		--output "$(PHASE070_EVIDENCE_DOCTOR_MD)" \
		--json-output "$(PHASE070_EVIDENCE_DOCTOR_JSON)" \
		--fail-on never

record_production_direct_evidence_state: run_production_direct_evidence_doctor
	$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(PHASE070_SUMMARY)" \
		--manifest "$(PHASE070_EVIDENCE_MANIFEST)" \
		--doctor "$(PHASE070_EVIDENCE_DOCTOR_MD)" \
		--path "$(PHASE070_EVIDENCE_DOCTOR_JSON)" \
		--target "$@" \
		--backend board \
		--run-label "phase070-production-direct-repeated-20260828" \
		--case-filter "mem_size=4096,nr_maps=96,iterations=$(BENCH_ITERATIONS),production_direct=1" \
		--evidence-role "production-public" \
		$(EVIDENCE_DOC_REF_ARGS)

check_production_direct_evidence_freshness:
	$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob "$(TOPIC_DOC_PREFIX)/doc/phases/070-production-integration-loop/production-direct-repeated-*.md" \
		--scan-glob "$(TOPIC_DOC_PREFIX)/doc/phases/070-production-integration-loop/production-direct-repeated-*.json" \
		$(EVIDENCE_DOC_ARGS) \
		--require-doc-ref \
		--fail-on never

$(PHASE080_TEST_STD_BINARY): $(PHASE080_TEST_SRCS) | $(LOG_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Phase 080 semi-scale production-direct Unit Test Std ($(ARCH))..."
	$(CXX) $(filter-out -D__RVV10__,$(CXXFLAGS)) -DPCLAPI_EXPORTS $(PHASE080_TEST_SRCS) $(LDFLAGS) $(EXTRA_LDFLAGS) $(PHASE080_LIBS_TEST) -o $@

$(PHASE080_TEST_RVV_BINARY): $(PHASE080_TEST_SRCS) | $(LOG_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Phase 080 semi-scale production-direct Unit Test RVV ($(ARCH))..."
	$(CXX) $(CXXFLAGS) -DPCLAPI_EXPORTS $(PHASE080_TEST_SRCS) $(LDFLAGS) $(EXTRA_LDFLAGS) $(PHASE080_LIBS_TEST) -o $@

$(PHASE080_BENCH_STD_BINARY): $(PHASE080_BENCH_SRCS) | $(LOG_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Phase 080 semi-scale production-direct Benchmark Std ($(ARCH))..."
	$(CXX) $(filter-out -D__RVV10__,$(CXXFLAGS)) -DPCLAPI_EXPORTS $(PHASE080_BENCH_SRCS) $(LDFLAGS) $(EXTRA_LDFLAGS) $(PHASE080_LIBS_BENCH) -o $@

$(PHASE080_BENCH_RVV_BINARY): $(PHASE080_BENCH_SRCS) | $(LOG_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Phase 080 semi-scale production-direct Benchmark RVV ($(ARCH))..."
	$(CXX) $(CXXFLAGS) -DPCLAPI_EXPORTS $(PHASE080_BENCH_SRCS) $(LDFLAGS) $(EXTRA_LDFLAGS) $(PHASE080_LIBS_BENCH) -o $@

run_semiscale_production_direct_test_std: $(PHASE080_TEST_STD_BINARY) | $(OUTPUT_DIR_QEMU)
	$(RUN_CMD) ./$(PHASE080_TEST_STD_BINARY) $(TEST_ARGS) 2>&1 | tee $(OUTPUT_DIR_QEMU)/phase080-semi-scale-production-direct-test-std.log

run_semiscale_production_direct_test_rvv: $(PHASE080_TEST_RVV_BINARY) | $(OUTPUT_DIR_QEMU)
	$(RUN_CMD) ./$(PHASE080_TEST_RVV_BINARY) $(TEST_ARGS) 2>&1 | tee $(OUTPUT_DIR_QEMU)/phase080-semi-scale-production-direct-test-rvv.log

run_semiscale_production_direct_test_compare: run_semiscale_production_direct_test_std run_semiscale_production_direct_test_rvv

dump_semiscale_production_direct_bench_rvv: $(PHASE080_BENCH_RVV_BINARY) | $(ASM_DIR)/$(ARCH)
	$(OBJDUMP) -d -C $(PHASE080_BENCH_RVV_BINARY) > $(PHASE080_ASM)
	grep -E $(VEC_REGEX_STR) $(PHASE080_ASM) > $(ASM_DIR)/$(ARCH)/$(PHASE080_TARGET_BENCH_RVV).asm
	grep -E 'linearizeEnergyMapRVV' $(PHASE080_ASM)
	@test "$$(grep -Ec 'jal[[:space:]].*linearizeEnergyMapRVV' $(PHASE080_ASM))" -ge 3
	grep -E '\bvlse8\.v\b' $(PHASE080_ASM)
	grep -E '\bvse8\.v\b' $(PHASE080_ASM)
	@echo "[DONE] Phase 080 semi-scale production-direct RVV asm dump: $(PHASE080_ASM)"

collect_semiscale_production_direct_repeated_board: dump_semiscale_production_direct_bench_rvv
	rm -rf $(PHASE080_REPEATED_BOARD_DIR)
	@for run in $(PHASE080_REPEATED_RUNS); do \
		echo "[BOARD] Phase 080 semi-scale production-direct repeated run $$run"; \
		$(MAKE) -C $(CURDIR) deploy_board \
			TARGET_BENCH_STD="$(PHASE080_TARGET_BENCH_STD)" \
			TARGET_BENCH_RVV="$(PHASE080_TARGET_BENCH_RVV)" \
			SRCS_BENCH="$(PHASE080_BENCH_SRCS)" \
			LIBS_BENCH="$(PHASE080_LIBS_BENCH)" \
			TARGET_TEST="$(PHASE080_TARGET_TEST_RVV)" \
			BOARD_TARGET_TEST="$(PHASE080_TARGET_TEST_RVV)" \
			SRCS_TEST="$(PHASE080_TEST_SRCS)" \
			LIBS_TEST="$(PHASE080_LIBS_TEST)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(PHASE080_REPEATED_REMOTE_DIR)/run-$$run' REMOTE_TEST='$(PHASE080_TARGET_TEST_RVV)' REMOTE_TEST_ARGS='--gtest_filter=LINEMODTemplateScoringProductionDirect.SemiScale*'"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$(PHASE080_REPEATED_REMOTE_DIR)/run-$$run' REMOTE_BENCH_STD='$(PHASE080_TARGET_BENCH_STD)' REMOTE_BENCH_RVV='$(PHASE080_TARGET_BENCH_RVV)' BENCH_COMPARE_SAVE='$(PHASE080_REPEATED_REMOTE_DIR)/run-$$run/analyze_bench_compare.log' BOARD_LABEL='Milkv-Jupiter Phase 080 semi-scale production-direct' REMOTE_BENCH_ARGS='$(REMOTE_BENCH_ARGS)'"; \
		mkdir -p "$(PHASE080_REPEATED_BOARD_DIR)/run-$$run"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$(PHASE080_REPEATED_REMOTE_DIR)/run-$$run/ $(PHASE080_REPEATED_BOARD_DIR)/run-$$run/; \
	done

generate_semiscale_production_direct_evidence_manifest: dump_semiscale_production_direct_bench_rvv
	$(PYTHON_RUN) script/generate_linemod_template_scoring_evidence_manifest.py \
		--repeated-dir "$(PHASE080_REPEATED_BOARD_DIR)" \
		--asm "$(PHASE080_ASM)" \
		--rvv-binary "$(PHASE080_BENCH_RVV_BINARY)" \
		--summary "$(PHASE080_SUMMARY)" \
		--output "$(PHASE080_EVIDENCE_MANIFEST)" \
		--evidence-role "production-public" \
		--include-label "LINEMOD production semi-scale detectTemplates total"

run_semiscale_production_direct_evidence_doctor: generate_semiscale_production_direct_evidence_manifest
	$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest "$(PHASE080_EVIDENCE_MANIFEST)" \
		--output "$(PHASE080_EVIDENCE_DOCTOR_MD)" \
		--json-output "$(PHASE080_EVIDENCE_DOCTOR_JSON)" \
		--fail-on never

record_semiscale_production_direct_evidence_state: run_semiscale_production_direct_evidence_doctor
	$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(PHASE080_SUMMARY)" \
		--manifest "$(PHASE080_EVIDENCE_MANIFEST)" \
		--doctor "$(PHASE080_EVIDENCE_DOCTOR_MD)" \
		--path "$(PHASE080_EVIDENCE_DOCTOR_JSON)" \
		--target "$@" \
		--backend board \
		--run-label "phase080-semi-scale-production-direct-repeated-20260828" \
		--case-filter "mem_size=4096,nr_maps=96,iterations=$(BENCH_ITERATIONS),production_direct=1,semi_scale=1" \
		--evidence-role "production-public" \
		$(EVIDENCE_DOC_REF_ARGS)

check_semiscale_production_direct_evidence_freshness:
	$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob "$(TOPIC_DOC_PREFIX)/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-*.md" \
		--scan-glob "$(TOPIC_DOC_PREFIX)/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-*.json" \
		$(EVIDENCE_DOC_ARGS) \
		--require-doc-ref \
		--fail-on never
