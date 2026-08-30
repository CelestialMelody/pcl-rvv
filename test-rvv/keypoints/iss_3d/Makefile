# =============================================================================
# PCL keypoints/iss_3d RVV topic Makefile
#
# Phase 000 先做 getScatterMatrix 的 component ablation（组件消融）。
# QEMU 只用于 correctness（正确性）和日志形状；性能结论必须来自板卡。
# =============================================================================

TOPIC  := iss_3d
MODULE := keypoints
TOPIC_DOC_PREFIX := test-rvv/keypoints/iss_3d

FOCUS_DIR      := pcl/keypoints/include/pcl/keypoints/impl/iss_3d.hpp
FILTER_REPORT  := log/filtered_iss_3d.log
ANALYZE_REPORT := log/analyze_iss_3d.log

TARGET_TEST     := test_iss_3d
TARGET_TEST_STD := test_iss_3d_std
TARGET_TEST_RVV := test_iss_3d_rvv
SRCS_TEST       := src/test_iss_3d.cpp

TARGET_BENCH     := bench_iss_3d
TARGET_BENCH_STD := bench_iss_3d_std
TARGET_BENCH_RVV := bench_iss_3d_rvv
SRCS_BENCH       := src/bench_iss_3d.cpp

BENCH_ARGS ?= --case-filter all --iterations 20 --warmup 3 --points 8192 --neighbors 256

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

REMOTE_DIR ?= /root/pcl-test/keypoints/iss_3d

REPEATED_BOARD_RUNS ?= 5
REPEATED_BOARD_TAG ?= phase000_scatter_f64
REPEATED_BOARD_TITLE ?= ISS 3D scatter f64 diagnostic repeated board summary
REPEATED_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated_$(REPEATED_BOARD_TAG)
REPEATED_BOARD_REMOTE_TAG ?= phase000_scatter_f64
REPEATED_BOARD_RUN_LABEL ?= iss_3d_phase000_scatter_f64_repeated
ISS3D_REPEATED_BENCH_ARGS ?= --case-filter all --iterations 20 --warmup 3 --points 8192 --neighbors 256
EVIDENCE_MANIFEST_REPEATED ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_manifest.json
EVIDENCE_DOCTOR_REPEATED_MD ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.md
EVIDENCE_DOCTOR_REPEATED_JSON ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.json
EVIDENCE_REGISTRY ?= $(LOG_DIR)/evidence_registry.json

PRODUCTION_BOARD_TAG ?= phase010_production_public
PRODUCTION_BOARD_TITLE ?= ISS 3D production public repeated board summary
PRODUCTION_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated_$(PRODUCTION_BOARD_TAG)
PRODUCTION_BOARD_REMOTE_TAG ?= phase010_production_public
PRODUCTION_BOARD_RUN_LABEL ?= iss_3d_phase010_production_public_repeated
ISS3D_PRODUCTION_BENCH_ARGS ?= --mode public --case-filter public_iss_3d_grid_4096 --iterations 5 --warmup 1 --points 4096 --neighbors 256
EVIDENCE_MANIFEST_PRODUCTION ?= $(PRODUCTION_BOARD_OUTPUT_DIR)/evidence_manifest.json
EVIDENCE_DOCTOR_PRODUCTION_MD ?= $(PRODUCTION_BOARD_OUTPUT_DIR)/evidence_doctor.md
EVIDENCE_DOCTOR_PRODUCTION_JSON ?= $(PRODUCTION_BOARD_OUTPUT_DIR)/evidence_doctor.json
ISS3D_ALLOW_HISTORICAL_PRODUCTION_PROBE ?= 0

include ../../mk/rvv-topic.mk

run_qemu_smoke: run_test_compare

check_iss_3d_rvv_asm: dump_bench_rvv
	@grep -Eq 'computeScatterMatrixCandidate|computeScatterMatrixRVV' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq '\bvluxei32\.v\b|\bvluxseg3ei32\.v\b' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq '\bvfred[ou]sum\.vs\b' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@echo "[PASS] ISS 3D scatter RVV asm gate matched indexed xyz load and vector reduction."

board_repeated:
	@set -eu; \
	for i in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
			BENCH_ARGS="$(ISS3D_REPEATED_BENCH_ARGS)" \
			OUTPUT_DIR_BOARD="$(REPEATED_BOARD_OUTPUT_DIR)/$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated_$(REPEATED_BOARD_REMOTE_TAG)/$$run"; \
	done

evidence_manifest_repeated:
	python3 script/generate_iss_3d_evidence_manifest.py \
		--summary-dir "$(REPEATED_BOARD_OUTPUT_DIR)" \
		--output "$(EVIDENCE_MANIFEST_REPEATED)" \
		--summary-output "$(REPEATED_BOARD_OUTPUT_DIR)/summary.md" \
		--title "$(REPEATED_BOARD_TITLE)" \
		--run-label "$(REPEATED_BOARD_RUN_LABEL)" \
		--evidence-role diagnostic \
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
		--case-filter "$(ISS3D_REPEATED_BENCH_ARGS)" \
		--evidence-role diagnostic \
		--doc-ref doc/phases/000-current-state-and-scatter-diagnostic/result.zh.md \
		--doc-ref doc/iss_3d-evaluation.zh.md \
		--freshness-state fresh

board_repeated_production:
	@if [ "$(ISS3D_ALLOW_HISTORICAL_PRODUCTION_PROBE)" != "1" ]; then \
		echo "[guard] ISS 3D production probe is historical/no-production; set ISS3D_ALLOW_HISTORICAL_PRODUCTION_PROBE=1 only after reintroducing a production probe patch." >&2; \
		exit 2; \
	fi
	@set -eu; \
	for i in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
			BENCH_ARGS="$(ISS3D_PRODUCTION_BENCH_ARGS)" \
			OUTPUT_DIR_BOARD="$(PRODUCTION_BOARD_OUTPUT_DIR)/$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated_$(PRODUCTION_BOARD_REMOTE_TAG)/$$run"; \
	done

evidence_manifest_production:
	python3 script/generate_iss_3d_evidence_manifest.py \
		--summary-dir "$(PRODUCTION_BOARD_OUTPUT_DIR)" \
		--output "$(EVIDENCE_MANIFEST_PRODUCTION)" \
		--summary-output "$(PRODUCTION_BOARD_OUTPUT_DIR)/summary.md" \
		--title "$(PRODUCTION_BOARD_TITLE)" \
		--run-label "$(PRODUCTION_BOARD_RUN_LABEL)" \
		--evidence-role production_public \
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
		--case-filter "$(ISS3D_PRODUCTION_BENCH_ARGS)" \
		--evidence-role production_public \
		--doc-ref doc/phases/010-production-integration/result.zh.md \
		--doc-ref doc/iss_3d-evaluation.zh.md \
		--freshness-state fresh

check_evidence_freshness:
	python3 ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob 'test-rvv/keypoints/iss_3d/log/board/repeated_phase000_scatter_f64/summary.md' \
		--scan-glob 'test-rvv/keypoints/iss_3d/log/board/repeated_phase000_scatter_f64/evidence_manifest.json' \
		--scan-glob 'test-rvv/keypoints/iss_3d/log/board/repeated_phase000_scatter_f64/evidence_doctor.md' \
		--scan-glob 'test-rvv/keypoints/iss_3d/log/board/repeated_phase000_scatter_f64/evidence_doctor.json' \
		--scan-glob 'test-rvv/keypoints/iss_3d/log/board/repeated_phase010_production_public/summary.md' \
		--scan-glob 'test-rvv/keypoints/iss_3d/log/board/repeated_phase010_production_public/evidence_manifest.json' \
		--scan-glob 'test-rvv/keypoints/iss_3d/log/board/repeated_phase010_production_public/evidence_doctor.md' \
		--scan-glob 'test-rvv/keypoints/iss_3d/log/board/repeated_phase010_production_public/evidence_doctor.json' \
		--scan-glob 'test-rvv/keypoints/iss_3d/log/board/repeated_phase010_production_public_reduction_shape/summary.md' \
		--scan-glob 'test-rvv/keypoints/iss_3d/log/board/repeated_phase010_production_public_reduction_shape/evidence_manifest.json' \
		--scan-glob 'test-rvv/keypoints/iss_3d/log/board/repeated_phase010_production_public_reduction_shape/evidence_doctor.md' \
		--scan-glob 'test-rvv/keypoints/iss_3d/log/board/repeated_phase010_production_public_reduction_shape/evidence_doctor.json' \
		--doc test-rvv/keypoints/iss_3d/doc/phases/000-current-state-and-scatter-diagnostic/result.zh.md \
		--doc test-rvv/keypoints/iss_3d/doc/phases/010-production-integration/result.zh.md \
		--doc test-rvv/keypoints/iss_3d/doc/iss_3d-evaluation.zh.md \
		--require-doc-ref \
		--fail-on never

.PHONY: run_qemu_smoke check_iss_3d_rvv_asm board_repeated evidence_manifest_repeated evidence_doctor_repeated record_evidence_state_repeated board_repeated_production evidence_manifest_production evidence_doctor_production record_evidence_state_production check_evidence_freshness
