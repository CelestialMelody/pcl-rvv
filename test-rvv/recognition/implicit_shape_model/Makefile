# =============================================================================
# PCL recognition/implicit_shape_model RVV topic Makefile
#
# 本 topic 先做 ISM 局部公式 diagnostic（诊断）和 profile prerequisite
# （热点前置验证）。QEMU（仿真器）只用于 correctness（正确性）、构建和
# 日志形状；性能结论必须来自 board（板卡）或目标硬件 repeated benchmark。
# =============================================================================

TOPIC  := implicit_shape_model
MODULE := recognition

FOCUS_DIR      := pcl/recognition/include/pcl/recognition/impl/implicit_shape_model.hpp
FILTER_REPORT  := log/filtered_ism.log
ANALYZE_REPORT := log/analyze_ism.log

TARGET_TEST     := test_ism
TARGET_TEST_STD := test_ism_std
TARGET_TEST_RVV := test_ism_rvv
SRCS_TEST       := src/test_ism.cpp

TARGET_UPSTREAM_TEST     := test_recognition_ism_upstream
TARGET_UPSTREAM_TEST_STD := test_recognition_ism_upstream_std
TARGET_UPSTREAM_TEST_RVV := test_recognition_ism_upstream_rvv
SRCS_UPSTREAM_TEST       = $(PCL_SOURCE_ROOT)/test/recognition/test_recognition_ism.cpp
UPSTREAM_TEST_ARGS       = $(PCL_SOURCE_ROOT)/test/ism_train.pcd $(PCL_SOURCE_ROOT)/test/ism_test.pcd
LIBS_UPSTREAM_TEST := \
	-lpcl_recognition \
	-lpcl_features \
	-lpcl_io \
	-lpcl_search \
	-lpcl_kdtree \
	-lpcl_common \
	-lflann_cpp \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lgtest -lgtest_main -lpthread \
	-lz -lhdf5 -llz4 -lpng -lm

TARGET_BENCH     := bench_ism
TARGET_BENCH_STD := bench_ism_std
TARGET_BENCH_RVV := bench_ism_rvv
SRCS_BENCH       := src/bench_ism.cpp
ISM_REPEATED_BENCH_ARGS ?= --case-filter all --clusters 184 --points 768 --votes 8192 --iterations 100 --warmup-iterations 5
ISM_PUBLIC_ENTRY_BENCH_ARGS ?= --case-filter public_find_objects_descriptor_assignment --clusters 184 --descriptors 512 --iterations 80 --warmup-iterations 5
BENCH_ARGS ?= $(ISM_REPEATED_BENCH_ARGS)

REMOTE_DIR ?= /root/pcl-test/recognition/implicit_shape_model

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
LIBS_BENCH := -lpcl_common -llz4 -lm

REPEATED_BOARD_RUNS ?= 5
REPEATED_BOARD_TAG ?= phase000_ism_local_formula_diagnostic
REPEATED_BOARD_TITLE ?= ISM local formula diagnostic repeated board summary
REPEATED_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated_$(REPEATED_BOARD_TAG)
REPEATED_BOARD_REMOTE_TAG ?= phase000_ism_local_formula_diagnostic
REPEATED_BOARD_RUN_LABEL ?= ism_phase000_local_formula_diagnostic_repeated
REPEATED_BOARD_CASE_NAME ?=
EVIDENCE_MANIFEST_REPEATED ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_manifest.json
EVIDENCE_DOCTOR_REPEATED_MD ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.md
EVIDENCE_DOCTOR_REPEATED_JSON ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.json
EVIDENCE_REGISTRY ?= $(LOG_DIR)/evidence_registry.json
EVIDENCE_ROLE_REPEATED ?= diagnostic

PUBLIC_ENTRY_BOARD_RUNS ?= 5
PUBLIC_ENTRY_BOARD_TAG ?= phase020_public_entry_findobjects_production_direct
PUBLIC_ENTRY_BOARD_TITLE ?= ISM public-entry production-direct repeated board summary
PUBLIC_ENTRY_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated_$(PUBLIC_ENTRY_BOARD_TAG)
PUBLIC_ENTRY_BOARD_REMOTE_TAG ?= phase020_public_entry_findobjects_production_direct
PUBLIC_ENTRY_BOARD_RUN_LABEL ?= ism_phase020_public_entry_findobjects_production_direct_repeated
PUBLIC_ENTRY_BOARD_CASE_NAME ?= public_find_objects_descriptor_assignment
PUBLIC_ENTRY_EVIDENCE_MANIFEST ?= $(PUBLIC_ENTRY_BOARD_OUTPUT_DIR)/evidence_manifest.json
PUBLIC_ENTRY_EVIDENCE_DOCTOR_MD ?= $(PUBLIC_ENTRY_BOARD_OUTPUT_DIR)/evidence_doctor.md
PUBLIC_ENTRY_EVIDENCE_DOCTOR_JSON ?= $(PUBLIC_ENTRY_BOARD_OUTPUT_DIR)/evidence_doctor.json
PUBLIC_ENTRY_EVIDENCE_ROLE ?= production_direct

EVIDENCE_DOC_REFS := \
	test-rvv/recognition/implicit_shape_model/README.zh.md \
	test-rvv/recognition/implicit_shape_model/doc/implicit_shape_model-evaluation.zh.md \
	test-rvv/recognition/implicit_shape_model/doc/testing-overview.zh.md \
	test-rvv/recognition/implicit_shape_model/doc/correctness-tests.zh.md \
	test-rvv/recognition/implicit_shape_model/doc/benchmark-and-evidence.zh.md \
	test-rvv/recognition/implicit_shape_model/doc/optimization-evidence.zh.md \
	test-rvv/recognition/implicit_shape_model/doc/test-support-code-map.zh.md \
	test-rvv/recognition/implicit_shape_model/doc/optimization-roadmap.zh.md \
	test-rvv/recognition/implicit_shape_model/doc/phases/README.zh.md \
	test-rvv/recognition/implicit_shape_model/doc/phases/optimization-matrix.zh.md \
	test-rvv/recognition/implicit_shape_model/doc/phases/000-current-state-and-gaps/plan.zh.md \
	test-rvv/recognition/implicit_shape_model/doc/phases/000-current-state-and-gaps/result.zh.md \
	test-rvv/recognition/implicit_shape_model/doc/phases/010-production-shaped-ism-subkernel-diagnostic/plan.zh.md \
	test-rvv/recognition/implicit_shape_model/doc/phases/010-production-shaped-ism-subkernel-diagnostic/result.zh.md \
	test-rvv/recognition/implicit_shape_model/doc/phases/020-production-integration-findobjects-public-entry/plan.zh.md \
	test-rvv/recognition/implicit_shape_model/doc/phases/020-production-integration-findobjects-public-entry/result.zh.md \
	doc-rvv/recognition/implicit_shape_model-RVV.zh.md
EVIDENCE_DOC_ARGS := $(foreach doc,$(EVIDENCE_DOC_REFS),--doc $(doc))
EVIDENCE_DOC_REF_ARGS := $(foreach doc,$(EVIDENCE_DOC_REFS),--doc-ref $(doc))

include ../../mk/rvv-topic.mk

run_qemu_smoke: run_test_compare

run_upstream_test_compare: run_upstream_test_std run_upstream_test_rvv

check_ism_rvv_asm: dump_bench_rvv
	@grep -Eq 'vle32|vlse32' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vfmul|vfmacc|vfred|vf' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@echo "[PASS] implicit_shape_model diagnostic RVV asm gate matched local formula instructions."

board_repeated:
	@set -eu; \
	for i in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
			BENCH_ARGS="$(ISM_REPEATED_BENCH_ARGS)" \
			OUTPUT_DIR_BOARD="$(REPEATED_BOARD_OUTPUT_DIR)/$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated_$(REPEATED_BOARD_REMOTE_TAG)/$$run"; \
		done

public_entry_board_repeated:
	@set -eu; \
	for i in $$(seq 1 $(PUBLIC_ENTRY_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
			BENCH_ARGS="$(ISM_PUBLIC_ENTRY_BENCH_ARGS)" \
			OUTPUT_DIR_BOARD="$(PUBLIC_ENTRY_BOARD_OUTPUT_DIR)/$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated_$(PUBLIC_ENTRY_BOARD_REMOTE_TAG)/$$run"; \
	done

evidence_manifest_repeated:
	python3 script/generate_ism_evidence_manifest.py \
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
		--case-filter "$(ISM_REPEATED_BENCH_ARGS)" \
		--evidence-role "$(EVIDENCE_ROLE_REPEATED)" \
		$(EVIDENCE_DOC_REF_ARGS) \
		--freshness-state fresh

check_evidence_freshness:
	python3 ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob 'test-rvv/recognition/implicit_shape_model/log/board/**/summary.md' \
		--scan-glob 'test-rvv/recognition/implicit_shape_model/log/board/**/evidence_manifest.json' \
		--scan-glob 'test-rvv/recognition/implicit_shape_model/log/board/**/evidence_doctor.md' \
		--scan-glob 'test-rvv/recognition/implicit_shape_model/log/board/**/evidence_doctor.json' \
		$(EVIDENCE_DOC_ARGS) \
		--require-doc-ref \
		--fail-on never

evidence_manifest_public_entry:
	python3 script/generate_ism_evidence_manifest.py \
		--summary-dir "$(PUBLIC_ENTRY_BOARD_OUTPUT_DIR)" \
		--output "$(PUBLIC_ENTRY_EVIDENCE_MANIFEST)" \
		--summary-output "$(PUBLIC_ENTRY_BOARD_OUTPUT_DIR)/summary.md" \
		--title "$(PUBLIC_ENTRY_BOARD_TITLE)" \
		--run-label "$(PUBLIC_ENTRY_BOARD_RUN_LABEL)" \
		--case-name "$(PUBLIC_ENTRY_BOARD_CASE_NAME)" \
		--expected-runs "$(PUBLIC_ENTRY_BOARD_RUNS)" \
		--evidence-role "$(PUBLIC_ENTRY_EVIDENCE_ROLE)" \
		--asm "$(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm"

evidence_doctor_public_entry: evidence_manifest_public_entry
	python3 ../../script/evidence_doctor.py \
		--manifest "$(PUBLIC_ENTRY_EVIDENCE_MANIFEST)" \
		--output "$(PUBLIC_ENTRY_EVIDENCE_DOCTOR_MD)" \
		--json-output "$(PUBLIC_ENTRY_EVIDENCE_DOCTOR_JSON)" \
		--fail-on never

record_evidence_state_public_entry: evidence_doctor_public_entry
	python3 ../../script/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(PUBLIC_ENTRY_BOARD_OUTPUT_DIR)/summary.md" \
		--manifest "$(PUBLIC_ENTRY_EVIDENCE_MANIFEST)" \
		--doctor "$(PUBLIC_ENTRY_EVIDENCE_DOCTOR_MD)" \
		--path "$(PUBLIC_ENTRY_EVIDENCE_DOCTOR_JSON)" \
		--target public_entry_board_repeated \
		--backend board \
		--run-label "$(PUBLIC_ENTRY_BOARD_RUN_LABEL)" \
		--case-filter "$(ISM_PUBLIC_ENTRY_BENCH_ARGS)" \
		--evidence-role "$(PUBLIC_ENTRY_EVIDENCE_ROLE)" \
		$(EVIDENCE_DOC_REF_ARGS) \
		--freshness-state fresh

check_public_entry_evidence_freshness:
	python3 ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob 'test-rvv/recognition/implicit_shape_model/log/board/**/summary.md' \
		--scan-glob 'test-rvv/recognition/implicit_shape_model/log/board/**/evidence_manifest.json' \
		--scan-glob 'test-rvv/recognition/implicit_shape_model/log/board/**/evidence_doctor.md' \
		--scan-glob 'test-rvv/recognition/implicit_shape_model/log/board/**/evidence_doctor.json' \
		$(EVIDENCE_DOC_ARGS) \
		--require-doc-ref \
		--fail-on never

.PHONY: run_qemu_smoke run_upstream_test_compare check_ism_rvv_asm board_repeated public_entry_board_repeated evidence_manifest_repeated evidence_doctor_repeated record_evidence_state_repeated evidence_manifest_public_entry evidence_doctor_public_entry record_evidence_state_public_entry check_evidence_freshness check_public_entry_evidence_freshness
