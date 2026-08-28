# =============================================================================
# PCL sample_consensus/sac_model_cylinder RVV topic Makefile
#
# 本 topic 使用共享 rvv-topic.mk；QEMU 只跑 correctness / log-shape
# （正确性 / 日志形状），性能结论只来自 board（板卡）或目标硬件。
# =============================================================================

TOPIC  := sac_model_cylinder
MODULE := sample_consensus
TOPIC_DOC_PREFIX := test-rvv/sample_consensus/sac_model_cylinder

EIGEN_RVV_FLAGS := -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=2 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl

FOCUS_DIR      := pcl/sample_consensus/include/pcl/sample_consensus
FILTER_REPORT  := log/filtered_sac_model_cylinder.log
ANALYZE_REPORT := log/analyze_sac_model_cylinder.log

TARGET_TEST     := test_sac_model_cylinder
TARGET_TEST_STD := test_sac_model_cylinder_std
TARGET_TEST_RVV := test_sac_model_cylinder_rvv
SRCS_TEST       := src/test_sac_model_cylinder.cpp

TARGET_BENCH     := bench_sac_model_cylinder
TARGET_BENCH_STD := bench_sac_model_cylinder_std
TARGET_BENCH_RVV := bench_sac_model_cylinder_rvv
SRCS_BENCH       := src/bench_sac_model_cylinder.cpp
BENCH_ARGS       ?= 65536 200 shuffled

REMOTE_DIR ?= /root/pcl-test/sample_consensus/sac_model_cylinder
REMOTE_BENCH_ARGS ?= $(BENCH_ARGS)
REMOTE_TEST_ARGS ?=

CYLINDER_COUNT_SELECT_FILTER := SampleConsensusModelCylinder.CountCandidateMatchesPublicEntryWithEarlyEuclidGate:SampleConsensusModelCylinder.SelectCandidatePreservesOrderAndErrorDistances:SampleConsensusModelCylinder.SelectCandidateClearsStaleStateWhenNoInliers
CYLINDER_PRODUCTION_FILTER := SampleConsensusModelCylinder.CountPublicEntryMatchesProductionStandardHelper:SampleConsensusModelCylinder.SelectPublicEntryMatchesProductionStandardHelper:SampleConsensusModelCylinder.GetDistancesPublicEntryMatchesProductionStandardHelper:SampleConsensusModelCylinder.PublicEntryFallsBackWhenNormalsDoNotCoverInput:SampleConsensusModelCylinder.AdditionalSourcePointTypesMatchStandardPath:SampleConsensusModelCylinder.AdditionalNormalPointTypesMatchStandardPath:SampleConsensusModelCylinder.AdditionalPointTypesBenchShapedMatchStandardPath:SampleConsensusModelCylinder.SelectCandidateClearsStaleStateWhenNoInliers
PHASE000_DIR := doc/phases/000-cylinder-count-select-diagnostic
PHASE020_DIR := doc/phases/020-cylinder-production-integration
REPEATED_BOARD_DIR ?= log/board/repeated-phase000-cylinder
PRODUCTION_REPEATED_BOARD_DIR ?= log/board/repeated-phase020-cylinder-production
REPEATED_EVIDENCE_MANIFEST := $(PHASE000_DIR)/repeated-evidence-manifest.json
REPEATED_EVIDENCE_DOCTOR_MD := $(PHASE000_DIR)/repeated-evidence-doctor.md
REPEATED_EVIDENCE_DOCTOR_JSON := $(PHASE000_DIR)/repeated-evidence-doctor.json
PRODUCTION_EVIDENCE_MANIFEST := $(PHASE020_DIR)/production-repeated-evidence-manifest.json
PRODUCTION_EVIDENCE_DOCTOR_MD := $(PHASE020_DIR)/production-repeated-evidence-doctor.md
PRODUCTION_EVIDENCE_DOCTOR_JSON := $(PHASE020_DIR)/production-repeated-evidence-doctor.json
EVIDENCE_REGISTRY := log/evidence_registry.json
REPEATED_EVIDENCE_RUN_LABEL := cylinder-phase000-repeated-board
PRODUCTION_EVIDENCE_RUN_LABEL := cylinder-phase020-production-repeated-board
EVIDENCE_DOC_REFS := \
	$(TOPIC_DOC_PREFIX)/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/sac_model_cylinder-evaluation.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-roadmap.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/optimization-matrix.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/000-cylinder-count-select-diagnostic/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/000-cylinder-count-select-diagnostic/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-cylinder-production-integration/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-cylinder-production-integration/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/030-cylinder-getdistances-dense-output/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/030-cylinder-getdistances-dense-output/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/040-cylinder-point-type-expansion/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/040-cylinder-point-type-expansion/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/benchmark-and-evidence.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-evidence.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/testing-overview.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/correctness-tests.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/test-support-code-map.zh.md \
	doc-rvv/sample_consensus/sac_model_cylinder-RVV.zh.md
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

run_cylinder_count_select_tests:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=$(CYLINDER_COUNT_SELECT_FILTER)"

check_diagnostic_asm: dump_bench_rvv
	@python3 script/check_cylinder_diagnostic_asm.py \
	  --asm build/asm/riscv/bench_sac_model_cylinder_rvv.full.asm

check_production_asm: dump_bench_rvv
	@python3 script/check_cylinder_production_asm.py \
	  --asm build/asm/riscv/bench_sac_model_cylinder_rvv.full.asm

run_board_cylinder_count_select_tests: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' REMOTE_TEST_ARGS='--gtest_filter=$(CYLINDER_COUNT_SELECT_FILTER)'"

run_board_cylinder_production_tests: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' REMOTE_TEST_ARGS='--gtest_filter=$(CYLINDER_PRODUCTION_FILTER)'"

collect_repeated_board_evidence:
	@rm -rf $(REPEATED_BOARD_DIR)
	@set -e; \
	for run in 01 02 03 04 05; do \
	  echo "[BOARD] cylinder count/select repeated run $$run -> $(REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase000-cylinder-run-$$run"; \
	done

generate_board_evidence_manifest:
	@python3 script/generate_cylinder_board_evidence_manifest.py \
	  --repeat-root $(REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_cylinder_rvv.full.asm \
	  --output $(REPEATED_EVIDENCE_MANIFEST)

collect_production_repeated_board_evidence:
	@rm -rf $(PRODUCTION_REPEATED_BOARD_DIR)
	@set -e; \
	for run in 01 02 03 04 05; do \
	  echo "[BOARD] cylinder production repeated run $$run -> $(PRODUCTION_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(PRODUCTION_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase020-cylinder-production-run-$$run"; \
	done

generate_production_board_evidence_manifest:
	@python3 script/generate_cylinder_board_evidence_manifest.py \
	  --mode production \
	  --repeat-root $(PRODUCTION_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_cylinder_rvv.full.asm \
	  --output $(PRODUCTION_EVIDENCE_MANIFEST) \
	  --items count,select,getdistances,pointtypes

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
	  --case-filter cylinder-count-select-getdistances-pointtypes-production-repeated \
	  --evidence-role production_direct \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/020-cylinder-production-integration/result.zh.md \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/030-cylinder-getdistances-dense-output/result.zh.md \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/040-cylinder-point-type-expansion/result.zh.md \
	  --doc-ref doc-rvv/sample_consensus/sac_model_cylinder-RVV.zh.md \
	  --freshness-state recorded

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
	  --case-filter cylinder-count-select-phase000-repeated \
	  --evidence-role production_shaped_diagnostic \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/000-cylinder-count-select-diagnostic/result.zh.md \
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

production_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PRODUCTION_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PRODUCTION_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PRODUCTION_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any
