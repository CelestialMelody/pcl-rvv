# =============================================================================
# PCL recognition/surface_normal_modality RVV diagnostic Makefile
#
# 本 topic 评估 SurfaceNormalModality 的 depth-to-normal / quantize、5x5
# filter 和 spread RVV production direct（真实生产路径）证据。QEMU（仿真器）
# 只用于 correctness（正确性）、构建和日志形状；性能结论必须来自 board
#（板卡）或目标硬件 repeated benchmark（重复性能测试）。
# =============================================================================

TOPIC  := surface_normal_modality
MODULE := recognition

FOCUS_DIR      := pcl/recognition/include/pcl/recognition/surface_normal_modality.h
FILTER_REPORT  := log/filtered_snm.log
ANALYZE_REPORT := log/analyze_snm.log

TARGET_TEST     := test_snm
TARGET_TEST_STD := test_snm_std
TARGET_TEST_RVV := test_snm_rvv
SRCS_TEST       := src/test_snm.cpp

TARGET_BENCH     := bench_snm
TARGET_BENCH_STD := bench_snm_std
TARGET_BENCH_RVV := bench_snm_rvv
SRCS_BENCH       := src/bench_snm.cpp

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

LIBS_TEST  := -lpcl_recognition -lpcl_common -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lpcl_recognition -lpcl_common -lm

REMOTE_DIR ?= /root/pcl-test/recognition/surface_normal_modality

REPEATED_BOARD_RUNS ?= 5
REPEATED_BOARD_TAG ?= phase030_spread_rvv
REPEATED_BOARD_TITLE ?= SNM spread RVV production direct repeated board summary
REPEATED_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated_$(REPEATED_BOARD_TAG)
REPEATED_BOARD_REMOTE_TAG ?= phase030_spread_rvv
REPEATED_BOARD_RUN_LABEL ?= snm_phase030_spread_rvv_repeated
REPEATED_BOARD_CASE_NAME ?=
EVIDENCE_MANIFEST_REPEATED ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_manifest.json
EVIDENCE_DOCTOR_REPEATED_MD ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.md
EVIDENCE_DOCTOR_REPEATED_JSON ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.json
EVIDENCE_REGISTRY ?= $(LOG_DIR)/evidence_registry.json
EVIDENCE_DOC_REF_PRIMARY ?= doc/phases/030-spread-quantized-map-rvv/result.zh.md
EVIDENCE_DOC_REF_SECONDARY ?= doc/surface_normal_modality-evaluation.zh.md
EVIDENCE_ROLE_REPEATED ?= production_direct
SNM_REPEATED_BENCH_ARGS ?= --case-filter production_process_320x240,production_process_641x481_tail --iterations 20 --warmup-iterations 3

include ../../mk/rvv-topic.mk

run_qemu_smoke: run_test_compare

check_snm_rvv_asm: dump_bench_rvv
	@grep -Eq 'vle|vlse|vse|vfcvt|vfwcvt|vfsqrt|vfdiv|vfmul|vfadd|vfsub' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@echo "[PASS] surface_normal_modality RVV asm gate matched expected depth/normal candidate instructions."

check_snm_production_rvv_asm: dump_bench_rvv
	@grep -Eq 'computeAndQuantizeSurfaceNormals2RVV|SurfaceNormalModality.*processInputData|production_process_' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq 'filterQuantizedSurfaceNormals' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq 'spreadFilteredQuantizedSurfaceNormals|spreadQuantizedMap' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq 'vle16|vse32|vsub|vmul|vadd|vmslt|vmerge' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vle8|vse8|vmseq|vmsgtu' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vor|vse8' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@echo "[PASS] surface_normal_modality production RVV asm gate matched expected production path instructions."

board_repeated:
	@set -eu; \
	for i in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
			BENCH_ARGS="$(SNM_REPEATED_BENCH_ARGS)" \
			OUTPUT_DIR_BOARD="$(REPEATED_BOARD_OUTPUT_DIR)/$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated_$(REPEATED_BOARD_REMOTE_TAG)/$$run"; \
	done

evidence_manifest_repeated:
	python3 script/generate_snm_evidence_manifest.py \
		--summary-dir "$(REPEATED_BOARD_OUTPUT_DIR)" \
		--output "$(EVIDENCE_MANIFEST_REPEATED)" \
		--summary-output "$(REPEATED_BOARD_OUTPUT_DIR)/summary.md" \
		--title "$(REPEATED_BOARD_TITLE)" \
		--run-label "$(REPEATED_BOARD_RUN_LABEL)" \
		$(if $(REPEATED_BOARD_CASE_NAME),--case-name "$(REPEATED_BOARD_CASE_NAME)",) \
		--expected-runs "$(REPEATED_BOARD_RUNS)" \
		--evidence-role "$(EVIDENCE_ROLE_REPEATED)" \
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
		--case-filter "$(SNM_REPEATED_BENCH_ARGS)" \
		--evidence-role "$(EVIDENCE_ROLE_REPEATED)" \
		--doc-ref "$(EVIDENCE_DOC_REF_PRIMARY)" \
		--doc-ref "$(EVIDENCE_DOC_REF_SECONDARY)" \
		--freshness-state fresh

check_evidence_freshness:
	python3 ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob 'test-rvv/recognition/surface_normal_modality/log/board/repeated_phase000_depth_quantize/summary.md' \
		--scan-glob 'test-rvv/recognition/surface_normal_modality/log/board/repeated_phase000_depth_quantize/evidence_manifest.json' \
		--scan-glob 'test-rvv/recognition/surface_normal_modality/log/board/repeated_phase000_depth_quantize/evidence_doctor.md' \
		--scan-glob 'test-rvv/recognition/surface_normal_modality/log/board/repeated_phase000_depth_quantize/evidence_doctor.json' \
		--scan-glob 'test-rvv/recognition/surface_normal_modality/log/board/repeated_phase010_production_direct/summary.md' \
		--scan-glob 'test-rvv/recognition/surface_normal_modality/log/board/repeated_phase010_production_direct/evidence_manifest.json' \
		--scan-glob 'test-rvv/recognition/surface_normal_modality/log/board/repeated_phase010_production_direct/evidence_doctor.md' \
		--scan-glob 'test-rvv/recognition/surface_normal_modality/log/board/repeated_phase010_production_direct/evidence_doctor.json' \
		--scan-glob 'test-rvv/recognition/surface_normal_modality/log/board/repeated_phase020_filter_5x5/summary.md' \
		--scan-glob 'test-rvv/recognition/surface_normal_modality/log/board/repeated_phase020_filter_5x5/evidence_manifest.json' \
		--scan-glob 'test-rvv/recognition/surface_normal_modality/log/board/repeated_phase020_filter_5x5/evidence_doctor.md' \
		--scan-glob 'test-rvv/recognition/surface_normal_modality/log/board/repeated_phase020_filter_5x5/evidence_doctor.json' \
		--scan-glob 'test-rvv/recognition/surface_normal_modality/log/board/repeated_phase030_spread_rvv/summary.md' \
		--scan-glob 'test-rvv/recognition/surface_normal_modality/log/board/repeated_phase030_spread_rvv/evidence_manifest.json' \
		--scan-glob 'test-rvv/recognition/surface_normal_modality/log/board/repeated_phase030_spread_rvv/evidence_doctor.md' \
		--scan-glob 'test-rvv/recognition/surface_normal_modality/log/board/repeated_phase030_spread_rvv/evidence_doctor.json' \
		--doc test-rvv/recognition/surface_normal_modality/doc/phases/000-current-state-and-snm-scaffold/result.zh.md \
		--doc test-rvv/recognition/surface_normal_modality/doc/phases/010-production-integration/result.zh.md \
		--doc test-rvv/recognition/surface_normal_modality/doc/phases/020-filter-5x5-rvv/result.zh.md \
		--doc test-rvv/recognition/surface_normal_modality/doc/phases/030-spread-quantized-map-rvv/result.zh.md \
		--doc test-rvv/recognition/surface_normal_modality/doc/surface_normal_modality-evaluation.zh.md \
		--require-doc-ref \
		--fail-on never

.PHONY: run_qemu_smoke check_snm_rvv_asm check_snm_production_rvv_asm board_repeated evidence_manifest_repeated evidence_doctor_repeated record_evidence_state_repeated check_evidence_freshness
