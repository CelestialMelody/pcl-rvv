# =============================================================================
#  PCL segmentation/min_cut_segmentation RVV diagnostic
# =============================================================================

TOPIC  := min_cut_segmentation
MODULE := segmentation

FOCUS_DIR      := pcl/segmentation/include/pcl/segmentation/impl/min_cut_segmentation.hpp
FILTER_REPORT  := log/filtered_min_cut_segmentation.log
ANALYZE_REPORT := log/analyze_min_cut_segmentation.log

TARGET_BENCH     := bench_min_cut_segmentation
TARGET_BENCH_STD := bench_min_cut_segmentation_std
TARGET_BENCH_RVV := bench_min_cut_segmentation_rvv
SRCS_BENCH       := src/bench_min_cut_segmentation.cpp

TARGET_TEST     := test_min_cut_segmentation
TARGET_TEST_STD := test_min_cut_segmentation_std
TARGET_TEST_RVV := test_min_cut_segmentation_rvv
SRCS_TEST       := src/test_min_cut_segmentation.cpp

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/segmentation/include \
	-I$(PCL_SOURCE_ROOT)/search/include \
	-I$(PCL_SOURCE_ROOT)/kdtree/include \
	-I$(PCL_SOURCE_ROOT)/octree/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include \
	-I$(FLANN_ROOT)/include \
	-I$(LZ4_ROOT)/include \
	-I$(HDF5_ROOT)/include \
	-I$(ZLIB_ROOT)/include \
	-I$(LIBPNG_ROOT)/include

LIBS_CORE := -lpcl_segmentation -lpcl_search -lpcl_kdtree -lpcl_octree -lpcl_common -lflann -llz4 -lboost_system -lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread

include ../../mk/rvv-topic.mk

MINCUT_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/repeated
MINCUT_REMOTE_REPEATED_DIR ?= $(REMOTE_DIR)/$(MINCUT_REPEATED_DIR)
MINCUT_REPEATED_RUNS ?= 5
MINCUT_REPEATED_SUMMARY := $(MINCUT_REPEATED_DIR)/summary.md
MINCUT_EVIDENCE_MANIFEST := $(MINCUT_REPEATED_DIR)/evidence_manifest.json
MINCUT_EVIDENCE_DOCTOR_MD := $(MINCUT_REPEATED_DIR)/evidence_doctor.md
MINCUT_BINARY_HASH ?= $(shell if [ -f build/riscv/bench_min_cut_segmentation_std ] && [ -f build/riscv/bench_min_cut_segmentation_rvv ]; then sha256sum build/riscv/bench_min_cut_segmentation_std build/riscv/bench_min_cut_segmentation_rvv | sha256sum | awk '{print $$1}'; fi)
MINCUT_BENCH_ARGS ?= --size 65536 --foreground 19 --iterations 8 --warmup 2
MINCUT_BUILDGRAPH_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/buildgraph-repeated
MINCUT_REMOTE_BUILDGRAPH_REPEATED_DIR ?= $(REMOTE_DIR)/$(MINCUT_BUILDGRAPH_REPEATED_DIR)
MINCUT_BUILDGRAPH_REPEATED_RUNS ?= 5
MINCUT_BUILDGRAPH_REPEATED_SUMMARY := $(MINCUT_BUILDGRAPH_REPEATED_DIR)/summary.md
MINCUT_BUILDGRAPH_EVIDENCE_MANIFEST := $(MINCUT_BUILDGRAPH_REPEATED_DIR)/evidence_manifest.json
MINCUT_BUILDGRAPH_EVIDENCE_DOCTOR_MD := $(MINCUT_BUILDGRAPH_REPEATED_DIR)/evidence_doctor.md
MINCUT_BUILDGRAPH_BENCH_ARGS ?= --size 8192 --foreground 19 --neighbours 14 --iterations 3 --warmup 1 --case-filter buildgraph_potential_batch

collect_board_min_cut_repeated: deploy_board
	@mkdir -p $(MINCUT_REPEATED_DIR)
	@for run_id in $$(seq 1 $(MINCUT_REPEATED_RUNS)); do \
		local_dir="$(MINCUT_REPEATED_DIR)/run$${run_id}"; \
		remote_output="$(MINCUT_REMOTE_REPEATED_DIR)/run$${run_id}"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] min_cut repeated run $$run_id/$(MINCUT_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $$remote_output && cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_output' BENCH_COMPARE_SAVE='$$remote_output/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(MINCUT_BENCH_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -az $(REMOTE_USER)@$(REMOTE_IP):$$remote_output/ "$$local_dir"/; \
	done

generate_board_evidence_manifest:
	$(PYTHON) script/generate_min_cut_board_evidence_manifest.py \
		--repeated-dir $(MINCUT_REPEATED_DIR) \
		--summary $(MINCUT_REPEATED_SUMMARY) \
		--output $(MINCUT_EVIDENCE_MANIFEST) \
		--device "$(BOARD_LABEL)" \
		--size 65536 \
		--foreground 19 \
		--iterations 8 \
		--warmup-iterations 2 \
		--binary-hash "$(MINCUT_BINARY_HASH)"

run_board_evidence_doctor: generate_board_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(MINCUT_EVIDENCE_MANIFEST) \
		--output $(MINCUT_EVIDENCE_DOCTOR_MD) \
		--fail-on never

run_board_min_cut_repeated: collect_board_min_cut_repeated run_board_evidence_doctor

collect_board_min_cut_buildgraph_repeated: deploy_board
	@mkdir -p $(MINCUT_BUILDGRAPH_REPEATED_DIR)
	@for run_id in $$(seq 1 $(MINCUT_BUILDGRAPH_REPEATED_RUNS)); do \
		local_dir="$(MINCUT_BUILDGRAPH_REPEATED_DIR)/run$${run_id}"; \
		remote_output="$(MINCUT_REMOTE_BUILDGRAPH_REPEATED_DIR)/run$${run_id}"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] min_cut buildgraph repeated run $$run_id/$(MINCUT_BUILDGRAPH_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $$remote_output && cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_output' BENCH_COMPARE_SAVE='$$remote_output/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(MINCUT_BUILDGRAPH_BENCH_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -az $(REMOTE_USER)@$(REMOTE_IP):$$remote_output/ "$$local_dir"/; \
	done

generate_board_buildgraph_evidence_manifest:
	$(PYTHON) script/generate_min_cut_board_evidence_manifest.py \
		--repeated-dir $(MINCUT_BUILDGRAPH_REPEATED_DIR) \
		--summary $(MINCUT_BUILDGRAPH_REPEATED_SUMMARY) \
		--output $(MINCUT_BUILDGRAPH_EVIDENCE_MANIFEST) \
		--device "$(BOARD_LABEL)" \
		--size 8192 \
		--foreground 19 \
		--neighbours 14 \
		--iterations 3 \
		--warmup-iterations 1 \
		--binary-hash "$(MINCUT_BINARY_HASH)"

run_board_buildgraph_evidence_doctor: generate_board_buildgraph_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(MINCUT_BUILDGRAPH_EVIDENCE_MANIFEST) \
		--output $(MINCUT_BUILDGRAPH_EVIDENCE_DOCTOR_MD) \
		--fail-on never

run_board_min_cut_buildgraph_repeated: collect_board_min_cut_buildgraph_repeated run_board_buildgraph_evidence_doctor

.PHONY: collect_board_min_cut_repeated generate_board_evidence_manifest run_board_evidence_doctor run_board_min_cut_repeated collect_board_min_cut_buildgraph_repeated generate_board_buildgraph_evidence_manifest run_board_buildgraph_evidence_doctor run_board_min_cut_buildgraph_repeated
