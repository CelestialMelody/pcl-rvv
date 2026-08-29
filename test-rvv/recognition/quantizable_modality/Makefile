# =============================================================================
# PCL recognition/quantizable_modality RVV topic Makefile
#
# 本 topic 评估公共 QuantizedMap::spreadQuantizedMap byte-map spread helper。
# QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状；性能结论
# 必须来自 board（板卡）或目标硬件 repeated benchmark（重复性能测试）。
# =============================================================================

TOPIC  := quantizable_modality
MODULE := recognition

FOCUS_DIR      := pcl/recognition/src/quantizable_modality.cpp
FILTER_REPORT  := log/filtered_qm.log
ANALYZE_REPORT := log/analyze_qm.log

TARGET_TEST     := test_qm
TARGET_TEST_STD := test_qm_std
TARGET_TEST_RVV := test_qm_rvv
SRCS_TEST       = src/test_qm.cpp $(PCL_SOURCE_ROOT)/recognition/src/quantizable_modality.cpp

TARGET_BENCH     := bench_qm
TARGET_BENCH_STD := bench_qm_std
TARGET_BENCH_RVV := bench_qm_rvv
SRCS_BENCH       = src/bench_qm.cpp $(PCL_SOURCE_ROOT)/recognition/src/quantizable_modality.cpp

EXTRA_CXXFLAGS ?= -DPCL_RVV_QM_TEST_HOOK

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

LIBS_TEST  := -lpcl_common -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lpcl_common -lm

REMOTE_DIR ?= /root/pcl-test/recognition/quantizable_modality

REPEATED_BOARD_RUNS ?= 5
REPEATED_BOARD_TAG ?= phase000_shared_spread_rvv
REPEATED_BOARD_TITLE ?= QuantizableModality shared spread RVV production-detail repeated board summary
REPEATED_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated_$(REPEATED_BOARD_TAG)
REPEATED_BOARD_REMOTE_TAG ?= phase000_shared_spread_rvv
REPEATED_BOARD_RUN_LABEL ?= qm_phase000_shared_spread_rvv_repeated
REPEATED_BOARD_CASE_NAME ?=
EVIDENCE_MANIFEST_REPEATED ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_manifest.json
EVIDENCE_DOCTOR_REPEATED_MD ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.md
EVIDENCE_DOCTOR_REPEATED_JSON ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.json
EVIDENCE_REGISTRY ?= $(LOG_DIR)/evidence_registry.json
EVIDENCE_DOC_REF_PRIMARY ?= doc/phases/000-shared-spread-rvv/result.zh.md
EVIDENCE_DOC_REF_SECONDARY ?= doc/quantizable_modality-evaluation.zh.md
EVIDENCE_ROLE_REPEATED ?= production_detail
QM_REPEATED_BENCH_ARGS ?= --case-filter shared_spread_320x240,shared_spread_641x481_tail --iterations 50 --warmup-iterations 5

include ../../mk/rvv-topic.mk

run_qemu_smoke: run_test_compare

check_qm_rvv_asm: dump_bench_rvv
	@grep -Eq 'QuantizedMap::spreadQuantizedMap|spreadQuantizedMapRVV|shared_spread_' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq 'vle8' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vor' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vse8' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@echo "[PASS] quantizable_modality shared spread RVV asm gate matched byte load/or/store instructions."

board_repeated:
	@set -eu; \
	for i in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
			BENCH_ARGS="$(QM_REPEATED_BENCH_ARGS)" \
			OUTPUT_DIR_BOARD="$(REPEATED_BOARD_OUTPUT_DIR)/$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated_$(REPEATED_BOARD_REMOTE_TAG)/$$run"; \
	done

evidence_manifest_repeated:
	python3 script/generate_qm_evidence_manifest.py \
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
		--case-filter "$(QM_REPEATED_BENCH_ARGS)" \
		--evidence-role "$(EVIDENCE_ROLE_REPEATED)" \
		--doc-ref "$(EVIDENCE_DOC_REF_PRIMARY)" \
		--doc-ref "$(EVIDENCE_DOC_REF_SECONDARY)" \
		--freshness-state fresh

check_evidence_freshness:
	python3 ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob 'test-rvv/recognition/quantizable_modality/log/board/repeated_phase000_shared_spread_rvv/summary.md' \
		--scan-glob 'test-rvv/recognition/quantizable_modality/log/board/repeated_phase000_shared_spread_rvv/evidence_manifest.json' \
		--scan-glob 'test-rvv/recognition/quantizable_modality/log/board/repeated_phase000_shared_spread_rvv/evidence_doctor.md' \
		--scan-glob 'test-rvv/recognition/quantizable_modality/log/board/repeated_phase000_shared_spread_rvv/evidence_doctor.json' \
		--doc test-rvv/recognition/quantizable_modality/doc/phases/000-shared-spread-rvv/result.zh.md \
		--doc test-rvv/recognition/quantizable_modality/doc/quantizable_modality-evaluation.zh.md \
		--doc doc-rvv/recognition/quantizable_modality-RVV.zh.md \
		--require-doc-ref \
		--fail-on never

.PHONY: run_qemu_smoke check_qm_rvv_asm board_repeated evidence_manifest_repeated evidence_doctor_repeated record_evidence_state_repeated check_evidence_freshness
