# =============================================================================
# PCL keypoints/harris_2d RVV diagnostic Makefile
#
# 本 topic 首阶段评估 HarrisKeypoint2D organized derivative + response map
# production-shaped diagnostic（生产形态诊断）。QEMU（仿真器）只用于
# correctness（正确性）、构建和日志形状；性能结论必须来自 board（板卡）
# 或目标硬件 repeated benchmark（重复性能测试）。
# =============================================================================

TOPIC  := harris_2d
MODULE := keypoints

FOCUS_DIR      := keypoints/include/pcl/keypoints/impl/harris_2d.hpp
FILTER_REPORT  ?= log/filtered_harris_2d.log
ANALYZE_REPORT ?= log/analyze_harris_2d.log

TARGET_TEST     := test_harris_2d
TARGET_TEST_STD := test_harris_2d_std
TARGET_TEST_RVV := test_harris_2d_rvv
SRCS_TEST       := src/test_harris_2d.cpp

TARGET_BENCH     := bench_harris_2d
TARGET_BENCH_STD := bench_harris_2d_std
TARGET_BENCH_RVV := bench_harris_2d_rvv
SRCS_BENCH       := src/bench_harris_2d.cpp

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/keypoints/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/search/include \
	-I$(PCL_SOURCE_ROOT)/kdtree/include \
	-I$(PCL_SOURCE_ROOT)/octree/include \
	-I$(PCL_SOURCE_ROOT)/features/include \
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

LIBS_TEST  := -lpcl_keypoints -lpcl_features -lpcl_search -lpcl_kdtree -lpcl_common -lflann_cpp -llz4 -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lpcl_keypoints -lpcl_features -lpcl_search -lpcl_kdtree -lpcl_common -lflann_cpp -llz4 -lm

REMOTE_DIR ?= /root/pcl-test/keypoints/harris_2d

REPEATED_BOARD_RUNS ?= 5
REPEATED_BOARD_TAG ?= phase000_response_map
REPEATED_BOARD_TITLE ?= Harris 2D response map production-shaped diagnostic repeated board summary
REPEATED_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated_$(REPEATED_BOARD_TAG)
REPEATED_BOARD_REMOTE_TAG ?= phase000_response_map
REPEATED_BOARD_RUN_LABEL ?= harris_2d_phase000_response_map_repeated
HARRIS2D_REPEATED_BENCH_ARGS ?= --case-filter all --iterations 20 --warmup-iterations 3
REPEATED_EVIDENCE_ROLE ?= production_shaped_diagnostic
REPEATED_CASE_KIND ?= production_shaped_diagnostic
REPEATED_AB_BOUNDARY ?= test_helper
REPEATED_POINT_TYPE ?= test synthetic organized float intensity image
REPEATED_TIMER_BOUNDARY ?= computeResponsesCandidate includes derivative map, second-moment accumulation and response formula; excludes NMS sort, occupancy map and public dispatch
REPEATED_BASELINE_ASM_BOUNDARY ?= scalar public compute build; no responseRVV dispatch expected
REPEATED_ASM_BOUNDARY ?= harris_2d test-support candidate / inlined bench_harris_2d_rvv
REPEATED_EVIDENCE_NOTES ?= Production-shaped diagnostic only; production direct evidence requires harris_2d.hpp dispatch patch and public entry tests.
EVIDENCE_MANIFEST_REPEATED ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_manifest.json
EVIDENCE_DOCTOR_REPEATED_MD ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.md
EVIDENCE_DOCTOR_REPEATED_JSON ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.json
EVIDENCE_REGISTRY ?= $(LOG_DIR)/evidence_registry.json

include ../../mk/rvv-topic.mk

run_qemu_smoke: run_test_compare run_bench_rvv

check_harris_2d_rvv_asm: dump_bench_rvv
	@grep -Eq 'vle32|vse32|vsse32|vlse32|vfmacc|vfsqrt|vfdiv|vfmul|vfadd|vfsub' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@echo "[PASS] harris_2d RVV asm gate matched expected float response instructions."

board_repeated:
	@set -eu; \
	for i in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
			BENCH_ARGS="$(HARRIS2D_REPEATED_BENCH_ARGS)" \
			OUTPUT_DIR_BOARD="$(REPEATED_BOARD_OUTPUT_DIR)/$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated_$(REPEATED_BOARD_REMOTE_TAG)/$$run"; \
	done

evidence_manifest_repeated:
	python3 script/generate_harris_2d_evidence_manifest.py \
		--summary-dir "$(REPEATED_BOARD_OUTPUT_DIR)" \
		--output "$(EVIDENCE_MANIFEST_REPEATED)" \
		--summary-output "$(REPEATED_BOARD_OUTPUT_DIR)/summary.md" \
		--title "$(REPEATED_BOARD_TITLE)" \
		--run-label "$(REPEATED_BOARD_RUN_LABEL)" \
		--expected-runs "$(REPEATED_BOARD_RUNS)" \
		--asm "$(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm" \
		--evidence-role "$(REPEATED_EVIDENCE_ROLE)" \
		--case-kind "$(REPEATED_CASE_KIND)" \
		--ab-boundary "$(REPEATED_AB_BOUNDARY)" \
		--point-type "$(REPEATED_POINT_TYPE)" \
		--timer-boundary "$(REPEATED_TIMER_BOUNDARY)" \
		--baseline-asm-boundary "$(REPEATED_BASELINE_ASM_BOUNDARY)" \
		--asm-boundary "$(REPEATED_ASM_BOUNDARY)" \
		--notes "$(REPEATED_EVIDENCE_NOTES)"

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
		--case-filter "$(HARRIS2D_REPEATED_BENCH_ARGS)" \
		--evidence-role "$(REPEATED_EVIDENCE_ROLE)" \
		--doc-ref doc/phases/020-direct-intensity-stride-store/result.zh.md \
		--doc-ref doc/harris_2d-evaluation.zh.md \
		--freshness-state fresh

check_evidence_freshness:
	python3 ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob 'test-rvv/keypoints/harris_2d/log/board/repeated_phase020_direct_intensity_stride_store_public_entry/summary.md' \
		--scan-glob 'test-rvv/keypoints/harris_2d/log/board/repeated_phase020_direct_intensity_stride_store_public_entry/evidence_manifest.json' \
		--scan-glob 'test-rvv/keypoints/harris_2d/log/board/repeated_phase020_direct_intensity_stride_store_public_entry/evidence_doctor.md' \
		--scan-glob 'test-rvv/keypoints/harris_2d/log/board/repeated_phase020_direct_intensity_stride_store_public_entry/evidence_doctor.json' \
		--doc test-rvv/keypoints/harris_2d/doc/phases/000-current-state-and-diagnostic-scaffold/result.zh.md \
		--doc test-rvv/keypoints/harris_2d/doc/phases/020-direct-intensity-stride-store/result.zh.md \
		--doc test-rvv/keypoints/harris_2d/doc/harris_2d-evaluation.zh.md \
		--require-doc-ref \
		--fail-on never

.PHONY: run_qemu_smoke check_harris_2d_rvv_asm board_repeated evidence_manifest_repeated evidence_doctor_repeated record_evidence_state_repeated check_evidence_freshness
