# =============================================================================
#  PCL segmentation/grabcut_segmentation RVV diagnostic
# =============================================================================

TOPIC  := grabcut_segmentation
MODULE := segmentation

FOCUS_DIR      := pcl/segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp
FILTER_REPORT  := log/filtered_grabcut.log
ANALYZE_REPORT := log/analyze_grabcut.log

TARGET_BENCH     := bench_grabcut
TARGET_BENCH_STD := bench_grabcut_std
TARGET_BENCH_RVV := bench_grabcut_rvv
SRCS_PRODUCTION  = $(PCL_SOURCE_ROOT)/segmentation/src/grabcut_segmentation.cpp
SRCS_BENCH       = src/bench_grabcut.cpp $(SRCS_PRODUCTION)

TARGET_TEST     := test_grabcut
TARGET_TEST_STD := test_grabcut_std
TARGET_TEST_RVV := test_grabcut_rvv
SRCS_TEST       = src/test_grabcut.cpp $(SRCS_PRODUCTION)

EVIDENCE_MANIFEST  := log/board/evidence_manifest.json
EVIDENCE_DOCTOR_MD := log/board/evidence_doctor.md
EVIDENCE_DOCTOR_JSON := log/board/evidence_doctor.json
EVIDENCE_REGISTRY := log/evidence_registry.json
TOPIC_DOC_PREFIX := test-rvv/segmentation/grabcut_segmentation
GMM_EVIDENCE_MANIFEST  := doc/phases/010-gmm-probability-density-diagnostic/evidence-manifest.json
GMM_EVIDENCE_DOCTOR_MD := doc/phases/010-gmm-probability-density-diagnostic/evidence-doctor.md
GMM_EVIDENCE_DOCTOR_JSON := doc/phases/010-gmm-probability-density-diagnostic/evidence-doctor.json
TERMINAL_EVIDENCE_MANIFEST  := doc/phases/020-terminal-weight-public-shaped-diagnostic/evidence-manifest.json
TERMINAL_EVIDENCE_DOCTOR_MD := doc/phases/020-terminal-weight-public-shaped-diagnostic/evidence-doctor.md
TERMINAL_EVIDENCE_DOCTOR_JSON := doc/phases/020-terminal-weight-public-shaped-diagnostic/evidence-doctor.json
INITGRAPH_EVIDENCE_MANIFEST  := doc/phases/030-initgraph-no-solve-diagnostic/evidence-manifest.json
INITGRAPH_EVIDENCE_DOCTOR_MD := doc/phases/030-initgraph-no-solve-diagnostic/evidence-doctor.md
INITGRAPH_EVIDENCE_DOCTOR_JSON := doc/phases/030-initgraph-no-solve-diagnostic/evidence-doctor.json
INITGRAPH_REPEATED_DIR ?= doc/phases/030-initgraph-no-solve-diagnostic/repeated-board-20260827-144235
INITGRAPH_REPEATED_EVIDENCE_MANIFEST  := doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-manifest.json
INITGRAPH_REPEATED_EVIDENCE_DOCTOR_MD := doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-doctor.md
INITGRAPH_REPEATED_EVIDENCE_DOCTOR_JSON := doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-doctor.json
PRODUCTION_REPEATED_CURRENT_DIR_FILE := doc/phases/060-production-initgraph-terminal-evidence/repeated-board-current-dir.txt
PRODUCTION_REPEATED_DIR ?= $(shell if [ -f $(PRODUCTION_REPEATED_CURRENT_DIR_FILE) ]; then cat $(PRODUCTION_REPEATED_CURRENT_DIR_FILE); else date '+doc/phases/060-production-initgraph-terminal-evidence/repeated-board-%Y%m%d-%H%M%S'; fi)
PRODUCTION_REPEATED_EVIDENCE_MANIFEST  := doc/phases/060-production-initgraph-terminal-evidence/repeated-evidence-manifest.json
PRODUCTION_REPEATED_EVIDENCE_DOCTOR_MD := doc/phases/060-production-initgraph-terminal-evidence/repeated-evidence-doctor.md
PRODUCTION_REPEATED_EVIDENCE_DOCTOR_JSON := doc/phases/060-production-initgraph-terminal-evidence/repeated-evidence-doctor.json
PUBLIC_EXTRACT_REPEATED_CURRENT_DIR_FILE := doc/phases/070-public-extract-wall-time-adoption-check/repeated-board-current-dir.txt
PUBLIC_EXTRACT_REPEATED_DIR ?= $(shell if [ -f $(PUBLIC_EXTRACT_REPEATED_CURRENT_DIR_FILE) ]; then cat $(PUBLIC_EXTRACT_REPEATED_CURRENT_DIR_FILE); else date '+doc/phases/070-public-extract-wall-time-adoption-check/repeated-board-%Y%m%d-%H%M%S'; fi)
PUBLIC_EXTRACT_REPEATED_EVIDENCE_MANIFEST  := doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-manifest.json
PUBLIC_EXTRACT_REPEATED_EVIDENCE_DOCTOR_MD := doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-doctor.md
PUBLIC_EXTRACT_REPEATED_EVIDENCE_DOCTOR_JSON := doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-doctor.json
PUBLIC_EXTRACT_PROFILE_REPEATED_CURRENT_DIR_FILE := doc/phases/080-public-extract-component-profile/repeated-board-current-dir.txt
PUBLIC_EXTRACT_PROFILE_REPEATED_DIR ?= $(shell if [ -f $(PUBLIC_EXTRACT_PROFILE_REPEATED_CURRENT_DIR_FILE) ]; then cat $(PUBLIC_EXTRACT_PROFILE_REPEATED_CURRENT_DIR_FILE); else date '+doc/phases/080-public-extract-component-profile/repeated-board-%Y%m%d-%H%M%S'; fi)
PUBLIC_EXTRACT_PROFILE_REPEATED_EVIDENCE_MANIFEST  := doc/phases/080-public-extract-component-profile/repeated-evidence-manifest.json
PUBLIC_EXTRACT_PROFILE_REPEATED_EVIDENCE_DOCTOR_MD := doc/phases/080-public-extract-component-profile/repeated-evidence-doctor.md
PUBLIC_EXTRACT_PROFILE_REPEATED_EVIDENCE_DOCTOR_JSON := doc/phases/080-public-extract-component-profile/repeated-evidence-doctor.json
LEARN_GMM_ASSIGNMENT_REPEATED_CURRENT_DIR_FILE := doc/phases/090-learn-gmm-component-assignment-diagnostic/repeated-board-current-dir.txt
LEARN_GMM_ASSIGNMENT_REPEATED_DIR ?= $(shell if [ -f $(LEARN_GMM_ASSIGNMENT_REPEATED_CURRENT_DIR_FILE) ]; then cat $(LEARN_GMM_ASSIGNMENT_REPEATED_CURRENT_DIR_FILE); else date '+doc/phases/090-learn-gmm-component-assignment-diagnostic/repeated-board-%Y%m%d-%H%M%S'; fi)
LEARN_GMM_ASSIGNMENT_REPEATED_EVIDENCE_MANIFEST  := doc/phases/090-learn-gmm-component-assignment-diagnostic/repeated-evidence-manifest.json
LEARN_GMM_ASSIGNMENT_REPEATED_EVIDENCE_DOCTOR_MD := doc/phases/090-learn-gmm-component-assignment-diagnostic/repeated-evidence-doctor.md
LEARN_GMM_ASSIGNMENT_REPEATED_EVIDENCE_DOCTOR_JSON := doc/phases/090-learn-gmm-component-assignment-diagnostic/repeated-evidence-doctor.json
LEARN_GMMS_FULL_REPEATED_CURRENT_DIR_FILE := doc/phases/100-learn-gmms-full-production-shaped-diagnostic/repeated-board-current-dir.txt
LEARN_GMMS_FULL_REPEATED_DIR ?= $(shell if [ -f $(LEARN_GMMS_FULL_REPEATED_CURRENT_DIR_FILE) ]; then cat $(LEARN_GMMS_FULL_REPEATED_CURRENT_DIR_FILE); else date '+doc/phases/100-learn-gmms-full-production-shaped-diagnostic/repeated-board-%Y%m%d-%H%M%S'; fi)
LEARN_GMMS_FULL_REPEATED_EVIDENCE_MANIFEST  := doc/phases/100-learn-gmms-full-production-shaped-diagnostic/repeated-evidence-manifest.json
LEARN_GMMS_FULL_REPEATED_EVIDENCE_DOCTOR_MD := doc/phases/100-learn-gmms-full-production-shaped-diagnostic/repeated-evidence-doctor.md
LEARN_GMMS_FULL_REPEATED_EVIDENCE_DOCTOR_JSON := doc/phases/100-learn-gmms-full-production-shaped-diagnostic/repeated-evidence-doctor.json
PRODUCTION_LEARN_GMMS_REPEATED_CURRENT_DIR_FILE := doc/phases/110-learn-gmms-production-integration-plan/repeated-board-current-dir.txt
PRODUCTION_LEARN_GMMS_REPEATED_DIR ?= $(shell if [ -f $(PRODUCTION_LEARN_GMMS_REPEATED_CURRENT_DIR_FILE) ]; then cat $(PRODUCTION_LEARN_GMMS_REPEATED_CURRENT_DIR_FILE); else date '+doc/phases/110-learn-gmms-production-integration-plan/repeated-board-%Y%m%d-%H%M%S'; fi)
PRODUCTION_LEARN_GMMS_REPEATED_EVIDENCE_MANIFEST  := doc/phases/110-learn-gmms-production-integration-plan/repeated-evidence-manifest.json
PRODUCTION_LEARN_GMMS_REPEATED_EVIDENCE_DOCTOR_MD := doc/phases/110-learn-gmms-production-integration-plan/repeated-evidence-doctor.md
PRODUCTION_LEARN_GMMS_REPEATED_EVIDENCE_DOCTOR_JSON := doc/phases/110-learn-gmms-production-integration-plan/repeated-evidence-doctor.json
PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_CURRENT_DIR_FILE := doc/phases/110-learn-gmms-production-integration-plan/public-repeated-board-current-dir.txt
PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_DIR ?= $(shell if [ -f $(PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_CURRENT_DIR_FILE) ]; then cat $(PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_CURRENT_DIR_FILE); else date '+doc/phases/110-learn-gmms-production-integration-plan/public-repeated-board-%Y%m%d-%H%M%S'; fi)
PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_EVIDENCE_MANIFEST  := doc/phases/110-learn-gmms-production-integration-plan/public-repeated-evidence-manifest.json
PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_EVIDENCE_DOCTOR_MD := doc/phases/110-learn-gmms-production-integration-plan/public-repeated-evidence-doctor.md
PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_EVIDENCE_DOCTOR_JSON := doc/phases/110-learn-gmms-production-integration-plan/public-repeated-evidence-doctor.json
POST_LEARN_GMMS_PROFILE_REPEATED_CURRENT_DIR_FILE := doc/phases/120-post-learn-gmms-adoption-profile/repeated-board-current-dir.txt
POST_LEARN_GMMS_PROFILE_REPEATED_DIR ?= $(shell if [ -f $(POST_LEARN_GMMS_PROFILE_REPEATED_CURRENT_DIR_FILE) ]; then cat $(POST_LEARN_GMMS_PROFILE_REPEATED_CURRENT_DIR_FILE); else date '+doc/phases/120-post-learn-gmms-adoption-profile/repeated-board-%Y%m%d-%H%M%S'; fi)
POST_LEARN_GMMS_PROFILE_REPEATED_EVIDENCE_MANIFEST  := doc/phases/120-post-learn-gmms-adoption-profile/repeated-evidence-manifest.json
POST_LEARN_GMMS_PROFILE_REPEATED_EVIDENCE_DOCTOR_MD := doc/phases/120-post-learn-gmms-adoption-profile/repeated-evidence-doctor.md
POST_LEARN_GMMS_PROFILE_REPEATED_EVIDENCE_DOCTOR_JSON := doc/phases/120-post-learn-gmms-adoption-profile/repeated-evidence-doctor.json

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/search/include \
	-I$(PCL_SOURCE_ROOT)/segmentation/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(FLANN_ROOT)/include \
	-I$(LZ4_ROOT)/include \
	-I$(GTEST_ROOT)/include

LIBS_CORE := -lpcl_search -lpcl_common -lboost_system -llz4 -lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread

EVIDENCE_DOC_REFS := \
	$(TOPIC_DOC_PREFIX)/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/grabcut_segmentation-evaluation.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-roadmap.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/optimization-matrix.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/030-initgraph-no-solve-diagnostic/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/040-production-integration-plan/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/050-evidence-registry-target-alias/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/050-evidence-registry-target-alias/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/060-production-initgraph-terminal-evidence/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/060-production-initgraph-terminal-evidence/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/070-public-extract-wall-time-adoption-check/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/070-public-extract-wall-time-adoption-check/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/080-public-extract-component-profile/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/080-public-extract-component-profile/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/090-learn-gmm-component-assignment-diagnostic/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/090-learn-gmm-component-assignment-diagnostic/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/100-learn-gmms-full-production-shaped-diagnostic/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/100-learn-gmms-full-production-shaped-diagnostic/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/110-learn-gmms-production-integration-plan/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/110-learn-gmms-production-integration-plan/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/120-post-learn-gmms-adoption-profile/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/120-post-learn-gmms-adoption-profile/result.zh.md \
	doc-rvv/segmentation/grabcut_segmentation-RVV.zh.md

EVIDENCE_DOC_ARGS := $(foreach doc,$(EVIDENCE_DOC_REFS),--doc $(doc))

include ../../mk/rvv-topic.mk

.PHONY: run_grabcut_test_compare dump_grabcut_bench_rvv generate_board_evidence_manifest run_board_evidence_doctor generate_gmm_evidence_manifest run_gmm_evidence_doctor generate_terminal_evidence_manifest run_terminal_evidence_doctor generate_initgraph_evidence_manifest run_initgraph_evidence_doctor generate_initgraph_repeated_evidence_manifest run_initgraph_repeated_evidence_doctor record_initgraph_repeated_evidence_files record_initgraph_repeated_evidence_state collect_production_initgraph_terminal_repeated_board generate_production_repeated_evidence_manifest run_production_repeated_evidence_doctor record_production_repeated_evidence_files record_production_repeated_evidence_state collect_public_extract_repeated_board generate_public_extract_repeated_evidence_manifest run_public_extract_repeated_evidence_doctor record_public_extract_repeated_evidence_files record_public_extract_repeated_evidence_state collect_public_extract_profile_repeated_board generate_public_extract_profile_repeated_evidence_manifest run_public_extract_profile_repeated_evidence_doctor record_public_extract_profile_repeated_evidence_files record_public_extract_profile_repeated_evidence_state collect_learn_gmm_assignment_repeated_board generate_learn_gmm_assignment_repeated_evidence_manifest run_learn_gmm_assignment_repeated_evidence_doctor record_learn_gmm_assignment_repeated_evidence_files record_learn_gmm_assignment_repeated_evidence_state collect_learn_gmms_full_repeated_board generate_learn_gmms_full_repeated_evidence_manifest run_learn_gmms_full_repeated_evidence_doctor record_learn_gmms_full_repeated_evidence_files record_learn_gmms_full_repeated_evidence_state collect_production_learn_gmms_repeated_board generate_production_learn_gmms_repeated_evidence_manifest run_production_learn_gmms_repeated_evidence_doctor record_production_learn_gmms_repeated_evidence_files record_production_learn_gmms_repeated_evidence_state collect_production_learn_gmms_public_repeated_board generate_production_learn_gmms_public_repeated_evidence_manifest run_production_learn_gmms_public_repeated_evidence_doctor record_production_learn_gmms_public_repeated_evidence_files record_production_learn_gmms_public_repeated_evidence_state collect_post_learn_gmms_profile_repeated_board generate_post_learn_gmms_profile_repeated_evidence_manifest run_post_learn_gmms_profile_repeated_evidence_doctor record_post_learn_gmms_profile_repeated_evidence_files record_post_learn_gmms_profile_repeated_evidence_state evidence_status

run_grabcut_test_compare: run_test_compare

dump_grabcut_bench_rvv: dump_bench_rvv

generate_board_evidence_manifest:
	python3 script/generate_grabcut_board_evidence_manifest.py \
	  --std-log log/board/run_bench_std.log \
	  --rvv-log log/board/run_bench_rvv.log \
	  --output $(EVIDENCE_MANIFEST)

run_board_evidence_doctor: generate_board_evidence_manifest
	python3 ../../script/evidence_doctor.py \
	  --manifest $(EVIDENCE_MANIFEST) \
	  --output $(EVIDENCE_DOCTOR_MD) \
	  --json-output $(EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

generate_gmm_evidence_manifest:
	python3 script/generate_grabcut_board_evidence_manifest.py \
	  --std-log log/board/run_bench_std.log \
	  --rvv-log log/board/run_bench_rvv.log \
	  --output $(GMM_EVIDENCE_MANIFEST) \
	  --case gmm_probability

run_gmm_evidence_doctor: generate_gmm_evidence_manifest
	python3 ../../script/evidence_doctor.py \
	  --manifest $(GMM_EVIDENCE_MANIFEST) \
	  --output $(GMM_EVIDENCE_DOCTOR_MD) \
	  --json-output $(GMM_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

generate_terminal_evidence_manifest:
	python3 script/generate_grabcut_board_evidence_manifest.py \
	  --std-log log/board/run_bench_std.log \
	  --rvv-log log/board/run_bench_rvv.log \
	  --output $(TERMINAL_EVIDENCE_MANIFEST) \
	  --case terminal_weights

run_terminal_evidence_doctor: generate_terminal_evidence_manifest
	python3 ../../script/evidence_doctor.py \
	  --manifest $(TERMINAL_EVIDENCE_MANIFEST) \
	  --output $(TERMINAL_EVIDENCE_DOCTOR_MD) \
	  --json-output $(TERMINAL_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

generate_initgraph_evidence_manifest:
	python3 script/generate_grabcut_board_evidence_manifest.py \
	  --std-log log/board/run_bench_std.log \
	  --rvv-log log/board/run_bench_rvv.log \
	  --output $(INITGRAPH_EVIDENCE_MANIFEST) \
	  --case initgraph_no_solve

run_initgraph_evidence_doctor: generate_initgraph_evidence_manifest
	python3 ../../script/evidence_doctor.py \
	  --manifest $(INITGRAPH_EVIDENCE_MANIFEST) \
	  --output $(INITGRAPH_EVIDENCE_DOCTOR_MD) \
	  --json-output $(INITGRAPH_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never

generate_initgraph_repeated_evidence_manifest:
	python3 script/generate_grabcut_board_evidence_manifest.py \
	  --repeated-dir $(INITGRAPH_REPEATED_DIR) \
	  --output $(INITGRAPH_REPEATED_EVIDENCE_MANIFEST) \
	  --case initgraph_no_solve

run_initgraph_repeated_evidence_doctor: generate_initgraph_repeated_evidence_manifest
	python3 ../../script/evidence_doctor.py \
	  --manifest $(INITGRAPH_REPEATED_EVIDENCE_MANIFEST) \
	  --output $(INITGRAPH_REPEATED_EVIDENCE_DOCTOR_MD) \
	  --json-output $(INITGRAPH_REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never
	$(MAKE) -C $(CURDIR) record_initgraph_repeated_evidence_files

record_initgraph_repeated_evidence_files:
	python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(INITGRAPH_REPEATED_EVIDENCE_MANIFEST) \
	  --doctor $(INITGRAPH_REPEATED_EVIDENCE_DOCTOR_MD) \
	  --path $(INITGRAPH_REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --target record_initgraph_repeated_evidence_state \
	  --backend board \
	  --run-label $(notdir $(INITGRAPH_REPEATED_DIR)) \
	  --case-filter initgraph_no_solve \
	  --evidence-role production_shaped_diagnostic \
	  --freshness-state recorded

record_initgraph_repeated_evidence_state: run_initgraph_repeated_evidence_doctor

collect_production_initgraph_terminal_repeated_board:
	mkdir -p $(PRODUCTION_REPEATED_DIR)
	printf '%s\n' '$(PRODUCTION_REPEATED_DIR)' > $(PRODUCTION_REPEATED_CURRENT_DIR_FILE)
	set -e; \
	for run in 01 02 03 04 05; do \
	  $(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--width 640 --height 480 --iterations 8 --warmup 2 --case production_initgraph_terminal"; \
	  $(MAKE) -C $(CURDIR) fetch_board_logs; \
	  cp log/board/run_bench_std.log $(PRODUCTION_REPEATED_DIR)/run$${run}_std.log; \
	  cp log/board/run_bench_rvv.log $(PRODUCTION_REPEATED_DIR)/run$${run}_rvv.log; \
	  cp log/board/analyze_bench_compare.log $(PRODUCTION_REPEATED_DIR)/run$${run}_analyze.log; \
	done

generate_production_repeated_evidence_manifest:
	python3 script/generate_grabcut_board_evidence_manifest.py \
	  --repeated-dir $(PRODUCTION_REPEATED_DIR) \
	  --output $(PRODUCTION_REPEATED_EVIDENCE_MANIFEST) \
	  --case production_initgraph_terminal

run_production_repeated_evidence_doctor: generate_production_repeated_evidence_manifest
	python3 ../../script/evidence_doctor.py \
	  --manifest $(PRODUCTION_REPEATED_EVIDENCE_MANIFEST) \
	  --output $(PRODUCTION_REPEATED_EVIDENCE_DOCTOR_MD) \
	  --json-output $(PRODUCTION_REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never
	$(MAKE) -C $(CURDIR) record_production_repeated_evidence_files

record_production_repeated_evidence_files:
	python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(PRODUCTION_REPEATED_EVIDENCE_MANIFEST) \
	  --doctor $(PRODUCTION_REPEATED_EVIDENCE_DOCTOR_MD) \
	  --path $(PRODUCTION_REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --target record_production_repeated_evidence_state \
	  --backend board \
	  --run-label $(notdir $(PRODUCTION_REPEATED_DIR)) \
	  --case-filter production_initgraph_terminal \
	  --evidence-role production-detail \
	  --freshness-state recorded

record_production_repeated_evidence_state: run_production_repeated_evidence_doctor

collect_public_extract_repeated_board:
	mkdir -p $(PUBLIC_EXTRACT_REPEATED_DIR)
	printf '%s\n' '$(PUBLIC_EXTRACT_REPEATED_DIR)' > $(PUBLIC_EXTRACT_REPEATED_CURRENT_DIR_FILE)
	set -e; \
	for run in 01 02 03 04 05; do \
	  $(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--width 96 --height 72 --iterations 3 --warmup 1 --case public_extract"; \
	  $(MAKE) -C $(CURDIR) fetch_board_logs; \
	  cp log/board/run_bench_std.log $(PUBLIC_EXTRACT_REPEATED_DIR)/run$${run}_std.log; \
	  cp log/board/run_bench_rvv.log $(PUBLIC_EXTRACT_REPEATED_DIR)/run$${run}_rvv.log; \
	  cp log/board/analyze_bench_compare.log $(PUBLIC_EXTRACT_REPEATED_DIR)/run$${run}_analyze.log; \
	done

generate_public_extract_repeated_evidence_manifest:
	python3 script/generate_grabcut_board_evidence_manifest.py \
	  --repeated-dir $(PUBLIC_EXTRACT_REPEATED_DIR) \
	  --output $(PUBLIC_EXTRACT_REPEATED_EVIDENCE_MANIFEST) \
	  --case public_extract

run_public_extract_repeated_evidence_doctor: generate_public_extract_repeated_evidence_manifest
	python3 ../../script/evidence_doctor.py \
	  --manifest $(PUBLIC_EXTRACT_REPEATED_EVIDENCE_MANIFEST) \
	  --output $(PUBLIC_EXTRACT_REPEATED_EVIDENCE_DOCTOR_MD) \
	  --json-output $(PUBLIC_EXTRACT_REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never
	$(MAKE) -C $(CURDIR) record_public_extract_repeated_evidence_files

record_public_extract_repeated_evidence_files:
	python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(PUBLIC_EXTRACT_REPEATED_EVIDENCE_MANIFEST) \
	  --doctor $(PUBLIC_EXTRACT_REPEATED_EVIDENCE_DOCTOR_MD) \
	  --path $(PUBLIC_EXTRACT_REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --target record_public_extract_repeated_evidence_state \
	  --backend board \
	  --run-label $(notdir $(PUBLIC_EXTRACT_REPEATED_DIR)) \
	  --case-filter public_extract \
	  --evidence-role production-public \
	  --freshness-state recorded

record_public_extract_repeated_evidence_state: run_public_extract_repeated_evidence_doctor

collect_public_extract_profile_repeated_board:
	mkdir -p $(PUBLIC_EXTRACT_PROFILE_REPEATED_DIR)
	printf '%s\n' '$(PUBLIC_EXTRACT_PROFILE_REPEATED_DIR)' > $(PUBLIC_EXTRACT_PROFILE_REPEATED_CURRENT_DIR_FILE)
	set -e; \
	for run in 01 02 03 04 05; do \
	  $(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--width 96 --height 72 --iterations 3 --warmup 1 --case public_extract_profile"; \
	  $(MAKE) -C $(CURDIR) fetch_board_logs; \
	  cp log/board/run_bench_std.log $(PUBLIC_EXTRACT_PROFILE_REPEATED_DIR)/run$${run}_std.log; \
	  cp log/board/run_bench_rvv.log $(PUBLIC_EXTRACT_PROFILE_REPEATED_DIR)/run$${run}_rvv.log; \
	  cp log/board/analyze_bench_compare.log $(PUBLIC_EXTRACT_PROFILE_REPEATED_DIR)/run$${run}_analyze.log; \
	done

generate_public_extract_profile_repeated_evidence_manifest:
	python3 script/generate_grabcut_board_evidence_manifest.py \
	  --repeated-dir $(PUBLIC_EXTRACT_PROFILE_REPEATED_DIR) \
	  --output $(PUBLIC_EXTRACT_PROFILE_REPEATED_EVIDENCE_MANIFEST) \
	  --case public_extract_profile

run_public_extract_profile_repeated_evidence_doctor: generate_public_extract_profile_repeated_evidence_manifest
	python3 ../../script/evidence_doctor.py \
	  --manifest $(PUBLIC_EXTRACT_PROFILE_REPEATED_EVIDENCE_MANIFEST) \
	  --output $(PUBLIC_EXTRACT_PROFILE_REPEATED_EVIDENCE_DOCTOR_MD) \
	  --json-output $(PUBLIC_EXTRACT_PROFILE_REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never
	$(MAKE) -C $(CURDIR) record_public_extract_profile_repeated_evidence_files

record_public_extract_profile_repeated_evidence_files:
	python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(PUBLIC_EXTRACT_PROFILE_REPEATED_EVIDENCE_MANIFEST) \
	  --doctor $(PUBLIC_EXTRACT_PROFILE_REPEATED_EVIDENCE_DOCTOR_MD) \
	  --path $(PUBLIC_EXTRACT_PROFILE_REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --target record_public_extract_profile_repeated_evidence_state \
	  --backend board \
	  --run-label $(notdir $(PUBLIC_EXTRACT_PROFILE_REPEATED_DIR)) \
	  --case-filter public_extract_profile \
	  --evidence-role diagnostic-profile \
	  --freshness-state recorded

record_public_extract_profile_repeated_evidence_state: run_public_extract_profile_repeated_evidence_doctor

collect_post_learn_gmms_profile_repeated_board:
	mkdir -p $(POST_LEARN_GMMS_PROFILE_REPEATED_DIR)
	printf '%s\n' '$(POST_LEARN_GMMS_PROFILE_REPEATED_DIR)' > $(POST_LEARN_GMMS_PROFILE_REPEATED_CURRENT_DIR_FILE)
	set -e; \
	for run in 01 02 03 04 05; do \
	  $(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--width 96 --height 72 --iterations 3 --warmup 1 --case public_extract_profile"; \
	  $(MAKE) -C $(CURDIR) fetch_board_logs; \
	  cp log/board/run_bench_std.log $(POST_LEARN_GMMS_PROFILE_REPEATED_DIR)/run$${run}_std.log; \
	  cp log/board/run_bench_rvv.log $(POST_LEARN_GMMS_PROFILE_REPEATED_DIR)/run$${run}_rvv.log; \
	  cp log/board/analyze_bench_compare.log $(POST_LEARN_GMMS_PROFILE_REPEATED_DIR)/run$${run}_analyze.log; \
	done

generate_post_learn_gmms_profile_repeated_evidence_manifest:
	python3 script/generate_grabcut_board_evidence_manifest.py \
	  --repeated-dir $(POST_LEARN_GMMS_PROFILE_REPEATED_DIR) \
	  --output $(POST_LEARN_GMMS_PROFILE_REPEATED_EVIDENCE_MANIFEST) \
	  --case public_extract_profile

run_post_learn_gmms_profile_repeated_evidence_doctor: generate_post_learn_gmms_profile_repeated_evidence_manifest
	python3 ../../script/evidence_doctor.py \
	  --manifest $(POST_LEARN_GMMS_PROFILE_REPEATED_EVIDENCE_MANIFEST) \
	  --output $(POST_LEARN_GMMS_PROFILE_REPEATED_EVIDENCE_DOCTOR_MD) \
	  --json-output $(POST_LEARN_GMMS_PROFILE_REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never
	$(MAKE) -C $(CURDIR) record_post_learn_gmms_profile_repeated_evidence_files

record_post_learn_gmms_profile_repeated_evidence_files:
	python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(POST_LEARN_GMMS_PROFILE_REPEATED_EVIDENCE_MANIFEST) \
	  --doctor $(POST_LEARN_GMMS_PROFILE_REPEATED_EVIDENCE_DOCTOR_MD) \
	  --path $(POST_LEARN_GMMS_PROFILE_REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --target record_post_learn_gmms_profile_repeated_evidence_state \
	  --backend board \
	  --run-label $(notdir $(POST_LEARN_GMMS_PROFILE_REPEATED_DIR)) \
	  --case-filter public_extract_profile \
	  --evidence-role diagnostic-profile \
	  --freshness-state recorded

record_post_learn_gmms_profile_repeated_evidence_state: run_post_learn_gmms_profile_repeated_evidence_doctor

collect_learn_gmm_assignment_repeated_board:
	mkdir -p $(LEARN_GMM_ASSIGNMENT_REPEATED_DIR)
	printf '%s\n' '$(LEARN_GMM_ASSIGNMENT_REPEATED_DIR)' > $(LEARN_GMM_ASSIGNMENT_REPEATED_CURRENT_DIR_FILE)
	set -e; \
	for run in 01 02 03 04 05; do \
	  $(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--width 96 --height 72 --iterations 8 --warmup 2 --case learn_gmm_assignment"; \
	  $(MAKE) -C $(CURDIR) fetch_board_logs; \
	  cp log/board/run_bench_std.log $(LEARN_GMM_ASSIGNMENT_REPEATED_DIR)/run$${run}_std.log; \
	  cp log/board/run_bench_rvv.log $(LEARN_GMM_ASSIGNMENT_REPEATED_DIR)/run$${run}_rvv.log; \
	  cp log/board/analyze_bench_compare.log $(LEARN_GMM_ASSIGNMENT_REPEATED_DIR)/run$${run}_analyze.log; \
	done

generate_learn_gmm_assignment_repeated_evidence_manifest:
	python3 script/generate_grabcut_board_evidence_manifest.py \
	  --repeated-dir $(LEARN_GMM_ASSIGNMENT_REPEATED_DIR) \
	  --output $(LEARN_GMM_ASSIGNMENT_REPEATED_EVIDENCE_MANIFEST) \
	  --case learn_gmm_assignment

run_learn_gmm_assignment_repeated_evidence_doctor: generate_learn_gmm_assignment_repeated_evidence_manifest
	python3 ../../script/evidence_doctor.py \
	  --manifest $(LEARN_GMM_ASSIGNMENT_REPEATED_EVIDENCE_MANIFEST) \
	  --output $(LEARN_GMM_ASSIGNMENT_REPEATED_EVIDENCE_DOCTOR_MD) \
	  --json-output $(LEARN_GMM_ASSIGNMENT_REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never
	$(MAKE) -C $(CURDIR) record_learn_gmm_assignment_repeated_evidence_files

record_learn_gmm_assignment_repeated_evidence_files:
	python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(LEARN_GMM_ASSIGNMENT_REPEATED_EVIDENCE_MANIFEST) \
	  --doctor $(LEARN_GMM_ASSIGNMENT_REPEATED_EVIDENCE_DOCTOR_MD) \
	  --path $(LEARN_GMM_ASSIGNMENT_REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --target record_learn_gmm_assignment_repeated_evidence_state \
	  --backend board \
	  --run-label $(notdir $(LEARN_GMM_ASSIGNMENT_REPEATED_DIR)) \
	  --case-filter learn_gmm_assignment \
	  --evidence-role diagnostic \
	  --freshness-state recorded

record_learn_gmm_assignment_repeated_evidence_state: run_learn_gmm_assignment_repeated_evidence_doctor

collect_learn_gmms_full_repeated_board:
	mkdir -p $(LEARN_GMMS_FULL_REPEATED_DIR)
	printf '%s\n' '$(LEARN_GMMS_FULL_REPEATED_DIR)' > $(LEARN_GMMS_FULL_REPEATED_CURRENT_DIR_FILE)
	set -e; \
	for run in 01 02 03 04 05; do \
	  $(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--width 96 --height 72 --iterations 8 --warmup 2 --case learn_gmms_full"; \
	  $(MAKE) -C $(CURDIR) fetch_board_logs; \
	  cp log/board/run_bench_std.log $(LEARN_GMMS_FULL_REPEATED_DIR)/run$${run}_std.log; \
	  cp log/board/run_bench_rvv.log $(LEARN_GMMS_FULL_REPEATED_DIR)/run$${run}_rvv.log; \
	  cp log/board/analyze_bench_compare.log $(LEARN_GMMS_FULL_REPEATED_DIR)/run$${run}_analyze.log; \
	done

generate_learn_gmms_full_repeated_evidence_manifest:
	python3 script/generate_grabcut_board_evidence_manifest.py \
	  --repeated-dir $(LEARN_GMMS_FULL_REPEATED_DIR) \
	  --output $(LEARN_GMMS_FULL_REPEATED_EVIDENCE_MANIFEST) \
	  --case learn_gmms_full

run_learn_gmms_full_repeated_evidence_doctor: generate_learn_gmms_full_repeated_evidence_manifest
	python3 ../../script/evidence_doctor.py \
	  --manifest $(LEARN_GMMS_FULL_REPEATED_EVIDENCE_MANIFEST) \
	  --output $(LEARN_GMMS_FULL_REPEATED_EVIDENCE_DOCTOR_MD) \
	  --json-output $(LEARN_GMMS_FULL_REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never
	$(MAKE) -C $(CURDIR) record_learn_gmms_full_repeated_evidence_files

record_learn_gmms_full_repeated_evidence_files:
	python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(LEARN_GMMS_FULL_REPEATED_EVIDENCE_MANIFEST) \
	  --doctor $(LEARN_GMMS_FULL_REPEATED_EVIDENCE_DOCTOR_MD) \
	  --path $(LEARN_GMMS_FULL_REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --target record_learn_gmms_full_repeated_evidence_state \
	  --backend board \
	  --run-label $(notdir $(LEARN_GMMS_FULL_REPEATED_DIR)) \
	  --case-filter learn_gmms_full \
	  --evidence-role production-shaped-diagnostic \
	  --freshness-state recorded

record_learn_gmms_full_repeated_evidence_state: run_learn_gmms_full_repeated_evidence_doctor

collect_production_learn_gmms_repeated_board:
	mkdir -p $(PRODUCTION_LEARN_GMMS_REPEATED_DIR)
	printf '%s\n' '$(PRODUCTION_LEARN_GMMS_REPEATED_DIR)' > $(PRODUCTION_LEARN_GMMS_REPEATED_CURRENT_DIR_FILE)
	set -e; \
	for run in 01 02 03 04 05; do \
	  $(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--width 96 --height 72 --iterations 8 --warmup 2 --case production_learn_gmms"; \
	  $(MAKE) -C $(CURDIR) fetch_board_logs; \
	  cp log/board/run_bench_std.log $(PRODUCTION_LEARN_GMMS_REPEATED_DIR)/run$${run}_std.log; \
	  cp log/board/run_bench_rvv.log $(PRODUCTION_LEARN_GMMS_REPEATED_DIR)/run$${run}_rvv.log; \
	  cp log/board/analyze_bench_compare.log $(PRODUCTION_LEARN_GMMS_REPEATED_DIR)/run$${run}_analyze.log; \
	done

generate_production_learn_gmms_repeated_evidence_manifest:
	python3 script/generate_grabcut_board_evidence_manifest.py \
	  --repeated-dir $(PRODUCTION_LEARN_GMMS_REPEATED_DIR) \
	  --output $(PRODUCTION_LEARN_GMMS_REPEATED_EVIDENCE_MANIFEST) \
	  --case production_learn_gmms

run_production_learn_gmms_repeated_evidence_doctor: generate_production_learn_gmms_repeated_evidence_manifest
	python3 ../../script/evidence_doctor.py \
	  --manifest $(PRODUCTION_LEARN_GMMS_REPEATED_EVIDENCE_MANIFEST) \
	  --output $(PRODUCTION_LEARN_GMMS_REPEATED_EVIDENCE_DOCTOR_MD) \
	  --json-output $(PRODUCTION_LEARN_GMMS_REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never
	$(MAKE) -C $(CURDIR) record_production_learn_gmms_repeated_evidence_files

record_production_learn_gmms_repeated_evidence_files:
	python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(PRODUCTION_LEARN_GMMS_REPEATED_EVIDENCE_MANIFEST) \
	  --doctor $(PRODUCTION_LEARN_GMMS_REPEATED_EVIDENCE_DOCTOR_MD) \
	  --path $(PRODUCTION_LEARN_GMMS_REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --target record_production_learn_gmms_repeated_evidence_state \
	  --backend board \
	  --run-label $(notdir $(PRODUCTION_LEARN_GMMS_REPEATED_DIR)) \
	  --case-filter production_learn_gmms \
	  --evidence-role production-detail \
	  --freshness-state recorded

record_production_learn_gmms_repeated_evidence_state: run_production_learn_gmms_repeated_evidence_doctor

collect_production_learn_gmms_public_repeated_board:
	mkdir -p $(PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_DIR)
	printf '%s\n' '$(PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_DIR)' > $(PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_CURRENT_DIR_FILE)
	set -e; \
	for run in 01 02 03 04 05; do \
	  $(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--width 96 --height 72 --iterations 3 --warmup 1 --case public_extract"; \
	  $(MAKE) -C $(CURDIR) fetch_board_logs; \
	  cp log/board/run_bench_std.log $(PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_DIR)/run$${run}_std.log; \
	  cp log/board/run_bench_rvv.log $(PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_DIR)/run$${run}_rvv.log; \
	  cp log/board/analyze_bench_compare.log $(PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_DIR)/run$${run}_analyze.log; \
	done

generate_production_learn_gmms_public_repeated_evidence_manifest:
	python3 script/generate_grabcut_board_evidence_manifest.py \
	  --repeated-dir $(PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_DIR) \
	  --output $(PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_EVIDENCE_MANIFEST) \
	  --case public_extract

run_production_learn_gmms_public_repeated_evidence_doctor: generate_production_learn_gmms_public_repeated_evidence_manifest
	python3 ../../script/evidence_doctor.py \
	  --manifest $(PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_EVIDENCE_MANIFEST) \
	  --output $(PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_EVIDENCE_DOCTOR_MD) \
	  --json-output $(PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --fail-on never
	$(MAKE) -C $(CURDIR) record_production_learn_gmms_public_repeated_evidence_files

record_production_learn_gmms_public_repeated_evidence_files:
	python3 ../../script/evidence_registry.py record \
	  --registry $(EVIDENCE_REGISTRY) \
	  --manifest $(PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_EVIDENCE_MANIFEST) \
	  --doctor $(PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_EVIDENCE_DOCTOR_MD) \
	  --path $(PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_EVIDENCE_DOCTOR_JSON) \
	  --target record_production_learn_gmms_public_repeated_evidence_state \
	  --backend board \
	  --run-label $(notdir $(PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_DIR)) \
	  --case-filter public_extract \
	  --evidence-role production-public \
	  --freshness-state recorded

record_production_learn_gmms_public_repeated_evidence_state: run_production_learn_gmms_public_repeated_evidence_doctor

evidence_status:
	python3 ../../script/evidence_registry.py check \
	  --registry $(EVIDENCE_REGISTRY) \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-manifest.json' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-doctor.md' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-doctor.json' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/060-production-initgraph-terminal-evidence/repeated-evidence-manifest.json' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/060-production-initgraph-terminal-evidence/repeated-evidence-doctor.md' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/060-production-initgraph-terminal-evidence/repeated-evidence-doctor.json' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-manifest.json' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-doctor.md' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-doctor.json' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/080-public-extract-component-profile/repeated-evidence-manifest.json' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/080-public-extract-component-profile/repeated-evidence-doctor.md' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/080-public-extract-component-profile/repeated-evidence-doctor.json' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/090-learn-gmm-component-assignment-diagnostic/repeated-evidence-manifest.json' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/090-learn-gmm-component-assignment-diagnostic/repeated-evidence-doctor.md' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/090-learn-gmm-component-assignment-diagnostic/repeated-evidence-doctor.json' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/100-learn-gmms-full-production-shaped-diagnostic/repeated-evidence-manifest.json' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/100-learn-gmms-full-production-shaped-diagnostic/repeated-evidence-doctor.md' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/100-learn-gmms-full-production-shaped-diagnostic/repeated-evidence-doctor.json' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/110-learn-gmms-production-integration-plan/repeated-evidence-manifest.json' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/110-learn-gmms-production-integration-plan/repeated-evidence-doctor.md' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/110-learn-gmms-production-integration-plan/repeated-evidence-doctor.json' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/110-learn-gmms-production-integration-plan/public-repeated-evidence-manifest.json' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/110-learn-gmms-production-integration-plan/public-repeated-evidence-doctor.md' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/110-learn-gmms-production-integration-plan/public-repeated-evidence-doctor.json' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/120-post-learn-gmms-adoption-profile/repeated-evidence-manifest.json' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/120-post-learn-gmms-adoption-profile/repeated-evidence-doctor.md' \
	  --scan-glob '$(TOPIC_DOC_PREFIX)/doc/phases/120-post-learn-gmms-adoption-profile/repeated-evidence-doctor.json' \
	  $(EVIDENCE_DOC_ARGS) \
	  --require-doc-ref \
	  --fail-on any
