# =============================================================================
#  PCL io/point_coding RVV component-ablation Makefile
#
#  本 topic 先做 component ablation（组件消融）：测试专用 helper 复刻
#  PointCoding 的 leaf-level 坐标量化、截断和 decode 公式，用来判断这段局部
#  成本是否值得后续进入 production-shaped diagnostic（生产形态诊断）。
#  QEMU 只用于 correctness（正确性）和日志形状；性能结论必须来自板卡。
# =============================================================================

TOPIC  := point_coding
MODULE := io

FOCUS_DIR      := pcl/compression/point_coding.h
FILTER_REPORT  := log/filtered_point_coding.log
ANALYZE_REPORT := log/analyze_point_coding.log

TARGET_TEST     := test_point_coding
TARGET_TEST_STD := test_point_coding_std
TARGET_TEST_RVV := test_point_coding_rvv
SRCS_TEST       := src/test_point_coding.cpp

TARGET_BENCH     := bench_point_coding
TARGET_BENCH_STD := bench_point_coding_std
TARGET_BENCH_RVV := bench_point_coding_rvv
SRCS_BENCH       := src/bench_point_coding.cpp

BENCH_ARGS ?= --iterations 20 --warmup-iterations 3 --case-filter all
POINT_CODING_QEMU_BENCH_SMOKE_ARGS ?= --iterations 1 --warmup-iterations 0 --case-filter encode_indexed_1024

POINT_CODING_ENABLE_F64_QUANTIZE ?= 0
ifeq ($(POINT_CODING_ENABLE_F64_QUANTIZE),1)
EXTRA_CXXFLAGS += -DPCL_POINT_CODING_RVV_F64_QUANTIZE_PROBE=1
endif

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/io/include \
	-I$(PCL_SOURCE_ROOT)/octree/include \
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

POINT_CODING_BOARD_MANIFEST_JSON := log/board/evidence_manifest.json
POINT_CODING_BOARD_DOCTOR_MD := log/board/evidence_doctor.md
POINT_CODING_BOARD_DOCTOR_JSON := log/board/evidence_doctor.json
POINT_CODING_REPEATED_DIR := log/board/repeated_phase000
POINT_CODING_REPEATED_SUMMARY_MD := $(POINT_CODING_REPEATED_DIR)/summary.md
POINT_CODING_REPEATED_MANIFEST_JSON := $(POINT_CODING_REPEATED_DIR)/evidence_manifest.json
POINT_CODING_REPEATED_DOCTOR_MD := $(POINT_CODING_REPEATED_DIR)/evidence_doctor.md
POINT_CODING_REPEATED_DOCTOR_JSON := $(POINT_CODING_REPEATED_DIR)/evidence_doctor.json
POINT_CODING_REPEATED_RUNS ?= 5

run_qemu_smoke: run_test_compare

run_qemu_bench_smoke: clean_bench_rvv | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench USE_PCL_RVV10=1 TARGET_BENCH=$(TARGET_BENCH_RVV) BENCH_ARGS="$(POINT_CODING_QEMU_BENCH_SMOKE_ARGS)" 2>&1 | tee $(OUTPUT_DIR_QEMU)/run_bench_rvv_smoke.log

generate_board_evidence_manifest:
	$(PYTHON) script/generate_point_coding_evidence_manifest.py \
		--std-log log/board/run_bench_std.log \
		--rvv-log log/board/run_bench_rvv.log \
		--summary-log log/board/analyze_bench_compare.log \
		--output $(POINT_CODING_BOARD_MANIFEST_JSON)

run_board_evidence_doctor: generate_board_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(POINT_CODING_BOARD_MANIFEST_JSON) \
		--output $(POINT_CODING_BOARD_DOCTOR_MD) \
		--json-output $(POINT_CODING_BOARD_DOCTOR_JSON) \
		--fail-on never

collect_board_repeated:
	@mkdir -p $(POINT_CODING_REPEATED_DIR)
	@for run_id in $$(seq 1 $(POINT_CODING_REPEATED_RUNS)); do \
		echo "[BOARD] point_coding repeated run $$run_id/$(POINT_CODING_REPEATED_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="$(BENCH_ARGS)" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		mkdir -p $(POINT_CODING_REPEATED_DIR)/run-$$run_id; \
		cp log/board/analyze_bench_compare.log $(POINT_CODING_REPEATED_DIR)/run-$$run_id/analyze_bench_compare.log; \
		cp log/board/run_bench_std.log $(POINT_CODING_REPEATED_DIR)/run-$$run_id/run_bench_std.log; \
		cp log/board/run_bench_rvv.log $(POINT_CODING_REPEATED_DIR)/run-$$run_id/run_bench_rvv.log; \
	done

generate_board_repeated_summary:
	$(PYTHON) ../../script/analyze_bench_repeated.py \
		$(POINT_CODING_REPEATED_DIR)/run-*/analyze_bench_compare.log \
		> $(POINT_CODING_REPEATED_SUMMARY_MD)

generate_board_repeated_evidence_manifest: generate_board_repeated_summary
	$(PYTHON) script/generate_point_coding_evidence_manifest.py \
		--repeat-dir $(POINT_CODING_REPEATED_DIR) \
		--summary-log $(POINT_CODING_REPEATED_SUMMARY_MD) \
		--output $(POINT_CODING_REPEATED_MANIFEST_JSON)

run_board_repeated_evidence_doctor: generate_board_repeated_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(POINT_CODING_REPEATED_MANIFEST_JSON) \
		--output $(POINT_CODING_REPEATED_DOCTOR_MD) \
		--json-output $(POINT_CODING_REPEATED_DOCTOR_JSON) \
		--fail-on never

.PHONY: run_qemu_smoke run_qemu_bench_smoke generate_board_evidence_manifest run_board_evidence_doctor collect_board_repeated generate_board_repeated_summary generate_board_repeated_evidence_manifest run_board_repeated_evidence_doctor
