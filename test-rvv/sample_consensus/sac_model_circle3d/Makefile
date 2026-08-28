# =============================================================================
# PCL sample_consensus/sac_model_circle3d RVV topic Makefile
#
# 本 topic 使用共享 rvv-topic.mk。QEMU（仿真器）只用于 correctness
# （正确性）、路径和日志形状；性能结论只来自 board（板卡）或目标硬件。
# =============================================================================

TOPIC  := sac_model_circle3d
MODULE := sample_consensus
TOPIC_DOC_PREFIX := test-rvv/sample_consensus/sac_model_circle3d

EIGEN_RVV_FLAGS := -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=2 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl

FOCUS_DIR      := pcl/sample_consensus/include/pcl/sample_consensus
FILTER_REPORT  := log/filtered_sac_model_circle3d.log
ANALYZE_REPORT := log/analyze_sac_model_circle3d.log

TARGET_TEST     := test_sac_model_circle3d
TARGET_TEST_STD := test_sac_model_circle3d_std
TARGET_TEST_RVV := test_sac_model_circle3d_rvv
SRCS_TEST       := src/test_sac_model_circle3d.cpp

TARGET_BENCH     := bench_sac_model_circle3d
TARGET_BENCH_STD := bench_sac_model_circle3d_std
TARGET_BENCH_RVV := bench_sac_model_circle3d_rvv
SRCS_BENCH       := src/bench_sac_model_circle3d.cpp
BENCH_ARGS       ?= 65536 200 20

REMOTE_DIR ?= /root/pcl-test/sample_consensus/sac_model_circle3d
REMOTE_BENCH_ARGS ?= $(BENCH_ARGS)
REMOTE_TEST_ARGS ?=

CIRCLE3D_PHASE000_FILTER := SampleConsensusModelCircle3D.CountAndSelectCandidateMatchesPublicPath:SampleConsensusModelCircle3D.DegenerateProjectionFallsBackToScalarPath
PHASE000_DIR := doc/phases/000-circle3d-projection-component-ablation
PHASE010_DIR := doc/phases/010-selectwithin-production-probe
PHASE020_DIR := doc/phases/020-select-point-type-expansion
REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase000-circle3d-projection
REPEATED_EVIDENCE_MANIFEST := $(PHASE000_DIR)/projection-repeated-evidence-manifest.json
REPEATED_EVIDENCE_DOCTOR_MD := $(PHASE000_DIR)/projection-repeated-evidence-doctor.md
REPEATED_EVIDENCE_DOCTOR_JSON := $(PHASE000_DIR)/projection-repeated-evidence-doctor.json
EVIDENCE_REGISTRY := log/evidence_registry.json
REPEATED_EVIDENCE_RUN_LABEL := circle3d-phase000-projection-component-repeated-board
SELECT_PRODUCTION_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase010-circle3d-select-production
SELECT_PRODUCTION_EVIDENCE_MANIFEST := $(PHASE010_DIR)/select-production-repeated-evidence-manifest.json
SELECT_PRODUCTION_EVIDENCE_DOCTOR_MD := $(PHASE010_DIR)/select-production-repeated-evidence-doctor.md
SELECT_PRODUCTION_EVIDENCE_DOCTOR_JSON := $(PHASE010_DIR)/select-production-repeated-evidence-doctor.json
SELECT_PRODUCTION_EVIDENCE_RUN_LABEL := circle3d-phase010-select-production-repeated-board
SELECT_XYZI_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase020-select-xyzi
SELECT_XYZI_EVIDENCE_MANIFEST := $(PHASE020_DIR)/select-xyzi-repeated-evidence-manifest.json
SELECT_XYZI_EVIDENCE_DOCTOR_MD := $(PHASE020_DIR)/select-xyzi-repeated-evidence-doctor.md
SELECT_XYZI_EVIDENCE_DOCTOR_JSON := $(PHASE020_DIR)/select-xyzi-repeated-evidence-doctor.json
SELECT_XYZI_EVIDENCE_RUN_LABEL := circle3d-phase020-select-xyzi-repeated-board
SELECT_XYZRGB_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase020-select-xyzrgb
SELECT_XYZRGB_EVIDENCE_MANIFEST := $(PHASE020_DIR)/select-xyzrgb-repeated-evidence-manifest.json
SELECT_XYZRGB_EVIDENCE_DOCTOR_MD := $(PHASE020_DIR)/select-xyzrgb-repeated-evidence-doctor.md
SELECT_XYZRGB_EVIDENCE_DOCTOR_JSON := $(PHASE020_DIR)/select-xyzrgb-repeated-evidence-doctor.json
SELECT_XYZRGB_EVIDENCE_RUN_LABEL := circle3d-phase020-select-xyzrgb-repeated-board
SELECT_XYZRGBA_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase020-select-xyzrgba
SELECT_XYZRGBA_EVIDENCE_MANIFEST := $(PHASE020_DIR)/select-xyzrgba-repeated-evidence-manifest.json
SELECT_XYZRGBA_EVIDENCE_DOCTOR_MD := $(PHASE020_DIR)/select-xyzrgba-repeated-evidence-doctor.md
SELECT_XYZRGBA_EVIDENCE_DOCTOR_JSON := $(PHASE020_DIR)/select-xyzrgba-repeated-evidence-doctor.json
SELECT_XYZRGBA_EVIDENCE_RUN_LABEL := circle3d-phase020-select-xyzrgba-repeated-board
EVIDENCE_DOC_REFS := \
	$(TOPIC_DOC_PREFIX)/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/sac_model_circle3d-evaluation.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/testing-overview.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/correctness-tests.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/benchmark-and-evidence.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-evidence.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/test-support-code-map.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-roadmap.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/optimization-matrix.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/000-circle3d-projection-component-ablation/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/000-circle3d-projection-component-ablation/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/010-selectwithin-production-probe/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/010-selectwithin-production-probe/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-select-point-type-expansion/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-select-point-type-expansion/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/030-rollback-no-production-closeout/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/030-rollback-no-production-closeout/result.zh.md \
	doc-rvv/library-screening/sample_consensus/sample_consensus-retained-candidate-rescreen.zh.md
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

run_circle3d_phase000_tests:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=$(CIRCLE3D_PHASE000_FILTER)"

run_board_circle3d_phase000_tests: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' REMOTE_TEST_ARGS='--gtest_filter=$(CIRCLE3D_PHASE000_FILTER)'"

check_projection_asm: dump_bench_rvv
	@python3 script/check_circle3d_projection_asm.py \
	  --asm build/asm/riscv/bench_sac_model_circle3d_rvv.full.asm

collect_projection_repeated_board_evidence:
	@rm -rf $(REPEATED_BOARD_DIR)
	@set -e; \
	for run in 01 02 03 04 05; do \
	  echo "[BOARD] circle3d projection repeated run $$run -> $(REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase000-circle3d-projection-run-$$run"; \
	done

generate_projection_board_evidence_manifest:
	@python3 script/generate_circle3d_board_evidence_manifest.py \
	  --repeat-root $(REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_circle3d_rvv.full.asm \
	  --output $(REPEATED_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_circle3d Phase 000 projection component repeated board summary" \
	  --mode component

run_projection_board_evidence_doctor: generate_projection_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(REPEATED_EVIDENCE_MANIFEST) \
	  --output $(REPEATED_EVIDENCE_DOCTOR_MD) \
	  --json-output $(REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_projection_board_evidence_state: run_projection_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(REPEATED_EVIDENCE_MANIFEST) \
	  --doctor $(REPEATED_EVIDENCE_DOCTOR_MD) \
	  --path $(REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --target run_projection_board_evidence_doctor \
	  --backend board \
	  --run-label $(REPEATED_EVIDENCE_RUN_LABEL) \
	  --case-filter circle3d-projection-phase000-component-repeated \
	  --evidence-role component_ablation \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/000-circle3d-projection-component-ablation/result.zh.md \
	  --freshness-state recorded

projection_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

collect_select_production_repeated_board_evidence:
	@rm -rf $(SELECT_PRODUCTION_REPEATED_BOARD_DIR)
	@set -e; \
	for run in 01 02 03 04 05 06 07 08 09 10; do \
	  echo "[BOARD] circle3d select production repeated run $$run -> $(SELECT_PRODUCTION_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(SELECT_PRODUCTION_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase010-circle3d-select-production-run-$$run"; \
	done

generate_select_production_board_evidence_manifest:
	@python3 script/generate_circle3d_board_evidence_manifest.py \
	  --repeat-root $(SELECT_PRODUCTION_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_circle3d_rvv.full.asm \
	  --output $(SELECT_PRODUCTION_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_circle3d Phase 010 selectWithinDistance production repeated board summary" \
	  --mode production-select

run_select_production_board_evidence_doctor: generate_select_production_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(SELECT_PRODUCTION_EVIDENCE_MANIFEST) \
	  --output $(SELECT_PRODUCTION_EVIDENCE_DOCTOR_MD) \
	  --json-output $(SELECT_PRODUCTION_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_select_production_board_evidence_state: run_select_production_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(SELECT_PRODUCTION_EVIDENCE_MANIFEST) \
	  --doctor $(SELECT_PRODUCTION_EVIDENCE_DOCTOR_MD) \
	  --path $(SELECT_PRODUCTION_EVIDENCE_DOCTOR_JSON) \
	  --target run_select_production_board_evidence_doctor \
	  --backend board \
	  --run-label $(SELECT_PRODUCTION_EVIDENCE_RUN_LABEL) \
	  --case-filter circle3d-select-phase010-production-repeated \
	  --evidence-role production-public \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/010-selectwithin-production-probe/result.zh.md \
	  --freshness-state recorded

select_production_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(SELECT_PRODUCTION_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(SELECT_PRODUCTION_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(SELECT_PRODUCTION_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

collect_select_xyzi_repeated_board_evidence:
	@rm -rf $(SELECT_XYZI_REPEATED_BOARD_DIR)
	@set -e; \
	for run in 01 02 03 04 05; do \
	  echo "[BOARD] circle3d select PointXYZI repeated run $$run -> $(SELECT_XYZI_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    BENCH_ARGS="65536 200 20 PointXYZI" \
	    OUTPUT_DIR_BOARD="$(SELECT_XYZI_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase020-circle3d-select-xyzi-run-$$run"; \
	done

generate_select_xyzi_board_evidence_manifest:
	@python3 script/generate_circle3d_board_evidence_manifest.py \
	  --repeat-root $(SELECT_XYZI_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_circle3d_rvv.full.asm \
	  --output $(SELECT_XYZI_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_circle3d Phase 020 PointXYZI select production repeated board summary" \
	  --candidate-gate "pre_narrowing_RVVXYZAoSFloatLayout_PointT_signed_i32_index_u32_offset_non_degenerate_projection" \
	  --mode production-select

run_select_xyzi_board_evidence_doctor: generate_select_xyzi_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(SELECT_XYZI_EVIDENCE_MANIFEST) \
	  --output $(SELECT_XYZI_EVIDENCE_DOCTOR_MD) \
	  --json-output $(SELECT_XYZI_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_select_xyzi_board_evidence_state: run_select_xyzi_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(SELECT_XYZI_EVIDENCE_MANIFEST) \
	  --doctor $(SELECT_XYZI_EVIDENCE_DOCTOR_MD) \
	  --path $(SELECT_XYZI_EVIDENCE_DOCTOR_JSON) \
	  --target run_select_xyzi_board_evidence_doctor \
	  --backend board \
	  --run-label $(SELECT_XYZI_EVIDENCE_RUN_LABEL) \
	  --case-filter circle3d-select-phase020-xyzi-repeated \
	  --evidence-role production-public \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/020-select-point-type-expansion/result.zh.md \
	  --freshness-state recorded

select_xyzi_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(SELECT_XYZI_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(SELECT_XYZI_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(SELECT_XYZI_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

collect_select_xyzrgb_repeated_board_evidence:
	@rm -rf $(SELECT_XYZRGB_REPEATED_BOARD_DIR)
	@set -e; \
	for run in 01 02 03 04 05; do \
	  echo "[BOARD] circle3d select PointXYZRGB repeated run $$run -> $(SELECT_XYZRGB_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    BENCH_ARGS="65536 200 20 PointXYZRGB" \
	    OUTPUT_DIR_BOARD="$(SELECT_XYZRGB_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase020-circle3d-select-xyzrgb-run-$$run"; \
	done

generate_select_xyzrgb_board_evidence_manifest:
	@python3 script/generate_circle3d_board_evidence_manifest.py \
	  --repeat-root $(SELECT_XYZRGB_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_circle3d_rvv.full.asm \
	  --output $(SELECT_XYZRGB_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_circle3d Phase 020 PointXYZRGB select production repeated board summary" \
	  --candidate-gate "pre_narrowing_RVVXYZAoSFloatLayout_PointT_signed_i32_index_u32_offset_non_degenerate_projection" \
	  --mode production-select

run_select_xyzrgb_board_evidence_doctor: generate_select_xyzrgb_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(SELECT_XYZRGB_EVIDENCE_MANIFEST) \
	  --output $(SELECT_XYZRGB_EVIDENCE_DOCTOR_MD) \
	  --json-output $(SELECT_XYZRGB_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_select_xyzrgb_board_evidence_state: run_select_xyzrgb_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(SELECT_XYZRGB_EVIDENCE_MANIFEST) \
	  --doctor $(SELECT_XYZRGB_EVIDENCE_DOCTOR_MD) \
	  --path $(SELECT_XYZRGB_EVIDENCE_DOCTOR_JSON) \
	  --target run_select_xyzrgb_board_evidence_doctor \
	  --backend board \
	  --run-label $(SELECT_XYZRGB_EVIDENCE_RUN_LABEL) \
	  --case-filter circle3d-select-phase020-xyzrgb-repeated \
	  --evidence-role production-public \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/020-select-point-type-expansion/result.zh.md \
	  --freshness-state recorded

select_xyzrgb_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(SELECT_XYZRGB_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(SELECT_XYZRGB_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(SELECT_XYZRGB_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

collect_select_xyzrgba_repeated_board_evidence:
	@rm -rf $(SELECT_XYZRGBA_REPEATED_BOARD_DIR)
	@set -e; \
	for run in 01 02 03 04 05; do \
	  echo "[BOARD] circle3d select PointXYZRGBA repeated run $$run -> $(SELECT_XYZRGBA_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    BENCH_ARGS="65536 200 20 PointXYZRGBA" \
	    OUTPUT_DIR_BOARD="$(SELECT_XYZRGBA_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase020-circle3d-select-xyzrgba-run-$$run"; \
	done

generate_select_xyzrgba_board_evidence_manifest:
	@python3 script/generate_circle3d_board_evidence_manifest.py \
	  --repeat-root $(SELECT_XYZRGBA_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_circle3d_rvv.full.asm \
	  --output $(SELECT_XYZRGBA_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_circle3d Phase 020 PointXYZRGBA select production repeated board summary" \
	  --candidate-gate "pre_narrowing_RVVXYZAoSFloatLayout_PointT_signed_i32_index_u32_offset_non_degenerate_projection" \
	  --mode production-select

run_select_xyzrgba_board_evidence_doctor: generate_select_xyzrgba_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(SELECT_XYZRGBA_EVIDENCE_MANIFEST) \
	  --output $(SELECT_XYZRGBA_EVIDENCE_DOCTOR_MD) \
	  --json-output $(SELECT_XYZRGBA_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_select_xyzrgba_board_evidence_state: run_select_xyzrgba_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(SELECT_XYZRGBA_EVIDENCE_MANIFEST) \
	  --doctor $(SELECT_XYZRGBA_EVIDENCE_DOCTOR_MD) \
	  --path $(SELECT_XYZRGBA_EVIDENCE_DOCTOR_JSON) \
	  --target run_select_xyzrgba_board_evidence_doctor \
	  --backend board \
	  --run-label $(SELECT_XYZRGBA_EVIDENCE_RUN_LABEL) \
	  --case-filter circle3d-select-phase020-xyzrgba-repeated \
	  --evidence-role production-public \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/020-select-point-type-expansion/result.zh.md \
	  --freshness-state recorded

select_xyzrgba_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(SELECT_XYZRGBA_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(SELECT_XYZRGBA_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(SELECT_XYZRGBA_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

.PHONY: run_circle3d_phase000_tests run_board_circle3d_phase000_tests check_projection_asm collect_projection_repeated_board_evidence generate_projection_board_evidence_manifest run_projection_board_evidence_doctor record_projection_board_evidence_state projection_evidence_status collect_select_production_repeated_board_evidence generate_select_production_board_evidence_manifest run_select_production_board_evidence_doctor record_select_production_board_evidence_state select_production_evidence_status collect_select_xyzi_repeated_board_evidence generate_select_xyzi_board_evidence_manifest run_select_xyzi_board_evidence_doctor record_select_xyzi_board_evidence_state select_xyzi_evidence_status collect_select_xyzrgb_repeated_board_evidence generate_select_xyzrgb_board_evidence_manifest run_select_xyzrgb_board_evidence_doctor record_select_xyzrgb_board_evidence_state select_xyzrgb_evidence_status collect_select_xyzrgba_repeated_board_evidence generate_select_xyzrgba_board_evidence_manifest run_select_xyzrgba_board_evidence_doctor record_select_xyzrgba_board_evidence_state select_xyzrgba_evidence_status
