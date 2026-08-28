# =============================================================================
# PCL recognition/color_gradient_modality RVV diagnostic Makefile
#
# 本 topic 评估 ColorGradientModality 的 Sobel+quantize、3x3 dominant filter
# 和后续 full-chain 子链路 production-shaped diagnostic（生产形态诊断）。
# QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状；性能结论必须
# 来自 board（板卡）或目标硬件 repeated benchmark（重复性能测试）。
# =============================================================================

TOPIC  := color_gradient_modality
MODULE := recognition

FOCUS_DIR      := pcl/recognition/include/pcl/recognition/color_gradient_modality.h
FILTER_REPORT  := log/filtered_cgm.log
ANALYZE_REPORT := log/analyze_cgm.log

TARGET_TEST     := test_cgm
TARGET_TEST_STD := test_cgm_std
TARGET_TEST_RVV := test_cgm_rvv
SRCS_TEST       := src/test_cgm.cpp

TARGET_BENCH     := bench_cgm
TARGET_BENCH_STD := bench_cgm_std
TARGET_BENCH_RVV := bench_cgm_rvv
SRCS_BENCH       := src/bench_cgm.cpp

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/recognition/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/filters/include \
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

LIBS_TEST  := -lpcl_recognition -lpcl_filters -lpcl_common -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lpcl_recognition -lpcl_filters -lpcl_common -lm

REMOTE_DIR ?= /root/pcl-test/recognition/color_gradient_modality

REPEATED_BOARD_RUNS ?= 5
REPEATED_BOARD_TAG ?= phase000_sobel_quantize
REPEATED_BOARD_TITLE ?= CGM Sobel+quantize production-shaped diagnostic repeated board summary
REPEATED_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated_$(REPEATED_BOARD_TAG)
REPEATED_BOARD_REMOTE_TAG ?= phase000_sobel_quantize
REPEATED_BOARD_RUN_LABEL ?= cgm_phase000_sobel_quantize_repeated
REPEATED_BOARD_CASE_NAME ?=
EVIDENCE_MANIFEST_REPEATED ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_manifest.json
EVIDENCE_DOCTOR_REPEATED_MD ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.md
EVIDENCE_DOCTOR_REPEATED_JSON ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.json
EVIDENCE_REGISTRY ?= $(LOG_DIR)/evidence_registry.json
EVIDENCE_DOC_REF_PRIMARY ?= doc/phases/000-current-state-and-cgm-scaffold/result.zh.md
EVIDENCE_DOC_REF_SECONDARY ?= doc/color_gradient_modality-evaluation.zh.md
EVIDENCE_ROLE_REPEATED ?= production_shaped_diagnostic
CGM_REPEATED_BENCH_ARGS ?= --case-filter all --iterations 20 --warmup-iterations 3

include ../../mk/rvv-topic.mk

run_qemu_smoke: run_test_compare

check_cgm_rvv_asm: dump_bench_rvv
	@grep -Eq 'vle8|vlse8' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vfcvt|vfwcvt' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'atan2_RVV|vfmacc|vfdiv' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@echo "[PASS] color_gradient_modality RVV asm gate matched expected Sobel+atan2 candidate instructions."

check_cgm_filter_rvv_asm: dump_bench_rvv
	@grep -Eq 'vle8' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vse8' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vmseq|vmsgtu|vmsltu|vmerge' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@echo "[PASS] color_gradient_modality filter RVV asm gate matched expected byte compare/merge/store instructions."

check_cgm_full_chain_rvv_asm: dump_bench_rvv
	@grep -Eq 'vle8|vlse8' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vfcvt|vfwcvt|vfsqrt' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vmseq|vmsgtu|vmsltu|vmerge' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'atan2_RVV|vfdiv' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@echo "[PASS] color_gradient_modality full-chain RVV asm gate matched expected math and byte-filter instructions."

check_cgm_production_rvv_asm: dump_bench_rvv
	@grep -Eq 'ColorGradientModality<.*PointXYZRGB.*computeColorGradientPipelineRVV|production_process_320x240' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq 'vlse8' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vzext|vwaddu|vwmul|vmul|vsub|vadd' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vfsqrt' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vfcvt|vfwcvt' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vmseq|vmsgtu|vmsltu|vmerge' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'atan2_RVV|vfdiv' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@echo "[PASS] color_gradient_modality production RGB stencil RVV asm gate matched expected processInputData helper instructions."

check_cgm_stencil_rvv_asm: dump_bench_rvv
	@grep -Eq 'full_chain_stencil_320x240|computeSobelQuantizedStencilCandidate' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq 'vlse8' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vzext|vwaddu|vwmul|vmul|vsub|vadd' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vfsqrt' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'atan2_RVV|vfdiv' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq 'vmseq|vmsgtu|vmsltu|vmerge' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@echo "[PASS] color_gradient_modality RGB stencil RVV asm gate matched expected stride-load/math/filter instructions."

board_repeated:
	@set -eu; \
	for i in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
			BENCH_ARGS="$(CGM_REPEATED_BENCH_ARGS)" \
			OUTPUT_DIR_BOARD="$(REPEATED_BOARD_OUTPUT_DIR)/$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated_$(REPEATED_BOARD_REMOTE_TAG)/$$run"; \
	done

evidence_manifest_repeated:
	python3 script/generate_cgm_evidence_manifest.py \
		--summary-dir "$(REPEATED_BOARD_OUTPUT_DIR)" \
		--output "$(EVIDENCE_MANIFEST_REPEATED)" \
		--summary-output "$(REPEATED_BOARD_OUTPUT_DIR)/summary.md" \
		--title "$(REPEATED_BOARD_TITLE)" \
		--run-label "$(REPEATED_BOARD_RUN_LABEL)" \
		$(if $(REPEATED_BOARD_CASE_NAME),--case-name "$(REPEATED_BOARD_CASE_NAME)",) \
		--expected-runs "$(REPEATED_BOARD_RUNS)" \
		--asm "$(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm"

evidence_doctor_repeated: evidence_manifest_repeated
	python3 ../../script/evidence_doctor.py \
		--manifest "$(EVIDENCE_MANIFEST_REPEATED)" \
		--output "$(EVIDENCE_DOCTOR_REPEATED_MD)" \
		--json-output "$(EVIDENCE_DOCTOR_REPEATED_JSON)" \
		--fail-on never

record_evidence_state_repeated: evidence_doctor_repeated
	python3 ../../script/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(REPEATED_BOARD_OUTPUT_DIR)/summary.md" \
		--manifest "$(EVIDENCE_MANIFEST_REPEATED)" \
		--doctor "$(EVIDENCE_DOCTOR_REPEATED_MD)" \
		--path "$(EVIDENCE_DOCTOR_REPEATED_JSON)" \
		--target board_repeated \
		--backend board \
		--run-label "$(REPEATED_BOARD_RUN_LABEL)" \
		--case-filter "$(CGM_REPEATED_BENCH_ARGS)" \
		--evidence-role "$(EVIDENCE_ROLE_REPEATED)" \
		--doc-ref "$(EVIDENCE_DOC_REF_PRIMARY)" \
		--doc-ref "$(EVIDENCE_DOC_REF_SECONDARY)" \
		--freshness-state fresh

check_evidence_freshness:
	python3 ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase000_sobel_quantize/summary.md' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase000_sobel_quantize/evidence_manifest.json' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase000_sobel_quantize/evidence_doctor.md' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase000_sobel_quantize/evidence_doctor.json' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase010_dominant_filter/summary.md' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase010_dominant_filter/evidence_manifest.json' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase010_dominant_filter/evidence_doctor.md' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase010_dominant_filter/evidence_doctor.json' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase020_full_chain/summary.md' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase020_full_chain/evidence_manifest.json' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase020_full_chain/evidence_doctor.md' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase020_full_chain/evidence_doctor.json' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase030_production_direct/summary.md' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase030_production_direct/evidence_manifest.json' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase030_production_direct/evidence_doctor.md' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase030_production_direct/evidence_doctor.json' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase040_rgb_stencil/summary.md' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase040_rgb_stencil/evidence_manifest.json' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase040_rgb_stencil/evidence_doctor.md' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase040_rgb_stencil/evidence_doctor.json' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase050_rgb_stencil_production_direct/summary.md' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase050_rgb_stencil_production_direct/evidence_manifest.json' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase050_rgb_stencil_production_direct/evidence_doctor.md' \
		--scan-glob 'test-rvv/recognition/color_gradient_modality/log/board/repeated_phase050_rgb_stencil_production_direct/evidence_doctor.json' \
		--doc test-rvv/recognition/color_gradient_modality/doc/phases/000-current-state-and-cgm-scaffold/result.zh.md \
		--doc test-rvv/recognition/color_gradient_modality/doc/phases/010-dominant-filter-rvv/result.zh.md \
		--doc test-rvv/recognition/color_gradient_modality/doc/phases/020-full-chain-production-shaped-diagnostic/result.zh.md \
		--doc test-rvv/recognition/color_gradient_modality/doc/phases/030-production-integration-plan/result.zh.md \
		--doc test-rvv/recognition/color_gradient_modality/doc/phases/040-rgb-stencil-rvv-ablation/result.zh.md \
		--doc test-rvv/recognition/color_gradient_modality/doc/phases/050-rgb-stencil-production-integration/result.zh.md \
		--doc test-rvv/recognition/color_gradient_modality/doc/color_gradient_modality-evaluation.zh.md \
		--doc doc-rvv/recognition/color_gradient_modality-RVV.zh.md \
		--require-doc-ref \
		--fail-on never

.PHONY: run_qemu_smoke check_cgm_rvv_asm check_cgm_filter_rvv_asm check_cgm_full_chain_rvv_asm check_cgm_production_rvv_asm check_cgm_stencil_rvv_asm board_repeated evidence_manifest_repeated evidence_doctor_repeated record_evidence_state_repeated check_evidence_freshness
