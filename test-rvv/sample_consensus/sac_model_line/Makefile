# =============================================================================
# PCL sample_consensus/sac_model_line RVV topic Makefile
#
# 本 topic 使用共享 rvv-topic.mk；QEMU 只跑 correctness / log-shape
# （正确性 / 日志形状），性能结论只来自 board 或目标硬件。
# =============================================================================

TOPIC  := sac_model_line
MODULE := sample_consensus
TOPIC_DOC_PREFIX := test-rvv/sample_consensus/sac_model_line

EIGEN_RVV_FLAGS := -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=2 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl

FOCUS_DIR      := pcl/sample_consensus/include/pcl/sample_consensus
FILTER_REPORT  := log/filtered_sac_model_line.log
ANALYZE_REPORT := log/analyze_sac_model_line.log

TARGET_TEST     := test_sac_model_line
TARGET_TEST_STD := test_sac_model_line_std
TARGET_TEST_RVV := test_sac_model_line_rvv
SRCS_TEST       := src/test_sac_model_line.cpp

TARGET_BENCH     := bench_sac_model_line
TARGET_BENCH_STD := bench_sac_model_line_std
TARGET_BENCH_RVV := bench_sac_model_line_rvv
TARGET_BENCH_RVV_GATHER_BASELINE := bench_sac_model_line_rvv_gather_baseline
SRCS_BENCH       := src/bench_sac_model_line.cpp
BENCH_ARGS       ?= 65536 200
BENCH_IDENTITY_ARGS ?= 65536 200 identity
BENCH_SHUFFLED_ARGS ?= 65536 200 shuffled

REMOTE_DIR ?= /root/pcl-test/sample_consensus/sac_model_line
REMOTE_BENCH_ARGS ?= $(BENCH_ARGS)
REMOTE_TEST_ARGS ?=
REMOTE_BENCH_RVV_IDENTITY := bench_sac_model_line_rvv
REMOTE_BENCH_RVV_GATHER_BASELINE := bench_sac_model_line_rvv_gather_baseline

LINE_COUNT_FILTER := SampleConsensusModelLine.CountCandidateMatchesPublicEntryOnShuffledIndices
LINE_SELECT_FILTER := SampleConsensusModelLine.SelectCandidateMatchesPublicEntryOnShuffledIndices:SampleConsensusModelLine.SelectCandidateMatchesPublicEntryOnBenchScaleInput
LINE_GET_DISTANCES_FILTER := SampleConsensusModelLine.GetDistancesCandidateMatchesPublicEntryOnShuffledIndices:SampleConsensusModelLine.GetDistancesCandidateMatchesPublicEntryOnBenchScaleInput:SampleConsensusModelLine.GetDistancesCandidatePreservesOutputForInvalidModel
LINE_GET_DISTANCES_VFSQRT_FILTER := SampleConsensusModelLine.GetDistancesVFSqrtCandidateMatchesPublicEntryOnBenchScaleInput
PHASE000_DIR := doc/phases/000-line-count-diagnostic
PHASE010_DIR := doc/phases/010-line-select-diagnostic
PHASE020_DIR := doc/phases/020-line-get-distances-diagnostic
PHASE030_DIR := doc/phases/030-line-get-distances-vfsqrt-diagnostic
PHASE040_DIR := doc/phases/040-line-production-integration
PHASE050_DIR := doc/phases/050-line-get-distances-vse64-store
PHASE060_DIR := doc/phases/060-line-select-vse64-compressed-store
PHASE070_DIR := doc/phases/070-line-identity-index-strided-load
REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase000
REPEATED_BOARD_SELECT_DIR ?= log/board/repeated-20260828-phase010
REPEATED_BOARD_GET_DISTANCES_DIR ?= log/board/repeated-20260828-phase020
REPEATED_BOARD_GET_DISTANCES_VFSQRT_DIR ?= log/board/repeated-20260828-phase030
REPEATED_BOARD_PRODUCTION_DIR ?= log/board/repeated-20260828-phase040-production
REPEATED_BOARD_PRODUCTION_VSE64_DIR ?= log/board/repeated-20260828-phase050-production-vse64
REPEATED_BOARD_PRODUCTION_SELECT_VSE64_DIR ?= log/board/repeated-20260828-phase060-production-select-vse64
IDENTITY_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase070-identity-rvv-ab
IDENTITY_SHUFFLED_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase070-shuffled-rvv-ab
ALLOW_PHASE070_IDENTITY_REFRESH ?= 0
REPEATED_EVIDENCE_MANIFEST := $(PHASE000_DIR)/repeated-evidence-manifest.json
REPEATED_EVIDENCE_DOCTOR_MD := $(PHASE000_DIR)/repeated-evidence-doctor.md
REPEATED_EVIDENCE_DOCTOR_JSON := $(PHASE000_DIR)/repeated-evidence-doctor.json
REPEATED_SELECT_EVIDENCE_MANIFEST := $(PHASE010_DIR)/repeated-evidence-manifest.json
REPEATED_SELECT_EVIDENCE_DOCTOR_MD := $(PHASE010_DIR)/repeated-evidence-doctor.md
REPEATED_SELECT_EVIDENCE_DOCTOR_JSON := $(PHASE010_DIR)/repeated-evidence-doctor.json
REPEATED_GET_DISTANCES_EVIDENCE_MANIFEST := $(PHASE020_DIR)/repeated-evidence-manifest.json
REPEATED_GET_DISTANCES_EVIDENCE_DOCTOR_MD := $(PHASE020_DIR)/repeated-evidence-doctor.md
REPEATED_GET_DISTANCES_EVIDENCE_DOCTOR_JSON := $(PHASE020_DIR)/repeated-evidence-doctor.json
REPEATED_GET_DISTANCES_VFSQRT_EVIDENCE_MANIFEST := $(PHASE030_DIR)/repeated-evidence-manifest.json
REPEATED_GET_DISTANCES_VFSQRT_EVIDENCE_DOCTOR_MD := $(PHASE030_DIR)/repeated-evidence-doctor.md
REPEATED_GET_DISTANCES_VFSQRT_EVIDENCE_DOCTOR_JSON := $(PHASE030_DIR)/repeated-evidence-doctor.json
REPEATED_PRODUCTION_EVIDENCE_MANIFEST := $(PHASE040_DIR)/production-repeated-evidence-manifest.json
REPEATED_PRODUCTION_EVIDENCE_DOCTOR_MD := $(PHASE040_DIR)/production-repeated-evidence-doctor.md
REPEATED_PRODUCTION_EVIDENCE_DOCTOR_JSON := $(PHASE040_DIR)/production-repeated-evidence-doctor.json
REPEATED_PRODUCTION_VSE64_EVIDENCE_MANIFEST := $(PHASE050_DIR)/production-repeated-evidence-manifest.json
REPEATED_PRODUCTION_VSE64_EVIDENCE_DOCTOR_MD := $(PHASE050_DIR)/production-repeated-evidence-doctor.md
REPEATED_PRODUCTION_VSE64_EVIDENCE_DOCTOR_JSON := $(PHASE050_DIR)/production-repeated-evidence-doctor.json
REPEATED_PRODUCTION_SELECT_VSE64_EVIDENCE_MANIFEST := $(PHASE060_DIR)/production-repeated-evidence-manifest.json
REPEATED_PRODUCTION_SELECT_VSE64_EVIDENCE_DOCTOR_MD := $(PHASE060_DIR)/production-repeated-evidence-doctor.md
REPEATED_PRODUCTION_SELECT_VSE64_EVIDENCE_DOCTOR_JSON := $(PHASE060_DIR)/production-repeated-evidence-doctor.json
IDENTITY_EVIDENCE_MANIFEST := $(PHASE070_DIR)/identity-repeated-evidence-manifest.json
IDENTITY_EVIDENCE_DOCTOR_MD := $(PHASE070_DIR)/identity-repeated-evidence-doctor.md
IDENTITY_EVIDENCE_DOCTOR_JSON := $(PHASE070_DIR)/identity-repeated-evidence-doctor.json
IDENTITY_SHUFFLED_EVIDENCE_MANIFEST := $(PHASE070_DIR)/identity-shuffled-repeated-evidence-manifest.json
IDENTITY_SHUFFLED_EVIDENCE_DOCTOR_MD := $(PHASE070_DIR)/identity-shuffled-repeated-evidence-doctor.md
IDENTITY_SHUFFLED_EVIDENCE_DOCTOR_JSON := $(PHASE070_DIR)/identity-shuffled-repeated-evidence-doctor.json
EVIDENCE_REGISTRY := log/evidence_registry.json
REPEATED_EVIDENCE_RUN_LABEL := line-phase000-repeated-board
REPEATED_SELECT_EVIDENCE_RUN_LABEL := line-phase010-select-repeated-board
REPEATED_GET_DISTANCES_EVIDENCE_RUN_LABEL := line-phase020-get-distances-repeated-board
REPEATED_GET_DISTANCES_VFSQRT_EVIDENCE_RUN_LABEL := line-phase030-get-distances-vfsqrt-repeated-board
REPEATED_PRODUCTION_EVIDENCE_RUN_LABEL := line-phase040-production-repeated-board
REPEATED_PRODUCTION_VSE64_EVIDENCE_RUN_LABEL := line-phase050-production-vse64-repeated-board
REPEATED_PRODUCTION_SELECT_VSE64_EVIDENCE_RUN_LABEL := line-phase060-production-select-vse64-repeated-board
IDENTITY_EVIDENCE_RUN_LABEL := line-phase070-identity-rvv-ab-repeated-board
IDENTITY_SHUFFLED_EVIDENCE_RUN_LABEL := line-phase070-shuffled-rvv-ab-repeated-board
EVIDENCE_DOC_REFS := \
	$(TOPIC_DOC_PREFIX)/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/sac_model_line-evaluation.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/testing-overview.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/correctness-tests.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/benchmark-and-evidence.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-evidence.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/test-support-code-map.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-roadmap.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/optimization-matrix.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/000-line-count-diagnostic/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/000-line-count-diagnostic/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/010-line-select-diagnostic/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/010-line-select-diagnostic/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-line-get-distances-diagnostic/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-line-get-distances-diagnostic/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/030-line-get-distances-vfsqrt-diagnostic/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/030-line-get-distances-vfsqrt-diagnostic/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/040-line-production-integration/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/040-line-production-integration/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/050-line-get-distances-vse64-store/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/050-line-get-distances-vse64-store/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/060-line-select-vse64-compressed-store/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/060-line-select-vse64-compressed-store/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/070-line-identity-index-strided-load/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/070-line-identity-index-strided-load/result.zh.md \
	doc-rvv/sample_consensus/sac_model_line-RVV.zh.md \
	doc-rvv/library-screening/sample_consensus/sample_consensus-function-evaluation-queue.zh.md
EVIDENCE_DOC_ARGS := $(foreach doc,$(EVIDENCE_DOC_REFS),--doc $(doc))

USE_PCL_SOURCE_HEADERS ?= 1
PCL_SOURCE_INCLUDES = \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT)/sample_consensus/include
ifeq ($(USE_PCL_SOURCE_HEADERS),1)
PCL_INCLUDE = $(PCL_SOURCE_INCLUDES) -I$(PCL_INSTALL_ROOT)/include/pcl-1.15
else
PCL_INCLUDE = -I$(PCL_INSTALL_ROOT)/include/pcl-1.15
endif

INCLUDES = \
	-I$(CURDIR)/include \
	$(PCL_INCLUDE) \
	-I$(PCL_SOURCE_ROOT) \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include \
	-I$(FLANN_ROOT)/include \
	-I$(LZ4_ROOT)/include \
	-I$(HDF5_ROOT)/include \
	-I$(ZLIB_ROOT)/include \
	-I$(LIBPNG_ROOT)/include

LIBS_SAMPLE_CONSENSUS := \
	-lpcl_sample_consensus \
	-lpcl_common \
	-lpcl_io \
	-lpcl_search \
	-lpcl_kdtree \
	-lflann_cpp \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lz -lhdf5 -llz4 -lpng -lm
LIBS_BENCH := $(LIBS_SAMPLE_CONSENSUS)
LIBS_TEST  := $(LIBS_SAMPLE_CONSENSUS) -lgtest -lgtest_main -lpthread

include ../../mk/rvv-topic.mk

run_line_count_tests:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=$(LINE_COUNT_FILTER)"

run_line_select_tests:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=$(LINE_SELECT_FILTER)"

run_line_get_distances_tests:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=$(LINE_GET_DISTANCES_FILTER)"

run_line_get_distances_vfsqrt_tests:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=$(LINE_GET_DISTANCES_VFSQRT_FILTER)"

check_production_asm: dump_bench_rvv
	@python3 script/check_line_production_asm.py \
	  --asm build/asm/riscv/bench_sac_model_line_rvv.full.asm

check_identity_strided_asm: dump_bench_rvv dump_identity_gather_baseline
	@python3 script/check_line_identity_strided_asm.py \
	  --asm build/asm/riscv/bench_sac_model_line_rvv.full.asm
	@python3 script/check_line_identity_strided_asm.py \
	  --asm build/asm/riscv/bench_sac_model_line_rvv_gather_baseline.full.asm \
	  --expect-gather-only

run_board_line_count_tests: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' REMOTE_TEST_ARGS='--gtest_filter=$(LINE_COUNT_FILTER)'"

run_board_line_select_tests: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' REMOTE_TEST_ARGS='--gtest_filter=$(LINE_SELECT_FILTER)'"

run_board_line_get_distances_tests: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' REMOTE_TEST_ARGS='--gtest_filter=$(LINE_GET_DISTANCES_FILTER)'"

run_board_line_get_distances_vfsqrt_tests: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' REMOTE_TEST_ARGS='--gtest_filter=$(LINE_GET_DISTANCES_VFSQRT_FILTER)'"

collect_repeated_board_evidence:
	@rm -rf $(REPEATED_BOARD_DIR)
	@set -e; \
	for run in 01 02 03 04 05; do \
	  echo "[BOARD] line count repeated run $$run -> $(REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase000-line-count-run-$$run"; \
	done

collect_repeated_board_select_evidence:
	@rm -rf $(REPEATED_BOARD_SELECT_DIR)
	@set -e; \
	for run in 01 02 03 04 05; do \
	  echo "[BOARD] line select repeated run $$run -> $(REPEATED_BOARD_SELECT_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(REPEATED_BOARD_SELECT_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase010-line-select-run-$$run"; \
	done

collect_repeated_board_get_distances_evidence:
	@rm -rf $(REPEATED_BOARD_GET_DISTANCES_DIR)
	@set -e; \
	for run in 01 02 03 04 05; do \
	  echo "[BOARD] line getDistances repeated run $$run -> $(REPEATED_BOARD_GET_DISTANCES_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(REPEATED_BOARD_GET_DISTANCES_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase020-line-get-distances-run-$$run"; \
	done

collect_repeated_board_get_distances_vfsqrt_evidence:
	@rm -rf $(REPEATED_BOARD_GET_DISTANCES_VFSQRT_DIR)
	@set -e; \
	for run in 01 02 03 04 05; do \
	  echo "[BOARD] line getDistances vfsqrt repeated run $$run -> $(REPEATED_BOARD_GET_DISTANCES_VFSQRT_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(REPEATED_BOARD_GET_DISTANCES_VFSQRT_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase030-line-get-distances-vfsqrt-run-$$run"; \
	done

collect_repeated_board_production_evidence:
	@rm -rf $(REPEATED_BOARD_PRODUCTION_DIR)
	@set -e; \
	for run in 01 02 03 04 05; do \
	  echo "[BOARD] line production repeated run $$run -> $(REPEATED_BOARD_PRODUCTION_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(REPEATED_BOARD_PRODUCTION_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase040-line-production-run-$$run"; \
	done

collect_repeated_board_production_vse64_evidence:
	@rm -rf $(REPEATED_BOARD_PRODUCTION_VSE64_DIR)
	@set -e; \
	for run in 01 02 03 04 05; do \
	  echo "[BOARD] line production vse64 repeated run $$run -> $(REPEATED_BOARD_PRODUCTION_VSE64_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(REPEATED_BOARD_PRODUCTION_VSE64_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase050-line-production-vse64-run-$$run"; \
	done

collect_repeated_board_production_select_vse64_evidence:
	@rm -rf $(REPEATED_BOARD_PRODUCTION_SELECT_VSE64_DIR)
	@set -e; \
	for run in 01 02 03 04 05; do \
	  echo "[BOARD] line production select vse64 repeated run $$run -> $(REPEATED_BOARD_PRODUCTION_SELECT_VSE64_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(REPEATED_BOARD_PRODUCTION_SELECT_VSE64_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase060-line-production-select-vse64-run-$$run"; \
	done

deploy_identity_gather_baseline: build_identity_gather_baseline
	@$(STRIP) -s ./$(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE) -o ./$(TARGET_BENCH_RVV_GATHER_BASELINE)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_BENCH_RVV_GATHER_BASELINE)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_BENCH_RVV_GATHER_BASELINE)
	@rm -f ./$(TARGET_BENCH_RVV_GATHER_BASELINE)_stripped

deploy_identity_candidate_and_baseline: deploy_bench_rvv deploy_identity_gather_baseline

run_board_identity_rvv_ab_once: deploy_identity_candidate_and_baseline | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p '$(REMOTE_BOARD_OUTPUT_DIR)' && cd $(REMOTE_DIR) && \
	  LD_LIBRARY_PATH=/root/pcl-test/lib:\$$LD_LIBRARY_PATH ./$(REMOTE_BENCH_RVV_GATHER_BASELINE) $(REMOTE_BENCH_ARGS) 2>&1 | tee '$(REMOTE_BOARD_OUTPUT_DIR)/run_bench_rvv_gather_baseline.log' && \
	  LD_LIBRARY_PATH=/root/pcl-test/lib:\$$LD_LIBRARY_PATH ./$(REMOTE_BENCH_RVV_IDENTITY) $(REMOTE_BENCH_ARGS) 2>&1 | tee '$(REMOTE_BOARD_OUTPUT_DIR)/run_bench_rvv_identity.log'"
	@rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_BOARD_OUTPUT_DIR)/ $(OUTPUT_DIR_BOARD)/

collect_identity_repeated_board_evidence:
	@rm -rf $(IDENTITY_REPEATED_BOARD_DIR)
	@set -e; \
	for run in 01 02 03 04 05; do \
	  echo "[BOARD] line identity RVV-vs-RVV repeated run $$run -> $(IDENTITY_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) run_board_identity_rvv_ab_once \
	    OUTPUT_DIR_BOARD="$(IDENTITY_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase070-line-identity-run-$$run" \
	    REMOTE_BENCH_ARGS="$(BENCH_IDENTITY_ARGS)"; \
	done

collect_identity_shuffled_repeated_board_evidence:
	@rm -rf $(IDENTITY_SHUFFLED_REPEATED_BOARD_DIR)
	@set -e; \
	for run in 01 02 03 04 05; do \
	  echo "[BOARD] line shuffled RVV-vs-RVV repeated run $$run -> $(IDENTITY_SHUFFLED_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) run_board_identity_rvv_ab_once \
	    OUTPUT_DIR_BOARD="$(IDENTITY_SHUFFLED_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase070-line-shuffled-run-$$run" \
	    REMOTE_BENCH_ARGS="$(BENCH_SHUFFLED_ARGS)"; \
	done

generate_board_evidence_manifest:
	@python3 script/generate_line_board_evidence_manifest.py \
	  --repeat-root $(REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_line_rvv.full.asm \
	  --output $(REPEATED_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_line Phase 000 count repeated board summary"

generate_select_board_evidence_manifest:
	@python3 script/generate_line_board_evidence_manifest.py \
	  --repeat-root $(REPEATED_BOARD_SELECT_DIR) \
	  --asm build/asm/riscv/bench_sac_model_line_rvv.full.asm \
	  --output $(REPEATED_SELECT_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_line Phase 010 select repeated board summary" \
	  --items select

generate_get_distances_board_evidence_manifest:
	@python3 script/generate_line_board_evidence_manifest.py \
	  --repeat-root $(REPEATED_BOARD_GET_DISTANCES_DIR) \
	  --asm build/asm/riscv/bench_sac_model_line_rvv.full.asm \
	  --output $(REPEATED_GET_DISTANCES_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_line Phase 020 getDistances repeated board summary" \
	  --items getdist

generate_get_distances_vfsqrt_board_evidence_manifest:
	@python3 script/generate_line_board_evidence_manifest.py \
	  --repeat-root $(REPEATED_BOARD_GET_DISTANCES_VFSQRT_DIR) \
	  --asm build/asm/riscv/bench_sac_model_line_rvv.full.asm \
	  --output $(REPEATED_GET_DISTANCES_VFSQRT_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_line Phase 030 getDistances vfsqrt repeated board summary" \
	  --items getdist_vfsqrt

generate_production_board_evidence_manifest:
	@python3 script/generate_line_board_evidence_manifest.py \
	  --repeat-root $(REPEATED_BOARD_PRODUCTION_DIR) \
	  --asm build/asm/riscv/bench_sac_model_line_rvv.full.asm \
	  --output $(REPEATED_PRODUCTION_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_line Phase 040 production direct repeated board summary" \
	  --items production \
	  --production-direct

generate_production_vse64_board_evidence_manifest:
	@python3 script/generate_line_board_evidence_manifest.py \
	  --repeat-root $(REPEATED_BOARD_PRODUCTION_VSE64_DIR) \
	  --asm build/asm/riscv/bench_sac_model_line_rvv.full.asm \
	  --output $(REPEATED_PRODUCTION_VSE64_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_line Phase 050 production vfwcvt/vse64 repeated board summary" \
	  --items production \
	  --production-direct

generate_production_select_vse64_board_evidence_manifest:
	@python3 script/generate_line_board_evidence_manifest.py \
	  --repeat-root $(REPEATED_BOARD_PRODUCTION_SELECT_VSE64_DIR) \
	  --asm build/asm/riscv/bench_sac_model_line_rvv.full.asm \
	  --output $(REPEATED_PRODUCTION_SELECT_VSE64_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_line Phase 060 production select vfwcvt/vse64 repeated board summary" \
	  --items production \
	  --production-direct

guard_phase070_identity_refresh:
	@if [ "$(ALLOW_PHASE070_IDENTITY_REFRESH)" != "1" ]; then \
	  echo "[ERROR] Phase 070 identity-strided candidate was rejected and production is gather-only again."; \
	  echo "[ERROR] Refusing to regenerate historical identity A/B evidence unless ALLOW_PHASE070_IDENTITY_REFRESH=1 is set after reapplying the candidate."; \
	  exit 1; \
	fi

generate_identity_board_evidence_manifest: guard_phase070_identity_refresh
	@python3 script/generate_line_board_evidence_manifest.py \
	  --identity-mode identity \
	  --repeat-root $(IDENTITY_REPEATED_BOARD_DIR) \
	  --baseline-asm build/asm/riscv/bench_sac_model_line_rvv_gather_baseline.full.asm \
	  --asm build/asm/riscv/bench_sac_model_line_rvv.full.asm \
	  --output $(IDENTITY_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_line Phase 070 identity-index RVV-vs-RVV repeated board summary"

generate_identity_shuffled_board_evidence_manifest: guard_phase070_identity_refresh
	@python3 script/generate_line_board_evidence_manifest.py \
	  --identity-mode identity-shuffled \
	  --repeat-root $(IDENTITY_SHUFFLED_REPEATED_BOARD_DIR) \
	  --baseline-asm build/asm/riscv/bench_sac_model_line_rvv_gather_baseline.full.asm \
	  --asm build/asm/riscv/bench_sac_model_line_rvv.full.asm \
	  --output $(IDENTITY_SHUFFLED_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_line Phase 070 shuffled-index RVV-vs-RVV repeated board summary"

run_repeated_board_evidence_doctor: generate_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(REPEATED_EVIDENCE_MANIFEST) \
	  --output $(REPEATED_EVIDENCE_DOCTOR_MD) \
	  --json-output $(REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

run_repeated_board_select_evidence_doctor: generate_select_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(REPEATED_SELECT_EVIDENCE_MANIFEST) \
	  --output $(REPEATED_SELECT_EVIDENCE_DOCTOR_MD) \
	  --json-output $(REPEATED_SELECT_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

run_repeated_board_get_distances_evidence_doctor: generate_get_distances_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(REPEATED_GET_DISTANCES_EVIDENCE_MANIFEST) \
	  --output $(REPEATED_GET_DISTANCES_EVIDENCE_DOCTOR_MD) \
	  --json-output $(REPEATED_GET_DISTANCES_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

run_repeated_board_get_distances_vfsqrt_evidence_doctor: generate_get_distances_vfsqrt_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(REPEATED_GET_DISTANCES_VFSQRT_EVIDENCE_MANIFEST) \
	  --output $(REPEATED_GET_DISTANCES_VFSQRT_EVIDENCE_DOCTOR_MD) \
	  --json-output $(REPEATED_GET_DISTANCES_VFSQRT_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

run_repeated_board_production_evidence_doctor: generate_production_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(REPEATED_PRODUCTION_EVIDENCE_MANIFEST) \
	  --output $(REPEATED_PRODUCTION_EVIDENCE_DOCTOR_MD) \
	  --json-output $(REPEATED_PRODUCTION_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

run_repeated_board_production_vse64_evidence_doctor: generate_production_vse64_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(REPEATED_PRODUCTION_VSE64_EVIDENCE_MANIFEST) \
	  --output $(REPEATED_PRODUCTION_VSE64_EVIDENCE_DOCTOR_MD) \
	  --json-output $(REPEATED_PRODUCTION_VSE64_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

run_repeated_board_production_select_vse64_evidence_doctor: generate_production_select_vse64_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(REPEATED_PRODUCTION_SELECT_VSE64_EVIDENCE_MANIFEST) \
	  --output $(REPEATED_PRODUCTION_SELECT_VSE64_EVIDENCE_DOCTOR_MD) \
	  --json-output $(REPEATED_PRODUCTION_SELECT_VSE64_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

run_identity_board_evidence_doctor: generate_identity_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(IDENTITY_EVIDENCE_MANIFEST) \
	  --output $(IDENTITY_EVIDENCE_DOCTOR_MD) \
	  --json-output $(IDENTITY_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

run_identity_shuffled_board_evidence_doctor: generate_identity_shuffled_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(IDENTITY_SHUFFLED_EVIDENCE_MANIFEST) \
	  --output $(IDENTITY_SHUFFLED_EVIDENCE_DOCTOR_MD) \
	  --json-output $(IDENTITY_SHUFFLED_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_repeated_board_evidence_state: run_repeated_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(REPEATED_EVIDENCE_MANIFEST) \
	  --doctor $(REPEATED_EVIDENCE_DOCTOR_MD) \
	  --path $(REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --target run_repeated_board_evidence_doctor \
	  --backend board \
	  --run-label $(REPEATED_EVIDENCE_RUN_LABEL) \
	  --case-filter line-count-phase000-repeated \
	  --evidence-role production_shaped_diagnostic \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/000-line-count-diagnostic/result.zh.md \
	  --freshness-state recorded

record_repeated_board_select_evidence_state: run_repeated_board_select_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(REPEATED_SELECT_EVIDENCE_MANIFEST) \
	  --doctor $(REPEATED_SELECT_EVIDENCE_DOCTOR_MD) \
	  --path $(REPEATED_SELECT_EVIDENCE_DOCTOR_JSON) \
	  --target run_repeated_board_select_evidence_doctor \
	  --backend board \
	  --run-label $(REPEATED_SELECT_EVIDENCE_RUN_LABEL) \
	  --case-filter line-select-phase010-repeated \
	  --evidence-role production_shaped_diagnostic \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/010-line-select-diagnostic/result.zh.md \
	  --freshness-state recorded

record_repeated_board_get_distances_evidence_state: run_repeated_board_get_distances_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(REPEATED_GET_DISTANCES_EVIDENCE_MANIFEST) \
	  --doctor $(REPEATED_GET_DISTANCES_EVIDENCE_DOCTOR_MD) \
	  --path $(REPEATED_GET_DISTANCES_EVIDENCE_DOCTOR_JSON) \
	  --target run_repeated_board_get_distances_evidence_doctor \
	  --backend board \
	  --run-label $(REPEATED_GET_DISTANCES_EVIDENCE_RUN_LABEL) \
	  --case-filter line-get-distances-phase020-repeated \
	  --evidence-role production_shaped_diagnostic \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/020-line-get-distances-diagnostic/result.zh.md \
	  --freshness-state recorded

record_repeated_board_get_distances_vfsqrt_evidence_state: run_repeated_board_get_distances_vfsqrt_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(REPEATED_GET_DISTANCES_VFSQRT_EVIDENCE_MANIFEST) \
	  --doctor $(REPEATED_GET_DISTANCES_VFSQRT_EVIDENCE_DOCTOR_MD) \
	  --path $(REPEATED_GET_DISTANCES_VFSQRT_EVIDENCE_DOCTOR_JSON) \
	  --target run_repeated_board_get_distances_vfsqrt_evidence_doctor \
	  --backend board \
	  --run-label $(REPEATED_GET_DISTANCES_VFSQRT_EVIDENCE_RUN_LABEL) \
	  --case-filter line-get-distances-vfsqrt-phase030-repeated \
	  --evidence-role production_shaped_diagnostic \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/030-line-get-distances-vfsqrt-diagnostic/result.zh.md \
	  --freshness-state recorded

record_repeated_board_production_evidence_state: run_repeated_board_production_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(REPEATED_PRODUCTION_EVIDENCE_MANIFEST) \
	  --doctor $(REPEATED_PRODUCTION_EVIDENCE_DOCTOR_MD) \
	  --path $(REPEATED_PRODUCTION_EVIDENCE_DOCTOR_JSON) \
	  --target run_repeated_board_production_evidence_doctor \
	  --backend board \
	  --run-label $(REPEATED_PRODUCTION_EVIDENCE_RUN_LABEL) \
	  --case-filter line-production-phase040-repeated \
	  --evidence-role production_direct \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/040-line-production-integration/result.zh.md \
	  --freshness-state recorded

record_repeated_board_production_vse64_evidence_state: run_repeated_board_production_vse64_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(REPEATED_PRODUCTION_VSE64_EVIDENCE_MANIFEST) \
	  --doctor $(REPEATED_PRODUCTION_VSE64_EVIDENCE_DOCTOR_MD) \
	  --path $(REPEATED_PRODUCTION_VSE64_EVIDENCE_DOCTOR_JSON) \
	  --target run_repeated_board_production_vse64_evidence_doctor \
	  --backend board \
	  --run-label $(REPEATED_PRODUCTION_VSE64_EVIDENCE_RUN_LABEL) \
	  --case-filter line-production-vse64-phase050-repeated \
	  --evidence-role production_direct \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/050-line-get-distances-vse64-store/result.zh.md \
	  --freshness-state recorded

record_repeated_board_production_select_vse64_evidence_state: run_repeated_board_production_select_vse64_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(REPEATED_PRODUCTION_SELECT_VSE64_EVIDENCE_MANIFEST) \
	  --doctor $(REPEATED_PRODUCTION_SELECT_VSE64_EVIDENCE_DOCTOR_MD) \
	  --path $(REPEATED_PRODUCTION_SELECT_VSE64_EVIDENCE_DOCTOR_JSON) \
	  --target run_repeated_board_production_select_vse64_evidence_doctor \
	  --backend board \
	  --run-label $(REPEATED_PRODUCTION_SELECT_VSE64_EVIDENCE_RUN_LABEL) \
	  --case-filter line-production-select-vse64-phase060-repeated \
	  --evidence-role production_direct \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/060-line-select-vse64-compressed-store/result.zh.md \
	  --freshness-state recorded

record_identity_board_evidence_state: run_identity_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(IDENTITY_EVIDENCE_MANIFEST) \
	  --doctor $(IDENTITY_EVIDENCE_DOCTOR_MD) \
	  --path $(IDENTITY_EVIDENCE_DOCTOR_JSON) \
	  --target run_identity_board_evidence_doctor \
	  --backend board \
	  --run-label $(IDENTITY_EVIDENCE_RUN_LABEL) \
	  --case-filter line-phase070-identity-rvv-ab-repeated \
	  --evidence-role strict_ab \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/070-line-identity-index-strided-load/result.zh.md \
	  --freshness-state recorded

record_identity_shuffled_board_evidence_state: run_identity_shuffled_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(IDENTITY_SHUFFLED_EVIDENCE_MANIFEST) \
	  --doctor $(IDENTITY_SHUFFLED_EVIDENCE_DOCTOR_MD) \
	  --path $(IDENTITY_SHUFFLED_EVIDENCE_DOCTOR_JSON) \
	  --target run_identity_shuffled_board_evidence_doctor \
	  --backend board \
	  --run-label $(IDENTITY_SHUFFLED_EVIDENCE_RUN_LABEL) \
	  --case-filter line-phase070-shuffled-rvv-ab-repeated \
	  --evidence-role strict_ab \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/070-line-identity-index-strided-load/result.zh.md \
	  --freshness-state recorded

repeated_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

repeated_select_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_SELECT_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_SELECT_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_SELECT_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

repeated_get_distances_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_GET_DISTANCES_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_GET_DISTANCES_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_GET_DISTANCES_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

repeated_get_distances_vfsqrt_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_GET_DISTANCES_VFSQRT_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_GET_DISTANCES_VFSQRT_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_GET_DISTANCES_VFSQRT_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

repeated_production_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_PRODUCTION_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_PRODUCTION_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_PRODUCTION_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

repeated_production_vse64_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_PRODUCTION_VSE64_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_PRODUCTION_VSE64_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_PRODUCTION_VSE64_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

repeated_production_select_vse64_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_PRODUCTION_SELECT_VSE64_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_PRODUCTION_SELECT_VSE64_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_PRODUCTION_SELECT_VSE64_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

identity_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(IDENTITY_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(IDENTITY_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(IDENTITY_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

identity_shuffled_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(IDENTITY_SHUFFLED_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(IDENTITY_SHUFFLED_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(IDENTITY_SHUFFLED_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

build_identity_gather_baseline:
	@rm -f $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE) \
	  $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE).full.asm \
	  $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE).asm
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 \
	  TARGET_BENCH=$(TARGET_BENCH_RVV_GATHER_BASELINE) \
	  EXTRA_CXXFLAGS="-DPCL_RVV_LINE_DISABLE_IDENTITY_STRIDED" \
	  $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE)

dump_identity_gather_baseline: build_identity_gather_baseline | $(ASM_DIR)/$(ARCH)
	$(OBJDUMP) -d -C $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE) > $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE).full.asm
	grep -E $(VEC_REGEX_STR) $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE).full.asm > $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE).asm || true
	@echo "[DONE] RVV gather baseline asm dump: $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE).asm"

.PHONY: run_line_count_tests run_line_select_tests run_line_get_distances_tests run_line_get_distances_vfsqrt_tests check_production_asm check_identity_strided_asm run_board_line_count_tests run_board_line_select_tests run_board_line_get_distances_tests run_board_line_get_distances_vfsqrt_tests collect_repeated_board_evidence collect_repeated_board_select_evidence collect_repeated_board_get_distances_evidence collect_repeated_board_get_distances_vfsqrt_evidence collect_repeated_board_production_evidence collect_repeated_board_production_vse64_evidence collect_repeated_board_production_select_vse64_evidence collect_identity_repeated_board_evidence collect_identity_shuffled_repeated_board_evidence deploy_identity_gather_baseline deploy_identity_candidate_and_baseline run_board_identity_rvv_ab_once generate_board_evidence_manifest generate_select_board_evidence_manifest generate_get_distances_board_evidence_manifest generate_get_distances_vfsqrt_board_evidence_manifest generate_production_board_evidence_manifest generate_production_vse64_board_evidence_manifest generate_production_select_vse64_board_evidence_manifest guard_phase070_identity_refresh generate_identity_board_evidence_manifest generate_identity_shuffled_board_evidence_manifest run_repeated_board_evidence_doctor run_repeated_board_select_evidence_doctor run_repeated_board_get_distances_evidence_doctor run_repeated_board_get_distances_vfsqrt_evidence_doctor run_repeated_board_production_evidence_doctor run_repeated_board_production_vse64_evidence_doctor run_repeated_board_production_select_vse64_evidence_doctor run_identity_board_evidence_doctor run_identity_shuffled_board_evidence_doctor record_repeated_board_evidence_state record_repeated_board_select_evidence_state record_repeated_board_get_distances_evidence_state record_repeated_board_get_distances_vfsqrt_evidence_state record_repeated_board_production_evidence_state record_repeated_board_production_vse64_evidence_state record_repeated_board_production_select_vse64_evidence_state record_identity_board_evidence_state record_identity_shuffled_board_evidence_state repeated_evidence_status repeated_select_evidence_status repeated_get_distances_evidence_status repeated_get_distances_vfsqrt_evidence_status repeated_production_evidence_status repeated_production_vse64_evidence_status repeated_production_select_vse64_evidence_status identity_evidence_status identity_shuffled_evidence_status build_identity_gather_baseline dump_identity_gather_baseline
