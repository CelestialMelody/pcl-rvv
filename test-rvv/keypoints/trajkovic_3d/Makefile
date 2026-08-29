# =============================================================================
# PCL keypoints/trajkovic_3d RVV topic Makefile
#
# 本 topic 先评估 TrajkovicKeypoint3D response map（响应图）中的
# FOUR_CORNERS normal stencil（四邻域法线模板）。QEMU（仿真器）只用于
# correctness（正确性）和日志形状；性能结论必须来自 board（板卡）
# 或目标硬件 repeated benchmark（重复性能测试）。
# =============================================================================

TOPIC  := trajkovic_3d
MODULE := keypoints
TOPIC_DOC_PREFIX := test-rvv/keypoints/trajkovic_3d

FOCUS_DIR      := pcl/keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp
FILTER_REPORT  := log/filtered_trajkovic_3d.log
ANALYZE_REPORT := log/analyze_trajkovic_3d.log

TARGET_TEST     := test_trajkovic_3d
TARGET_TEST_STD := test_trajkovic_3d_std
TARGET_TEST_RVV := test_trajkovic_3d_rvv
SRCS_TEST       := src/test_trajkovic_3d.cpp

TARGET_BENCH     := bench_trajkovic_3d
TARGET_BENCH_STD := bench_trajkovic_3d_std
TARGET_BENCH_RVV := bench_trajkovic_3d_rvv
SRCS_BENCH       := src/bench_trajkovic_3d.cpp

BENCH_ARGS ?= --case-filter all --iterations 20 --warmup 3 --width 320 --height 240

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/keypoints/include \
	-I$(PCL_SOURCE_ROOT)/features/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/search/include \
	-I$(PCL_SOURCE_ROOT)/kdtree/include \
	-I$(PCL_SOURCE_ROOT)/octree/include \
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

LIBS_TEST := -lpcl_keypoints -lpcl_features -lpcl_search -lpcl_kdtree -lpcl_common -lflann_cpp -llz4 -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lpcl_keypoints -lpcl_features -lpcl_search -lpcl_kdtree -lpcl_common -lflann_cpp -llz4 -lm

REMOTE_DIR ?= /root/pcl-test/keypoints/trajkovic_3d

REPEATED_BOARD_RUNS ?= 5
REPEATED_BOARD_TAG ?= phase000_four_corners_response
REPEATED_BOARD_TITLE ?= Trajkovic 3D FOUR_CORNERS response diagnostic repeated board summary
REPEATED_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated_$(REPEATED_BOARD_TAG)
REPEATED_BOARD_REMOTE_TAG ?= phase000_four_corners_response
REPEATED_BOARD_RUN_LABEL ?= trajkovic_3d_phase000_four_corners_response_repeated
TRAJKOVIC3D_REPEATED_BENCH_ARGS ?= --case-filter all --iterations 20 --warmup 3 --width 320 --height 240
EVIDENCE_MANIFEST_REPEATED ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_manifest.json
EVIDENCE_DOCTOR_REPEATED_MD ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.md
EVIDENCE_DOCTOR_REPEATED_JSON ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.json
EVIDENCE_REGISTRY ?= $(LOG_DIR)/evidence_registry.json

PRODUCTION_BOARD_TAG ?= phase020_eight_corners_production_public
PRODUCTION_BOARD_TITLE ?= Trajkovic 3D FOUR_CORNERS / EIGHT_CORNERS production public repeated board summary
PRODUCTION_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated_$(PRODUCTION_BOARD_TAG)
PRODUCTION_BOARD_REMOTE_TAG ?= phase020_eight_corners_production_public
PRODUCTION_BOARD_RUN_LABEL ?= trajkovic_3d_phase020_eight_corners_production_public_repeated
TRAJKOVIC3D_PRODUCTION_BENCH_ARGS ?= --mode public --case-filter all --iterations 20 --warmup 3 --width 320 --height 240
EVIDENCE_MANIFEST_PRODUCTION ?= $(PRODUCTION_BOARD_OUTPUT_DIR)/evidence_manifest.json
EVIDENCE_DOCTOR_PRODUCTION_MD ?= $(PRODUCTION_BOARD_OUTPUT_DIR)/evidence_doctor.md
EVIDENCE_DOCTOR_PRODUCTION_JSON ?= $(PRODUCTION_BOARD_OUTPUT_DIR)/evidence_doctor.json

include ../../mk/rvv-topic.mk

run_qemu_smoke: run_test_compare

check_trajkovic_3d_rvv_asm: dump_bench_rvv
	@grep -Eq 'computeFourCornersResponseRVV' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq 'computeEightCornersResponseRVV' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq '\bvlse32\.v\b' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq '\bvfmul\.vv\b' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq '\bvfsqrt\.v\b' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@echo "[PASS] Trajkovic3D FOUR_CORNERS / EIGHT_CORNERS RVV asm gate matched strided normal loads, multiply, and sqrt instructions."

board_repeated:
	@set -eu; \
	for i in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
			BENCH_ARGS="$(TRAJKOVIC3D_REPEATED_BENCH_ARGS)" \
			OUTPUT_DIR_BOARD="$(REPEATED_BOARD_OUTPUT_DIR)/$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated_$(REPEATED_BOARD_REMOTE_TAG)/$$run"; \
	done

evidence_manifest_repeated:
	python3 script/generate_trajkovic_3d_evidence_manifest.py \
		--summary-dir "$(REPEATED_BOARD_OUTPUT_DIR)" \
		--output "$(EVIDENCE_MANIFEST_REPEATED)" \
		--summary-output "$(REPEATED_BOARD_OUTPUT_DIR)/summary.md" \
		--title "$(REPEATED_BOARD_TITLE)" \
		--run-label "$(REPEATED_BOARD_RUN_LABEL)" \
		--evidence-role production_shaped_diagnostic \
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
		--case-filter "$(TRAJKOVIC3D_REPEATED_BENCH_ARGS)" \
		--evidence-role production_shaped_diagnostic \
		--doc-ref doc/phases/000-current-state-and-four-corners-diagnostic/result.zh.md \
		--doc-ref doc/trajkovic_3d-evaluation.zh.md \
		--freshness-state fresh

board_repeated_production:
	@set -eu; \
	for i in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
			BENCH_ARGS="$(TRAJKOVIC3D_PRODUCTION_BENCH_ARGS)" \
			OUTPUT_DIR_BOARD="$(PRODUCTION_BOARD_OUTPUT_DIR)/$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated_$(PRODUCTION_BOARD_REMOTE_TAG)/$$run"; \
	done

evidence_manifest_production:
	python3 script/generate_trajkovic_3d_evidence_manifest.py \
		--summary-dir "$(PRODUCTION_BOARD_OUTPUT_DIR)" \
		--output "$(EVIDENCE_MANIFEST_PRODUCTION)" \
		--summary-output "$(PRODUCTION_BOARD_OUTPUT_DIR)/summary.md" \
		--title "$(PRODUCTION_BOARD_TITLE)" \
		--run-label "$(PRODUCTION_BOARD_RUN_LABEL)" \
		--evidence-role production_public \
		--include-case public_four_corners_320x240 \
		--include-case public_eight_corners_320x240 \
		--expected-runs "$(REPEATED_BOARD_RUNS)" \
		--asm "$(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm"

evidence_doctor_production: evidence_manifest_production
	python3 ../../script/evidence_doctor.py \
		--manifest "$(EVIDENCE_MANIFEST_PRODUCTION)" \
		--output "$(EVIDENCE_DOCTOR_PRODUCTION_MD)" \
		--json-output "$(EVIDENCE_DOCTOR_PRODUCTION_JSON)" \
		--fail-on never

record_evidence_state_production: evidence_doctor_production
	python3 ../../script/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(PRODUCTION_BOARD_OUTPUT_DIR)/summary.md" \
		--manifest "$(EVIDENCE_MANIFEST_PRODUCTION)" \
		--doctor "$(EVIDENCE_DOCTOR_PRODUCTION_MD)" \
		--path "$(EVIDENCE_DOCTOR_PRODUCTION_JSON)" \
		--target board_repeated_production \
		--backend board \
		--run-label "$(PRODUCTION_BOARD_RUN_LABEL)" \
		--case-filter "$(TRAJKOVIC3D_PRODUCTION_BENCH_ARGS)" \
		--evidence-role production_public \
		--doc-ref doc/phases/010-production-public-dispatch/result.zh.md \
		--doc-ref doc/phases/020-eight-corners-production-public-dispatch/plan.zh.md \
		--doc-ref doc/trajkovic_3d-evaluation.zh.md \
		--doc-ref ../../../../doc-rvv/keypoints/trajkovic_3d-RVV.zh.md \
		--freshness-state fresh

check_evidence_freshness:
	python3 ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob 'test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase000_four_corners_response/summary.md' \
		--scan-glob 'test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase000_four_corners_response/evidence_manifest.json' \
		--scan-glob 'test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase000_four_corners_response/evidence_doctor.md' \
		--scan-glob 'test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase000_four_corners_response/evidence_doctor.json' \
		--scan-glob 'test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase010_production_public/summary.md' \
		--scan-glob 'test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase010_production_public/evidence_manifest.json' \
		--scan-glob 'test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase010_production_public/evidence_doctor.md' \
		--scan-glob 'test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase010_production_public/evidence_doctor.json' \
		--scan-glob 'test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase020_eight_corners_production_public/summary.md' \
		--scan-glob 'test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase020_eight_corners_production_public/evidence_manifest.json' \
		--scan-glob 'test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase020_eight_corners_production_public/evidence_doctor.md' \
		--scan-glob 'test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase020_eight_corners_production_public/evidence_doctor.json' \
		--doc test-rvv/keypoints/trajkovic_3d/doc/phases/000-current-state-and-four-corners-diagnostic/result.zh.md \
		--doc test-rvv/keypoints/trajkovic_3d/doc/trajkovic_3d-evaluation.zh.md \
		--doc test-rvv/keypoints/trajkovic_3d/doc/phases/010-production-public-dispatch/result.zh.md \
		--doc test-rvv/keypoints/trajkovic_3d/doc/phases/020-eight-corners-production-public-dispatch/plan.zh.md \
		--doc doc-rvv/keypoints/trajkovic_3d-RVV.zh.md \
		--require-doc-ref \
		--fail-on never

.PHONY: run_qemu_smoke check_trajkovic_3d_rvv_asm board_repeated evidence_manifest_repeated evidence_doctor_repeated record_evidence_state_repeated board_repeated_production evidence_manifest_production evidence_doctor_production record_evidence_state_production check_evidence_freshness
