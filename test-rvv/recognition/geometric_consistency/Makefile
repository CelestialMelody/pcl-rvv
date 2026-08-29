# =============================================================================
# PCL recognition/geometric_consistency RVV topic Makefile
#
# 本 topic 先做 pairwise distance consistency predicate 的 diagnostic / component
# ablation。QEMU 只用于 correctness、构建和日志形状；性能结论必须来自 board
# 或目标硬件 repeated benchmark。
# =============================================================================

TOPIC  := geometric_consistency
MODULE := recognition

FOCUS_DIR      := pcl/recognition/include/pcl/recognition/impl/cg/geometric_consistency.hpp
FILTER_REPORT  := log/filtered_geometric_consistency.log
ANALYZE_REPORT := log/analyze_geometric_consistency.log

TARGET_TEST     := test_gc
TARGET_TEST_STD := test_gc_std
TARGET_TEST_RVV := test_gc_rvv
SRCS_TEST       := src/test_gc.cpp

TARGET_UPSTREAM_TEST     := test_geometric_consistency_upstream
TARGET_UPSTREAM_TEST_STD := test_geometric_consistency_upstream_std
TARGET_UPSTREAM_TEST_RVV := test_geometric_consistency_upstream_rvv
SRCS_UPSTREAM_TEST       = $(PCL_SOURCE_ROOT)/test/recognition/test_recognition_cg.cpp
UPSTREAM_TEST_ARGS       = $(PCL_SOURCE_ROOT)/test/milk.pcd $(PCL_SOURCE_ROOT)/test/milk_cartoon_all_small_clorox.pcd
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

TARGET_BENCH     := bench_gc
TARGET_BENCH_STD := bench_gc_std
TARGET_BENCH_RVV := bench_gc_rvv
SRCS_BENCH       := src/bench_gc.cpp
BENCH_ARGS       ?= 4096 200 5 0.03

REMOTE_DIR ?= /root/pcl-test/recognition/geometric_consistency

INCLUDES ?= 	-I$(CURDIR)/include 	-I$(PCL_SOURCE_ROOT)/recognition/include 	-I$(PCL_SOURCE_ROOT)/common/include 	-I$(PCL_SOURCE_ROOT)/test/include 	-I$(PCL_SOURCE_ROOT) 	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 	-I$(EIGEN_ROOT)/include/eigen3 	-I$(BOOST_ROOT)/include 	-I$(GTEST_ROOT)/include 	-I$(FLANN_ROOT)/include 	-I$(LZ4_ROOT)/include 	-I$(HDF5_ROOT)/include 	-I$(ZLIB_ROOT)/include 	-I$(LIBPNG_ROOT)/include

REPEATED_BOARD_RUNS ?= 5
REPEATED_BOARD_TAG ?= phase000_pairwise_consistency_diagnostic
REPEATED_BOARD_TITLE ?= Geometric consistency pairwise predicate diagnostic repeated board summary
REPEATED_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated_$(REPEATED_BOARD_TAG)
REPEATED_BOARD_REMOTE_TAG ?= phase000_pairwise_consistency_diagnostic
REPEATED_BOARD_RUN_LABEL ?= geometric_consistency_phase000_pairwise_consistency_diagnostic_repeated
REPEATED_BOARD_CASE_NAME ?= pairwise_consistency_batch
EVIDENCE_ROLE_REPEATED ?= diagnostic
EVIDENCE_MANIFEST_REPEATED ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_manifest.json
EVIDENCE_DOCTOR_REPEATED_MD ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.md
EVIDENCE_DOCTOR_REPEATED_JSON ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.json
EVIDENCE_REGISTRY ?= $(LOG_DIR)/evidence_registry.json
GROWTH_BOARD_RUNS ?= 5
GROWTH_BOARD_TAG ?= phase020_cluster_growth_production_probe
GROWTH_BOARD_TITLE ?= Geometric consistency cluster growth production probe repeated board summary
GROWTH_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated_$(GROWTH_BOARD_TAG)
GROWTH_BOARD_REMOTE_TAG ?= phase020_cluster_growth_production_probe
GROWTH_BOARD_RUN_LABEL ?= geometric_consistency_phase020_cluster_growth_production_probe_repeated
GROWTH_BOARD_CASE_NAME ?= cluster_growth
GROWTH_EVIDENCE_ROLE ?= diagnostic
GROWTH_EVIDENCE_MANIFEST ?= $(GROWTH_BOARD_OUTPUT_DIR)/evidence_manifest.json
GROWTH_EVIDENCE_DOCTOR_MD ?= $(GROWTH_BOARD_OUTPUT_DIR)/evidence_doctor.md
GROWTH_EVIDENCE_DOCTOR_JSON ?= $(GROWTH_BOARD_OUTPUT_DIR)/evidence_doctor.json
EVIDENCE_DOC_REFS := \
	test-rvv/recognition/geometric_consistency/README.zh.md \
	test-rvv/recognition/geometric_consistency/doc/geometric_consistency-evaluation.zh.md \
	test-rvv/recognition/geometric_consistency/doc/testing-overview.zh.md \
	test-rvv/recognition/geometric_consistency/doc/correctness-tests.zh.md \
	test-rvv/recognition/geometric_consistency/doc/benchmark-and-evidence.zh.md \
	test-rvv/recognition/geometric_consistency/doc/optimization-evidence.zh.md \
	test-rvv/recognition/geometric_consistency/doc/test-support-code-map.zh.md \
	test-rvv/recognition/geometric_consistency/doc/optimization-roadmap.zh.md \
	test-rvv/recognition/geometric_consistency/doc/phases/README.zh.md \
	test-rvv/recognition/geometric_consistency/doc/phases/optimization-matrix.zh.md \
	test-rvv/recognition/geometric_consistency/doc/phases/000-current-state-and-gaps/plan.zh.md \
	test-rvv/recognition/geometric_consistency/doc/phases/000-current-state-and-gaps/result.zh.md \
	test-rvv/recognition/geometric_consistency/doc/phases/010-cluster-growth-diagnostic/plan.zh.md \
	test-rvv/recognition/geometric_consistency/doc/phases/010-cluster-growth-diagnostic/result.zh.md \
	test-rvv/recognition/geometric_consistency/doc/phases/020-cluster-growth-production-probe/plan.zh.md \
	test-rvv/recognition/geometric_consistency/doc/phases/020-cluster-growth-production-probe/result.zh.md
GROWTH_EVIDENCE_DOC_REFS := \
	$(EVIDENCE_DOC_REFS) \
	test-rvv/recognition/geometric_consistency/doc/phases/020-cluster-growth-production-probe/plan.zh.md \
	test-rvv/recognition/geometric_consistency/doc/phases/020-cluster-growth-production-probe/result.zh.md
EVIDENCE_DOC_ARGS := $(foreach doc,$(EVIDENCE_DOC_REFS),--doc $(doc))
EVIDENCE_DOC_REF_ARGS := $(foreach doc,$(EVIDENCE_DOC_REFS),--doc-ref $(doc))
GROWTH_EVIDENCE_DOC_REF_ARGS := $(foreach doc,$(GROWTH_EVIDENCE_DOC_REFS),--doc-ref $(doc))

include ../../mk/rvv-topic.mk

run_qemu_smoke: run_test_compare

check_gc_rvv_asm: dump_bench_rvv
	@grep -Eq 'vluxei32|vle32' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vfsqrt|vfsub|vfadd|vfabs|vmfle' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@echo "[PASS] geometric consistency RVV asm gate matched expected predicate instructions."

board_repeated:
	@set -eu; \
	for i in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
			BENCH_ARGS="$(BENCH_ARGS)" \
			OUTPUT_DIR_BOARD="$(REPEATED_BOARD_OUTPUT_DIR)/$$run" \
		REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated_$(REPEATED_BOARD_REMOTE_TAG)/$$run"; \
	done

board_repeated_growth:
	@set -eu; \
	for i in $$(seq 1 $(GROWTH_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
			BENCH_ARGS="$(BENCH_ARGS)" \
			OUTPUT_DIR_BOARD="$(GROWTH_BOARD_OUTPUT_DIR)/$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated_$(GROWTH_BOARD_REMOTE_TAG)/$$run"; \
	done

evidence_manifest_repeated:
	python3 script/generate_gc_evidence_manifest.py \
		--summary-dir "$(REPEATED_BOARD_OUTPUT_DIR)" \
		--output "$(EVIDENCE_MANIFEST_REPEATED)" \
		--summary-output "$(REPEATED_BOARD_OUTPUT_DIR)/summary.md" \
		--title "$(REPEATED_BOARD_TITLE)" \
		--run-label "$(REPEATED_BOARD_RUN_LABEL)" \
		--case-name "$(REPEATED_BOARD_CASE_NAME)" \
		--evidence-role "$(EVIDENCE_ROLE_REPEATED)" \
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
		--case-filter "$(BENCH_ARGS)" \
		--evidence-role "$(EVIDENCE_ROLE_REPEATED)" \
		$(EVIDENCE_DOC_REF_ARGS) \
		--freshness-state fresh

evidence_manifest_growth:
	python3 script/generate_gc_evidence_manifest.py \
		--summary-dir "$(GROWTH_BOARD_OUTPUT_DIR)" \
		--output "$(GROWTH_EVIDENCE_MANIFEST)" \
		--summary-output "$(GROWTH_BOARD_OUTPUT_DIR)/summary.md" \
		--title "$(GROWTH_BOARD_TITLE)" \
		--run-label "$(GROWTH_BOARD_RUN_LABEL)" \
		--case-name "$(GROWTH_BOARD_CASE_NAME)" \
		--evidence-role "$(GROWTH_EVIDENCE_ROLE)" \
		--expected-runs "$(GROWTH_BOARD_RUNS)" \
		--asm "$(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm"

evidence_doctor_growth: evidence_manifest_growth
	python3 ../../script/evidence_doctor.py \
		--manifest "$(GROWTH_EVIDENCE_MANIFEST)" \
		--output "$(GROWTH_EVIDENCE_DOCTOR_MD)" \
		--json-output "$(GROWTH_EVIDENCE_DOCTOR_JSON)" \
		--fail-on never

record_evidence_state_growth: evidence_doctor_growth
	python3 ../../script/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(GROWTH_BOARD_OUTPUT_DIR)/summary.md" \
		--manifest "$(GROWTH_EVIDENCE_MANIFEST)" \
		--doctor "$(GROWTH_EVIDENCE_DOCTOR_MD)" \
		--path "$(GROWTH_EVIDENCE_DOCTOR_JSON)" \
		--target board_repeated_growth \
		--backend board \
		--run-label "$(GROWTH_BOARD_RUN_LABEL)" \
		--case-filter "$(BENCH_ARGS)" \
		--evidence-role "$(GROWTH_EVIDENCE_ROLE)" \
		$(GROWTH_EVIDENCE_DOC_REF_ARGS) \
		--freshness-state fresh

check_evidence_freshness:
	python3 ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob 'test-rvv/recognition/geometric_consistency/log/board/**/summary.md' \
		--scan-glob 'test-rvv/recognition/geometric_consistency/log/board/**/evidence_manifest.json' \
		--scan-glob 'test-rvv/recognition/geometric_consistency/log/board/**/evidence_doctor.md' \
		--scan-glob 'test-rvv/recognition/geometric_consistency/log/board/**/evidence_doctor.json' \
		$(EVIDENCE_DOC_ARGS) \
		--require-doc-ref \
		--fail-on never

.PHONY: run_qemu_smoke check_gc_rvv_asm board_repeated board_repeated_growth evidence_manifest_repeated evidence_doctor_repeated record_evidence_state_repeated evidence_manifest_growth evidence_doctor_growth record_evidence_state_growth check_evidence_freshness
