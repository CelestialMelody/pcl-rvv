# =============================================================================
#  PCL sample_consensus/plane_models RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk. The load-compare benchmark remains
# a plane_models-specific extension.
# =============================================================================

TOPIC  := plane_models
MODULE := sample_consensus
THIS_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

EIGEN_RVV_FLAGS := -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=2 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl

FOCUS_DIR      := pcl/sample_consensus/include/pcl/sample_consensus
FILTER_REPORT  := log/filtered_plane_models.log
ANALYZE_REPORT := log/analyze_plane_models.log

TARGET_TEST     := rvv_sac_plane_test
TARGET_TEST_STD := rvv_sac_plane_test_std
TARGET_TEST_RVV := rvv_sac_plane_test_rvv
BOARD_TARGET_TEST := rvv_sac_plane_test
SRCS_TEST       := src/test_sample_consensus_plane_models.cpp

TARGET_BENCH     := bench_sac_normal_plane
TARGET_BENCH_STD := bench_sac_normal_plane_std
TARGET_BENCH_RVV := bench_sac_normal_plane_rvv
SRCS_BENCH       := src/bench_sac_normal_plane.cpp

TARGET_BENCH_LOAD ?= bench_sac_normal_plane_load_compare
SRCS_BENCH_LOAD   := src/bench_sac_normal_plane_load_compare.cpp

TOPIC_DOC_PREFIX := test-rvv/sample_consensus/plane_models
EVIDENCE_MANIFEST := doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-manifest.json
EVIDENCE_DOCTOR_MD := doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-doctor.md
EVIDENCE_DOCTOR_JSON := doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-doctor.json
EVIDENCE_REGISTRY := log/evidence_registry.json
EVIDENCE_RUN_LABEL := normal-plane-phase000-board-current
REPEATED_BOARD_RUNS ?= 5
REPEATED_BOARD_LABEL := normal-plane-phase030-repeated-board
REPEATED_BOARD_DIR := log/board/$(REPEATED_BOARD_LABEL)
REPEATED_BOARD_REMOTE_DIR = $(REMOTE_BOARD_OUTPUT_DIR)/$(REPEATED_BOARD_LABEL)
REPEATED_BOARD_SUMMARY := $(REPEATED_BOARD_DIR)/summary.md
REPEATED_EVIDENCE_MANIFEST := $(REPEATED_BOARD_DIR)/evidence-manifest.json
REPEATED_EVIDENCE_DOCTOR_MD := $(REPEATED_BOARD_DIR)/evidence-doctor.md
REPEATED_EVIDENCE_DOCTOR_JSON := $(REPEATED_BOARD_DIR)/evidence-doctor.json
REPEATED_EVIDENCE_RUN_LABEL := normal-plane-phase030-repeated-board
PHASE050_BOARD_LABEL := normal-plane-phase050-representative-aos-source-performance
PHASE050_BOARD_DIR := log/board/$(PHASE050_BOARD_LABEL)
PHASE050_REMOTE_DIR = $(REMOTE_BOARD_OUTPUT_DIR)/$(PHASE050_BOARD_LABEL)
PHASE050_SUMMARY := $(PHASE050_BOARD_DIR)/summary.md
PHASE050_EVIDENCE_MANIFEST := $(PHASE050_BOARD_DIR)/evidence-manifest.json
PHASE050_EVIDENCE_DOCTOR_MD := $(PHASE050_BOARD_DIR)/evidence-doctor.md
PHASE050_EVIDENCE_DOCTOR_JSON := $(PHASE050_BOARD_DIR)/evidence-doctor.json
PHASE050_EVIDENCE_RUN_LABEL := normal-plane-phase050-representative-aos-source-performance
EVIDENCE_DOC_REFS := \
	$(TOPIC_DOC_PREFIX)/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/sac_model_normal_plane-evaluation.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/testing-overview.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/benchmark-and-evidence.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-evidence.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/test-support-code-map.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/000-normal-plane-current-state-and-public-entry-boundary/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/010-normal-plane-test-support-structure/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-normal-plane-evidence-registry-target-alias/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-normal-plane-evidence-registry-target-alias/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/030-normal-plane-repeated-board-summary/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/030-normal-plane-repeated-board-summary/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/040-normal-plane-aospoint-gate-expansion/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/040-normal-plane-aospoint-gate-expansion/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/050-normal-plane-representative-aos-source-performance/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/050-normal-plane-representative-aos-source-performance/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/060-normal-plane-normal-layout-expansion/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/060-normal-plane-normal-layout-expansion/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/070-normal-plane-cross-point-type-layout/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/070-normal-plane-cross-point-type-layout/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/optimization-matrix.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-roadmap.zh.md \
	doc-rvv/sample_consensus/selectWithinDistance_getDistancesToModel_RVV.zh.md
EVIDENCE_DOC_ARGS := $(foreach doc,$(EVIDENCE_DOC_REFS),--doc $(doc))

PLANE_PCD := $(THIS_DIR)/pcd/sac_plane_test.pcd
TEST_ARGS := $(PLANE_PCD)
BENCH_ARGS := $(PLANE_PCD)

REMOTE_DIR ?= /root/pcl-test/sample_consensus/plane_models
REMOTE_PLANE_PCD := /root/pcl-test/sample_consensus/plane_models/pcd/sac_plane_test.pcd
# Deploy the fixture beside the board binaries and pass the board-side path to
# shared board targets so host absolute paths do not leak into remote runs.
DEPLOY_EXTRA_FILES := $(PLANE_PCD)
DEPLOY_EXTRA_REMOTE_DIR := $(REMOTE_DIR)/pcd

NORMAL_PLANE_PUBLIC_FILTER := SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForSupportedLayout:SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForPointXYZISource:SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForPointXYZINormalSource:SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForPointNormalNormalLayout:SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForPointXYZINormalNormalLayout:SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForPointXYZIAndPointNormal:SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForPointXYZIAndPointXYZINormal:SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForPointXYZINormalAndPointNormal:SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForPointXYZINormalAndPointXYZINormal:SampleConsensusModelNormalPlane.PublicEntriesFallbackForNonAoSRegisteredXYZSource:SampleConsensusModelNormalPlane.PublicEntriesFallbackForNonAoSRegisteredNormalLayout:SampleConsensusModelNormalPlane.PublicEntriesFallbackForUnregisteredNormalLayout:SampleConsensusModelNormalPlane.SelectHelperResizesEmptyOutputBuffers
run_board_bench_compare board_smoke: BENCH_ARGS := $(REMOTE_PLANE_PCD)
run_board_test board_smoke run_board_normal_plane_public_tests: REMOTE_TEST_ARGS := $(REMOTE_PLANE_PCD)

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

TARGET_BENCH_LOAD_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_LOAD)

all: $(TARGET_TEST_BIN) $(TARGET_BENCH_BIN) $(TARGET_BENCH_LOAD_BIN)

$(TARGET_BENCH_LOAD_BIN): $(SRCS_BENCH_LOAD) | $(LOG_DIR) $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling load-compare benchmark ($(ARCH), USE_PCL_RVV10=$(USE_PCL_RVV10))..."
	$(CXX) $(CXXFLAGS) $(SRCS_BENCH_LOAD) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_BENCH) -o $(TARGET_BENCH_LOAD_BIN)

run_bench_load: clean_bench_load $(TARGET_BENCH_LOAD_BIN)
	@echo "[RUN] NormalPlane RVV load-compare on $(ARCH)..."
	$(RUN_CMD) ./$(TARGET_BENCH_LOAD_BIN) $(PLANE_PCD) 200

run_normal_plane_public_tests:
	@$(MAKE) -C $(CURDIR) run_test_rvv TEST_ARGS="$(PLANE_PCD) --gtest_filter=$(NORMAL_PLANE_PUBLIC_FILTER)"

run_board_normal_plane_public_tests: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' REMOTE_TEST_ARGS='$(REMOTE_PLANE_PCD) --gtest_filter=$(NORMAL_PLANE_PUBLIC_FILTER)'"

run_board_bench_compare_repeated: BENCH_ARGS := $(REMOTE_PLANE_PCD)
run_board_bench_compare_repeated: deploy_board | $(OUTPUT_DIR_BOARD)
	@for run in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run_label=$$(printf "run_%02d" $$run); \
		remote_output="$(REPEATED_BOARD_REMOTE_DIR)/$$run_label"; \
		echo "[BOARD] NormalPlane repeated bench $$run/$(REPEATED_BOARD_RUNS) -> $$remote_output"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p '$$remote_output' && cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_output' BENCH_COMPARE_SAVE='$$remote_output/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(BENCH_ARGS)'"; \
	done
	@mkdir -p $(REPEATED_BOARD_DIR)
	@rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$(REPEATED_BOARD_REMOTE_DIR)/ $(REPEATED_BOARD_DIR)/

analyze_board_bench_compare_repeated:
	@logs=$$(find "$(REPEATED_BOARD_DIR)" -mindepth 2 -maxdepth 2 -name analyze_bench_compare.log | sort); \
	if [ -z "$$logs" ]; then \
		echo "[repeated] No analyze_bench_compare.log files under $(REPEATED_BOARD_DIR). Run run_board_bench_compare_repeated first." >&2; \
		exit 2; \
	fi; \
	mkdir -p "$(REPEATED_BOARD_DIR)"; \
	python3 ../../script/analyze_bench_repeated.py $$logs | tee "$(REPEATED_BOARD_SUMMARY)"

generate_board_evidence_manifest:
	python3 script/generate_normal_plane_board_evidence_manifest.py \
	  --std-log log/board/run_bench_std.log \
	  --rvv-log log/board/run_bench_rvv.log \
	  --summary log/board/analyze_bench_compare.log \
	  --output $(EVIDENCE_MANIFEST)

run_board_evidence_doctor: generate_board_evidence_manifest
	python3 ../../script/evidence_doctor.py \
	  --manifest $(EVIDENCE_MANIFEST) \
	  --output $(EVIDENCE_DOCTOR_MD) \
	  --json-output $(EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_board_evidence_state: run_board_evidence_doctor
	python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --summary log/board/analyze_bench_compare.log \
	  --manifest $(EVIDENCE_MANIFEST) \
	  --doctor $(EVIDENCE_DOCTOR_MD) \
	  --path $(EVIDENCE_DOCTOR_JSON) \
	  --target run_board_evidence_doctor \
	  --backend board \
	  --run-label $(EVIDENCE_RUN_LABEL) \
	  --case-filter protected-helper-hot-path \
	  --evidence-role production_shaped_diagnostic \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/000-normal-plane-current-state-and-public-entry-boundary/result.zh.md \
	  --freshness-state recorded

evidence_status:
	python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/log/board/analyze_bench_compare.log' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-manifest.json' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-doctor.md' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-doctor.json' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

generate_repeated_board_evidence_manifest: analyze_board_bench_compare_repeated
	@logs=$$(find "$(REPEATED_BOARD_DIR)" -mindepth 2 -maxdepth 2 -name analyze_bench_compare.log | sort); \
	python3 script/generate_normal_plane_board_evidence_manifest.py \
	  --compare-log $$logs \
	  --summary $(REPEATED_BOARD_SUMMARY) \
	  --output $(REPEATED_EVIDENCE_MANIFEST)

run_repeated_board_evidence_doctor: generate_repeated_board_evidence_manifest
	python3 ../../script/evidence_doctor.py \
	  --manifest $(REPEATED_EVIDENCE_MANIFEST) \
	  --output $(REPEATED_EVIDENCE_DOCTOR_MD) \
	  --json-output $(REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_repeated_board_evidence_state: run_repeated_board_evidence_doctor
	python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --summary $(REPEATED_BOARD_SUMMARY) \
	  --manifest $(REPEATED_EVIDENCE_MANIFEST) \
	  --doctor $(REPEATED_EVIDENCE_DOCTOR_MD) \
	  --path $(REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --target run_repeated_board_evidence_doctor \
	  --backend board \
	  --run-label $(REPEATED_EVIDENCE_RUN_LABEL) \
	  --case-filter protected-helper-hot-path-repeated \
	  --evidence-role production_shaped_diagnostic \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/030-normal-plane-repeated-board-summary/plan.zh.md \
	  --freshness-state recorded

repeated_evidence_status:
	python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_BOARD_SUMMARY)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(REPEATED_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

run_board_bench_compare_phase050: BENCH_ARGS := $(REMOTE_PLANE_PCD) --representative-aos-sources
run_board_bench_compare_phase050: deploy_board | $(OUTPUT_DIR_BOARD)
	@for run in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run_label=$$(printf "run_%02d" $$run); \
		remote_output="$(PHASE050_REMOTE_DIR)/$$run_label"; \
		echo "[BOARD] NormalPlane Phase 050 repeated bench $$run/$(REPEATED_BOARD_RUNS) -> $$remote_output"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p '$$remote_output' && cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_output' BENCH_COMPARE_SAVE='$$remote_output/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(BENCH_ARGS)'"; \
	done
	@mkdir -p $(PHASE050_BOARD_DIR)
	@rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$(PHASE050_REMOTE_DIR)/ $(PHASE050_BOARD_DIR)/

analyze_board_bench_compare_phase050:
	@logs=$$(find "$(PHASE050_BOARD_DIR)" -mindepth 2 -maxdepth 2 -name analyze_bench_compare.log | sort); \
	if [ -z "$$logs" ]; then \
		echo "[phase050] No analyze_bench_compare.log files under $(PHASE050_BOARD_DIR). Run run_board_bench_compare_phase050 first." >&2; \
		exit 2; \
	fi; \
	mkdir -p "$(PHASE050_BOARD_DIR)"; \
	python3 ../../script/analyze_bench_repeated.py $$logs | tee "$(PHASE050_SUMMARY)"

generate_phase050_evidence_manifest: analyze_board_bench_compare_phase050
	@logs=$$(find "$(PHASE050_BOARD_DIR)" -mindepth 2 -maxdepth 2 -name analyze_bench_compare.log | sort); \
	python3 script/generate_normal_plane_board_evidence_manifest.py \
	  --case-set representative-aos \
	  --compare-log $$logs \
	  --summary $(PHASE050_SUMMARY) \
	  --output $(PHASE050_EVIDENCE_MANIFEST)

run_phase050_evidence_doctor: generate_phase050_evidence_manifest
	python3 ../../script/evidence_doctor.py \
	  --manifest $(PHASE050_EVIDENCE_MANIFEST) \
	  --output $(PHASE050_EVIDENCE_DOCTOR_MD) \
	  --json-output $(PHASE050_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

record_phase050_evidence_state: run_phase050_evidence_doctor
	python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --summary $(PHASE050_SUMMARY) \
	  --manifest $(PHASE050_EVIDENCE_MANIFEST) \
	  --doctor $(PHASE050_EVIDENCE_DOCTOR_MD) \
	  --path $(PHASE050_EVIDENCE_DOCTOR_JSON) \
	  --target run_phase050_evidence_doctor \
	  --backend board \
	  --run-label $(PHASE050_EVIDENCE_RUN_LABEL) \
	  --case-filter representative-aos-source-performance \
	  --evidence-role production_shaped_diagnostic \
	  --doc-ref $(TOPIC_DOC_PREFIX)/doc/phases/050-normal-plane-representative-aos-source-performance/plan.zh.md \
	  --freshness-state recorded

phase050_evidence_status:
	python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PHASE050_SUMMARY)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PHASE050_EVIDENCE_MANIFEST)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PHASE050_EVIDENCE_DOCTOR_MD)' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/$(PHASE050_EVIDENCE_DOCTOR_JSON)' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any

clean_bench_load:
	rm -f $(TARGET_BENCH_LOAD_BIN)

deploy_bench: deploy_bench_rvv

deploy_bench_load: deploy_files clean_bench_load
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_BENCH_LOAD=$(TARGET_BENCH_LOAD) $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_LOAD)
	@$(STRIP) -s ./$(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_LOAD) -o ./$(TARGET_BENCH_LOAD)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_BENCH_LOAD)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_BENCH_LOAD)
	@rm -f ./$(TARGET_BENCH_LOAD)_stripped

clean: clean_bench_load

.PHONY: all run_bench_load run_normal_plane_public_tests run_board_normal_plane_public_tests run_board_bench_compare_repeated analyze_board_bench_compare_repeated generate_board_evidence_manifest run_board_evidence_doctor record_board_evidence_state evidence_status generate_repeated_board_evidence_manifest run_repeated_board_evidence_doctor record_repeated_board_evidence_state repeated_evidence_status run_board_bench_compare_phase050 analyze_board_bench_compare_phase050 generate_phase050_evidence_manifest run_phase050_evidence_doctor record_phase050_evidence_state phase050_evidence_status clean_bench_load deploy_bench deploy_bench_load
