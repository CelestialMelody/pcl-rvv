# =============================================================================
#  PCL io/pcd_io RVV component-ablation Makefile
#
#  本 topic 先做 component ablation（组件消融）：测试专用 helper 复刻
#  PCDPointCloud2 binary_compressed pack/unpack 的字段布局转换，用来判断
#  AoS（结构数组）和 field-major（字段连续）互转是否值得后续进入
#  production-shaped diagnostic（生产形态诊断）。QEMU 只用于 correctness
#  （正确性）和日志形状；性能结论必须来自板卡。
# =============================================================================

TOPIC  := pcd_io
MODULE := io

FOCUS_DIR      := pcl/io/src/pcd_io.cpp
FILTER_REPORT  := log/filtered_pcd_io.log
ANALYZE_REPORT := log/analyze_pcd_io.log

TARGET_TEST     := test_pcd_io
TARGET_TEST_STD := test_pcd_io_std
TARGET_TEST_RVV := test_pcd_io_rvv
SRCS_TEST       := src/test_pcd_io.cpp ../../../io/src/pcd_io.cpp

TARGET_BENCH     := bench_pcd_io
TARGET_BENCH_STD := bench_pcd_io_std
TARGET_BENCH_RVV := bench_pcd_io_rvv
SRCS_BENCH       := src/bench_pcd_io.cpp ../../../io/src/pcd_io.cpp

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

EXTRA_CXXFLAGS += -DPCL_PCD_IO_RVV_TEST_HOOKS=1

include ../../mk/rvv-topic.mk

PCD_IO_REPEAT_RUNS ?= 5
PCD_IO_REPEAT_DIR := log/board/component_ablation_repeat_5
PCD_IO_REPEAT_SUMMARY := $(PCD_IO_REPEAT_DIR)/summary.md
PCD_IO_EVIDENCE_MANIFEST := $(PCD_IO_REPEAT_DIR)/evidence_manifest.json
PCD_IO_EVIDENCE_DOCTOR := $(PCD_IO_REPEAT_DIR)/evidence_doctor.md
PCD_IO_EVIDENCE_REGISTRY := log/evidence_registry.json
PCD_IO_WRITER_REPEAT_DIR := log/board/writer_payload_repeat_5
PCD_IO_WRITER_REPEAT_SUMMARY := $(PCD_IO_WRITER_REPEAT_DIR)/summary.md
PCD_IO_WRITER_EVIDENCE_MANIFEST := $(PCD_IO_WRITER_REPEAT_DIR)/evidence_manifest.json
PCD_IO_WRITER_EVIDENCE_DOCTOR := $(PCD_IO_WRITER_REPEAT_DIR)/evidence_doctor.md
PCD_IO_READER_REPEAT_DIR := log/board/reader_payload_repeat_5
PCD_IO_READER_REPEAT_SUMMARY := $(PCD_IO_READER_REPEAT_DIR)/summary.md
PCD_IO_READER_EVIDENCE_MANIFEST := $(PCD_IO_READER_REPEAT_DIR)/evidence_manifest.json
PCD_IO_READER_EVIDENCE_DOCTOR := $(PCD_IO_READER_REPEAT_DIR)/evidence_doctor.md
PCD_IO_PRODUCTION_WRITER_REPEAT_DIR := log/board/production_writer_repeat_5
PCD_IO_PRODUCTION_WRITER_REPEAT_SUMMARY := $(PCD_IO_PRODUCTION_WRITER_REPEAT_DIR)/summary.md
PCD_IO_PRODUCTION_WRITER_EVIDENCE_MANIFEST := $(PCD_IO_PRODUCTION_WRITER_REPEAT_DIR)/evidence_manifest.json
PCD_IO_PRODUCTION_WRITER_EVIDENCE_DOCTOR := $(PCD_IO_PRODUCTION_WRITER_REPEAT_DIR)/evidence_doctor.md

run_qemu_smoke: run_test_compare run_bench_rvv

collect_board_pcd_io_repeated:
	@mkdir -p $(PCD_IO_REPEAT_DIR)
	@set -e; for run_id in $$(seq 1 $(PCD_IO_REPEAT_RUNS)); do \
		echo "[BOARD] pcd_io component repeated run $$run_id/$(PCD_IO_REPEAT_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter component --iterations 20 --warmup-iterations 3" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		mkdir -p $(PCD_IO_REPEAT_DIR)/run$$run_id; \
		cp log/board/analyze_bench_compare.log $(PCD_IO_REPEAT_DIR)/run$$run_id/analyze_bench_compare.log; \
		cp log/board/run_bench_std.log $(PCD_IO_REPEAT_DIR)/run$$run_id/run_bench_std.log; \
		cp log/board/run_bench_rvv.log $(PCD_IO_REPEAT_DIR)/run$$run_id/run_bench_rvv.log; \
	done

generate_board_pcd_io_repeat_summary:
	@$(PYTHON_RUN) ../../script/analyze_bench_repeated.py \
		$(PCD_IO_REPEAT_DIR)/run*/analyze_bench_compare.log \
		> $(PCD_IO_REPEAT_SUMMARY)

generate_board_pcd_io_evidence_manifest: generate_board_pcd_io_repeat_summary
	@$(PYTHON_RUN) script/generate_pcd_io_evidence_manifest.py \
		--repeat-dir $(PCD_IO_REPEAT_DIR) \
		--summary-log $(PCD_IO_REPEAT_SUMMARY) \
		--output $(PCD_IO_EVIDENCE_MANIFEST)

run_board_pcd_io_evidence_doctor: generate_board_pcd_io_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(PCD_IO_EVIDENCE_MANIFEST) \
		--output $(PCD_IO_EVIDENCE_DOCTOR) \
		--fail-on never

record_board_pcd_io_evidence_state: run_board_pcd_io_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(PCD_IO_EVIDENCE_REGISTRY) \
		--summary $(PCD_IO_REPEAT_SUMMARY) \
		--manifest $(PCD_IO_EVIDENCE_MANIFEST) \
		--doctor $(PCD_IO_EVIDENCE_DOCTOR) \
		--target run_board_pcd_io_repeated \
		--backend board \
		--run-label board-pcd-io-component-ablation-repeat-phase000 \
		--case-filter component \
		--evidence-role component_ablation \
		--doc-ref doc/phases/000-current-state-and-component-ablation/result.zh.md \
		--doc-ref doc/pcd_io-evaluation.zh.md \
		--freshness-state fresh

run_board_pcd_io_repeated: collect_board_pcd_io_repeated record_board_pcd_io_evidence_state

collect_board_pcd_io_writer_payload_repeated:
	@mkdir -p $(PCD_IO_WRITER_REPEAT_DIR)
	@set -e; for run_id in $$(seq 1 $(PCD_IO_REPEAT_RUNS)); do \
		echo "[BOARD] pcd_io writer payload repeated run $$run_id/$(PCD_IO_REPEAT_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter writer_payload --iterations 20 --warmup-iterations 3" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		mkdir -p $(PCD_IO_WRITER_REPEAT_DIR)/run$$run_id; \
		cp log/board/analyze_bench_compare.log $(PCD_IO_WRITER_REPEAT_DIR)/run$$run_id/analyze_bench_compare.log; \
		cp log/board/run_bench_std.log $(PCD_IO_WRITER_REPEAT_DIR)/run$$run_id/run_bench_std.log; \
		cp log/board/run_bench_rvv.log $(PCD_IO_WRITER_REPEAT_DIR)/run$$run_id/run_bench_rvv.log; \
	done

generate_board_pcd_io_writer_payload_repeat_summary:
	@$(PYTHON_RUN) ../../script/analyze_bench_repeated.py \
		$(PCD_IO_WRITER_REPEAT_DIR)/run*/analyze_bench_compare.log \
		> $(PCD_IO_WRITER_REPEAT_SUMMARY)

generate_board_pcd_io_writer_payload_evidence_manifest: generate_board_pcd_io_writer_payload_repeat_summary
	@$(PYTHON_RUN) script/generate_pcd_io_evidence_manifest.py \
		--repeat-dir $(PCD_IO_WRITER_REPEAT_DIR) \
		--summary-log $(PCD_IO_WRITER_REPEAT_SUMMARY) \
		--output $(PCD_IO_WRITER_EVIDENCE_MANIFEST)

run_board_pcd_io_writer_payload_evidence_doctor: generate_board_pcd_io_writer_payload_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(PCD_IO_WRITER_EVIDENCE_MANIFEST) \
		--output $(PCD_IO_WRITER_EVIDENCE_DOCTOR) \
		--fail-on never

record_board_pcd_io_writer_payload_evidence_state: run_board_pcd_io_writer_payload_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(PCD_IO_EVIDENCE_REGISTRY) \
		--summary $(PCD_IO_WRITER_REPEAT_SUMMARY) \
		--manifest $(PCD_IO_WRITER_EVIDENCE_MANIFEST) \
		--doctor $(PCD_IO_WRITER_EVIDENCE_DOCTOR) \
		--target run_board_pcd_io_writer_payload_repeated \
		--backend board \
		--run-label board-pcd-io-writer-payload-repeat-phase010 \
		--case-filter writer_payload \
		--evidence-role production_shaped_diagnostic \
		--doc-ref doc/phases/010-production-shaped-compressed-writer-diagnostic/result.zh.md \
		--doc-ref doc/pcd_io-evaluation.zh.md \
		--freshness-state fresh

run_board_pcd_io_writer_payload_repeated: collect_board_pcd_io_writer_payload_repeated record_board_pcd_io_writer_payload_evidence_state

collect_board_pcd_io_reader_payload_repeated:
	@mkdir -p $(PCD_IO_READER_REPEAT_DIR)
	@set -e; for run_id in $$(seq 1 $(PCD_IO_REPEAT_RUNS)); do \
		echo "[BOARD] pcd_io reader payload repeated run $$run_id/$(PCD_IO_REPEAT_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter reader_payload --iterations 20 --warmup-iterations 3" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		mkdir -p $(PCD_IO_READER_REPEAT_DIR)/run$$run_id; \
		cp log/board/analyze_bench_compare.log $(PCD_IO_READER_REPEAT_DIR)/run$$run_id/analyze_bench_compare.log; \
		cp log/board/run_bench_std.log $(PCD_IO_READER_REPEAT_DIR)/run$$run_id/run_bench_std.log; \
		cp log/board/run_bench_rvv.log $(PCD_IO_READER_REPEAT_DIR)/run$$run_id/run_bench_rvv.log; \
	done

generate_board_pcd_io_reader_payload_repeat_summary:
	@$(PYTHON_RUN) ../../script/analyze_bench_repeated.py \
		$(PCD_IO_READER_REPEAT_DIR)/run*/analyze_bench_compare.log \
		> $(PCD_IO_READER_REPEAT_SUMMARY)

generate_board_pcd_io_reader_payload_evidence_manifest: generate_board_pcd_io_reader_payload_repeat_summary
	@$(PYTHON_RUN) script/generate_pcd_io_evidence_manifest.py \
		--repeat-dir $(PCD_IO_READER_REPEAT_DIR) \
		--summary-log $(PCD_IO_READER_REPEAT_SUMMARY) \
		--output $(PCD_IO_READER_EVIDENCE_MANIFEST)

run_board_pcd_io_reader_payload_evidence_doctor: generate_board_pcd_io_reader_payload_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(PCD_IO_READER_EVIDENCE_MANIFEST) \
		--output $(PCD_IO_READER_EVIDENCE_DOCTOR) \
		--fail-on never

record_board_pcd_io_reader_payload_evidence_state: run_board_pcd_io_reader_payload_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(PCD_IO_EVIDENCE_REGISTRY) \
		--summary $(PCD_IO_READER_REPEAT_SUMMARY) \
		--manifest $(PCD_IO_READER_EVIDENCE_MANIFEST) \
		--doctor $(PCD_IO_READER_EVIDENCE_DOCTOR) \
		--target run_board_pcd_io_reader_payload_repeated \
		--backend board \
		--run-label board-pcd-io-reader-payload-repeat-phase040 \
		--case-filter reader_payload \
		--evidence-role production_shaped_diagnostic \
		--doc-ref doc/phases/040-reader-shaped-unpack-finite-scan/result.zh.md \
		--doc-ref doc/pcd_io-evaluation.zh.md \
		--freshness-state fresh

run_board_pcd_io_reader_payload_repeated: collect_board_pcd_io_reader_payload_repeated record_board_pcd_io_reader_payload_evidence_state

collect_board_pcd_io_production_writer_repeated:
	@mkdir -p $(PCD_IO_PRODUCTION_WRITER_REPEAT_DIR)
	@set -e; for run_id in $$(seq 1 $(PCD_IO_REPEAT_RUNS)); do \
		echo "[BOARD] pcd_io production writer repeated run $$run_id/$(PCD_IO_REPEAT_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter production_writer --iterations 20 --warmup-iterations 3" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		mkdir -p $(PCD_IO_PRODUCTION_WRITER_REPEAT_DIR)/run$$run_id; \
		cp log/board/analyze_bench_compare.log $(PCD_IO_PRODUCTION_WRITER_REPEAT_DIR)/run$$run_id/analyze_bench_compare.log; \
		cp log/board/run_bench_std.log $(PCD_IO_PRODUCTION_WRITER_REPEAT_DIR)/run$$run_id/run_bench_std.log; \
		cp log/board/run_bench_rvv.log $(PCD_IO_PRODUCTION_WRITER_REPEAT_DIR)/run$$run_id/run_bench_rvv.log; \
	done

generate_board_pcd_io_production_writer_repeat_summary:
	@$(PYTHON_RUN) ../../script/analyze_bench_repeated.py \
		$(PCD_IO_PRODUCTION_WRITER_REPEAT_DIR)/run*/analyze_bench_compare.log \
		> $(PCD_IO_PRODUCTION_WRITER_REPEAT_SUMMARY)

generate_board_pcd_io_production_writer_evidence_manifest: generate_board_pcd_io_production_writer_repeat_summary
	@$(PYTHON_RUN) script/generate_pcd_io_evidence_manifest.py \
		--repeat-dir $(PCD_IO_PRODUCTION_WRITER_REPEAT_DIR) \
		--summary-log $(PCD_IO_PRODUCTION_WRITER_REPEAT_SUMMARY) \
		--output $(PCD_IO_PRODUCTION_WRITER_EVIDENCE_MANIFEST)

run_board_pcd_io_production_writer_evidence_doctor: generate_board_pcd_io_production_writer_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(PCD_IO_PRODUCTION_WRITER_EVIDENCE_MANIFEST) \
		--output $(PCD_IO_PRODUCTION_WRITER_EVIDENCE_DOCTOR) \
		--fail-on never

record_board_pcd_io_production_writer_evidence_state: run_board_pcd_io_production_writer_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(PCD_IO_EVIDENCE_REGISTRY) \
		--summary $(PCD_IO_PRODUCTION_WRITER_REPEAT_SUMMARY) \
		--manifest $(PCD_IO_PRODUCTION_WRITER_EVIDENCE_MANIFEST) \
		--doctor $(PCD_IO_PRODUCTION_WRITER_EVIDENCE_DOCTOR) \
		--target run_board_pcd_io_production_writer_repeated \
		--backend board \
		--run-label board-pcd-io-production-writer-repeat-phase050 \
		--case-filter production_writer \
		--evidence-role production_public \
		--doc-ref doc/phases/050-production-writer-payload-integration/result.zh.md \
		--doc-ref doc/pcd_io-evaluation.zh.md \
		--freshness-state fresh

run_board_pcd_io_production_writer_repeated: collect_board_pcd_io_production_writer_repeated record_board_pcd_io_production_writer_evidence_state

.PHONY: run_qemu_smoke collect_board_pcd_io_repeated generate_board_pcd_io_repeat_summary generate_board_pcd_io_evidence_manifest run_board_pcd_io_evidence_doctor record_board_pcd_io_evidence_state run_board_pcd_io_repeated collect_board_pcd_io_writer_payload_repeated generate_board_pcd_io_writer_payload_repeat_summary generate_board_pcd_io_writer_payload_evidence_manifest run_board_pcd_io_writer_payload_evidence_doctor record_board_pcd_io_writer_payload_evidence_state run_board_pcd_io_writer_payload_repeated collect_board_pcd_io_reader_payload_repeated generate_board_pcd_io_reader_payload_repeat_summary generate_board_pcd_io_reader_payload_evidence_manifest run_board_pcd_io_reader_payload_evidence_doctor record_board_pcd_io_reader_payload_evidence_state run_board_pcd_io_reader_payload_repeated collect_board_pcd_io_production_writer_repeated generate_board_pcd_io_production_writer_repeat_summary generate_board_pcd_io_production_writer_evidence_manifest run_board_pcd_io_production_writer_evidence_doctor record_board_pcd_io_production_writer_evidence_state run_board_pcd_io_production_writer_repeated
