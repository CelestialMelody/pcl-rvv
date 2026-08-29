# =============================================================================
# PCL recognition/hough_3d RVV topic Makefile
#
# 本 topic 先做 Hough3DGrouping::houghVoting() 的 vote generation 诊断，
# 以便把 scene vote 生成、min/max reduction 和保序候选索引拆成可测 helper。
# QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状；性能结论
# 必须来自 board（板卡）或目标硬件 repeated benchmark（重复性能测试）。
# =============================================================================

TOPIC  := hough_3d
MODULE := recognition
TOPIC_DOC_PREFIX := test-rvv/recognition/hough_3d

FOCUS_DIR      := pcl/recognition/include/pcl/recognition/impl/cg/hough_3d.hpp
FILTER_REPORT  := log/filtered_hough_3d.log
ANALYZE_REPORT := log/analyze_hough_3d.log

TARGET_TEST     := test_hough_3d
TARGET_TEST_STD := test_hough_3d_std
TARGET_TEST_RVV := test_hough_3d_rvv
SRCS_TEST       := src/test_hough_3d.cpp

TARGET_BENCH     := bench_hough_3d
TARGET_BENCH_STD := bench_hough_3d_std
TARGET_BENCH_RVV := bench_hough_3d_rvv
SRCS_BENCH       := src/bench_hough_3d.cpp
BENCH_ARGS       ?= 65536 200 5 0.03 10 1

REMOTE_DIR ?= /root/pcl-test/recognition/hough_3d
REMOTE_BENCH_ARGS ?= $(BENCH_ARGS)
REMOTE_TEST_ARGS ?=

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

LIBS_TEST  := -lpcl_recognition -lpcl_common -lgtest -lgtest_main -lpthread -llz4 -lm
LIBS_BENCH := -lpcl_recognition -lpcl_common -llz4 -lm

include ../../mk/rvv-topic.mk

PHASE000_REPEATED_BOARD_DIR ?= log/board/repeated_phase000_vote_generation_diagnostic
PHASE000_REPEATED_REMOTE_DIR ?= $(REMOTE_DIR)/log/board/repeated_phase000_vote_generation_diagnostic
PHASE000_REPEATED_RUNS ?= 01 02 03 04 05
PHASE000_EVIDENCE_SUMMARY ?= $(PHASE000_REPEATED_BOARD_DIR)/summary.md
PHASE000_EVIDENCE_MANIFEST ?= $(PHASE000_REPEATED_BOARD_DIR)/evidence_manifest.json
PHASE000_EVIDENCE_DOCTOR_MD ?= $(PHASE000_REPEATED_BOARD_DIR)/evidence_doctor.md
PHASE000_EVIDENCE_DOCTOR_JSON ?= $(PHASE000_REPEATED_BOARD_DIR)/evidence_doctor.json
PHASE010_PRODUCTION_BOARD_DIR ?= log/board/repeated_phase010_production_direct
PHASE010_PRODUCTION_REMOTE_DIR ?= $(REMOTE_DIR)/log/board/repeated_phase010_production_direct
PHASE010_PRODUCTION_RUNS ?= 01 02 03 04 05
PHASE010_BENCH_ARGS ?= 65536 30 3 0.03 10 1
PHASE010_EVIDENCE_SUMMARY ?= $(PHASE010_PRODUCTION_BOARD_DIR)/summary.md
PHASE010_EVIDENCE_MANIFEST ?= $(PHASE010_PRODUCTION_BOARD_DIR)/evidence_manifest.json
PHASE010_EVIDENCE_DOCTOR_MD ?= $(PHASE010_PRODUCTION_BOARD_DIR)/evidence_doctor.md
PHASE010_EVIDENCE_DOCTOR_JSON ?= $(PHASE010_PRODUCTION_BOARD_DIR)/evidence_doctor.json
PHASE020_NO_INTERP_BOARD_DIR ?= log/board/repeated_phase020_no_interpolation
PHASE020_NO_INTERP_REMOTE_DIR ?= $(REMOTE_DIR)/log/board/repeated_phase020_no_interpolation
PHASE020_NO_INTERP_RUNS ?= 01 02 03
PHASE020_NO_INTERP_BENCH_ARGS ?= 65536 30 3 0.03 10 1 0 1
PHASE020_NO_INTERP_EVIDENCE_SUMMARY ?= $(PHASE020_NO_INTERP_BOARD_DIR)/summary.md
PHASE020_NO_INTERP_EVIDENCE_MANIFEST ?= $(PHASE020_NO_INTERP_BOARD_DIR)/evidence_manifest.json
PHASE020_NO_INTERP_EVIDENCE_DOCTOR_MD ?= $(PHASE020_NO_INTERP_BOARD_DIR)/evidence_doctor.md
PHASE020_NO_INTERP_EVIDENCE_DOCTOR_JSON ?= $(PHASE020_NO_INTERP_BOARD_DIR)/evidence_doctor.json
PHASE030_DEFAULT_WEIGHT_BOARD_DIR ?= log/board/repeated_phase030_default_distance_weight
PHASE030_DEFAULT_WEIGHT_REMOTE_DIR ?= $(REMOTE_DIR)/log/board/repeated_phase030_default_distance_weight
PHASE030_DEFAULT_WEIGHT_RUNS ?= 01 02 03 04 05
PHASE030_DEFAULT_WEIGHT_BENCH_ARGS ?= 65536 30 3 0.03 10 1 1 0
PHASE030_DEFAULT_WEIGHT_EVIDENCE_SUMMARY ?= $(PHASE030_DEFAULT_WEIGHT_BOARD_DIR)/summary.md
PHASE030_DEFAULT_WEIGHT_EVIDENCE_MANIFEST ?= $(PHASE030_DEFAULT_WEIGHT_BOARD_DIR)/evidence_manifest.json
PHASE030_DEFAULT_WEIGHT_EVIDENCE_DOCTOR_MD ?= $(PHASE030_DEFAULT_WEIGHT_BOARD_DIR)/evidence_doctor.md
PHASE030_DEFAULT_WEIGHT_EVIDENCE_DOCTOR_JSON ?= $(PHASE030_DEFAULT_WEIGHT_BOARD_DIR)/evidence_doctor.json
EVIDENCE_REGISTRY ?= log/evidence_registry.json
EVIDENCE_DOC_REFS := \
	$(TOPIC_DOC_PREFIX)/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/hough_3d-evaluation.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/testing-overview.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/correctness-tests.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/benchmark-and-evidence.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-evidence.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/test-support-code-map.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-roadmap.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/optimization-matrix.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/000-current-state-and-vote-generation-diagnostic/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/000-current-state-and-vote-generation-diagnostic/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/010-production-direct-vote-generation-probe/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/010-production-direct-vote-generation-probe/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-accumulator-scatter-audit/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-accumulator-scatter-audit/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/030-default-distance-weight-production-direct/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/030-default-distance-weight-production-direct/result.zh.md
EVIDENCE_DOC_REF_ARGS := $(foreach doc,$(EVIDENCE_DOC_REFS),--doc-ref $(doc))
EVIDENCE_DOC_CHECK_ARGS := $(foreach doc,$(EVIDENCE_DOC_REFS),--doc $(doc))

.PHONY: run_qemu_smoke check_hough_3d_vote_generation_rvv_asm check_hough_3d_production_rvv_asm board_repeated board_production_repeated board_phase020_no_interpolation_repeated board_phase030_default_distance_weight_repeated generate_phase000_evidence_manifest evidence_doctor_repeated record_evidence_state_repeated generate_phase010_evidence_manifest evidence_doctor_production_repeated record_evidence_state_production_repeated generate_phase020_no_interpolation_evidence_manifest evidence_doctor_phase020_no_interpolation record_evidence_state_phase020_no_interpolation generate_phase030_default_distance_weight_evidence_manifest evidence_doctor_phase030_default_distance_weight record_evidence_state_phase030_default_distance_weight check_evidence_freshness

run_qemu_smoke: run_test_compare

check_hough_3d_vote_generation_rvv_asm: dump_bench_rvv
	@grep -Eq 'computeSceneVotesCandidate' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq '\bvlse32\.v\b' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq '\bvfmul\.vv\b' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq '\bvfadd\.vv\b' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@echo "[PASS] Hough3D vote-generation RVV asm gate matched strided load and fused vote arithmetic instructions."

check_hough_3d_production_rvv_asm: dump_bench_rvv
	@grep -Eq 'Hough3DGrouping<.*>::houghVoting' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq '\bvluxei32\.v\b' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq '\bvfmul\.vv\b' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@grep -Eq '\bvfadd\.vv\b' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	@echo "[PASS] Hough3D production houghVoting RVV asm gate matched indexed loads and vote arithmetic instructions."

board_repeated: check_hough_3d_vote_generation_rvv_asm
	rm -rf $(PHASE000_REPEATED_BOARD_DIR)
	@for run in $(PHASE000_REPEATED_RUNS); do \
		echo "[BOARD] Phase 000 vote-generation repeated run $$run"; \
		$(MAKE) -C $(CURDIR) board_smoke \
			OUTPUT_DIR_BOARD="$(PHASE000_REPEATED_BOARD_DIR)/run_$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(PHASE000_REPEATED_REMOTE_DIR)/run_$$run" \
			BENCH_ARGS="$(BENCH_ARGS)" \
			REMOTE_BENCH_ARGS="$(BENCH_ARGS)" \
			REMOTE_TEST_ARGS="$(REMOTE_TEST_ARGS)"; \
	done

board_production_repeated: check_hough_3d_production_rvv_asm
	rm -rf $(PHASE010_PRODUCTION_BOARD_DIR)
	@for run in $(PHASE010_PRODUCTION_RUNS); do \
		echo "[BOARD] Phase 010 production direct repeated run $$run"; \
		$(MAKE) -C $(CURDIR) board_smoke \
			OUTPUT_DIR_BOARD="$(PHASE010_PRODUCTION_BOARD_DIR)/run_$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(PHASE010_PRODUCTION_REMOTE_DIR)/run_$$run" \
			BENCH_ARGS="$(PHASE010_BENCH_ARGS)" \
			REMOTE_BENCH_ARGS="$(PHASE010_BENCH_ARGS)" \
			REMOTE_TEST_ARGS="$(REMOTE_TEST_ARGS)"; \
	done

board_phase020_no_interpolation_repeated: check_hough_3d_production_rvv_asm
	rm -rf $(PHASE020_NO_INTERP_BOARD_DIR)
	@for run in $(PHASE020_NO_INTERP_RUNS); do \
		echo "[BOARD] Phase 020 no-interpolation repeated run $$run"; \
		$(MAKE) -C $(CURDIR) board_smoke \
			OUTPUT_DIR_BOARD="$(PHASE020_NO_INTERP_BOARD_DIR)/run_$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(PHASE020_NO_INTERP_REMOTE_DIR)/run_$$run" \
			BENCH_ARGS="$(PHASE020_NO_INTERP_BENCH_ARGS)" \
			REMOTE_BENCH_ARGS="$(PHASE020_NO_INTERP_BENCH_ARGS)" \
			REMOTE_TEST_ARGS="$(REMOTE_TEST_ARGS)"; \
	done

board_phase030_default_distance_weight_repeated: check_hough_3d_production_rvv_asm
	rm -rf $(PHASE030_DEFAULT_WEIGHT_BOARD_DIR)
	@for run in $(PHASE030_DEFAULT_WEIGHT_RUNS); do \
		echo "[BOARD] Phase 030 default-distance-weight repeated run $$run"; \
		$(MAKE) -C $(CURDIR) board_smoke \
			OUTPUT_DIR_BOARD="$(PHASE030_DEFAULT_WEIGHT_BOARD_DIR)/run_$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(PHASE030_DEFAULT_WEIGHT_REMOTE_DIR)/run_$$run" \
			BENCH_ARGS="$(PHASE030_DEFAULT_WEIGHT_BENCH_ARGS)" \
			REMOTE_BENCH_ARGS="$(PHASE030_DEFAULT_WEIGHT_BENCH_ARGS)" \
			REMOTE_TEST_ARGS="$(REMOTE_TEST_ARGS)"; \
	done

generate_phase000_evidence_manifest:
	@$(PYTHON_RUN) script/generate_hough_3d_evidence_manifest.py \
		--input-dir "$(PHASE000_REPEATED_BOARD_DIR)" \
		--run-label hough_3d_phase000_vote_generation_repeated \
		--summary "$(PHASE000_EVIDENCE_SUMMARY)" \
		--manifest "$(PHASE000_EVIDENCE_MANIFEST)" \
		--asm "$(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm"

generate_phase010_evidence_manifest:
	@$(PYTHON_RUN) script/generate_hough_3d_evidence_manifest.py \
		--input-dir "$(PHASE010_PRODUCTION_BOARD_DIR)" \
		--run-label hough_3d_phase010_production_direct_repeated \
		--summary "$(PHASE010_EVIDENCE_SUMMARY)" \
		--manifest "$(PHASE010_EVIDENCE_MANIFEST)" \
		--asm "$(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm" \
		--row-label "Hough3D production houghVoting" \
		--case-name hough_3d_production_hough_voting \
		--evidence-role production_direct \
		--group hough_3d_phase010_production_direct \
		--title "Hough3D Phase 010 Production Direct Board Summary" \
		--timer-boundary "Hough3DGrouping::houghVoting production boundary; includes scene vote generation, HoughSpace construction, and voteInt scatter; excludes findMaxima and RANSAC filtering." \
		--gate "PointXYZ / ReferenceFrame production instantiation; RVV build must hit the houghVoting vote-generation RVV path." \
		--asm-boundary "pcl::Hough3DGrouping<pcl::PointXYZ, pcl::PointXYZ, pcl::ReferenceFrame, pcl::ReferenceFrame>::houghVoting" \
		--notes "Production row compares Std build scalar houghVoting with RVV build houghVoting after the vote-generation RVV patch; accumulator scatter remains scalar in both builds."

generate_phase020_no_interpolation_evidence_manifest:
	@$(PYTHON_RUN) script/generate_hough_3d_evidence_manifest.py \
		--input-dir "$(PHASE020_NO_INTERP_BOARD_DIR)" \
		--run-label hough_3d_phase020_no_interpolation_repeated \
		--summary "$(PHASE020_NO_INTERP_EVIDENCE_SUMMARY)" \
		--manifest "$(PHASE020_NO_INTERP_EVIDENCE_MANIFEST)" \
		--asm "$(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm" \
		--row-label "Hough3D production houghVoting" \
		--case-name hough_3d_no_interpolation_hough_voting \
		--evidence-role production_direct \
		--group hough_3d_phase020_no_interpolation \
		--title "Hough3D Phase 020 No-Interpolation Board Summary" \
		--timer-boundary "Hough3DGrouping::houghVoting production boundary with use_interpolation=false; includes scene vote generation, HoughSpace construction, and vote scatter; excludes findMaxima and RANSAC filtering." \
		--gate "PointXYZ / ReferenceFrame production instantiation; RVV build must hit the houghVoting vote-generation RVV path; interpolation disabled for scatter ablation." \
		--asm-boundary "pcl::Hough3DGrouping<pcl::PointXYZ, pcl::PointXYZ, pcl::ReferenceFrame, pcl::ReferenceFrame>::houghVoting" \
		--notes "No-interpolation row checks whether the simpler HoughSpace::vote path exposes vote-generation RVV benefit once voteInt's 27-neighbor interpolation is removed."

generate_phase030_default_distance_weight_evidence_manifest:
	@$(PYTHON_RUN) script/generate_hough_3d_evidence_manifest.py \
		--input-dir "$(PHASE030_DEFAULT_WEIGHT_BOARD_DIR)" \
		--run-label hough_3d_phase030_default_distance_weight_repeated \
		--summary "$(PHASE030_DEFAULT_WEIGHT_EVIDENCE_SUMMARY)" \
		--manifest "$(PHASE030_DEFAULT_WEIGHT_EVIDENCE_MANIFEST)" \
		--asm "$(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm" \
		--row-label "Hough3D production houghVoting" \
		--case-name hough_3d_default_distance_weight_hough_voting \
		--evidence-role production_direct \
		--group hough_3d_phase030_default_distance_weight \
		--title "Hough3D Phase 030 Default Distance Weight Board Summary" \
		--timer-boundary "Hough3DGrouping::houghVoting production boundary with use_interpolation=true and use_distance_weight=false; includes scene vote generation, HoughSpace construction, and voteInt scatter; excludes findMaxima and RANSAC filtering." \
		--gate "PointXYZ / ReferenceFrame production instantiation; RVV build must hit the houghVoting vote-generation RVV path; distance weighting disabled to match the class default." \
		--asm-boundary "pcl::Hough3DGrouping<pcl::PointXYZ, pcl::PointXYZ, pcl::ReferenceFrame, pcl::ReferenceFrame>::houghVoting" \
		--notes "Default-distance-weight row checks the class default use_distance_weight=false boundary so phase 010's non-default weighted result is not over-generalized."

evidence_doctor_repeated: generate_phase000_evidence_manifest
	@$(PYTHON_RUN) $(TEST_RVV_SHARED_SCRIPT_DIR)/evidence_doctor.py \
		--manifest "$(PHASE000_EVIDENCE_MANIFEST)" \
		--output "$(PHASE000_EVIDENCE_DOCTOR_MD)" \
		--json-output "$(PHASE000_EVIDENCE_DOCTOR_JSON)" \
		--fail-on never

evidence_doctor_production_repeated: generate_phase010_evidence_manifest
	@$(PYTHON_RUN) $(TEST_RVV_SHARED_SCRIPT_DIR)/evidence_doctor.py \
		--manifest "$(PHASE010_EVIDENCE_MANIFEST)" \
		--output "$(PHASE010_EVIDENCE_DOCTOR_MD)" \
		--json-output "$(PHASE010_EVIDENCE_DOCTOR_JSON)" \
		--fail-on never

evidence_doctor_phase020_no_interpolation: generate_phase020_no_interpolation_evidence_manifest
	@$(PYTHON_RUN) $(TEST_RVV_SHARED_SCRIPT_DIR)/evidence_doctor.py \
		--manifest "$(PHASE020_NO_INTERP_EVIDENCE_MANIFEST)" \
		--output "$(PHASE020_NO_INTERP_EVIDENCE_DOCTOR_MD)" \
		--json-output "$(PHASE020_NO_INTERP_EVIDENCE_DOCTOR_JSON)" \
		--fail-on never

evidence_doctor_phase030_default_distance_weight: generate_phase030_default_distance_weight_evidence_manifest
	@$(PYTHON_RUN) $(TEST_RVV_SHARED_SCRIPT_DIR)/evidence_doctor.py \
		--manifest "$(PHASE030_DEFAULT_WEIGHT_EVIDENCE_MANIFEST)" \
		--output "$(PHASE030_DEFAULT_WEIGHT_EVIDENCE_DOCTOR_MD)" \
		--json-output "$(PHASE030_DEFAULT_WEIGHT_EVIDENCE_DOCTOR_JSON)" \
		--fail-on never

record_evidence_state_repeated: evidence_doctor_repeated
	@$(PYTHON_RUN) $(TEST_RVV_SHARED_SCRIPT_DIR)/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(PHASE000_EVIDENCE_SUMMARY)" \
		--manifest "$(PHASE000_EVIDENCE_MANIFEST)" \
		--doctor "$(PHASE000_EVIDENCE_DOCTOR_MD)" \
		--path "$(PHASE000_EVIDENCE_DOCTOR_JSON)" \
		--target board_repeated \
		--backend board \
		--run-label hough_3d_phase000_vote_generation_repeated \
		--case-filter "$(BENCH_ARGS)" \
		--evidence-role production_shaped_diagnostic \
		$(EVIDENCE_DOC_REF_ARGS) \
		--freshness-state fresh

record_evidence_state_production_repeated: evidence_doctor_production_repeated
	@$(PYTHON_RUN) $(TEST_RVV_SHARED_SCRIPT_DIR)/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(PHASE010_EVIDENCE_SUMMARY)" \
		--manifest "$(PHASE010_EVIDENCE_MANIFEST)" \
		--doctor "$(PHASE010_EVIDENCE_DOCTOR_MD)" \
		--path "$(PHASE010_EVIDENCE_DOCTOR_JSON)" \
		--target board_production_repeated \
		--backend board \
		--run-label hough_3d_phase010_production_direct_repeated \
		--case-filter "$(PHASE010_BENCH_ARGS)" \
		--evidence-role production_direct \
		$(EVIDENCE_DOC_REF_ARGS) \
		--freshness-state fresh

record_evidence_state_phase020_no_interpolation: evidence_doctor_phase020_no_interpolation
	@$(PYTHON_RUN) $(TEST_RVV_SHARED_SCRIPT_DIR)/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(PHASE020_NO_INTERP_EVIDENCE_SUMMARY)" \
		--manifest "$(PHASE020_NO_INTERP_EVIDENCE_MANIFEST)" \
		--doctor "$(PHASE020_NO_INTERP_EVIDENCE_DOCTOR_MD)" \
		--path "$(PHASE020_NO_INTERP_EVIDENCE_DOCTOR_JSON)" \
		--target board_phase020_no_interpolation_repeated \
		--backend board \
		--run-label hough_3d_phase020_no_interpolation_repeated \
		--case-filter "$(PHASE020_NO_INTERP_BENCH_ARGS)" \
		--evidence-role production_direct \
		$(EVIDENCE_DOC_REF_ARGS) \
		--freshness-state fresh

record_evidence_state_phase030_default_distance_weight: evidence_doctor_phase030_default_distance_weight
	@$(PYTHON_RUN) $(TEST_RVV_SHARED_SCRIPT_DIR)/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(PHASE030_DEFAULT_WEIGHT_EVIDENCE_SUMMARY)" \
		--manifest "$(PHASE030_DEFAULT_WEIGHT_EVIDENCE_MANIFEST)" \
		--doctor "$(PHASE030_DEFAULT_WEIGHT_EVIDENCE_DOCTOR_MD)" \
		--path "$(PHASE030_DEFAULT_WEIGHT_EVIDENCE_DOCTOR_JSON)" \
		--target board_phase030_default_distance_weight_repeated \
		--backend board \
		--run-label hough_3d_phase030_default_distance_weight_repeated \
		--case-filter "$(PHASE030_DEFAULT_WEIGHT_BENCH_ARGS)" \
		--evidence-role production_direct \
		$(EVIDENCE_DOC_REF_ARGS) \
		--freshness-state fresh

check_evidence_freshness:
	@$(PYTHON_RUN) $(TEST_RVV_SHARED_SCRIPT_DIR)/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob "$(PHASE000_REPEATED_BOARD_DIR)/**/*.md" \
		--scan-glob "$(PHASE000_REPEATED_BOARD_DIR)/**/*.json" \
		--scan-glob "$(PHASE010_PRODUCTION_BOARD_DIR)/**/*.md" \
		--scan-glob "$(PHASE010_PRODUCTION_BOARD_DIR)/**/*.json" \
		--scan-glob "$(PHASE020_NO_INTERP_BOARD_DIR)/**/*.md" \
		--scan-glob "$(PHASE020_NO_INTERP_BOARD_DIR)/**/*.json" \
		--scan-glob "$(PHASE030_DEFAULT_WEIGHT_BOARD_DIR)/**/*.md" \
		--scan-glob "$(PHASE030_DEFAULT_WEIGHT_BOARD_DIR)/**/*.json" \
		$(EVIDENCE_DOC_CHECK_ARGS) \
		--require-doc-ref \
		--fail-on any
