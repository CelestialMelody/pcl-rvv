# =============================================================================
# PCL sample_consensus/sac_model_circle RVV topic Makefile
#
# 本 topic 使用共享 rvv-topic.mk；QEMU 只跑 correctness（正确性）和日志形状，
# 性能结论只来自 board（板卡）或目标硬件。
# =============================================================================

TOPIC  := sac_model_circle
MODULE := sample_consensus
TOPIC_DOC_PREFIX := test-rvv/sample_consensus/sac_model_circle

EIGEN_RVV_FLAGS := -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=2 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl

FOCUS_DIR      := pcl/sample_consensus/include/pcl/sample_consensus
FILTER_REPORT  := log/filtered_sac_model_circle.log
ANALYZE_REPORT := log/analyze_sac_model_circle.log

TARGET_TEST     := test_sac_model_circle
TARGET_TEST_STD := test_sac_model_circle_std
TARGET_TEST_RVV := test_sac_model_circle_rvv
SRCS_TEST       := src/test_sac_model_circle.cpp

TARGET_BENCH     := bench_sac_model_circle
TARGET_BENCH_STD := bench_sac_model_circle_std
TARGET_BENCH_RVV := bench_sac_model_circle_rvv
TARGET_BENCH_RVV_GATHER_BASELINE := bench_sac_model_circle_rvv_gather_baseline
SRCS_BENCH       := src/bench_sac_model_circle.cpp
BENCH_ARGS       ?= 65536 200
BENCH_IDENTITY_ARGS ?= 65536 200 identity
BENCH_SHUFFLED_ARGS ?= 65536 200 shuffled

REMOTE_DIR ?= /root/pcl-test/sample_consensus/sac_model_circle
REMOTE_BENCH_ARGS ?= $(BENCH_ARGS)
REMOTE_TEST_ARGS ?=
REMOTE_BENCH_RVV_IDENTITY := bench_sac_model_circle_rvv
REMOTE_BENCH_RVV_GATHER_BASELINE := bench_sac_model_circle_rvv_gather_baseline

CIRCLE_PUBLIC_FILTER := SampleConsensusModelCircle2D.PublicSelectWithinDistanceMatchesDirectRVVForSupportedLayout:SampleConsensusModelCircle2D.SelectFullRVVErrorTailCandidateMatchesDirectRVV:SampleConsensusModelCircle2D.PointXYZILayoutMatchesStandardPath:SampleConsensusModelCircle2D.CloudOnlyIdentityIndicesMatchStandardPath:SampleConsensusModelCircle2D.ExplicitEmptyIndicesMatchStandardPath:SampleConsensusModelCircle2D.GetDistancesCandidateMatchesPublicPath:SampleConsensusModelCircle2D.GetDistancesFullRVVCandidateMatchesPublicPath:SampleConsensusModelCircle2D.PublicGetDistancesMatchesStandardAndDirectRVV
CIRCLE_SELECT_ERROR_TAIL_FILTER := SampleConsensusModelCircle2D.SelectFullRVVErrorTailCandidateMatchesDirectRVV
CIRCLE_GETDISTANCES_FILTER := SampleConsensusModelCircle2D.GetDistancesCandidateMatchesPublicPath
CIRCLE_GETDISTANCES_FULL_RVV_FILTER := SampleConsensusModelCircle2D.GetDistancesFullRVVCandidateMatchesPublicPath
CIRCLE_GETDISTANCES_PRODUCTION_FILTER := SampleConsensusModelCircle2D.PublicGetDistancesMatchesStandardAndDirectRVV
PHASE000_DIR := doc/phases/000-circle-select-distance-production
PHASE020_DIR := doc/phases/020-circle-getdistances-ablation
PHASE040_DIR := doc/phases/040-production-closeout-and-identity-frontier
PHASE050_DIR := doc/phases/050-getdistances-vfsqrt-full-rvv
PHASE060_DIR := doc/phases/060-getdistances-production-probe
PHASE080_DIR := doc/phases/080-select-compressed-error-tail
PHASE090_DIR := doc/phases/090-select-error-tail-production-probe
PRODUCTION_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase000-production-select
GETDISTANCES_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase020-getdistances-ablation
IDENTITY_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase040-identity-rvv-ab
IDENTITY_SHUFFLED_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase040-shuffled-rvv-ab
GETDISTANCES_FULL_RVV_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase050-getdistances-full-rvv
GETDISTANCES_PRODUCTION_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase060-getdistances-production
SELECT_ERROR_TAIL_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase080-select-error-tail
SELECT_ERROR_TAIL_PRODUCTION_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase090-select-error-tail-production
PRODUCTION_EVIDENCE_MANIFEST := $(PHASE000_DIR)/production-repeated-evidence-manifest.json
PRODUCTION_EVIDENCE_DOCTOR_MD := $(PHASE000_DIR)/production-repeated-evidence-doctor.md
PRODUCTION_EVIDENCE_DOCTOR_JSON := $(PHASE000_DIR)/production-repeated-evidence-doctor.json
GETDISTANCES_EVIDENCE_MANIFEST := $(PHASE020_DIR)/getdistances-repeated-evidence-manifest.json
GETDISTANCES_EVIDENCE_DOCTOR_MD := $(PHASE020_DIR)/getdistances-repeated-evidence-doctor.md
GETDISTANCES_EVIDENCE_DOCTOR_JSON := $(PHASE020_DIR)/getdistances-repeated-evidence-doctor.json
GETDISTANCES_FULL_RVV_EVIDENCE_MANIFEST := $(PHASE050_DIR)/getdistances-full-rvv-repeated-evidence-manifest.json
GETDISTANCES_FULL_RVV_EVIDENCE_DOCTOR_MD := $(PHASE050_DIR)/getdistances-full-rvv-repeated-evidence-doctor.md
GETDISTANCES_FULL_RVV_EVIDENCE_DOCTOR_JSON := $(PHASE050_DIR)/getdistances-full-rvv-repeated-evidence-doctor.json
GETDISTANCES_PRODUCTION_EVIDENCE_MANIFEST := $(PHASE060_DIR)/getdistances-production-repeated-evidence-manifest.json
GETDISTANCES_PRODUCTION_EVIDENCE_DOCTOR_MD := $(PHASE060_DIR)/getdistances-production-repeated-evidence-doctor.md
GETDISTANCES_PRODUCTION_EVIDENCE_DOCTOR_JSON := $(PHASE060_DIR)/getdistances-production-repeated-evidence-doctor.json
SELECT_ERROR_TAIL_EVIDENCE_MANIFEST := $(PHASE080_DIR)/select-error-tail-repeated-evidence-manifest.json
SELECT_ERROR_TAIL_EVIDENCE_DOCTOR_MD := $(PHASE080_DIR)/select-error-tail-repeated-evidence-doctor.md
SELECT_ERROR_TAIL_EVIDENCE_DOCTOR_JSON := $(PHASE080_DIR)/select-error-tail-repeated-evidence-doctor.json
SELECT_ERROR_TAIL_PRODUCTION_EVIDENCE_MANIFEST := $(PHASE090_DIR)/select-error-tail-production-repeated-evidence-manifest.json
SELECT_ERROR_TAIL_PRODUCTION_EVIDENCE_DOCTOR_MD := $(PHASE090_DIR)/select-error-tail-production-repeated-evidence-doctor.md
SELECT_ERROR_TAIL_PRODUCTION_EVIDENCE_DOCTOR_JSON := $(PHASE090_DIR)/select-error-tail-production-repeated-evidence-doctor.json
IDENTITY_EVIDENCE_MANIFEST := $(PHASE040_DIR)/identity-repeated-evidence-manifest.json
IDENTITY_EVIDENCE_DOCTOR_MD := $(PHASE040_DIR)/identity-repeated-evidence-doctor.md
IDENTITY_EVIDENCE_DOCTOR_JSON := $(PHASE040_DIR)/identity-repeated-evidence-doctor.json
IDENTITY_SHUFFLED_EVIDENCE_MANIFEST := $(PHASE040_DIR)/identity-shuffled-repeated-evidence-manifest.json
IDENTITY_SHUFFLED_EVIDENCE_DOCTOR_MD := $(PHASE040_DIR)/identity-shuffled-repeated-evidence-doctor.md
IDENTITY_SHUFFLED_EVIDENCE_DOCTOR_JSON := $(PHASE040_DIR)/identity-shuffled-repeated-evidence-doctor.json
EVIDENCE_REGISTRY := log/evidence_registry.json
PRODUCTION_EVIDENCE_RUN_LABEL := circle-phase000-production-select-repeated-board
GETDISTANCES_EVIDENCE_RUN_LABEL := circle-phase020-getdistances-repeated-board
GETDISTANCES_FULL_RVV_EVIDENCE_RUN_LABEL := circle-phase050-getdistances-full-rvv-repeated-board
GETDISTANCES_PRODUCTION_EVIDENCE_RUN_LABEL := circle-phase060-getdistances-production-repeated-board
SELECT_ERROR_TAIL_EVIDENCE_RUN_LABEL := circle-phase080-select-error-tail-rvv-ab-repeated-board
SELECT_ERROR_TAIL_PRODUCTION_EVIDENCE_RUN_LABEL := circle-phase090-select-error-tail-production-repeated-board
IDENTITY_EVIDENCE_RUN_LABEL := circle-phase040-identity-rvv-ab-repeated-board
IDENTITY_SHUFFLED_EVIDENCE_RUN_LABEL := circle-phase040-shuffled-rvv-ab-repeated-board
EVIDENCE_DOC_REFS := \
	$(TOPIC_DOC_PREFIX)/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/sac_model_circle-evaluation.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/benchmark-and-evidence.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-evidence.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-roadmap.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/optimization-matrix.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/000-circle-select-distance-production/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/000-circle-select-distance-production/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-circle-getdistances-ablation/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-circle-getdistances-ablation/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/040-production-closeout-and-identity-frontier/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/040-production-closeout-and-identity-frontier/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/050-getdistances-vfsqrt-full-rvv/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/050-getdistances-vfsqrt-full-rvv/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/060-getdistances-production-probe/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/060-getdistances-production-probe/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/080-select-compressed-error-tail/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/080-select-compressed-error-tail/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/090-select-error-tail-production-probe/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/090-select-error-tail-production-probe/result.zh.md \
	doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md \
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

run_circle_public_tests:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=$(CIRCLE_PUBLIC_FILTER)"

run_circle_select_error_tail_candidate_test:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=$(CIRCLE_SELECT_ERROR_TAIL_FILTER)"

run_circle_getdistances_candidate_test:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=$(CIRCLE_GETDISTANCES_FILTER)"

run_circle_getdistances_full_rvv_candidate_test:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=$(CIRCLE_GETDISTANCES_FULL_RVV_FILTER)"

run_circle_getdistances_production_test:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=$(CIRCLE_GETDISTANCES_PRODUCTION_FILTER)"

run_board_circle_public_tests: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' REMOTE_TEST_ARGS='--gtest_filter=$(CIRCLE_PUBLIC_FILTER)'"

run_board_circle_getdistances_candidate_test: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' REMOTE_TEST_ARGS='--gtest_filter=$(CIRCLE_GETDISTANCES_FILTER)'"

run_board_circle_getdistances_full_rvv_candidate_test: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' REMOTE_TEST_ARGS='--gtest_filter=$(CIRCLE_GETDISTANCES_FULL_RVV_FILTER)'"

run_board_circle_getdistances_production_test: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' REMOTE_TEST_ARGS='--gtest_filter=$(CIRCLE_GETDISTANCES_PRODUCTION_FILTER)'"

run_board_circle_select_error_tail_candidate_test: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' REMOTE_TEST_ARGS='--gtest_filter=$(CIRCLE_SELECT_ERROR_TAIL_FILTER)'"

collect_production_repeated_board_evidence:
	@rm -rf $(PRODUCTION_REPEATED_BOARD_DIR)
	@for run in 01 02 03 04 05; do \
	  echo "[BOARD] production repeated run $$run -> $(PRODUCTION_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(PRODUCTION_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase000-production-select-run-$$run"; \
	done

generate_production_board_evidence_manifest:
	@python3 script/generate_circle_board_evidence_manifest.py \
	  --mode production \
	  --repeat-root $(PRODUCTION_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_circle_rvv.full.asm \
	  --output $(PRODUCTION_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_circle Phase 000 production select/count repeated board summary"

run_production_board_evidence_doctor: generate_production_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(PRODUCTION_EVIDENCE_MANIFEST) \
	  --output $(PRODUCTION_EVIDENCE_DOCTOR_MD) \
	  --json-output $(PRODUCTION_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_production_board_evidence_state: run_production_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(PRODUCTION_EVIDENCE_MANIFEST) \
	  --doctor $(PRODUCTION_EVIDENCE_DOCTOR_MD) \
	  --path $(PRODUCTION_EVIDENCE_DOCTOR_JSON) \
	  --target run_production_board_evidence_doctor \
	  --backend board \
	  --run-label $(PRODUCTION_EVIDENCE_RUN_LABEL) \
	  --case-filter circle-select-distance-phase000-production-repeated \
	  --evidence-role production_direct \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/000-circle-select-distance-production/result.zh.md \
	  --freshness-state recorded

production_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PRODUCTION_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PRODUCTION_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PRODUCTION_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

collect_getdistances_repeated_board_evidence:
	@rm -rf $(GETDISTANCES_REPEATED_BOARD_DIR)
	@for run in 01 02 03 04 05; do \
	  echo "[BOARD] getDistances diagnostic repeated run $$run -> $(GETDISTANCES_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(GETDISTANCES_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_TEST_ARGS="--gtest_filter=$(CIRCLE_GETDISTANCES_FILTER)" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase020-getdistances-run-$$run"; \
	done

generate_getdistances_board_evidence_manifest:
	@python3 script/generate_circle_board_evidence_manifest.py \
	  --mode getdistances \
	  --repeat-root $(GETDISTANCES_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_circle_rvv.full.asm \
	  --output $(GETDISTANCES_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_circle Phase 020 getDistances diagnostic repeated board summary"

run_getdistances_board_evidence_doctor: generate_getdistances_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(GETDISTANCES_EVIDENCE_MANIFEST) \
	  --output $(GETDISTANCES_EVIDENCE_DOCTOR_MD) \
	  --json-output $(GETDISTANCES_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_getdistances_board_evidence_state: run_getdistances_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(GETDISTANCES_EVIDENCE_MANIFEST) \
	  --doctor $(GETDISTANCES_EVIDENCE_DOCTOR_MD) \
	  --path $(GETDISTANCES_EVIDENCE_DOCTOR_JSON) \
	  --target run_getdistances_board_evidence_doctor \
	  --backend board \
	  --run-label $(GETDISTANCES_EVIDENCE_RUN_LABEL) \
	  --case-filter circle-getdistances-phase020-diagnostic-repeated \
	  --evidence-role production_shaped_diagnostic \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/020-circle-getdistances-ablation/result.zh.md \
	  --freshness-state recorded

getdistances_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(GETDISTANCES_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(GETDISTANCES_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(GETDISTANCES_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

collect_getdistances_full_rvv_repeated_board_evidence:
	@rm -rf $(GETDISTANCES_FULL_RVV_REPEATED_BOARD_DIR)
	@for run in 01 02 03 04 05; do \
	  echo "[BOARD] getDistances full-RVV diagnostic repeated run $$run -> $(GETDISTANCES_FULL_RVV_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(GETDISTANCES_FULL_RVV_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_TEST_ARGS="--gtest_filter=$(CIRCLE_GETDISTANCES_FULL_RVV_FILTER)" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase050-getdistances-full-rvv-run-$$run"; \
	done

generate_getdistances_full_rvv_board_evidence_manifest:
	@python3 script/generate_circle_board_evidence_manifest.py \
	  --mode getdistances-full-rvv \
	  --repeat-root $(GETDISTANCES_FULL_RVV_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_circle_rvv.full.asm \
	  --output $(GETDISTANCES_FULL_RVV_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_circle Phase 050 getDistances full-RVV diagnostic repeated board summary"

run_getdistances_full_rvv_board_evidence_doctor: generate_getdistances_full_rvv_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(GETDISTANCES_FULL_RVV_EVIDENCE_MANIFEST) \
	  --output $(GETDISTANCES_FULL_RVV_EVIDENCE_DOCTOR_MD) \
	  --json-output $(GETDISTANCES_FULL_RVV_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_getdistances_full_rvv_board_evidence_state: run_getdistances_full_rvv_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(GETDISTANCES_FULL_RVV_EVIDENCE_MANIFEST) \
	  --doctor $(GETDISTANCES_FULL_RVV_EVIDENCE_DOCTOR_MD) \
	  --path $(GETDISTANCES_FULL_RVV_EVIDENCE_DOCTOR_JSON) \
	  --target run_getdistances_full_rvv_board_evidence_doctor \
	  --backend board \
	  --run-label $(GETDISTANCES_FULL_RVV_EVIDENCE_RUN_LABEL) \
	  --case-filter circle-getdistances-phase050-full-rvv-diagnostic-repeated \
	  --evidence-role production_shaped_diagnostic \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/050-getdistances-vfsqrt-full-rvv/result.zh.md \
	  --freshness-state recorded

getdistances_full_rvv_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(GETDISTANCES_FULL_RVV_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(GETDISTANCES_FULL_RVV_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(GETDISTANCES_FULL_RVV_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

collect_getdistances_production_repeated_board_evidence:
	@rm -rf $(GETDISTANCES_PRODUCTION_REPEATED_BOARD_DIR)
	@for run in 01 02 03 04 05; do \
	  echo "[BOARD] getDistances production repeated run $$run -> $(GETDISTANCES_PRODUCTION_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(GETDISTANCES_PRODUCTION_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_TEST_ARGS="--gtest_filter=$(CIRCLE_GETDISTANCES_PRODUCTION_FILTER)" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase060-getdistances-production-run-$$run"; \
	done

generate_getdistances_production_board_evidence_manifest:
	@python3 script/generate_circle_board_evidence_manifest.py \
	  --mode getdistances-production \
	  --repeat-root $(GETDISTANCES_PRODUCTION_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_circle_rvv.full.asm \
	  --output $(GETDISTANCES_PRODUCTION_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_circle Phase 060 getDistances production repeated board summary"

run_getdistances_production_board_evidence_doctor: generate_getdistances_production_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(GETDISTANCES_PRODUCTION_EVIDENCE_MANIFEST) \
	  --output $(GETDISTANCES_PRODUCTION_EVIDENCE_DOCTOR_MD) \
	  --json-output $(GETDISTANCES_PRODUCTION_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_getdistances_production_board_evidence_state: run_getdistances_production_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(GETDISTANCES_PRODUCTION_EVIDENCE_MANIFEST) \
	  --doctor $(GETDISTANCES_PRODUCTION_EVIDENCE_DOCTOR_MD) \
	  --path $(GETDISTANCES_PRODUCTION_EVIDENCE_DOCTOR_JSON) \
	  --target run_getdistances_production_board_evidence_doctor \
	  --backend board \
	  --run-label $(GETDISTANCES_PRODUCTION_EVIDENCE_RUN_LABEL) \
	  --case-filter circle-getdistances-phase060-production-repeated \
	  --evidence-role production_direct \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/060-getdistances-production-probe/result.zh.md \
	  --freshness-state recorded

getdistances_production_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(GETDISTANCES_PRODUCTION_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(GETDISTANCES_PRODUCTION_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(GETDISTANCES_PRODUCTION_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

collect_select_error_tail_repeated_board_evidence:
	@rm -rf $(SELECT_ERROR_TAIL_REPEATED_BOARD_DIR)
	@for run in 01 02 03 04 05; do \
	  echo "[BOARD] select error-tail RVV-vs-RVV repeated run $$run -> $(SELECT_ERROR_TAIL_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(SELECT_ERROR_TAIL_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_TEST_ARGS="--gtest_filter=$(CIRCLE_SELECT_ERROR_TAIL_FILTER)" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase080-select-error-tail-run-$$run"; \
	done

generate_select_error_tail_board_evidence_manifest:
	@python3 script/generate_circle_board_evidence_manifest.py \
	  --mode select-error-tail \
	  --repeat-root $(SELECT_ERROR_TAIL_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_circle_rvv.full.asm \
	  --output $(SELECT_ERROR_TAIL_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_circle Phase 080 select error-tail RVV-vs-RVV repeated board summary"

run_select_error_tail_board_evidence_doctor: generate_select_error_tail_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(SELECT_ERROR_TAIL_EVIDENCE_MANIFEST) \
	  --output $(SELECT_ERROR_TAIL_EVIDENCE_DOCTOR_MD) \
	  --json-output $(SELECT_ERROR_TAIL_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_select_error_tail_board_evidence_state: run_select_error_tail_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(SELECT_ERROR_TAIL_EVIDENCE_MANIFEST) \
	  --doctor $(SELECT_ERROR_TAIL_EVIDENCE_DOCTOR_MD) \
	  --path $(SELECT_ERROR_TAIL_EVIDENCE_DOCTOR_JSON) \
	  --target run_select_error_tail_board_evidence_doctor \
	  --backend board \
	  --run-label $(SELECT_ERROR_TAIL_EVIDENCE_RUN_LABEL) \
	  --case-filter circle-phase080-select-error-tail-rvv-ab-repeated \
	  --evidence-role strict_ab \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/080-select-compressed-error-tail/result.zh.md \
	  --freshness-state recorded

select_error_tail_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(SELECT_ERROR_TAIL_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(SELECT_ERROR_TAIL_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(SELECT_ERROR_TAIL_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

collect_select_error_tail_production_repeated_board_evidence:
	@rm -rf $(SELECT_ERROR_TAIL_PRODUCTION_REPEATED_BOARD_DIR)
	@for run in 01 02 03 04 05; do \
	  echo "[BOARD] select error-tail production repeated run $$run -> $(SELECT_ERROR_TAIL_PRODUCTION_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(SELECT_ERROR_TAIL_PRODUCTION_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_TEST_ARGS="--gtest_filter=$(CIRCLE_PUBLIC_FILTER)" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase090-select-error-tail-production-run-$$run"; \
	done

generate_select_error_tail_production_board_evidence_manifest:
	@python3 script/generate_circle_board_evidence_manifest.py \
	  --mode select-error-tail-production \
	  --repeat-root $(SELECT_ERROR_TAIL_PRODUCTION_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_circle_rvv.full.asm \
	  --output $(SELECT_ERROR_TAIL_PRODUCTION_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_circle Phase 090 select error-tail production repeated board summary"

run_select_error_tail_production_board_evidence_doctor: generate_select_error_tail_production_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(SELECT_ERROR_TAIL_PRODUCTION_EVIDENCE_MANIFEST) \
	  --output $(SELECT_ERROR_TAIL_PRODUCTION_EVIDENCE_DOCTOR_MD) \
	  --json-output $(SELECT_ERROR_TAIL_PRODUCTION_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_select_error_tail_production_board_evidence_state: run_select_error_tail_production_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(SELECT_ERROR_TAIL_PRODUCTION_EVIDENCE_MANIFEST) \
	  --doctor $(SELECT_ERROR_TAIL_PRODUCTION_EVIDENCE_DOCTOR_MD) \
	  --path $(SELECT_ERROR_TAIL_PRODUCTION_EVIDENCE_DOCTOR_JSON) \
	  --target run_select_error_tail_production_board_evidence_doctor \
	  --backend board \
	  --run-label $(SELECT_ERROR_TAIL_PRODUCTION_EVIDENCE_RUN_LABEL) \
	  --case-filter circle-phase090-select-error-tail-production-repeated \
	  --evidence-role production_direct \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/090-select-error-tail-production-probe/result.zh.md \
	  --freshness-state recorded

select_error_tail_production_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(SELECT_ERROR_TAIL_PRODUCTION_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(SELECT_ERROR_TAIL_PRODUCTION_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(SELECT_ERROR_TAIL_PRODUCTION_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

check_getdistances_full_rvv_asm:
	@$(MAKE) -C $(CURDIR) clean_bench_rvv
	@$(MAKE) -C $(CURDIR) dump_bench_rvv
	@awk '/^[[:space:]]*[0-9a-f]+ <.*getDistancesToModelFullRVV.*>:/{in_symbol=1; next} in_symbol && /^[[:space:]]*[0-9a-f]+ </{exit} in_symbol{print}' \
	  build/asm/riscv/bench_sac_model_circle_rvv.full.asm | grep -q 'vfsqrt.v'
	@awk '/^[[:space:]]*[0-9a-f]+ <.*getDistancesToModelFullRVV.*>:/{in_symbol=1; next} in_symbol && /^[[:space:]]*[0-9a-f]+ </{exit} in_symbol{print}' \
	  build/asm/riscv/bench_sac_model_circle_rvv.full.asm | grep -q 'vfwcvt.f.f.v'
	@awk '/^[[:space:]]*[0-9a-f]+ <.*getDistancesToModelFullRVV.*>:/{in_symbol=1; next} in_symbol && /^[[:space:]]*[0-9a-f]+ </{exit} in_symbol{print}' \
	  build/asm/riscv/bench_sac_model_circle_rvv.full.asm | grep -q 'vse64.v'
	@echo "[DONE] getDistances full-RVV asm contains vfsqrt.v, vfwcvt and vse64.v"

check_getdistances_production_asm:
	@$(MAKE) -C $(CURDIR) clean_bench_rvv
	@$(MAKE) -C $(CURDIR) dump_bench_rvv
	@awk '/^[[:space:]]*[0-9a-f]+ <.*getDistancesToModelRVV.*>:/{in_symbol=1; next} in_symbol && /^[[:space:]]*[0-9a-f]+ </{exit} in_symbol{print}' \
	  build/asm/riscv/bench_sac_model_circle_rvv.full.asm | grep -q 'vfsqrt.v'
	@awk '/^[[:space:]]*[0-9a-f]+ <.*getDistancesToModelRVV.*>:/{in_symbol=1; next} in_symbol && /^[[:space:]]*[0-9a-f]+ </{exit} in_symbol{print}' \
	  build/asm/riscv/bench_sac_model_circle_rvv.full.asm | grep -q 'vfwcvt.f.f.v'
	@awk '/^[[:space:]]*[0-9a-f]+ <.*getDistancesToModelRVV.*>:/{in_symbol=1; next} in_symbol && /^[[:space:]]*[0-9a-f]+ </{exit} in_symbol{print}' \
		  build/asm/riscv/bench_sac_model_circle_rvv.full.asm | grep -q 'vse64.v'
	@echo "[DONE] production getDistances RVV asm contains vfsqrt.v, vfwcvt and vse64.v"

check_select_error_tail_asm:
	@$(MAKE) -C $(CURDIR) clean_bench_rvv
	@$(MAKE) -C $(CURDIR) dump_bench_rvv
	@python3 script/check_circle_select_error_tail_asm.py \
	  --asm build/asm/riscv/bench_sac_model_circle_rvv.full.asm \
	  --symbol selectWithinDistanceFullRVVErrorTail
	@echo "[DONE] select error-tail candidate asm contains vcompress.vm, vfsqrt.v, vfwcvt and vse64.v"

check_select_production_error_tail_asm:
	@$(MAKE) -C $(CURDIR) clean_bench_rvv
	@$(MAKE) -C $(CURDIR) dump_bench_rvv
	@python3 script/check_circle_select_error_tail_asm.py \
	  --asm build/asm/riscv/bench_sac_model_circle_rvv.full.asm \
	  --symbol selectWithinDistanceRVV
	@echo "[DONE] production select RVV asm contains vcompress.vm, vfsqrt.v, vfwcvt and vse64.v"

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
	@for run in 01 02 03 04 05; do \
	  echo "[BOARD] identity RVV-vs-RVV repeated run $$run -> $(IDENTITY_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) run_board_identity_rvv_ab_once \
	    OUTPUT_DIR_BOARD="$(IDENTITY_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase040-identity-run-$$run" \
	    REMOTE_BENCH_ARGS="$(BENCH_IDENTITY_ARGS)"; \
	done

collect_identity_shuffled_repeated_board_evidence:
	@rm -rf $(IDENTITY_SHUFFLED_REPEATED_BOARD_DIR)
	@for run in 01 02 03 04 05; do \
	  echo "[BOARD] shuffled RVV-vs-RVV repeated run $$run -> $(IDENTITY_SHUFFLED_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) run_board_identity_rvv_ab_once \
	    OUTPUT_DIR_BOARD="$(IDENTITY_SHUFFLED_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase040-shuffled-run-$$run" \
	    REMOTE_BENCH_ARGS="$(BENCH_SHUFFLED_ARGS)"; \
	done

generate_identity_board_evidence_manifest: guard_historical_identity_probe
	@python3 script/generate_circle_board_evidence_manifest.py \
	  --mode identity \
	  --repeat-root $(IDENTITY_REPEATED_BOARD_DIR) \
	  --baseline-asm build/asm/riscv/bench_sac_model_circle_rvv_gather_baseline.full.asm \
	  --asm build/asm/riscv/bench_sac_model_circle_rvv.full.asm \
	  --output $(IDENTITY_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_circle Phase 040 identity-index RVV-vs-RVV repeated board summary"

run_identity_board_evidence_doctor: generate_identity_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(IDENTITY_EVIDENCE_MANIFEST) \
	  --output $(IDENTITY_EVIDENCE_DOCTOR_MD) \
	  --json-output $(IDENTITY_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_identity_board_evidence_state: run_identity_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(IDENTITY_EVIDENCE_MANIFEST) \
	  --doctor $(IDENTITY_EVIDENCE_DOCTOR_MD) \
	  --path $(IDENTITY_EVIDENCE_DOCTOR_JSON) \
	  --target run_identity_board_evidence_doctor \
	  --backend board \
	  --run-label $(IDENTITY_EVIDENCE_RUN_LABEL) \
	  --case-filter circle-phase040-identity-rvv-ab-repeated \
	  --evidence-role strict_ab \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/040-production-closeout-and-identity-frontier/result.zh.md \
	  --freshness-state recorded

generate_identity_shuffled_board_evidence_manifest: guard_historical_identity_probe
	@python3 script/generate_circle_board_evidence_manifest.py \
	  --mode identity-shuffled \
	  --repeat-root $(IDENTITY_SHUFFLED_REPEATED_BOARD_DIR) \
	  --baseline-asm build/asm/riscv/bench_sac_model_circle_rvv_gather_baseline.full.asm \
	  --asm build/asm/riscv/bench_sac_model_circle_rvv.full.asm \
	  --output $(IDENTITY_SHUFFLED_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_circle Phase 040 shuffled-index RVV-vs-RVV repeated board summary"

run_identity_shuffled_board_evidence_doctor: generate_identity_shuffled_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(IDENTITY_SHUFFLED_EVIDENCE_MANIFEST) \
	  --output $(IDENTITY_SHUFFLED_EVIDENCE_DOCTOR_MD) \
	  --json-output $(IDENTITY_SHUFFLED_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_identity_shuffled_board_evidence_state: run_identity_shuffled_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(IDENTITY_SHUFFLED_EVIDENCE_MANIFEST) \
	  --doctor $(IDENTITY_SHUFFLED_EVIDENCE_DOCTOR_MD) \
	  --path $(IDENTITY_SHUFFLED_EVIDENCE_DOCTOR_JSON) \
	  --target run_identity_shuffled_board_evidence_doctor \
	  --backend board \
	  --run-label $(IDENTITY_SHUFFLED_EVIDENCE_RUN_LABEL) \
	  --case-filter circle-phase040-shuffled-rvv-ab-repeated \
	  --evidence-role strict_ab \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/040-production-closeout-and-identity-frontier/result.zh.md \
	  --freshness-state recorded

identity_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(IDENTITY_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(IDENTITY_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(IDENTITY_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

guard_historical_identity_probe:
	@if [ "$(ALLOW_HISTORICAL_IDENTITY_PROBE)" != "1" ]; then \
	  echo "Phase 040 identity-strided targets are historical probe-only."; \
	  echo "Current production source is gather-only after board A/B rejected the identity candidate."; \
	  echo "Set ALLOW_HISTORICAL_IDENTITY_PROBE=1 only when intentionally restoring the historical candidate for replay."; \
	  exit 2; \
	fi

check_identity_strided_asm: guard_historical_identity_probe
	@$(MAKE) -C $(CURDIR) clean_bench_rvv
	@$(MAKE) -C $(CURDIR) dump_bench_rvv
	@python3 script/check_circle_identity_strided_asm.py \
	  --asm build/asm/riscv/bench_sac_model_circle_rvv.full.asm

build_identity_gather_baseline: guard_historical_identity_probe
	@rm -f $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE) \
	  $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE).full.asm \
	  $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE).asm
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 \
	  TARGET_BENCH=$(TARGET_BENCH_RVV_GATHER_BASELINE) \
	  EXTRA_CXXFLAGS="-DPCL_RVV_CIRCLE_DISABLE_IDENTITY_STRIDED" \
	  $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE)

dump_identity_gather_baseline: build_identity_gather_baseline | $(ASM_DIR)/$(ARCH)
	$(OBJDUMP) -d -C $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE) > $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE).full.asm
	grep -E $(VEC_REGEX_STR) $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE).full.asm > $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE).asm || true
	@echo "[DONE] RVV gather baseline asm dump: $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV_GATHER_BASELINE).asm"

.PHONY: run_circle_public_tests run_circle_select_error_tail_candidate_test run_circle_getdistances_candidate_test run_circle_getdistances_full_rvv_candidate_test run_circle_getdistances_production_test run_board_circle_public_tests run_board_circle_select_error_tail_candidate_test run_board_circle_getdistances_candidate_test run_board_circle_getdistances_full_rvv_candidate_test run_board_circle_getdistances_production_test collect_production_repeated_board_evidence generate_production_board_evidence_manifest run_production_board_evidence_doctor record_production_board_evidence_state production_evidence_status collect_getdistances_repeated_board_evidence generate_getdistances_board_evidence_manifest run_getdistances_board_evidence_doctor record_getdistances_board_evidence_state getdistances_evidence_status collect_getdistances_full_rvv_repeated_board_evidence generate_getdistances_full_rvv_board_evidence_manifest run_getdistances_full_rvv_board_evidence_doctor record_getdistances_full_rvv_board_evidence_state getdistances_full_rvv_evidence_status collect_getdistances_production_repeated_board_evidence generate_getdistances_production_board_evidence_manifest run_getdistances_production_board_evidence_doctor record_getdistances_production_board_evidence_state getdistances_production_evidence_status collect_select_error_tail_repeated_board_evidence generate_select_error_tail_board_evidence_manifest run_select_error_tail_board_evidence_doctor record_select_error_tail_board_evidence_state select_error_tail_evidence_status collect_select_error_tail_production_repeated_board_evidence generate_select_error_tail_production_board_evidence_manifest run_select_error_tail_production_board_evidence_doctor record_select_error_tail_production_board_evidence_state select_error_tail_production_evidence_status check_getdistances_full_rvv_asm check_getdistances_production_asm check_select_error_tail_asm check_select_production_error_tail_asm deploy_identity_gather_baseline deploy_identity_candidate_and_baseline run_board_identity_rvv_ab_once collect_identity_repeated_board_evidence collect_identity_shuffled_repeated_board_evidence generate_identity_board_evidence_manifest run_identity_board_evidence_doctor record_identity_board_evidence_state generate_identity_shuffled_board_evidence_manifest run_identity_shuffled_board_evidence_doctor record_identity_shuffled_board_evidence_state identity_evidence_status guard_historical_identity_probe check_identity_strided_asm build_identity_gather_baseline dump_identity_gather_baseline
