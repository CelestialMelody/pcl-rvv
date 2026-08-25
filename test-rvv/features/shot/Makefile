# =============================================================================
# PCL features/shot RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk.
# =============================================================================

TOPIC  := shot
MODULE := features

FOCUS_DIR      := pcl/features/include/pcl/features/impl/shot.hpp
FILTER_REPORT  := log/filtered_shot.log
ANALYZE_REPORT := log/analyze_shot.log
EVIDENCE_MANIFEST := log/board/evidence_manifest.json
EVIDENCE_DOCTOR_MD := log/board/evidence_doctor.md
SHOT_EVIDENCE_REGISTRY := log/evidence_registry.json
SHOT_BOARD_EVIDENCE_CASE ?= interpolation_bin_selection_component
SHOT_BOARD_EVIDENCE_ROLE ?= diagnostic
SHOT_BOARD_EVIDENCE_RUN_LABEL ?= board-shot-diagnostic-current
SHOT_BOARD_EVIDENCE_LOCAL_DIR ?= log/board/$(SHOT_BOARD_EVIDENCE_RUN_LABEL)
SHOT_BOARD_EVIDENCE_REMOTE_DIR ?= $(REMOTE_DIR)/log/board/$(SHOT_BOARD_EVIDENCE_RUN_LABEL)
SHOT_BOARD_EVIDENCE_SUMMARY := $(SHOT_BOARD_EVIDENCE_LOCAL_DIR)/analyze_bench_compare.log
SHOT_BOARD_EVIDENCE_MANIFEST := $(SHOT_BOARD_EVIDENCE_LOCAL_DIR)/evidence_manifest.json
SHOT_BOARD_EVIDENCE_DOCTOR_MD := $(SHOT_BOARD_EVIDENCE_LOCAL_DIR)/evidence_doctor.md

TARGET_BENCH     := bench_shot
TARGET_BENCH_STD := bench_shot_std
TARGET_BENCH_RVV := bench_shot_rvv
SRCS_BENCH       := src/bench_shot.cpp

TARGET_TEST     := test_shot
TARGET_TEST_STD := test_shot_std
TARGET_TEST_RVV := test_shot_rvv
BOARD_TARGET_TEST := test_shot_rvv
SRCS_TEST       := src/test_shot.cpp

EXTRA_CXXFLAGS = \
	-Iinclude \
	-I$(PCL_SOURCE_ROOT)/features/include \
	-I$(PCL_SOURCE_ROOT)/search/include \
	-I$(PCL_SOURCE_ROOT)/kdtree/include \
	-I$(PCL_SOURCE_ROOT)/octree/include

REMOTE_DIR ?= /root/pcl-test/features/shot

LIBS_BENCH := \
	-lpcl_features \
	-lpcl_search \
	-lpcl_kdtree \
	-lpcl_common \
	-lflann_cpp \
	-llz4 \
	-lboost_system \
	-lm

LIBS_TEST := \
	-lpcl_features \
	-lpcl_search \
	-lpcl_kdtree \
	-lpcl_common \
	-lflann_cpp \
	-llz4 \
	-lboost_system \
	-lgtest -lgtest_main -lpthread \
	-lm

generate_evidence_manifest: log/board/analyze_bench_compare.log log/board/run_bench_std.log log/board/run_bench_rvv.log
	$(PYTHON) script/generate_shot_evidence_manifest.py \
		--summary log/board/analyze_bench_compare.log \
		--std-log log/board/run_bench_std.log \
		--rvv-log log/board/run_bench_rvv.log \
		--output $(EVIDENCE_MANIFEST)

run_evidence_doctor: generate_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(EVIDENCE_MANIFEST) \
		--output $(EVIDENCE_DOCTOR_MD) \
		--fail-on never

include ../../mk/rvv-topic.mk

run_test_public:
	@$(MAKE) -C $(CURDIR) run_test_compare TEST_ARGS="--gtest_filter=Shot352.*:Shot1344.*"

run_test_normalize:
	@$(MAKE) -C $(CURDIR) run_test_compare TEST_ARGS="--gtest_filter=ShotNormalizeComponent.*"

run_test_shape_bin:
	@$(MAKE) -C $(CURDIR) run_test_compare TEST_ARGS="--gtest_filter=ShotShapeBinComponent.*:ShotShapeBinAosComponent.*:ShotShapeBinIndexedComponent.*:ShotShapeBinProductionDetail.*"

run_test_interpolation:
	@$(MAKE) -C $(CURDIR) run_test_compare TEST_ARGS="--gtest_filter=ShotInterpolationGeometryComponent.*:ShotInterpolationBinSelectionComponent.*"

run_test_color:
	@$(MAKE) -C $(CURDIR) run_test_compare TEST_ARGS="--gtest_filter=ShotColorLabDistanceComponent.*:ShotColorRgbLutIndexedComponent.*"

generate_board_case_evidence_manifest:
	@$(PYTHON_RUN) script/generate_shot_evidence_manifest.py \
		--summary $(SHOT_BOARD_EVIDENCE_SUMMARY) \
		--std-log $(SHOT_BOARD_EVIDENCE_LOCAL_DIR)/run_bench_std.log \
		--rvv-log $(SHOT_BOARD_EVIDENCE_LOCAL_DIR)/run_bench_rvv.log \
		--output $(SHOT_BOARD_EVIDENCE_MANIFEST)

run_board_case_evidence_doctor: generate_board_case_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(SHOT_BOARD_EVIDENCE_MANIFEST) \
		--output $(SHOT_BOARD_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_evidence_state: run_board_case_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(SHOT_EVIDENCE_REGISTRY) \
		--summary $(SHOT_BOARD_EVIDENCE_SUMMARY) \
		--manifest $(SHOT_BOARD_EVIDENCE_MANIFEST) \
		--doctor $(SHOT_BOARD_EVIDENCE_DOCTOR_MD) \
		--target run_board_shot_case_evidence \
		--backend board \
		--run-label $(SHOT_BOARD_EVIDENCE_RUN_LABEL) \
		--case-filter $(SHOT_BOARD_EVIDENCE_CASE) \
		--evidence-role $(SHOT_BOARD_EVIDENCE_ROLE) \
		--doc-ref doc/phases/080-interpolation-bin-selection-scalar-tail-diagnostic/result.zh.md \
		--doc-ref doc/phases/100-evidence-registry-target-alias-diagnostic/result.zh.md \
		--doc-ref doc/phases/PI2-shape-bin-indexed-production-probe/result.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/shot-evaluation.zh.md \
		--freshness-state fresh

run_board_shot_case_evidence:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs record_board_evidence_state \
		OUTPUT_DIR_BOARD="$(SHOT_BOARD_EVIDENCE_LOCAL_DIR)" \
		REMOTE_BOARD_OUTPUT_DIR="$(SHOT_BOARD_EVIDENCE_REMOTE_DIR)" \
		BENCH_ARGS="--case-filter $(SHOT_BOARD_EVIDENCE_CASE)" \
		SHOT_BOARD_EVIDENCE_CASE="$(SHOT_BOARD_EVIDENCE_CASE)" \
		SHOT_BOARD_EVIDENCE_ROLE="$(SHOT_BOARD_EVIDENCE_ROLE)" \
		SHOT_BOARD_EVIDENCE_RUN_LABEL="$(SHOT_BOARD_EVIDENCE_RUN_LABEL)" \
		SHOT_BOARD_EVIDENCE_LOCAL_DIR="$(SHOT_BOARD_EVIDENCE_LOCAL_DIR)" \
		SHOT_BOARD_EVIDENCE_REMOTE_DIR="$(SHOT_BOARD_EVIDENCE_REMOTE_DIR)"

run_board_shape_bin_indexed_evidence:
	@$(MAKE) -C $(CURDIR) run_board_shot_case_evidence \
		SHOT_BOARD_EVIDENCE_CASE=shape_bin_indexed_component \
		SHOT_BOARD_EVIDENCE_RUN_LABEL=board-shot-shape-bin-indexed-diagnostic-phase040

run_board_production_shape_bin_direct_evidence:
	@$(MAKE) -C $(CURDIR) run_board_shot_case_evidence \
		SHOT_BOARD_EVIDENCE_CASE=production_shape_bin_direct \
		SHOT_BOARD_EVIDENCE_ROLE=production-detail \
		SHOT_BOARD_EVIDENCE_RUN_LABEL=board-shot-production-shape-bin-direct-pi2

run_board_interpolation_bin_selection_evidence:
	@$(MAKE) -C $(CURDIR) run_board_shot_case_evidence \
		SHOT_BOARD_EVIDENCE_CASE=interpolation_bin_selection_component \
		SHOT_BOARD_EVIDENCE_RUN_LABEL=board-shot-interpolation-bin-selection-diagnostic-phase080

run_board_color_rgb_lut_evidence:
	@$(MAKE) -C $(CURDIR) run_board_shot_case_evidence \
		SHOT_BOARD_EVIDENCE_CASE=color_rgb_lut_indexed_component \
		SHOT_BOARD_EVIDENCE_RUN_LABEL=board-shot-color-rgb-lut-indexed-diagnostic-phase070

evidence_status:
	@$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry $(SHOT_EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/features/shot/log/board/board-shot-*/analyze_bench_compare.log' \
		--scan-glob 'test-rvv/features/shot/log/board/board-shot-*/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/shot/log/board/board-shot-*/evidence_doctor.md' \
		--doc test-rvv/features/shot/doc/phases/080-interpolation-bin-selection-scalar-tail-diagnostic/result.zh.md \
		--doc test-rvv/features/shot/doc/phases/100-evidence-registry-target-alias-diagnostic/result.zh.md \
		--doc test-rvv/features/shot/doc/benchmark-and-evidence.zh.md \
		--doc test-rvv/features/shot/doc/shot-evaluation.zh.md \
		--fail-on never

check_evidence_doc_refs:
	@$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry $(SHOT_EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/features/shot/log/board/board-shot-*/analyze_bench_compare.log' \
		--scan-glob 'test-rvv/features/shot/log/board/board-shot-*/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/shot/log/board/board-shot-*/evidence_doctor.md' \
		--doc test-rvv/features/shot/doc/phases/080-interpolation-bin-selection-scalar-tail-diagnostic/result.zh.md \
		--doc test-rvv/features/shot/doc/phases/100-evidence-registry-target-alias-diagnostic/result.zh.md \
		--doc test-rvv/features/shot/doc/benchmark-and-evidence.zh.md \
		--doc test-rvv/features/shot/doc/shot-evaluation.zh.md \
		--require-doc-ref \
		--fail-on never

.PHONY: run_test_public run_test_normalize run_test_shape_bin run_test_interpolation run_test_color generate_board_case_evidence_manifest run_board_case_evidence_doctor record_board_evidence_state run_board_shot_case_evidence run_board_shape_bin_indexed_evidence run_board_production_shape_bin_direct_evidence run_board_interpolation_bin_selection_evidence run_board_color_rgb_lut_evidence evidence_status check_evidence_doc_refs
