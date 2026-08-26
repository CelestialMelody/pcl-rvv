# =============================================================================
#  PCL segmentation/extract_polygonal_prism_data RVV diagnostic
# =============================================================================

TOPIC  := extract_polygonal_prism_data
MODULE := segmentation

FOCUS_DIR      := pcl/segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp
FILTER_REPORT  := log/filtered_eppd.log
ANALYZE_REPORT := log/analyze_eppd.log

TARGET_BENCH     := bench_eppd
TARGET_BENCH_STD := bench_eppd_std
TARGET_BENCH_RVV := bench_eppd_rvv
SRCS_BENCH       := src/bench_eppd.cpp

TARGET_TEST     := test_eppd
TARGET_TEST_STD := test_eppd_std
TARGET_TEST_RVV := test_eppd_rvv
SRCS_TEST       := src/test_eppd.cpp

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/sample_consensus/include \
	-I$(PCL_SOURCE_ROOT)/segmentation/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include

LIBS_CORE := -lpcl_segmentation -lpcl_sample_consensus -lpcl_common -lboost_system -lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread

include ../../mk/rvv-topic.mk

EPPD_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/repeated
EPPD_REMOTE_REPEATED_DIR ?= $(REMOTE_DIR)/$(EPPD_REPEATED_DIR)
EPPD_REPEATED_RUNS ?= 5
EPPD_REPEATED_SUMMARY := $(EPPD_REPEATED_DIR)/summary.md
EPPD_EVIDENCE_MANIFEST := $(EPPD_REPEATED_DIR)/evidence_manifest.json
EPPD_EVIDENCE_DOCTOR_MD := $(EPPD_REPEATED_DIR)/evidence_doctor.md
EPPD_BINARY_HASH ?= $(shell if [ -f build/riscv/bench_eppd_std ] && [ -f build/riscv/bench_eppd_rvv ]; then sha256sum build/riscv/bench_eppd_std build/riscv/bench_eppd_rvv | sha256sum | awk '{print $$1}'; fi)
EPPD_EVIDENCE_ITERATIONS ?= 8
EPPD_EVIDENCE_WARMUP_ITERATIONS ?= 2
EPPD_INDEXED_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/repeated-indexed
EPPD_INDEXED_BENCH_ARGS ?= --size 262144 --iterations 8 --warmup 2 --indices indexed
EPPD_PRODUCTION_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/repeated-production
EPPD_PRODUCTION_INDEXED_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/repeated-production-indexed
EPPD_PRODUCTION_NESTED_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/repeated-production-nested
EPPD_PRODUCTION_NESTED_INDEXED_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/repeated-production-nested-indexed
EPPD_PRODUCTION_XYZI_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/repeated-production-xyzi
EPPD_PRODUCTION_XYZRGB_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/repeated-production-xyzrgb
EPPD_PRODUCTION_XYZRGBA_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/repeated-production-xyzrgba
EPPD_PRODUCTION_XYZINORMAL_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/repeated-production-xyzinormal
EPPD_PRODUCTION_BENCH_ARGS ?= --size 262144 --iterations 8 --warmup 2 --path production
EPPD_PRODUCTION_INDEXED_BENCH_ARGS ?= --size 262144 --iterations 8 --warmup 2 --path production --indices indexed
EPPD_PRODUCTION_NESTED_BENCH_ARGS ?= --size 262144 --iterations 8 --warmup 2 --path production --polygons nested
EPPD_PRODUCTION_NESTED_INDEXED_BENCH_ARGS ?= --size 262144 --iterations 8 --warmup 2 --path production --indices indexed --polygons nested
EPPD_PRODUCTION_XYZI_BENCH_ARGS ?= --size 262144 --iterations 8 --warmup 2 --path production --point-type xyzi
EPPD_PRODUCTION_XYZRGB_BENCH_ARGS ?= --size 262144 --iterations 8 --warmup 2 --path production --point-type xyzrgb
EPPD_PRODUCTION_XYZRGBA_BENCH_ARGS ?= --size 262144 --iterations 8 --warmup 2 --path production --point-type xyzrgba
EPPD_PRODUCTION_XYZINORMAL_BENCH_ARGS ?= --size 262144 --iterations 8 --warmup 2 --path production --point-type xyzinormal

collect_board_eppd_repeated: deploy_board
	@mkdir -p $(EPPD_REPEATED_DIR)
	@for run_id in $$(seq 1 $(EPPD_REPEATED_RUNS)); do \
		local_dir="$(EPPD_REPEATED_DIR)/run$${run_id}"; \
		remote_output="$(EPPD_REMOTE_REPEATED_DIR)/run$${run_id}"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] EPPD repeated run $$run_id/$(EPPD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $$remote_output && cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_output' BENCH_COMPARE_SAVE='$$remote_output/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(BENCH_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -az $(REMOTE_USER)@$(REMOTE_IP):$$remote_output/ "$$local_dir"/; \
	done

generate_board_evidence_manifest:
	$(PYTHON) script/generate_eppd_board_evidence_manifest.py \
		--repeated-dir $(EPPD_REPEATED_DIR) \
		--summary $(EPPD_REPEATED_SUMMARY) \
		--output $(EPPD_EVIDENCE_MANIFEST) \
		--device "$(BOARD_LABEL)" \
		--iterations $(EPPD_EVIDENCE_ITERATIONS) \
		--warmup-iterations $(EPPD_EVIDENCE_WARMUP_ITERATIONS) \
		--binary-hash "$(EPPD_BINARY_HASH)"

run_board_evidence_doctor: generate_board_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(EPPD_EVIDENCE_MANIFEST) \
		--output $(EPPD_EVIDENCE_DOCTOR_MD) \
		--fail-on never

run_board_eppd_repeated: collect_board_eppd_repeated run_board_evidence_doctor

run_board_eppd_repeated_indexed:
	$(MAKE) collect_board_eppd_repeated \
		EPPD_REPEATED_DIR="$(EPPD_INDEXED_REPEATED_DIR)" \
		EPPD_REMOTE_REPEATED_DIR="$(REMOTE_DIR)/$(EPPD_INDEXED_REPEATED_DIR)" \
		BENCH_ARGS="$(EPPD_INDEXED_BENCH_ARGS)"
	$(MAKE) run_board_evidence_doctor \
		EPPD_REPEATED_DIR="$(EPPD_INDEXED_REPEATED_DIR)"

run_board_eppd_repeated_production:
	$(MAKE) collect_board_eppd_repeated \
		EPPD_REPEATED_DIR="$(EPPD_PRODUCTION_REPEATED_DIR)" \
		EPPD_REMOTE_REPEATED_DIR="$(REMOTE_DIR)/$(EPPD_PRODUCTION_REPEATED_DIR)" \
		BENCH_ARGS="$(EPPD_PRODUCTION_BENCH_ARGS)"
	$(MAKE) run_board_evidence_doctor \
		EPPD_REPEATED_DIR="$(EPPD_PRODUCTION_REPEATED_DIR)"

run_board_eppd_repeated_production_indexed:
	$(MAKE) collect_board_eppd_repeated \
		EPPD_REPEATED_DIR="$(EPPD_PRODUCTION_INDEXED_REPEATED_DIR)" \
		EPPD_REMOTE_REPEATED_DIR="$(REMOTE_DIR)/$(EPPD_PRODUCTION_INDEXED_REPEATED_DIR)" \
		BENCH_ARGS="$(EPPD_PRODUCTION_INDEXED_BENCH_ARGS)"
	$(MAKE) run_board_evidence_doctor \
		EPPD_REPEATED_DIR="$(EPPD_PRODUCTION_INDEXED_REPEATED_DIR)"

run_board_eppd_repeated_production_nested:
	$(MAKE) collect_board_eppd_repeated \
		EPPD_REPEATED_DIR="$(EPPD_PRODUCTION_NESTED_REPEATED_DIR)" \
		EPPD_REMOTE_REPEATED_DIR="$(REMOTE_DIR)/$(EPPD_PRODUCTION_NESTED_REPEATED_DIR)" \
		BENCH_ARGS="$(EPPD_PRODUCTION_NESTED_BENCH_ARGS)"
	$(MAKE) run_board_evidence_doctor \
		EPPD_REPEATED_DIR="$(EPPD_PRODUCTION_NESTED_REPEATED_DIR)"

run_board_eppd_repeated_production_nested_indexed:
	$(MAKE) collect_board_eppd_repeated \
		EPPD_REPEATED_DIR="$(EPPD_PRODUCTION_NESTED_INDEXED_REPEATED_DIR)" \
		EPPD_REMOTE_REPEATED_DIR="$(REMOTE_DIR)/$(EPPD_PRODUCTION_NESTED_INDEXED_REPEATED_DIR)" \
		BENCH_ARGS="$(EPPD_PRODUCTION_NESTED_INDEXED_BENCH_ARGS)"
	$(MAKE) run_board_evidence_doctor \
		EPPD_REPEATED_DIR="$(EPPD_PRODUCTION_NESTED_INDEXED_REPEATED_DIR)"

run_board_eppd_repeated_production_xyzi:
	$(MAKE) collect_board_eppd_repeated \
		EPPD_REPEATED_DIR="$(EPPD_PRODUCTION_XYZI_REPEATED_DIR)" \
		EPPD_REMOTE_REPEATED_DIR="$(REMOTE_DIR)/$(EPPD_PRODUCTION_XYZI_REPEATED_DIR)" \
		BENCH_ARGS="$(EPPD_PRODUCTION_XYZI_BENCH_ARGS)"
	$(MAKE) run_board_evidence_doctor \
		EPPD_REPEATED_DIR="$(EPPD_PRODUCTION_XYZI_REPEATED_DIR)"

run_board_eppd_repeated_production_xyzrgb:
	$(MAKE) collect_board_eppd_repeated \
		EPPD_REPEATED_DIR="$(EPPD_PRODUCTION_XYZRGB_REPEATED_DIR)" \
		EPPD_REMOTE_REPEATED_DIR="$(REMOTE_DIR)/$(EPPD_PRODUCTION_XYZRGB_REPEATED_DIR)" \
		BENCH_ARGS="$(EPPD_PRODUCTION_XYZRGB_BENCH_ARGS)"
	$(MAKE) run_board_evidence_doctor \
		EPPD_REPEATED_DIR="$(EPPD_PRODUCTION_XYZRGB_REPEATED_DIR)"

run_board_eppd_repeated_production_xyzrgba:
	$(MAKE) collect_board_eppd_repeated \
		EPPD_REPEATED_DIR="$(EPPD_PRODUCTION_XYZRGBA_REPEATED_DIR)" \
		EPPD_REMOTE_REPEATED_DIR="$(REMOTE_DIR)/$(EPPD_PRODUCTION_XYZRGBA_REPEATED_DIR)" \
		BENCH_ARGS="$(EPPD_PRODUCTION_XYZRGBA_BENCH_ARGS)"
	$(MAKE) run_board_evidence_doctor \
		EPPD_REPEATED_DIR="$(EPPD_PRODUCTION_XYZRGBA_REPEATED_DIR)"

run_board_eppd_repeated_production_xyzinormal:
	$(MAKE) collect_board_eppd_repeated \
		EPPD_REPEATED_DIR="$(EPPD_PRODUCTION_XYZINORMAL_REPEATED_DIR)" \
		EPPD_REMOTE_REPEATED_DIR="$(REMOTE_DIR)/$(EPPD_PRODUCTION_XYZINORMAL_REPEATED_DIR)" \
		BENCH_ARGS="$(EPPD_PRODUCTION_XYZINORMAL_BENCH_ARGS)"
	$(MAKE) run_board_evidence_doctor \
		EPPD_REPEATED_DIR="$(EPPD_PRODUCTION_XYZINORMAL_REPEATED_DIR)"

run_board_eppd_repeated_production_pointtypes: \
	run_board_eppd_repeated_production_xyzi \
	run_board_eppd_repeated_production_xyzrgb \
	run_board_eppd_repeated_production_xyzrgba \
	run_board_eppd_repeated_production_xyzinormal

.PHONY: collect_board_eppd_repeated generate_board_evidence_manifest run_board_evidence_doctor run_board_eppd_repeated run_board_eppd_repeated_indexed run_board_eppd_repeated_production run_board_eppd_repeated_production_indexed run_board_eppd_repeated_production_nested run_board_eppd_repeated_production_nested_indexed run_board_eppd_repeated_production_xyzi run_board_eppd_repeated_production_xyzrgb run_board_eppd_repeated_production_xyzrgba run_board_eppd_repeated_production_xyzinormal run_board_eppd_repeated_production_pointtypes
