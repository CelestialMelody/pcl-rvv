# =============================================================================
#  PCL io/point_cloud_image_extractors RVV diagnostic Makefile
#
#  本 topic 先建立 production-shaped diagnostic（生产形态诊断）：测试专用
#  helper 复刻 PointCloudImageExtractor 的 RGB/RGBA unpack 和 scaling 输出
#  热点，用来筛选 RVV 候选。QEMU 只用于正确性和日志形状；性能结论必须
#  来自板卡或目标硬件。
# =============================================================================

TOPIC  := point_cloud_image_extractors
MODULE := io

FOCUS_DIR      := pcl/io/impl/point_cloud_image_extractors.hpp
FILTER_REPORT  := log/filtered_point_cloud_image_extractors.log
ANALYZE_REPORT := log/analyze_point_cloud_image_extractors.log

TARGET_TEST     := test_pcie
TARGET_TEST_STD := test_pcie_std
TARGET_TEST_RVV := test_pcie_rvv
SRCS_TEST       := src/test_pcie.cpp

TARGET_BENCH     := bench_pcie
TARGET_BENCH_STD := bench_pcie_std
TARGET_BENCH_RVV := bench_pcie_rvv
SRCS_BENCH       := src/bench_pcie.cpp

BENCH_ARGS ?= --iterations 20 --warmup-iterations 3 --case-filter all

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/io/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
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

LIBS_TEST  := -lpcl_io -lpcl_common -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lpcl_io -lpcl_common -lm

include ../../mk/rvv-topic.mk

PCIE_BOARD_MANIFEST_JSON := log/board/evidence_manifest.json
PCIE_BOARD_DOCTOR_MD := log/board/evidence_doctor.md
PCIE_BOARD_DOCTOR_JSON := log/board/evidence_doctor.json
PCIE_REPEATED_DIR := log/board/repeated_phase000
PCIE_REPEATED_SUMMARY_MD := $(PCIE_REPEATED_DIR)/summary.md
PCIE_REPEATED_MANIFEST_JSON := $(PCIE_REPEATED_DIR)/evidence_manifest.json
PCIE_REPEATED_DOCTOR_MD := $(PCIE_REPEATED_DIR)/evidence_doctor.md
PCIE_REPEATED_DOCTOR_JSON := $(PCIE_REPEATED_DIR)/evidence_doctor.json
PCIE_REPEATED_RUNS ?= 5

generate_board_evidence_manifest:
	$(PYTHON) script/generate_pcie_evidence_manifest.py \
		--std-log log/board/run_bench_std.log \
		--rvv-log log/board/run_bench_rvv.log \
		--summary-log log/board/analyze_bench_compare.log \
		--output $(PCIE_BOARD_MANIFEST_JSON)

run_board_evidence_doctor: generate_board_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(PCIE_BOARD_MANIFEST_JSON) \
		--output $(PCIE_BOARD_DOCTOR_MD) \
		--json-output $(PCIE_BOARD_DOCTOR_JSON) \
		--fail-on never

collect_board_repeated:
	@mkdir -p $(PCIE_REPEATED_DIR)
	@for run_id in $$(seq 1 $(PCIE_REPEATED_RUNS)); do \
		echo "[BOARD] point_cloud_image_extractors repeated run $$run_id/$(PCIE_REPEATED_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="$(BENCH_ARGS)" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		mkdir -p $(PCIE_REPEATED_DIR)/run-$$run_id; \
		cp log/board/analyze_bench_compare.log $(PCIE_REPEATED_DIR)/run-$$run_id/analyze_bench_compare.log; \
		cp log/board/run_bench_std.log $(PCIE_REPEATED_DIR)/run-$$run_id/run_bench_std.log; \
		cp log/board/run_bench_rvv.log $(PCIE_REPEATED_DIR)/run-$$run_id/run_bench_rvv.log; \
	done

generate_board_repeated_summary:
	$(PYTHON) ../../script/analyze_bench_repeated.py \
		$(PCIE_REPEATED_DIR)/run-*/analyze_bench_compare.log \
		> $(PCIE_REPEATED_SUMMARY_MD)

generate_board_repeated_evidence_manifest: generate_board_repeated_summary
	$(PYTHON) script/generate_pcie_evidence_manifest.py \
		--repeat-dir $(PCIE_REPEATED_DIR) \
		--summary-log $(PCIE_REPEATED_SUMMARY_MD) \
		--output $(PCIE_REPEATED_MANIFEST_JSON)

run_board_repeated_evidence_doctor: generate_board_repeated_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(PCIE_REPEATED_MANIFEST_JSON) \
		--output $(PCIE_REPEATED_DOCTOR_MD) \
		--json-output $(PCIE_REPEATED_DOCTOR_JSON) \
		--fail-on never

.PHONY: generate_board_evidence_manifest run_board_evidence_doctor collect_board_repeated generate_board_repeated_summary generate_board_repeated_evidence_manifest run_board_repeated_evidence_doctor
