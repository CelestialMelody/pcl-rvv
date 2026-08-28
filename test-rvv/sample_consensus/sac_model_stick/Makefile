# =============================================================================
# PCL sample_consensus/sac_model_stick RVV topic Makefile
#
# 本 topic 使用共享 rvv-topic.mk；QEMU 只跑 correctness / log-shape
# （正确性 / 日志形状），性能结论只来自 board 或目标硬件。
# =============================================================================

TOPIC  := sac_model_stick
MODULE := sample_consensus
TOPIC_DOC_PREFIX := test-rvv/sample_consensus/sac_model_stick

EIGEN_RVV_FLAGS := -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=2 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl

FOCUS_DIR      := pcl/sample_consensus/include/pcl/sample_consensus
FILTER_REPORT  := log/filtered_sac_model_stick.log
ANALYZE_REPORT := log/analyze_sac_model_stick.log

TARGET_TEST     := test_sac_model_stick
TARGET_TEST_STD := test_sac_model_stick_std
TARGET_TEST_RVV := test_sac_model_stick_rvv
SRCS_TEST       := src/test_sac_model_stick.cpp

TARGET_BENCH     := bench_sac_model_stick
TARGET_BENCH_STD := bench_sac_model_stick_std
TARGET_BENCH_RVV := bench_sac_model_stick_rvv
SRCS_BENCH       := src/bench_sac_model_stick.cpp
BENCH_ARGS       ?= 65536 200

REMOTE_DIR ?= /root/pcl-test/sample_consensus/sac_model_stick
REMOTE_BENCH_ARGS ?= $(BENCH_ARGS)
REMOTE_TEST_ARGS ?=

STICK_COUNT_FILTER := SampleConsensusModelStick.CountCandidateMatchesPublicEntryWithInnerOuterPenalty:SampleConsensusModelStick.CountCandidateReturnsZeroWhenOuterBandDominates:SampleConsensusModelStick.CountPublicEntryPreservesInnerOuterPenalty
STICK_SELECT_FILTER := SampleConsensusModelStick.SelectCandidateMatchesPublicEntryAndErrorDistances:SampleConsensusModelStick.SelectCandidateClearsStaleErrorDistancesWhenNoInliers:SampleConsensusModelStick.SelectPublicEntryPreservesOrderAndErrorDistances:SampleConsensusModelStick.SelectPublicEntryClearsStaleStateWhenNoInliers
STICK_GETDISTANCES_FILTER := SampleConsensusModelStick.GetDistancesCandidateMatchesPublicDirectionCoefficientSemantics:SampleConsensusModelStick.GetDistancesCandidatePreservesPenaltyAndDenseIndexedOrder:SampleConsensusModelStick.GetDistancesPublicEntryPreservesDirectionPenaltyAndOrder
STICK_POINT_TYPE_FILTER := SampleConsensusModelStick.AdditionalAoSPointTypesMatchStandardPath
PHASE000_DIR := doc/phases/000-stick-count-diagnostic
PHASE020_DIR := doc/phases/020-stick-select-diagnostic
PHASE040_DIR := doc/phases/040-stick-getdistances-diagnostic
PHASE080_DIR := doc/phases/080-stick-production-integration
PHASE100_DIR := doc/phases/100-stick-getdistances-vector-writeback
REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase040
REPEATED_EVIDENCE_MANIFEST := $(PHASE040_DIR)/repeated-evidence-manifest.json
REPEATED_EVIDENCE_DOCTOR_MD := $(PHASE040_DIR)/repeated-evidence-doctor.md
REPEATED_EVIDENCE_DOCTOR_JSON := $(PHASE040_DIR)/repeated-evidence-doctor.json
PRODUCTION_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase080-production
PRODUCTION_EVIDENCE_MANIFEST := $(PHASE080_DIR)/production-repeated-evidence-manifest.json
PRODUCTION_EVIDENCE_DOCTOR_MD := $(PHASE080_DIR)/production-repeated-evidence-doctor.md
PRODUCTION_EVIDENCE_DOCTOR_JSON := $(PHASE080_DIR)/production-repeated-evidence-doctor.json
VECTOR_WRITEBACK_BOARD_DIR ?= log/board/repeated-20260828-phase100-vector-writeback
VECTOR_WRITEBACK_EVIDENCE_MANIFEST := $(PHASE100_DIR)/vector-writeback-evidence-manifest.json
VECTOR_WRITEBACK_EVIDENCE_DOCTOR_MD := $(PHASE100_DIR)/vector-writeback-evidence-doctor.md
VECTOR_WRITEBACK_EVIDENCE_DOCTOR_JSON := $(PHASE100_DIR)/vector-writeback-evidence-doctor.json
EVIDENCE_REGISTRY := log/evidence_registry.json
REPEATED_EVIDENCE_RUN_LABEL := stick-phase040-repeated-board
PRODUCTION_EVIDENCE_RUN_LABEL := stick-phase080-production-repeated-board
VECTOR_WRITEBACK_EVIDENCE_RUN_LABEL := stick-phase100-vector-writeback-board
EVIDENCE_DOC_REFS := \
	$(TOPIC_DOC_PREFIX)/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/sac_model_stick-evaluation.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/testing-overview.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/correctness-tests.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/benchmark-and-evidence.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-evidence.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/test-support-code-map.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-roadmap.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/optimization-matrix.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/000-stick-count-diagnostic/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/000-stick-count-diagnostic/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-stick-select-diagnostic/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-stick-select-diagnostic/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/040-stick-getdistances-diagnostic/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/040-stick-getdistances-diagnostic/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/050-stick-getdistances-production-integration-plan/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/060-stick-doc-suite-structure/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/060-stick-doc-suite-structure/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/070-stick-getdistances-target-granularity/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/070-stick-getdistances-target-granularity/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/080-stick-production-integration/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/080-stick-production-integration/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/090-stick-point-type-expansion/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/090-stick-point-type-expansion/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/100-stick-getdistances-vector-writeback/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/100-stick-getdistances-vector-writeback/result.zh.md \
	doc-rvv/sample_consensus/sac_model_stick-RVV.zh.md \
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

run_stick_count_tests:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=$(STICK_COUNT_FILTER)"

run_stick_select_tests:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=$(STICK_SELECT_FILTER)"

run_stick_getdistances_tests:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=$(STICK_GETDISTANCES_FILTER)"

run_stick_point_type_tests:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=$(STICK_POINT_TYPE_FILTER)"

check_production_asm: dump_bench_rvv
	@python3 script/check_stick_production_asm.py \
	  --asm build/asm/riscv/bench_sac_model_stick_rvv.full.asm

run_board_stick_count_tests: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' REMOTE_TEST_ARGS='--gtest_filter=$(STICK_COUNT_FILTER)'"

collect_repeated_board_evidence:
	@rm -rf $(REPEATED_BOARD_DIR)
	@for run in 01 02 03 04 05; do \
	  echo "[BOARD] stick repeated run $$run -> $(REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase040-stick-getdistances-run-$$run"; \
	done

generate_board_evidence_manifest:
	@python3 script/generate_stick_board_evidence_manifest.py \
	  --repeat-root $(REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_stick_rvv.full.asm \
	  --output $(REPEATED_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_stick Phase 040 repeated board summary" \
	  --summary-role production_shaped_diagnostic \
	  --focus getdistances

run_repeated_board_evidence_doctor: generate_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(REPEATED_EVIDENCE_MANIFEST) \
	  --output $(REPEATED_EVIDENCE_DOCTOR_MD) \
	  --json-output $(REPEATED_EVIDENCE_DOCTOR_JSON) \
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
	  --case-filter stick-getdistances-phase040-repeated \
	  --evidence-role production_shaped_diagnostic \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/040-stick-getdistances-diagnostic/result.zh.md \
	  --freshness-state recorded

collect_production_repeated_board_evidence:
	@rm -rf $(PRODUCTION_REPEATED_BOARD_DIR)
	@for run in 01 02 03 04 05; do \
	  echo "[BOARD] stick production repeated run $$run -> $(PRODUCTION_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(PRODUCTION_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase080-stick-production-run-$$run"; \
	done

generate_production_board_evidence_manifest:
	@python3 script/generate_stick_board_evidence_manifest.py \
	  --repeat-root $(PRODUCTION_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_stick_rvv.full.asm \
	  --output $(PRODUCTION_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_stick Phase 080 production repeated board summary" \
	  --summary-role production_direct \
	  --focus production

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
	  --case-filter stick-phase080-production-repeated \
	  --evidence-role production_direct \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/080-stick-production-integration/result.zh.md \
	  --freshness-state recorded

collect_vector_writeback_board_evidence:
	@rm -rf $(VECTOR_WRITEBACK_BOARD_DIR)
	@for run in 01 02 03 04 05; do \
	  echo "[BOARD] stick vector-writeback repeated run $$run -> $(VECTOR_WRITEBACK_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(VECTOR_WRITEBACK_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase100-stick-vector-writeback-run-$$run"; \
	done

generate_vector_writeback_board_evidence_manifest:
	@python3 script/generate_stick_board_evidence_manifest.py \
	  --repeat-root $(VECTOR_WRITEBACK_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_stick_rvv.full.asm \
	  --output $(VECTOR_WRITEBACK_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_stick Phase 100 vector writeback repeated board summary" \
	  --summary-role production_direct \
	  --focus production-getdistances

run_vector_writeback_board_evidence_doctor: generate_vector_writeback_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(VECTOR_WRITEBACK_EVIDENCE_MANIFEST) \
	  --output $(VECTOR_WRITEBACK_EVIDENCE_DOCTOR_MD) \
	  --json-output $(VECTOR_WRITEBACK_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_vector_writeback_board_evidence_state: run_vector_writeback_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(VECTOR_WRITEBACK_EVIDENCE_MANIFEST) \
	  --doctor $(VECTOR_WRITEBACK_EVIDENCE_DOCTOR_MD) \
	  --path $(VECTOR_WRITEBACK_EVIDENCE_DOCTOR_JSON) \
	  --target run_vector_writeback_board_evidence_doctor \
	  --backend board \
	  --run-label $(VECTOR_WRITEBACK_EVIDENCE_RUN_LABEL) \
	  --case-filter stick-phase100-getdistances-vector-writeback \
	  --evidence-role production_direct \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/100-stick-getdistances-vector-writeback/result.zh.md \
	  --freshness-state recorded

vector_writeback_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(VECTOR_WRITEBACK_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(VECTOR_WRITEBACK_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(VECTOR_WRITEBACK_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

production_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PRODUCTION_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PRODUCTION_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PRODUCTION_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

repeated_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

.PHONY: run_stick_count_tests run_stick_select_tests run_stick_getdistances_tests run_stick_point_type_tests check_production_asm run_board_stick_count_tests collect_repeated_board_evidence generate_board_evidence_manifest run_repeated_board_evidence_doctor record_repeated_board_evidence_state collect_production_repeated_board_evidence generate_production_board_evidence_manifest run_production_board_evidence_doctor record_production_board_evidence_state collect_vector_writeback_board_evidence generate_vector_writeback_board_evidence_manifest run_vector_writeback_board_evidence_doctor record_vector_writeback_board_evidence_state vector_writeback_evidence_status production_evidence_status repeated_evidence_status
