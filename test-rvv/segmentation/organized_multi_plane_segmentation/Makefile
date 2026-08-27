# =============================================================================
#  PCL segmentation/organized_multi_plane_segmentation RVV diagnostic
# =============================================================================

TOPIC  := organized_multi_plane_segmentation
MODULE := segmentation

FOCUS_DIR      := pcl/segmentation/include/pcl/segmentation/impl/organized_multi_plane_segmentation.hpp
FILTER_REPORT  := log/filtered_omps.log
ANALYZE_REPORT := log/analyze_omps.log

TARGET_BENCH     := bench_omps
TARGET_BENCH_STD := bench_omps_std
TARGET_BENCH_RVV := bench_omps_rvv
SRCS_BENCH       := src/bench_omps.cpp

TARGET_TEST     := test_omps
TARGET_TEST_STD := test_omps_std
TARGET_TEST_RVV := test_omps_rvv
SRCS_TEST       := src/test_omps.cpp

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/segmentation/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include

LIBS_CORE := -lpcl_segmentation -lpcl_common -lboost_system -lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread

include ../../mk/rvv-topic.mk

OMPS_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/repeated
OMPS_REMOTE_REPEATED_DIR ?= $(REMOTE_DIR)/$(OMPS_REPEATED_DIR)
OMPS_REPEATED_RUNS ?= 5
OMPS_REPEATED_SUMMARY := $(OMPS_REPEATED_DIR)/summary.md
OMPS_EVIDENCE_MANIFEST := $(OMPS_REPEATED_DIR)/evidence_manifest.json
OMPS_EVIDENCE_DOCTOR_MD := $(OMPS_REPEATED_DIR)/evidence_doctor.md
OMPS_BINARY_HASH ?= $(shell if [ -f build/riscv/bench_omps_std ] && [ -f build/riscv/bench_omps_rvv ]; then sha256sum build/riscv/bench_omps_std build/riscv/bench_omps_rvv | sha256sum | awk '{print $$1}'; fi)
OMPS_BENCH_ARGS ?= --size 262144 --iterations 8 --warmup 2

collect_board_omps_repeated: deploy_board
	@mkdir -p $(OMPS_REPEATED_DIR)
	@for run_id in $$(seq 1 $(OMPS_REPEATED_RUNS)); do \
		local_dir="$(OMPS_REPEATED_DIR)/run$${run_id}"; \
		remote_output="$(OMPS_REMOTE_REPEATED_DIR)/run$${run_id}"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] organized_multi_plane_segmentation repeated run $$run_id/$(OMPS_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $$remote_output && cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_output' BENCH_COMPARE_SAVE='$$remote_output/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(OMPS_BENCH_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -az $(REMOTE_USER)@$(REMOTE_IP):$$remote_output/ "$$local_dir"/; \
	done

generate_board_evidence_manifest:
	$(PYTHON) script/generate_omps_board_evidence_manifest.py \
		--repeated-dir $(OMPS_REPEATED_DIR) \
		--summary $(OMPS_REPEATED_SUMMARY) \
		--output $(OMPS_EVIDENCE_MANIFEST) \
		--device "$(BOARD_LABEL)" \
		--iterations 8 \
		--warmup-iterations 2 \
		--binary-hash "$(OMPS_BINARY_HASH)"

run_board_evidence_doctor: generate_board_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(OMPS_EVIDENCE_MANIFEST) \
		--output $(OMPS_EVIDENCE_DOCTOR_MD) \
		--fail-on never

run_board_omps_repeated: collect_board_omps_repeated run_board_evidence_doctor

.PHONY: collect_board_omps_repeated generate_board_evidence_manifest run_board_evidence_doctor run_board_omps_repeated
