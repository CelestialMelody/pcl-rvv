# =============================================================================
# PCL sample_consensus/sac_model_sphere RVV topic Makefile
#
# 本 topic 使用共享 rvv-topic.mk；QEMU 只跑 correctness / log-shape，
# 性能结论只来自 board 或目标硬件。
# =============================================================================

TOPIC  := sac_model_sphere
MODULE := sample_consensus
TOPIC_DOC_PREFIX := test-rvv/sample_consensus/sac_model_sphere

EIGEN_RVV_FLAGS := -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=2 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl

FOCUS_DIR      := pcl/sample_consensus/include/pcl/sample_consensus
FILTER_REPORT  := log/filtered_sac_model_sphere.log
ANALYZE_REPORT := log/analyze_sac_model_sphere.log

TARGET_TEST     := test_sac_model_sphere
TARGET_TEST_STD := test_sac_model_sphere_std
TARGET_TEST_RVV := test_sac_model_sphere_rvv
SRCS_TEST       := src/test_sac_model_sphere.cpp

TARGET_BENCH     := bench_sac_model_sphere
TARGET_BENCH_STD := bench_sac_model_sphere_std
TARGET_BENCH_RVV := bench_sac_model_sphere_rvv
SRCS_BENCH       := src/bench_sac_model_sphere.cpp
BENCH_ARGS       ?= 65536 200

REMOTE_DIR ?= /root/pcl-test/sample_consensus/sac_model_sphere
REMOTE_BENCH_ARGS ?= $(BENCH_ARGS)
REMOTE_TEST_ARGS ?=

SPHERE_PUBLIC_FILTER := SampleConsensusModelSphere.PublicEntriesMatchReferenceOnShellBoundaries:SampleConsensusModelSphere.PointXYZILayoutMatchesReference:SampleConsensusModelSphere.PointXYZRGBAndRGBALayoutsMatchReference:SampleConsensusModelSphere.DiagnosticCandidateMatchesPublicEntries:SampleConsensusModelSphere.ProductionSelectWithinDistanceMatchesStandardHelper
PHASE000_DIR := doc/phases/000-sphere-select-distance-diagnostic
PHASE020_DIR := doc/phases/020-select-production-integration-plan
PHASE040_DIR := doc/phases/040-select-vcompress-ablation
PHASE045_DIR := doc/phases/045-select-vcompress-production-integration
PHASE050_DIR := doc/phases/050-point-type-expansion
PHASE060_DIR := doc/phases/060-point-type-rgb-rgba-expansion
REPEATED_BOARD_DIR ?= log/board/repeated-20260827-phase000
REPEATED_EVIDENCE_MANIFEST := $(PHASE000_DIR)/repeated-evidence-manifest.json
REPEATED_EVIDENCE_DOCTOR_MD := $(PHASE000_DIR)/repeated-evidence-doctor.md
REPEATED_EVIDENCE_DOCTOR_JSON := $(PHASE000_DIR)/repeated-evidence-doctor.json
PRODUCTION_REPEATED_BOARD_DIR ?= log/board/repeated-20260827-phase020-production-select
PRODUCTION_EVIDENCE_MANIFEST := $(PHASE020_DIR)/production-repeated-evidence-manifest.json
PRODUCTION_EVIDENCE_DOCTOR_MD := $(PHASE020_DIR)/production-repeated-evidence-doctor.md
PRODUCTION_EVIDENCE_DOCTOR_JSON := $(PHASE020_DIR)/production-repeated-evidence-doctor.json
VCOMPRESS_REPEATED_BOARD_DIR ?= log/board/repeated-20260827-phase040-vcompress
VCOMPRESS_EVIDENCE_MANIFEST := $(PHASE040_DIR)/vcompress-repeated-evidence-manifest.json
VCOMPRESS_EVIDENCE_DOCTOR_MD := $(PHASE040_DIR)/vcompress-repeated-evidence-doctor.md
VCOMPRESS_EVIDENCE_DOCTOR_JSON := $(PHASE040_DIR)/vcompress-repeated-evidence-doctor.json
PRODUCTION_VCOMPRESS_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase045-production-vcompress-select
PRODUCTION_VCOMPRESS_EVIDENCE_MANIFEST := $(PHASE045_DIR)/production-vcompress-repeated-evidence-manifest.json
PRODUCTION_VCOMPRESS_EVIDENCE_DOCTOR_MD := $(PHASE045_DIR)/production-vcompress-repeated-evidence-doctor.md
PRODUCTION_VCOMPRESS_EVIDENCE_DOCTOR_JSON := $(PHASE045_DIR)/production-vcompress-repeated-evidence-doctor.json
POINT_TYPE_BENCH_ARGS ?= 65536 200 PointXYZI
POINT_TYPE_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase050-point-type-xyzi
POINT_TYPE_EVIDENCE_MANIFEST := $(PHASE050_DIR)/point-type-repeated-evidence-manifest.json
POINT_TYPE_EVIDENCE_DOCTOR_MD := $(PHASE050_DIR)/point-type-repeated-evidence-doctor.md
POINT_TYPE_EVIDENCE_DOCTOR_JSON := $(PHASE050_DIR)/point-type-repeated-evidence-doctor.json
RGB_POINT_TYPE_BENCH_ARGS ?= 65536 200 PointXYZRGB
RGB_POINT_TYPE_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase060-point-type-xyzrgb
RGB_POINT_TYPE_EVIDENCE_MANIFEST := $(PHASE060_DIR)/rgb-point-type-repeated-evidence-manifest.json
RGB_POINT_TYPE_EVIDENCE_DOCTOR_MD := $(PHASE060_DIR)/rgb-point-type-repeated-evidence-doctor.md
RGB_POINT_TYPE_EVIDENCE_DOCTOR_JSON := $(PHASE060_DIR)/rgb-point-type-repeated-evidence-doctor.json
RGBA_POINT_TYPE_BENCH_ARGS ?= 65536 200 PointXYZRGBA
RGBA_POINT_TYPE_REPEATED_BOARD_DIR ?= log/board/repeated-20260828-phase060-point-type-xyzrgba
RGBA_POINT_TYPE_EVIDENCE_MANIFEST := $(PHASE060_DIR)/rgba-point-type-repeated-evidence-manifest.json
RGBA_POINT_TYPE_EVIDENCE_DOCTOR_MD := $(PHASE060_DIR)/rgba-point-type-repeated-evidence-doctor.md
RGBA_POINT_TYPE_EVIDENCE_DOCTOR_JSON := $(PHASE060_DIR)/rgba-point-type-repeated-evidence-doctor.json
EVIDENCE_REGISTRY := log/evidence_registry.json
REPEATED_EVIDENCE_RUN_LABEL := sphere-phase000-repeated-board
PRODUCTION_EVIDENCE_RUN_LABEL := sphere-phase020-production-select-repeated-board
VCOMPRESS_EVIDENCE_RUN_LABEL := sphere-phase040-vcompress-repeated-board
PRODUCTION_VCOMPRESS_EVIDENCE_RUN_LABEL := sphere-phase045-production-vcompress-select-repeated-board
POINT_TYPE_EVIDENCE_RUN_LABEL := sphere-phase050-point-type-xyzi-repeated-board
RGB_POINT_TYPE_EVIDENCE_RUN_LABEL := sphere-phase060-point-type-xyzrgb-repeated-board
RGBA_POINT_TYPE_EVIDENCE_RUN_LABEL := sphere-phase060-point-type-xyzrgba-repeated-board
EVIDENCE_DOC_REFS := \
	$(TOPIC_DOC_PREFIX)/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/sac_model_sphere-evaluation.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/testing-overview.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/correctness-tests.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/benchmark-and-evidence.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-evidence.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/test-support-code-map.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-roadmap.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/optimization-matrix.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/000-sphere-select-distance-diagnostic/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/010-structure-parity-doc-suite/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/015-evidence-registry-target-alias/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/015-evidence-registry-target-alias/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-select-production-integration-plan/plan.zh.md \
	doc-rvv/library-screening/sample_consensus/sample_consensus-function-evaluation-queue.zh.md
EVIDENCE_DOC_ARGS := $(foreach doc,$(EVIDENCE_DOC_REFS),--doc $(doc))
PRODUCTION_EVIDENCE_DOC_REFS := \
	$(EVIDENCE_DOC_REFS) \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-select-production-integration-plan/result.zh.md \
	doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md
PRODUCTION_EVIDENCE_DOC_ARGS := $(foreach doc,$(PRODUCTION_EVIDENCE_DOC_REFS),--doc $(doc))
VCOMPRESS_EVIDENCE_DOC_REFS := \
	$(PRODUCTION_EVIDENCE_DOC_REFS) \
	$(TOPIC_DOC_PREFIX)/doc/phases/040-select-vcompress-ablation/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/040-select-vcompress-ablation/result.zh.md
VCOMPRESS_EVIDENCE_DOC_ARGS := $(foreach doc,$(VCOMPRESS_EVIDENCE_DOC_REFS),--doc $(doc))
PRODUCTION_VCOMPRESS_EVIDENCE_DOC_REFS := \
	$(VCOMPRESS_EVIDENCE_DOC_REFS) \
	$(TOPIC_DOC_PREFIX)/doc/phases/045-select-vcompress-production-integration/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/045-select-vcompress-production-integration/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/046-vcompress-production-closeout/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/046-vcompress-production-closeout/result.zh.md
PRODUCTION_VCOMPRESS_EVIDENCE_DOC_ARGS := $(foreach doc,$(PRODUCTION_VCOMPRESS_EVIDENCE_DOC_REFS),--doc $(doc))
POINT_TYPE_EVIDENCE_DOC_REFS := \
	$(PRODUCTION_VCOMPRESS_EVIDENCE_DOC_REFS) \
	$(TOPIC_DOC_PREFIX)/doc/phases/050-point-type-expansion/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/050-point-type-expansion/result.zh.md
POINT_TYPE_EVIDENCE_DOC_ARGS := $(foreach doc,$(POINT_TYPE_EVIDENCE_DOC_REFS),--doc $(doc))
RGB_RGBA_POINT_TYPE_EVIDENCE_DOC_REFS := \
	$(POINT_TYPE_EVIDENCE_DOC_REFS) \
	$(TOPIC_DOC_PREFIX)/doc/phases/055-test-support-include-layout/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/055-test-support-include-layout/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/060-point-type-rgb-rgba-expansion/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/060-point-type-rgb-rgba-expansion/result.zh.md
RGB_RGBA_POINT_TYPE_EVIDENCE_DOC_ARGS := $(foreach doc,$(RGB_RGBA_POINT_TYPE_EVIDENCE_DOC_REFS),--doc $(doc))

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
	$(PCL_INCLUDE) \
	-I$(CURDIR)/include \
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

run_sphere_public_tests:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=$(SPHERE_PUBLIC_FILTER)"

run_board_sphere_public_tests: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' REMOTE_TEST_ARGS='--gtest_filter=$(SPHERE_PUBLIC_FILTER)'"

generate_board_evidence_manifest:
	@python3 script/generate_sphere_board_evidence_manifest.py \
	  --repeat-root $(REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_sphere_rvv.full.asm \
	  --output $(REPEATED_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_sphere Phase 000 repeated board summary" \
	  --summary-role production_shaped_diagnostic \
	  --select-mode diagnostic

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
	  --case-filter sphere-select-distance-phase000-repeated \
	  --evidence-role production_shaped_diagnostic \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/000-sphere-select-distance-diagnostic/result.zh.md \
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

collect_production_repeated_board_evidence:
	@rm -rf $(PRODUCTION_REPEATED_BOARD_DIR)
	@for run in 01 02 03 04 05; do \
	  echo "[BOARD] production repeated run $$run -> $(PRODUCTION_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(PRODUCTION_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase020-production-select-run-$$run"; \
	done

generate_production_board_evidence_manifest:
	@python3 script/generate_sphere_board_evidence_manifest.py \
	  --repeat-root $(PRODUCTION_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_sphere_rvv.full.asm \
	  --output $(PRODUCTION_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_sphere Phase 020 production select repeated board summary" \
	  --summary-role production_direct \
	  --select-mode production

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
	  --case-filter sphere-select-distance-phase020-production-repeated \
	  --evidence-role production_direct \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/020-select-production-integration-plan/result.zh.md \
	  --doc-ref doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md \
	  --freshness-state recorded

production_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PRODUCTION_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PRODUCTION_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PRODUCTION_EVIDENCE_DOCTOR_JSON)' \
	  $(PRODUCTION_EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

collect_vcompress_repeated_board_evidence:
	@rm -rf $(VCOMPRESS_REPEATED_BOARD_DIR)
	@for run in 01 02 03 04 05; do \
	  echo "[BOARD] vcompress repeated run $$run -> $(VCOMPRESS_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(VCOMPRESS_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase040-vcompress-run-$$run"; \
	done

generate_vcompress_board_evidence_manifest:
	@python3 script/generate_sphere_board_evidence_manifest.py \
	  --repeat-root $(VCOMPRESS_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_sphere_rvv.full.asm \
	  --output $(VCOMPRESS_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_sphere Phase 040 vcompress repeated board summary" \
	  --summary-role production_detail \
	  --select-mode vcompress

run_vcompress_board_evidence_doctor: generate_vcompress_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(VCOMPRESS_EVIDENCE_MANIFEST) \
	  --output $(VCOMPRESS_EVIDENCE_DOCTOR_MD) \
	  --json-output $(VCOMPRESS_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_vcompress_board_evidence_state: run_vcompress_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(VCOMPRESS_EVIDENCE_MANIFEST) \
	  --doctor $(VCOMPRESS_EVIDENCE_DOCTOR_MD) \
	  --path $(VCOMPRESS_EVIDENCE_DOCTOR_JSON) \
	  --target run_vcompress_board_evidence_doctor \
	  --backend board \
	  --run-label $(VCOMPRESS_EVIDENCE_RUN_LABEL) \
	  --case-filter sphere-select-distance-phase040-vcompress-repeated \
	  --evidence-role production_detail \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/040-select-vcompress-ablation/plan.zh.md \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/040-select-vcompress-ablation/result.zh.md \
	  --freshness-state recorded

vcompress_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(VCOMPRESS_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(VCOMPRESS_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(VCOMPRESS_EVIDENCE_DOCTOR_JSON)' \
	  $(VCOMPRESS_EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

collect_production_vcompress_repeated_board_evidence:
	@rm -rf $(PRODUCTION_VCOMPRESS_REPEATED_BOARD_DIR)
	@for run in 01 02 03 04 05; do \
	  echo "[BOARD] production vcompress repeated run $$run -> $(PRODUCTION_VCOMPRESS_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    OUTPUT_DIR_BOARD="$(PRODUCTION_VCOMPRESS_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase045-production-vcompress-run-$$run"; \
	done

generate_production_vcompress_board_evidence_manifest:
	@python3 script/generate_sphere_board_evidence_manifest.py \
	  --repeat-root $(PRODUCTION_VCOMPRESS_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_sphere_rvv.full.asm \
	  --output $(PRODUCTION_VCOMPRESS_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_sphere Phase 045 production vcompress select repeated board summary" \
	  --summary-role production_direct \
	  --select-mode production

run_production_vcompress_board_evidence_doctor: generate_production_vcompress_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(PRODUCTION_VCOMPRESS_EVIDENCE_MANIFEST) \
	  --output $(PRODUCTION_VCOMPRESS_EVIDENCE_DOCTOR_MD) \
	  --json-output $(PRODUCTION_VCOMPRESS_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_production_vcompress_board_evidence_state: run_production_vcompress_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(PRODUCTION_VCOMPRESS_EVIDENCE_MANIFEST) \
	  --doctor $(PRODUCTION_VCOMPRESS_EVIDENCE_DOCTOR_MD) \
	  --path $(PRODUCTION_VCOMPRESS_EVIDENCE_DOCTOR_JSON) \
	  --target run_production_vcompress_board_evidence_doctor \
	  --backend board \
	  --run-label $(PRODUCTION_VCOMPRESS_EVIDENCE_RUN_LABEL) \
	  --case-filter sphere-select-distance-phase045-production-vcompress-repeated \
	  --evidence-role production_direct \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/045-select-vcompress-production-integration/result.zh.md \
	  --doc-ref doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md \
	  --freshness-state recorded

production_vcompress_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PRODUCTION_VCOMPRESS_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PRODUCTION_VCOMPRESS_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PRODUCTION_VCOMPRESS_EVIDENCE_DOCTOR_JSON)' \
	  $(PRODUCTION_VCOMPRESS_EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

collect_point_type_repeated_board_evidence:
	@rm -rf $(POINT_TYPE_REPEATED_BOARD_DIR)
	@for run in 01 02 03 04 05; do \
	  echo "[BOARD] PointXYZI repeated run $$run -> $(POINT_TYPE_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    BENCH_ARGS="$(POINT_TYPE_BENCH_ARGS)" \
	    OUTPUT_DIR_BOARD="$(POINT_TYPE_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase050-point-type-xyzi-run-$$run"; \
	done

generate_point_type_board_evidence_manifest:
	@python3 script/generate_sphere_board_evidence_manifest.py \
	  --repeat-root $(POINT_TYPE_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_sphere_rvv.full.asm \
	  --output $(POINT_TYPE_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_sphere Phase 050 PointXYZI repeated board summary" \
	  --summary-role production_direct \
	  --select-mode production

run_point_type_board_evidence_doctor: generate_point_type_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(POINT_TYPE_EVIDENCE_MANIFEST) \
	  --output $(POINT_TYPE_EVIDENCE_DOCTOR_MD) \
	  --json-output $(POINT_TYPE_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_point_type_board_evidence_state: run_point_type_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(POINT_TYPE_EVIDENCE_MANIFEST) \
	  --doctor $(POINT_TYPE_EVIDENCE_DOCTOR_MD) \
	  --path $(POINT_TYPE_EVIDENCE_DOCTOR_JSON) \
	  --target run_point_type_board_evidence_doctor \
	  --backend board \
	  --run-label $(POINT_TYPE_EVIDENCE_RUN_LABEL) \
	  --case-filter sphere-select-distance-phase050-point-type-xyzi-repeated \
	  --evidence-role production_direct \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/050-point-type-expansion/result.zh.md \
	  --doc-ref doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md \
	  --freshness-state recorded

point_type_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(POINT_TYPE_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(POINT_TYPE_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(POINT_TYPE_EVIDENCE_DOCTOR_JSON)' \
	  $(POINT_TYPE_EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

collect_rgb_point_type_repeated_board_evidence:
	@rm -rf $(RGB_POINT_TYPE_REPEATED_BOARD_DIR)
	@for run in 01 02 03 04 05; do \
	  echo "[BOARD] PointXYZRGB repeated run $$run -> $(RGB_POINT_TYPE_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    BENCH_ARGS="$(RGB_POINT_TYPE_BENCH_ARGS)" \
	    OUTPUT_DIR_BOARD="$(RGB_POINT_TYPE_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase060-point-type-xyzrgb-run-$$run"; \
	done

generate_rgb_point_type_board_evidence_manifest:
	@python3 script/generate_sphere_board_evidence_manifest.py \
	  --repeat-root $(RGB_POINT_TYPE_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_sphere_rvv.full.asm \
	  --output $(RGB_POINT_TYPE_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_sphere Phase 060 PointXYZRGB repeated board summary" \
	  --summary-role production_direct \
	  --select-mode production

run_rgb_point_type_board_evidence_doctor: generate_rgb_point_type_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(RGB_POINT_TYPE_EVIDENCE_MANIFEST) \
	  --output $(RGB_POINT_TYPE_EVIDENCE_DOCTOR_MD) \
	  --json-output $(RGB_POINT_TYPE_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_rgb_point_type_board_evidence_state: run_rgb_point_type_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(RGB_POINT_TYPE_EVIDENCE_MANIFEST) \
	  --doctor $(RGB_POINT_TYPE_EVIDENCE_DOCTOR_MD) \
	  --path $(RGB_POINT_TYPE_EVIDENCE_DOCTOR_JSON) \
	  --target run_rgb_point_type_board_evidence_doctor \
	  --backend board \
	  --run-label $(RGB_POINT_TYPE_EVIDENCE_RUN_LABEL) \
	  --case-filter sphere-select-distance-phase060-point-type-xyzrgb-repeated \
	  --evidence-role production_direct \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/060-point-type-rgb-rgba-expansion/result.zh.md \
	  --doc-ref doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md \
	  --freshness-state recorded

rgb_point_type_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(RGB_POINT_TYPE_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(RGB_POINT_TYPE_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(RGB_POINT_TYPE_EVIDENCE_DOCTOR_JSON)' \
	  $(RGB_RGBA_POINT_TYPE_EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

collect_rgba_point_type_repeated_board_evidence:
	@rm -rf $(RGBA_POINT_TYPE_REPEATED_BOARD_DIR)
	@for run in 01 02 03 04 05; do \
	  echo "[BOARD] PointXYZRGBA repeated run $$run -> $(RGBA_POINT_TYPE_REPEATED_BOARD_DIR)/run-$$run"; \
	  $(MAKE) -C $(CURDIR) board_smoke \
	    BENCH_ARGS="$(RGBA_POINT_TYPE_BENCH_ARGS)" \
	    OUTPUT_DIR_BOARD="$(RGBA_POINT_TYPE_REPEATED_BOARD_DIR)/run-$$run" \
	    REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/phase060-point-type-xyzrgba-run-$$run"; \
	done

generate_rgba_point_type_board_evidence_manifest:
	@python3 script/generate_sphere_board_evidence_manifest.py \
	  --repeat-root $(RGBA_POINT_TYPE_REPEATED_BOARD_DIR) \
	  --asm build/asm/riscv/bench_sac_model_sphere_rvv.full.asm \
	  --output $(RGBA_POINT_TYPE_EVIDENCE_MANIFEST) \
	  --summary-title "sac_model_sphere Phase 060 PointXYZRGBA repeated board summary" \
	  --summary-role production_direct \
	  --select-mode production

run_rgba_point_type_board_evidence_doctor: generate_rgba_point_type_board_evidence_manifest
	@python3 ../../script/evidence_doctor.py \
	  --manifest $(RGBA_POINT_TYPE_EVIDENCE_MANIFEST) \
	  --output $(RGBA_POINT_TYPE_EVIDENCE_DOCTOR_MD) \
	  --json-output $(RGBA_POINT_TYPE_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_rgba_point_type_board_evidence_state: run_rgba_point_type_board_evidence_doctor
	@python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(RGBA_POINT_TYPE_EVIDENCE_MANIFEST) \
	  --doctor $(RGBA_POINT_TYPE_EVIDENCE_DOCTOR_MD) \
	  --path $(RGBA_POINT_TYPE_EVIDENCE_DOCTOR_JSON) \
	  --target run_rgba_point_type_board_evidence_doctor \
	  --backend board \
	  --run-label $(RGBA_POINT_TYPE_EVIDENCE_RUN_LABEL) \
	  --case-filter sphere-select-distance-phase060-point-type-xyzrgba-repeated \
	  --evidence-role production_direct \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/060-point-type-rgb-rgba-expansion/result.zh.md \
	  --doc-ref doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md \
	  --freshness-state recorded

rgba_point_type_evidence_status:
	@python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(RGBA_POINT_TYPE_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(RGBA_POINT_TYPE_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(RGBA_POINT_TYPE_EVIDENCE_DOCTOR_JSON)' \
	  $(RGB_RGBA_POINT_TYPE_EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

.PHONY: run_sphere_public_tests run_board_sphere_public_tests generate_board_evidence_manifest run_repeated_board_evidence_doctor record_repeated_board_evidence_state repeated_evidence_status collect_production_repeated_board_evidence generate_production_board_evidence_manifest run_production_board_evidence_doctor record_production_board_evidence_state production_evidence_status collect_vcompress_repeated_board_evidence generate_vcompress_board_evidence_manifest run_vcompress_board_evidence_doctor record_vcompress_board_evidence_state vcompress_evidence_status collect_production_vcompress_repeated_board_evidence generate_production_vcompress_board_evidence_manifest run_production_vcompress_board_evidence_doctor record_production_vcompress_board_evidence_state production_vcompress_evidence_status collect_point_type_repeated_board_evidence generate_point_type_board_evidence_manifest run_point_type_board_evidence_doctor record_point_type_board_evidence_state point_type_evidence_status collect_rgb_point_type_repeated_board_evidence generate_rgb_point_type_board_evidence_manifest run_rgb_point_type_board_evidence_doctor record_rgb_point_type_board_evidence_state rgb_point_type_evidence_status collect_rgba_point_type_repeated_board_evidence generate_rgba_point_type_board_evidence_manifest run_rgba_point_type_board_evidence_doctor record_rgba_point_type_board_evidence_state rgba_point_type_evidence_status
