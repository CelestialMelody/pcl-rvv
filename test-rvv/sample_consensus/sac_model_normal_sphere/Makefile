# =============================================================================
# PCL sample_consensus/sac_model_normal_sphere RVV topic Makefile
#
# 本 topic 使用共享 rvv-topic.mk；QEMU 只跑 correctness / log-shape，
# 性能结论只来自 board 或目标硬件。
# =============================================================================

TOPIC  := sac_model_normal_sphere
MODULE := sample_consensus
TOPIC_DOC_PREFIX := test-rvv/sample_consensus/sac_model_normal_sphere

EIGEN_RVV_FLAGS := -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=2 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl

FOCUS_DIR      := pcl/sample_consensus/include/pcl/sample_consensus
FILTER_REPORT  := log/filtered_sac_model_normal_sphere.log
ANALYZE_REPORT := log/analyze_sac_model_normal_sphere.log

TARGET_TEST     := test_sac_model_normal_sphere
TARGET_TEST_STD := test_sac_model_normal_sphere_std
TARGET_TEST_RVV := test_sac_model_normal_sphere_rvv
SRCS_TEST       := src/test_sac_model_normal_sphere.cpp

TARGET_BENCH     := bench_sac_model_normal_sphere
TARGET_BENCH_STD := bench_sac_model_normal_sphere_std
TARGET_BENCH_RVV := bench_sac_model_normal_sphere_rvv
SRCS_BENCH       := src/bench_sac_model_normal_sphere.cpp
BENCH_ARGS       ?= 65536 200 PointXYZ

REMOTE_DIR ?= /root/pcl-test/sample_consensus/sac_model_normal_sphere
REMOTE_BENCH_ARGS ?= $(BENCH_ARGS)
REMOTE_TEST_ARGS ?=

NORMAL_SPHERE_PUBLIC_FILTER := SampleConsensusModelNormalSphere.PublicEntriesMatchReferenceOnNormalBoundaries:SampleConsensusModelNormalSphere.PointXYZILayoutMatchesReference:SampleConsensusModelNormalSphere.PointXYZRGBAndRGBALayoutsMatchReference:SampleConsensusModelNormalSphere.DegenerateCenterDirectionKeepsScalarBoundary:SampleConsensusModelNormalSphere.ProductionRVVDetailHelpersMatchPublicReference:SampleConsensusModelNormalSphere.ProductionPublicEntriesMatchReferenceOnLargeInputs:SampleConsensusModelNormalSphere.ProductionFallbacksKeepPublicReferenceSemantics
PHASE000_DIR := doc/phases/000-normal-sphere-count-select-diagnostic
PHASE010_DIR := doc/phases/010-vcompress-select-ablation
PHASE020_DIR := doc/phases/020-getdistances-dense-store-audit
PHASE030_DIR := doc/phases/030-rgb-rgba-point-type-expansion
PHASE060_DIR := doc/phases/060-production-integration-execution
REPEATED_BOARD_DIR ?= log/board/repeated-phase000-normal-sphere
REPEATED_EVIDENCE_MANIFEST := $(PHASE000_DIR)/repeated-evidence-manifest.json
REPEATED_EVIDENCE_DOCTOR_MD := $(PHASE000_DIR)/repeated-evidence-doctor.md
REPEATED_EVIDENCE_DOCTOR_JSON := $(PHASE000_DIR)/repeated-evidence-doctor.json
BOARD_EVIDENCE_SUMMARY := $(PHASE000_DIR)/board-evidence-summary.md
BOARD_EVIDENCE_MANIFEST := $(PHASE000_DIR)/board-evidence-manifest.json
BOARD_EVIDENCE_DOCTOR_MD := $(PHASE000_DIR)/board-evidence-doctor.md
BOARD_EVIDENCE_DOCTOR_JSON := $(PHASE000_DIR)/board-evidence-doctor.json
PHASE010_BOARD_EVIDENCE_SUMMARY := $(PHASE010_DIR)/board-evidence-summary.md
PHASE010_BOARD_EVIDENCE_MANIFEST := $(PHASE010_DIR)/board-evidence-manifest.json
PHASE010_BOARD_EVIDENCE_DOCTOR_MD := $(PHASE010_DIR)/board-evidence-doctor.md
PHASE010_BOARD_EVIDENCE_DOCTOR_JSON := $(PHASE010_DIR)/board-evidence-doctor.json
PHASE020_BOARD_EVIDENCE_SUMMARY := $(PHASE020_DIR)/board-evidence-summary.md
PHASE020_BOARD_EVIDENCE_MANIFEST := $(PHASE020_DIR)/board-evidence-manifest.json
PHASE020_BOARD_EVIDENCE_DOCTOR_MD := $(PHASE020_DIR)/board-evidence-doctor.md
PHASE020_BOARD_EVIDENCE_DOCTOR_JSON := $(PHASE020_DIR)/board-evidence-doctor.json
PHASE030_BOARD_EVIDENCE_SUMMARY := $(PHASE030_DIR)/board-evidence-summary.md
PHASE030_BOARD_EVIDENCE_MANIFEST := $(PHASE030_DIR)/board-evidence-manifest.json
PHASE030_BOARD_EVIDENCE_DOCTOR_MD := $(PHASE030_DIR)/board-evidence-doctor.md
PHASE030_BOARD_EVIDENCE_DOCTOR_JSON := $(PHASE030_DIR)/board-evidence-doctor.json
PHASE060_BOARD_EVIDENCE_SUMMARY := $(PHASE060_DIR)/board-evidence-summary.md
PHASE060_BOARD_EVIDENCE_MANIFEST := $(PHASE060_DIR)/board-evidence-manifest.json
PHASE060_BOARD_EVIDENCE_DOCTOR_MD := $(PHASE060_DIR)/board-evidence-doctor.md
PHASE060_BOARD_EVIDENCE_DOCTOR_JSON := $(PHASE060_DIR)/board-evidence-doctor.json
PHASE060_REPEATED_RUNS ?= 5
EVIDENCE_REGISTRY := log/evidence_registry.json
REPEATED_EVIDENCE_RUN_LABEL := normal-sphere-phase000-repeated-board

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

run_normal_sphere_public_tests:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="--gtest_filter=$(NORMAL_SPHERE_PUBLIC_FILTER)"

run_board_normal_sphere_public_tests: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' REMOTE_TEST_ARGS='--gtest_filter=$(NORMAL_SPHERE_PUBLIC_FILTER)'"

collect_phase060_repeated_board: deploy_board
	@for run in $$(seq 1 $(PHASE060_REPEATED_RUNS)); do \
		echo "[BOARD] Phase060 repeated run $$run/$(PHASE060_REPEATED_RUNS) $(BENCH_ARGS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)/run-$$run' BENCH_COMPARE_SAVE='$(REMOTE_BOARD_OUTPUT_DIR)/run-$$run/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(BENCH_ARGS)'" || exit $$?; \
	done
	@$(MAKE) -C $(CURDIR) fetch_board_logs OUTPUT_DIR_BOARD='$(OUTPUT_DIR_BOARD)' REMOTE_BOARD_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)'

generate_evidence_manifest:
	@$(PYTHON_RUN) script/generate_normal_sphere_evidence_manifest.py \
		--pointxyz-dir log/board \
		--pointxyzi-dir log/board-pointxyzi \
		--asm build/asm/riscv/bench_sac_model_normal_sphere_rvv.full.asm \
		--std-binary build/riscv/bench_sac_model_normal_sphere_std \
		--rvv-binary build/riscv/bench_sac_model_normal_sphere_rvv \
		--output $(BOARD_EVIDENCE_MANIFEST) \
		--summary-output $(BOARD_EVIDENCE_SUMMARY) \
		--phase 000 \
		--run-label normal-sphere-phase000-board-smoke

run_evidence_doctor: generate_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(BOARD_EVIDENCE_MANIFEST) \
		--output $(BOARD_EVIDENCE_DOCTOR_MD) \
		--json-output $(BOARD_EVIDENCE_DOCTOR_JSON) \
		--fail-on never

record_evidence_state: run_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(BOARD_EVIDENCE_SUMMARY) \
		--manifest $(BOARD_EVIDENCE_MANIFEST) \
		--doctor $(BOARD_EVIDENCE_DOCTOR_MD) \
		--target run_evidence_doctor \
		--backend board \
		--run-label normal-sphere-phase000-board-smoke \
		--case-filter 'PointXYZ+PointXYZI all Phase000 bench labels' \
		--evidence-role production_shaped_diagnostic \
		--doc-ref $(PHASE000_DIR)/result.zh.md \
		--doc-ref doc/sac_model_normal_sphere-evaluation.zh.md \
		--freshness-state recorded

generate_phase010_evidence_manifest:
	@$(PYTHON_RUN) script/generate_normal_sphere_evidence_manifest.py \
		--pointxyz-dir log/board-phase010-pointxyz \
		--pointxyzi-dir log/board-phase010-pointxyzi \
		--asm build/asm/riscv/bench_sac_model_normal_sphere_rvv.full.asm \
		--std-binary build/riscv/bench_sac_model_normal_sphere_std \
		--rvv-binary build/riscv/bench_sac_model_normal_sphere_rvv \
		--output $(PHASE010_BOARD_EVIDENCE_MANIFEST) \
		--summary-output $(PHASE010_BOARD_EVIDENCE_SUMMARY) \
		--phase 010 \
		--run-label normal-sphere-phase010-vcompress-board-smoke

run_phase010_evidence_doctor: generate_phase010_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(PHASE010_BOARD_EVIDENCE_MANIFEST) \
		--output $(PHASE010_BOARD_EVIDENCE_DOCTOR_MD) \
		--json-output $(PHASE010_BOARD_EVIDENCE_DOCTOR_JSON) \
		--fail-on never

record_phase010_evidence_state: run_phase010_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(PHASE010_BOARD_EVIDENCE_SUMMARY) \
		--manifest $(PHASE010_BOARD_EVIDENCE_MANIFEST) \
		--doctor $(PHASE010_BOARD_EVIDENCE_DOCTOR_MD) \
		--target run_phase010_evidence_doctor \
		--backend board \
		--run-label normal-sphere-phase010-vcompress-board-smoke \
		--case-filter 'PointXYZ+PointXYZI Phase010 vcompress select ablation' \
		--evidence-role component_ablation \
		--doc-ref $(PHASE010_DIR)/result.zh.md \
		--doc-ref doc/sac_model_normal_sphere-evaluation.zh.md \
		--freshness-state recorded

generate_phase020_evidence_manifest:
	@$(PYTHON_RUN) script/generate_normal_sphere_evidence_manifest.py \
		--pointxyz-dir log/board-phase020-pointxyz \
		--pointxyzi-dir log/board-phase020-pointxyzi \
		--asm build/asm/riscv/bench_sac_model_normal_sphere_rvv.full.asm \
		--std-binary build/riscv/bench_sac_model_normal_sphere_std \
		--rvv-binary build/riscv/bench_sac_model_normal_sphere_rvv \
		--output $(PHASE020_BOARD_EVIDENCE_MANIFEST) \
		--summary-output $(PHASE020_BOARD_EVIDENCE_SUMMARY) \
		--phase 020 \
		--run-label normal-sphere-phase020-getdistances-board-smoke

run_phase020_evidence_doctor: generate_phase020_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(PHASE020_BOARD_EVIDENCE_MANIFEST) \
		--output $(PHASE020_BOARD_EVIDENCE_DOCTOR_MD) \
		--json-output $(PHASE020_BOARD_EVIDENCE_DOCTOR_JSON) \
		--fail-on never

record_phase020_evidence_state: run_phase020_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(PHASE020_BOARD_EVIDENCE_SUMMARY) \
		--manifest $(PHASE020_BOARD_EVIDENCE_MANIFEST) \
		--doctor $(PHASE020_BOARD_EVIDENCE_DOCTOR_MD) \
		--target run_phase020_evidence_doctor \
		--backend board \
		--run-label normal-sphere-phase020-getdistances-board-smoke \
		--case-filter 'PointXYZ+PointXYZI Phase020 getDistances dense-store diagnostic' \
		--evidence-role production_shaped_diagnostic \
		--doc-ref $(PHASE020_DIR)/result.zh.md \
		--doc-ref doc/sac_model_normal_sphere-evaluation.zh.md \
		--freshness-state recorded

generate_phase030_evidence_manifest:
	@$(PYTHON_RUN) script/generate_normal_sphere_evidence_manifest.py \
		--pointxyz-dir log/board-phase030-pointxyzrgb \
		--pointxyz-type PointXYZRGB \
		--pointxyzi-dir log/board-phase030-pointxyzrgba \
		--pointxyzi-type PointXYZRGBA \
		--asm build/asm/riscv/bench_sac_model_normal_sphere_rvv.full.asm \
		--std-binary build/riscv/bench_sac_model_normal_sphere_std \
		--rvv-binary build/riscv/bench_sac_model_normal_sphere_rvv \
		--output $(PHASE030_BOARD_EVIDENCE_MANIFEST) \
		--summary-output $(PHASE030_BOARD_EVIDENCE_SUMMARY) \
		--phase 030 \
		--run-label normal-sphere-phase030-rgb-rgba-smoke

run_phase030_evidence_doctor: generate_phase030_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(PHASE030_BOARD_EVIDENCE_MANIFEST) \
		--output $(PHASE030_BOARD_EVIDENCE_DOCTOR_MD) \
		--json-output $(PHASE030_BOARD_EVIDENCE_DOCTOR_JSON) \
		--fail-on never

record_phase030_evidence_state: run_phase030_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(PHASE030_BOARD_EVIDENCE_SUMMARY) \
		--manifest $(PHASE030_BOARD_EVIDENCE_MANIFEST) \
		--doctor $(PHASE030_BOARD_EVIDENCE_DOCTOR_MD) \
		--target run_phase030_evidence_doctor \
		--backend board \
		--run-label normal-sphere-phase030-rgb-rgba-smoke \
		--case-filter 'PointXYZRGB+PointXYZRGBA Phase030 point-type expansion diagnostic' \
		--evidence-role production_shaped_diagnostic \
		--doc-ref $(PHASE030_DIR)/result.zh.md \
		--doc-ref doc/sac_model_normal_sphere-evaluation.zh.md \
		--freshness-state recorded

generate_phase060_evidence_manifest:
	@$(PYTHON_RUN) script/generate_normal_sphere_evidence_manifest.py \
		--pointxyz-dir log/board-phase060-repeated-pointxyz \
		--pointxyz-type PointXYZ \
		--pointxyzi-dir log/board-phase060-repeated-pointxyzi \
		--pointxyzi-type PointXYZI \
		--pointxyzrgb-dir log/board-phase060-repeated-pointxyzrgb \
		--pointxyzrgba-dir log/board-phase060-repeated-pointxyzrgba \
		--asm build/asm/riscv/bench_sac_model_normal_sphere_rvv.full.asm \
		--std-binary build/riscv/bench_sac_model_normal_sphere_std \
		--rvv-binary build/riscv/bench_sac_model_normal_sphere_rvv \
		--output $(PHASE060_BOARD_EVIDENCE_MANIFEST) \
		--summary-output $(PHASE060_BOARD_EVIDENCE_SUMMARY) \
		--phase 060 \
		--run-label normal-sphere-phase060-production-public

run_phase060_evidence_doctor: generate_phase060_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(PHASE060_BOARD_EVIDENCE_MANIFEST) \
		--output $(PHASE060_BOARD_EVIDENCE_DOCTOR_MD) \
		--json-output $(PHASE060_BOARD_EVIDENCE_DOCTOR_JSON) \
		--fail-on never

record_phase060_evidence_state: run_phase060_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(PHASE060_BOARD_EVIDENCE_SUMMARY) \
		--manifest $(PHASE060_BOARD_EVIDENCE_MANIFEST) \
		--doctor $(PHASE060_BOARD_EVIDENCE_DOCTOR_MD) \
		--target run_phase060_evidence_doctor \
		--backend board \
		--run-label normal-sphere-phase060-production-public \
		--case-filter 'PointXYZ+PointXYZI+PointXYZRGB+PointXYZRGBA Phase060 production public entries' \
		--evidence-role production_public \
		--doc-ref $(PHASE060_DIR)/result.zh.md \
		--doc-ref doc/sac_model_normal_sphere-evaluation.zh.md \
		--doc-ref ../../../doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md \
		--freshness-state recorded

evidence_status:
	@$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry $(EVIDENCE_REGISTRY) \
		--scan-glob '$(PHASE000_DIR)/*evidence*.md' \
		--scan-glob '$(PHASE000_DIR)/*evidence*.json' \
		--scan-glob '$(PHASE010_DIR)/*evidence*.md' \
		--scan-glob '$(PHASE010_DIR)/*evidence*.json' \
		--scan-glob '$(PHASE020_DIR)/*evidence*.md' \
		--scan-glob '$(PHASE020_DIR)/*evidence*.json' \
		--scan-glob '$(PHASE030_DIR)/*evidence*.md' \
		--scan-glob '$(PHASE030_DIR)/*evidence*.json' \
		--scan-glob '$(PHASE060_DIR)/*evidence*.md' \
		--scan-glob '$(PHASE060_DIR)/*evidence*.json' \
		--doc $(PHASE000_DIR)/result.zh.md \
		--doc $(PHASE010_DIR)/result.zh.md \
		--doc $(PHASE020_DIR)/result.zh.md \
		--doc $(PHASE030_DIR)/result.zh.md \
		--doc $(PHASE060_DIR)/result.zh.md \
		--doc doc/sac_model_normal_sphere-evaluation.zh.md \
		--fail-on never
