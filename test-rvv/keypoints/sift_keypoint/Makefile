# =============================================================================
# PCL keypoints/sift_keypoint RVV diagnostic Makefile
#
# 本 topic 先把 `computeScaleSpace` / `findScaleSpaceExtrema` 的 hot loop 铺成
# 可测的 diagnostic boundary。QEMU 只用于 correctness 和日志形状；性能结论
# 以后仍以板卡 repeated summary 为准。
# =============================================================================

TOPIC  := sift_keypoint
MODULE := keypoints

FOCUS_DIR      := keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp
FILTER_REPORT  ?= log/filtered_sift_keypoint.log
ANALYZE_REPORT ?= log/analyze_sift_keypoint.log

TARGET_TEST     := test_sift_keypoint
TARGET_TEST_STD := test_sift_keypoint_std
TARGET_TEST_RVV := test_sift_keypoint_rvv
SRCS_TEST       := src/test_sift_keypoint.cpp

TARGET_BENCH     := bench_sift_keypoint
TARGET_BENCH_STD := bench_sift_keypoint_std
TARGET_BENCH_RVV := bench_sift_keypoint_rvv
SRCS_BENCH       := src/bench_sift_keypoint.cpp

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/keypoints/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/search/include \
	-I$(PCL_SOURCE_ROOT)/kdtree/include \
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

REMOTE_DIR ?= /root/pcl-test/keypoints/sift_keypoint

REPEATED_BOARD_RUNS ?= 5
REPEATED_BOARD_TAG ?= phase000_scale_space_profile_prerequisite
REPEATED_BOARD_TITLE ?= SIFT scale-space profile prerequisite repeated board summary
REPEATED_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated_$(REPEATED_BOARD_TAG)
REPEATED_BOARD_REMOTE_TAG ?= phase000_scale_space_profile_prerequisite
REPEATED_BOARD_RUN_LABEL ?= sift_keypoint_phase000_scale_space_profile_prerequisite_repeated
SIFT_REPEATED_BENCH_ARGS ?= --case-filter all --iterations 50 --warmup-iterations 5
REPEATED_EVIDENCE_ROLE ?= diagnostic
REPEATED_CASE_KIND ?= diagnostic
REPEATED_AB_BOUNDARY ?= test_helper
REPEATED_POINT_TYPE ?= synthetic float neighborhood batches
REPEATED_TIMER_BOUNDARY ?= computeScaleSpace synthetic batch only; excludes radiusSearch, nearestKSearch and public dispatch
REPEATED_BASELINE_ASM_BOUNDARY ?= scalar helper fallback; no production dispatch expected
REPEATED_ASM_BOUNDARY ?= sift_keypoint test-support candidate / rvv candidate
REPEATED_EVIDENCE_NOTES ?= Diagnostic only; current phase asks whether the Gaussian weight loop and local extrema scan are hot enough to justify the next implementation phase.
EVIDENCE_MANIFEST_REPEATED ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_manifest.json
EVIDENCE_DOCTOR_REPEATED_MD ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.md
EVIDENCE_DOCTOR_REPEATED_JSON ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.json
EVIDENCE_REGISTRY ?= $(LOG_DIR)/evidence_registry.json

PUBLIC_BOARD_TAG ?= phase010_public_compute
PUBLIC_BOARD_TITLE ?= SIFT public compute repeated board summary
PUBLIC_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated_$(PUBLIC_BOARD_TAG)
PUBLIC_BOARD_REMOTE_TAG ?= phase010_public_compute
PUBLIC_BOARD_RUN_LABEL ?= sift_keypoint_phase010_public_compute_repeated
SIFT_PUBLIC_BENCH_ARGS ?= --mode public --case-filter public_sift_keypoint_320x240 --iterations 5 --warmup-iterations 1
PUBLIC_EVIDENCE_ROLE ?= production_public
PUBLIC_CASE_KIND ?= production_public
PUBLIC_AB_BOUNDARY ?= public_entry
PUBLIC_POINT_TYPE ?= PointXYZI organized cloud
PUBLIC_TIMER_BOUNDARY ?= public SIFTKeypoint::compute() on organized dense input; includes downsample, radiusSearch, scale-space, extrema and output assembly
PUBLIC_BASELINE_ASM_BOUNDARY ?= scalar public compute fallback; no production dispatch expected
PUBLIC_ASM_BOUNDARY ?= sift_keypoint production candidate / rvv computeScaleSpace
PUBLIC_EVIDENCE_NOTES ?= Production public compute evidence after RVV scale-space patch; public API unchanged.
EVIDENCE_MANIFEST_PUBLIC ?= $(PUBLIC_BOARD_OUTPUT_DIR)/evidence_manifest.json
EVIDENCE_DOCTOR_PUBLIC_MD ?= $(PUBLIC_BOARD_OUTPUT_DIR)/evidence_doctor.md
EVIDENCE_DOCTOR_PUBLIC_JSON ?= $(PUBLIC_BOARD_OUTPUT_DIR)/evidence_doctor.json
PUBLIC_TRACE_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/public_trace_$(PUBLIC_BOARD_TAG)
PUBLIC_TRACE_REMOTE_DIR ?= $(REMOTE_DIR)/log/board/public_trace_$(PUBLIC_BOARD_REMOTE_TAG)
PUBLIC_TRACE_ARGS ?= --mode public --case-filter public_sift_keypoint_320x240 --iterations 1 --warmup-iterations 0 --emit-public-trace
PUBLIC_TRACE_COMPARE_MD ?= $(PUBLIC_TRACE_OUTPUT_DIR)/public_trace_compare.md
PUBLIC_TRACE_COMPARE_JSON ?= $(PUBLIC_TRACE_OUTPUT_DIR)/public_trace_compare.json
PUBLIC_TRACE_TOLERANCE ?= 1e-5
PUBLIC_TRACE_RUN_LABEL ?= sift_keypoint_phase010_public_compute_public_trace

include ../../mk/rvv-topic.mk

run_qemu_smoke: run_test_compare run_bench_rvv

check_sift_keypoint_rvv_asm: dump_bench_rvv
	@grep -Eq 'expf_RVV_f32m2|vfredosum|vle32|vsetvl' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@echo "[PASS] sift_keypoint RVV asm gate matched expected Gaussian/RVV instruction patterns."

board_repeated:
	@set -eu; \
	for i in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
			BENCH_ARGS="$(SIFT_REPEATED_BENCH_ARGS)" \
			OUTPUT_DIR_BOARD="$(REPEATED_BOARD_OUTPUT_DIR)/$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated_$(REPEATED_BOARD_REMOTE_TAG)/$$run"; \
	done

evidence_manifest_repeated:
	python3 script/generate_sift_keypoint_evidence_manifest.py \
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
		--case-filter "$(SIFT_REPEATED_BENCH_ARGS)" \
		--evidence-role "$(REPEATED_EVIDENCE_ROLE)" \
		--doc-ref doc/phases/000-current-state-and-gaps/result.zh.md \
		--doc-ref doc/sift_keypoint-evaluation.zh.md \
		--freshness-state fresh

board_repeated_public:
	@set -eu; \
	for i in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
			BENCH_ARGS="$(SIFT_PUBLIC_BENCH_ARGS)" \
			OUTPUT_DIR_BOARD="$(PUBLIC_BOARD_OUTPUT_DIR)/$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated_$(PUBLIC_BOARD_REMOTE_TAG)/$$run"; \
	done

evidence_manifest_public:
	python3 script/generate_sift_keypoint_evidence_manifest.py \
		--summary-dir "$(PUBLIC_BOARD_OUTPUT_DIR)" \
		--output "$(EVIDENCE_MANIFEST_PUBLIC)" \
		--summary-output "$(PUBLIC_BOARD_OUTPUT_DIR)/summary.md" \
		--title "$(PUBLIC_BOARD_TITLE)" \
		--run-label "$(PUBLIC_BOARD_RUN_LABEL)" \
		--expected-runs "$(REPEATED_BOARD_RUNS)" \
		--asm "$(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm" \
		--summary-role "$(PUBLIC_EVIDENCE_ROLE)" \
		--evidence-role "$(PUBLIC_EVIDENCE_ROLE)" \
		--case-kind "$(PUBLIC_CASE_KIND)" \
		--ab-boundary "$(PUBLIC_AB_BOUNDARY)" \
		--point-type "$(PUBLIC_POINT_TYPE)" \
		--timer-boundary "$(PUBLIC_TIMER_BOUNDARY)" \
		--baseline-asm-boundary "$(PUBLIC_BASELINE_ASM_BOUNDARY)" \
		--asm-boundary "$(PUBLIC_ASM_BOUNDARY)" \
		--notes "$(PUBLIC_EVIDENCE_NOTES)"

evidence_doctor_public: evidence_manifest_public
	python3 ../../script/evidence_doctor.py \
		--manifest "$(EVIDENCE_MANIFEST_PUBLIC)" \
		--output "$(EVIDENCE_DOCTOR_PUBLIC_MD)" \
		--json-output "$(EVIDENCE_DOCTOR_PUBLIC_JSON)" \
		--fail-on never

record_evidence_state_public: evidence_doctor_public
	python3 ../../script/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(PUBLIC_BOARD_OUTPUT_DIR)/summary.md" \
		--manifest "$(EVIDENCE_MANIFEST_PUBLIC)" \
		--doctor "$(EVIDENCE_DOCTOR_PUBLIC_MD)" \
		--path "$(EVIDENCE_DOCTOR_PUBLIC_JSON)" \
		--target board_repeated_public \
		--backend board \
		--run-label "$(PUBLIC_BOARD_RUN_LABEL)" \
		--case-filter "$(SIFT_PUBLIC_BENCH_ARGS)" \
		--evidence-role "$(PUBLIC_EVIDENCE_ROLE)" \
		--doc-ref doc/phases/010-production-full-gaussian-rvv/result.zh.md \
		--doc-ref doc/sift_keypoint-evaluation.zh.md \
		--doc-ref ../../../doc-rvv/keypoints/sift_keypoint-RVV.zh.md \
		--freshness-state fresh

board_public_trace:
	$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
		BENCH_ARGS="$(PUBLIC_TRACE_ARGS)" \
		OUTPUT_DIR_BOARD="$(PUBLIC_TRACE_OUTPUT_DIR)" \
		REMOTE_BOARD_OUTPUT_DIR="$(PUBLIC_TRACE_REMOTE_DIR)"

compare_public_trace:
	python3 script/compare_sift_public_outputs.py \
		--std-log "$(PUBLIC_TRACE_OUTPUT_DIR)/run_bench_std.log" \
		--rvv-log "$(PUBLIC_TRACE_OUTPUT_DIR)/run_bench_rvv.log" \
		--case "public_sift_keypoint_320x240" \
		--tolerance "$(PUBLIC_TRACE_TOLERANCE)" \
		--output "$(PUBLIC_TRACE_COMPARE_MD)" \
		--json-output "$(PUBLIC_TRACE_COMPARE_JSON)"

record_public_trace_state: compare_public_trace
	python3 ../../script/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(PUBLIC_TRACE_COMPARE_MD)" \
		--path "$(PUBLIC_TRACE_COMPARE_JSON)" \
		--target compare_public_trace \
		--backend board \
		--run-label "$(PUBLIC_TRACE_RUN_LABEL)" \
		--case-filter "$(PUBLIC_TRACE_ARGS)" \
		--evidence-role "$(PUBLIC_EVIDENCE_ROLE)" \
		--doc-ref doc/phases/010-production-full-gaussian-rvv/result.zh.md \
		--doc-ref doc/sift_keypoint-evaluation.zh.md \
		--doc-ref ../../../doc-rvv/keypoints/sift_keypoint-RVV.zh.md \
		--freshness-state fresh

check_evidence_freshness:
	python3 ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob 'test-rvv/keypoints/sift_keypoint/log/board/repeated_phase000_scale_space_profile_prerequisite/summary.md' \
		--scan-glob 'test-rvv/keypoints/sift_keypoint/log/board/repeated_phase000_scale_space_profile_prerequisite/evidence_manifest.json' \
		--scan-glob 'test-rvv/keypoints/sift_keypoint/log/board/repeated_phase000_scale_space_profile_prerequisite/evidence_doctor.md' \
		--scan-glob 'test-rvv/keypoints/sift_keypoint/log/board/repeated_phase000_scale_space_profile_prerequisite/evidence_doctor.json' \
		--scan-glob 'test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/summary.md' \
		--scan-glob 'test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/evidence_manifest.json' \
		--scan-glob 'test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/evidence_doctor.md' \
		--scan-glob 'test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/evidence_doctor.json' \
		--scan-glob 'test-rvv/keypoints/sift_keypoint/log/board/public_trace_phase010_public_compute/public_trace_compare.md' \
		--scan-glob 'test-rvv/keypoints/sift_keypoint/log/board/public_trace_phase010_public_compute/public_trace_compare.json' \
		--doc test-rvv/keypoints/sift_keypoint/doc/phases/000-current-state-and-gaps/result.zh.md \
		--doc test-rvv/keypoints/sift_keypoint/doc/phases/010-production-full-gaussian-rvv/result.zh.md \
		--doc test-rvv/keypoints/sift_keypoint/doc/sift_keypoint-evaluation.zh.md \
		--doc doc-rvv/keypoints/sift_keypoint-RVV.zh.md \
		--require-doc-ref \
		--fail-on never

.PHONY: run_qemu_smoke check_sift_keypoint_rvv_asm board_repeated evidence_manifest_repeated evidence_doctor_repeated record_evidence_state_repeated board_repeated_public evidence_manifest_public evidence_doctor_public record_evidence_state_public board_public_trace compare_public_trace record_public_trace_state check_evidence_freshness
