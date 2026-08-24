# =============================================================================
#  PCL io/impl/pcd_io.hpp templated writer RVV component-ablation Makefile
#
#  本 topic 只评估 `PointCloud<PointT>` templated writer 的 compressed write
#  前置字段布局转换。QEMU 只用于 correctness（正确性）和日志形状；性能结论
#  必须来自板卡或目标硬件。
# =============================================================================

TOPIC  := pcd_io_templated_writer
MODULE := io

FOCUS_DIR      := pcl/io/include/pcl/io/impl/pcd_io.hpp
FILTER_REPORT  := log/filtered_pcd_io_templated_writer.log
ANALYZE_REPORT := log/analyze_pcd_io_templated_writer.log

TARGET_TEST     := test_pcdtw
TARGET_TEST_STD := test_pcdtw_std
TARGET_TEST_RVV := test_pcdtw_rvv
SRCS_TEST       := src/test_pcdtw.cpp

TARGET_BENCH     := bench_pcdtw
TARGET_BENCH_STD := bench_pcdtw_std
TARGET_BENCH_RVV := bench_pcdtw_rvv
SRCS_BENCH       := src/bench_pcdtw.cpp

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

PCDTW_REPEAT_RUNS ?= 5
PCDTW_REPEAT_DIR := $(OUTPUT_DIR_BOARD)/component_ablation_repeat_5
PCDTW_REPEAT_SUMMARY := $(PCDTW_REPEAT_DIR)/summary.md
PCDTW_SHAPED_REPEAT_DIR := $(OUTPUT_DIR_BOARD)/production_shaped_repeat_5
PCDTW_SHAPED_REPEAT_SUMMARY := $(PCDTW_SHAPED_REPEAT_DIR)/summary.md
PCDTW_PRODUCTION_COMPRESSED_REPEAT_DIR := $(OUTPUT_DIR_BOARD)/production_compressed_repeat_5
PCDTW_PRODUCTION_COMPRESSED_REPEAT_SUMMARY := $(PCDTW_PRODUCTION_COMPRESSED_REPEAT_DIR)/summary.md
PCDTW_BINARY_REPEAT_DIR := $(OUTPUT_DIR_BOARD)/binary_component_repeat_5
PCDTW_BINARY_REPEAT_SUMMARY := $(PCDTW_BINARY_REPEAT_DIR)/summary.md
PCDTW_BINARY_TUPLE_REPEAT_DIR := $(OUTPUT_DIR_BOARD)/binary_tuple_segment_repeat_5
PCDTW_BINARY_TUPLE_REPEAT_SUMMARY := $(PCDTW_BINARY_TUPLE_REPEAT_DIR)/summary.md
PCDTW_PRODUCTION_BINARY_TUPLE_REPEAT_DIR := $(OUTPUT_DIR_BOARD)/production_binary_tuple_repeat_5
PCDTW_PRODUCTION_BINARY_TUPLE_REPEAT_SUMMARY := $(PCDTW_PRODUCTION_BINARY_TUPLE_REPEAT_DIR)/summary.md
PCDTW_EVIDENCE_MANIFEST := $(OUTPUT_DIR_BOARD)/evidence_manifest.json
PCDTW_EVIDENCE_DOCTOR := $(OUTPUT_DIR_BOARD)/evidence_doctor.md
PCDTW_EVIDENCE_REGISTRY := log/evidence_registry.json

run_qemu_smoke: run_test_compare run_bench_rvv

run_board_pcdtw_smoke:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter pointxyzrgb_4f_262k --iterations 10 --warmup-iterations 2"

collect_board_pcdtw_repeated:
	@mkdir -p $(PCDTW_REPEAT_DIR)
	@set -e; for run_id in $$(seq 1 $(PCDTW_REPEAT_RUNS)); do \
		echo "[BOARD] pcd_io_templated_writer component repeated run $$run_id/$(PCDTW_REPEAT_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter all --iterations 20 --warmup-iterations 3" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		mkdir -p $(PCDTW_REPEAT_DIR)/run$$run_id; \
		cp $(OUTPUT_DIR_BOARD)/analyze_bench_compare.log $(PCDTW_REPEAT_DIR)/run$$run_id/analyze_bench_compare.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_std.log $(PCDTW_REPEAT_DIR)/run$$run_id/run_bench_std.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_rvv.log $(PCDTW_REPEAT_DIR)/run$$run_id/run_bench_rvv.log; \
	done

generate_board_pcdtw_repeat_summary:
	@$(PYTHON_RUN) script/generate_pcdtw_repeat_summary.py \
		--repeat-dir $(PCDTW_REPEAT_DIR) \
		--output $(PCDTW_REPEAT_SUMMARY) \
		--title "pcd_io_templated_writer component repeated board summary" \
		--evidence-role component_ablation

generate_board_pcdtw_evidence_manifest: generate_board_pcdtw_repeat_summary
	@$(PYTHON_RUN) script/generate_pcdtw_evidence_manifest.py \
		--repeat-dir $(PCDTW_REPEAT_DIR) \
		--summary-log $(PCDTW_REPEAT_SUMMARY) \
		--output $(PCDTW_EVIDENCE_MANIFEST)

run_board_pcdtw_evidence_doctor: generate_board_pcdtw_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(PCDTW_EVIDENCE_MANIFEST) \
		--output $(PCDTW_EVIDENCE_DOCTOR) \
		--fail-on never
	@cp $(PCDTW_EVIDENCE_MANIFEST) $(PCDTW_REPEAT_DIR)/evidence_manifest.json
	@cp $(PCDTW_EVIDENCE_DOCTOR) $(PCDTW_REPEAT_DIR)/evidence_doctor.md
	@rm -f $(PCDTW_EVIDENCE_MANIFEST) $(PCDTW_EVIDENCE_DOCTOR)

record_board_pcdtw_evidence_state: run_board_pcdtw_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(PCDTW_EVIDENCE_REGISTRY) \
		--summary $(PCDTW_REPEAT_SUMMARY) \
		--manifest $(PCDTW_REPEAT_DIR)/evidence_manifest.json \
		--doctor $(PCDTW_REPEAT_DIR)/evidence_doctor.md \
		--target run_board_pcdtw_repeated \
		--backend board \
		--run-label board-pcdtw-component-repeat-phase000 \
		--case-filter all \
		--evidence-role component_ablation \
		--doc-ref doc/phases/000-current-state-and-component-ablation/result.zh.md \
		--doc-ref doc/pcd_io_templated_writer-evaluation.zh.md \
		--freshness-state fresh

run_board_pcdtw_repeated: collect_board_pcdtw_repeated record_board_pcdtw_evidence_state

collect_board_pcdtw_shaped_repeated:
	@mkdir -p $(PCDTW_SHAPED_REPEAT_DIR)
	@set -e; for run_id in $$(seq 1 $(PCDTW_REPEAT_RUNS)); do \
		echo "[BOARD] pcd_io_templated_writer production-shaped repeated run $$run_id/$(PCDTW_REPEAT_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter compressed_* --iterations 20 --warmup-iterations 3" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		mkdir -p $(PCDTW_SHAPED_REPEAT_DIR)/run$$run_id; \
		cp $(OUTPUT_DIR_BOARD)/analyze_bench_compare.log $(PCDTW_SHAPED_REPEAT_DIR)/run$$run_id/analyze_bench_compare.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_std.log $(PCDTW_SHAPED_REPEAT_DIR)/run$$run_id/run_bench_std.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_rvv.log $(PCDTW_SHAPED_REPEAT_DIR)/run$$run_id/run_bench_rvv.log; \
	done

generate_board_pcdtw_shaped_repeat_summary:
	@$(PYTHON_RUN) script/generate_pcdtw_repeat_summary.py \
		--repeat-dir $(PCDTW_SHAPED_REPEAT_DIR) \
		--output $(PCDTW_SHAPED_REPEAT_SUMMARY) \
		--title "pcd_io_templated_writer production-shaped repeated board summary" \
		--evidence-role production_shaped_diagnostic

generate_board_pcdtw_shaped_evidence_manifest: generate_board_pcdtw_shaped_repeat_summary
	@$(PYTHON_RUN) script/generate_pcdtw_evidence_manifest.py \
		--repeat-dir $(PCDTW_SHAPED_REPEAT_DIR) \
		--summary-log $(PCDTW_SHAPED_REPEAT_SUMMARY) \
		--output $(PCDTW_EVIDENCE_MANIFEST) \
		--evidence-role production_shaped_diagnostic \
		--label "pcd_io_templated_writer board repeated phase010 production-shaped"

run_board_pcdtw_shaped_evidence_doctor: generate_board_pcdtw_shaped_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(PCDTW_EVIDENCE_MANIFEST) \
		--output $(PCDTW_EVIDENCE_DOCTOR) \
		--fail-on never
	@cp $(PCDTW_EVIDENCE_MANIFEST) $(PCDTW_SHAPED_REPEAT_DIR)/evidence_manifest.json
	@cp $(PCDTW_EVIDENCE_DOCTOR) $(PCDTW_SHAPED_REPEAT_DIR)/evidence_doctor.md
	@rm -f $(PCDTW_EVIDENCE_MANIFEST) $(PCDTW_EVIDENCE_DOCTOR)

record_board_pcdtw_shaped_evidence_state: run_board_pcdtw_shaped_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(PCDTW_EVIDENCE_REGISTRY) \
		--summary $(PCDTW_SHAPED_REPEAT_SUMMARY) \
		--manifest $(PCDTW_SHAPED_REPEAT_DIR)/evidence_manifest.json \
		--doctor $(PCDTW_SHAPED_REPEAT_DIR)/evidence_doctor.md \
		--target run_board_pcdtw_shaped_repeated \
		--backend board \
		--run-label board-pcdtw-production-shaped-repeat-phase010 \
		--case-filter "compressed_*" \
		--evidence-role production_shaped_diagnostic \
		--doc-ref doc/phases/010-production-shaped-compressed-writer-diagnostic/result.zh.md \
		--doc-ref doc/pcd_io_templated_writer-evaluation.zh.md \
		--freshness-state fresh

run_board_pcdtw_shaped_repeated: collect_board_pcdtw_shaped_repeated record_board_pcdtw_shaped_evidence_state

collect_board_pcdtw_production_compressed_repeated:
	@mkdir -p $(PCDTW_PRODUCTION_COMPRESSED_REPEAT_DIR)
	@set -e; for run_id in $$(seq 1 $(PCDTW_REPEAT_RUNS)); do \
		echo "[BOARD] pcd_io_templated_writer production compressed repeated run $$run_id/$(PCDTW_REPEAT_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter production_compressed_* --iterations 20 --warmup-iterations 3" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		mkdir -p $(PCDTW_PRODUCTION_COMPRESSED_REPEAT_DIR)/run$$run_id; \
		cp $(OUTPUT_DIR_BOARD)/analyze_bench_compare.log $(PCDTW_PRODUCTION_COMPRESSED_REPEAT_DIR)/run$$run_id/analyze_bench_compare.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_std.log $(PCDTW_PRODUCTION_COMPRESSED_REPEAT_DIR)/run$$run_id/run_bench_std.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_rvv.log $(PCDTW_PRODUCTION_COMPRESSED_REPEAT_DIR)/run$$run_id/run_bench_rvv.log; \
	done

generate_board_pcdtw_production_compressed_repeat_summary:
	@$(PYTHON_RUN) script/generate_pcdtw_repeat_summary.py \
		--repeat-dir $(PCDTW_PRODUCTION_COMPRESSED_REPEAT_DIR) \
		--output $(PCDTW_PRODUCTION_COMPRESSED_REPEAT_SUMMARY) \
		--title "pcd_io_templated_writer production compressed repeated board summary" \
		--evidence-role production_public

generate_board_pcdtw_production_compressed_evidence_manifest: generate_board_pcdtw_production_compressed_repeat_summary
	@$(PYTHON_RUN) script/generate_pcdtw_evidence_manifest.py \
		--repeat-dir $(PCDTW_PRODUCTION_COMPRESSED_REPEAT_DIR) \
		--summary-log $(PCDTW_PRODUCTION_COMPRESSED_REPEAT_SUMMARY) \
		--output $(PCDTW_EVIDENCE_MANIFEST) \
		--evidence-role production_public \
		--label "pcd_io_templated_writer board repeated phase060 production compressed"

run_board_pcdtw_production_compressed_evidence_doctor: generate_board_pcdtw_production_compressed_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(PCDTW_EVIDENCE_MANIFEST) \
		--output $(PCDTW_EVIDENCE_DOCTOR) \
		--fail-on never
	@cp $(PCDTW_EVIDENCE_MANIFEST) $(PCDTW_PRODUCTION_COMPRESSED_REPEAT_DIR)/evidence_manifest.json
	@cp $(PCDTW_EVIDENCE_DOCTOR) $(PCDTW_PRODUCTION_COMPRESSED_REPEAT_DIR)/evidence_doctor.md
	@rm -f $(PCDTW_EVIDENCE_MANIFEST) $(PCDTW_EVIDENCE_DOCTOR)

record_board_pcdtw_production_compressed_evidence_state: run_board_pcdtw_production_compressed_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(PCDTW_EVIDENCE_REGISTRY) \
		--summary $(PCDTW_PRODUCTION_COMPRESSED_REPEAT_SUMMARY) \
		--manifest $(PCDTW_PRODUCTION_COMPRESSED_REPEAT_DIR)/evidence_manifest.json \
		--doctor $(PCDTW_PRODUCTION_COMPRESSED_REPEAT_DIR)/evidence_doctor.md \
		--target run_board_pcdtw_production_compressed_repeated \
		--backend board \
		--run-label board-pcdtw-production-compressed-repeat-phase060 \
		--case-filter "production_compressed_*" \
		--evidence-role production_public \
		--doc-ref doc/phases/060-compressed-writer-PI2-production-patch/result.zh.md \
		--doc-ref doc/pcd_io_templated_writer-evaluation.zh.md \
		--freshness-state fresh

run_board_pcdtw_production_compressed_repeated: collect_board_pcdtw_production_compressed_repeated record_board_pcdtw_production_compressed_evidence_state

collect_board_pcdtw_binary_repeated:
	@mkdir -p $(PCDTW_BINARY_REPEAT_DIR)
	@set -e; for run_id in $$(seq 1 $(PCDTW_REPEAT_RUNS)); do \
		echo "[BOARD] pcd_io_templated_writer binary component repeated run $$run_id/$(PCDTW_REPEAT_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter binary_* --iterations 20 --warmup-iterations 3" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		mkdir -p $(PCDTW_BINARY_REPEAT_DIR)/run$$run_id; \
		cp $(OUTPUT_DIR_BOARD)/analyze_bench_compare.log $(PCDTW_BINARY_REPEAT_DIR)/run$$run_id/analyze_bench_compare.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_std.log $(PCDTW_BINARY_REPEAT_DIR)/run$$run_id/run_bench_std.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_rvv.log $(PCDTW_BINARY_REPEAT_DIR)/run$$run_id/run_bench_rvv.log; \
	done

generate_board_pcdtw_binary_repeat_summary:
	@$(PYTHON_RUN) script/generate_pcdtw_repeat_summary.py \
		--repeat-dir $(PCDTW_BINARY_REPEAT_DIR) \
		--output $(PCDTW_BINARY_REPEAT_SUMMARY) \
		--title "pcd_io_templated_writer binary component repeated board summary" \
		--evidence-role binary_component_ablation

generate_board_pcdtw_binary_evidence_manifest: generate_board_pcdtw_binary_repeat_summary
	@$(PYTHON_RUN) script/generate_pcdtw_evidence_manifest.py \
		--repeat-dir $(PCDTW_BINARY_REPEAT_DIR) \
		--summary-log $(PCDTW_BINARY_REPEAT_SUMMARY) \
		--output $(PCDTW_EVIDENCE_MANIFEST) \
		--evidence-role binary_component_ablation \
		--label "pcd_io_templated_writer board repeated phase040 binary"

run_board_pcdtw_binary_evidence_doctor: generate_board_pcdtw_binary_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(PCDTW_EVIDENCE_MANIFEST) \
		--output $(PCDTW_EVIDENCE_DOCTOR) \
		--fail-on never
	@cp $(PCDTW_EVIDENCE_MANIFEST) $(PCDTW_BINARY_REPEAT_DIR)/evidence_manifest.json
	@cp $(PCDTW_EVIDENCE_DOCTOR) $(PCDTW_BINARY_REPEAT_DIR)/evidence_doctor.md
	@rm -f $(PCDTW_EVIDENCE_MANIFEST) $(PCDTW_EVIDENCE_DOCTOR)

record_board_pcdtw_binary_evidence_state: run_board_pcdtw_binary_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(PCDTW_EVIDENCE_REGISTRY) \
		--summary $(PCDTW_BINARY_REPEAT_SUMMARY) \
		--manifest $(PCDTW_BINARY_REPEAT_DIR)/evidence_manifest.json \
		--doctor $(PCDTW_BINARY_REPEAT_DIR)/evidence_doctor.md \
		--target run_board_pcdtw_binary_repeated \
		--backend board \
		--run-label board-pcdtw-binary-component-repeat-phase040 \
		--case-filter "binary_*" \
		--evidence-role binary_component_ablation \
		--doc-ref doc/phases/040-binary-writer-component-ablation/result.zh.md \
		--doc-ref doc/pcd_io_templated_writer-evaluation.zh.md \
		--freshness-state fresh

run_board_pcdtw_binary_repeated: collect_board_pcdtw_binary_repeated record_board_pcdtw_binary_evidence_state

collect_board_pcdtw_binary_tuple_repeated:
	@mkdir -p $(PCDTW_BINARY_TUPLE_REPEAT_DIR)
	@set -e; for run_id in $$(seq 1 $(PCDTW_REPEAT_RUNS)); do \
		echo "[BOARD] pcd_io_templated_writer binary tuple/segment repeated run $$run_id/$(PCDTW_REPEAT_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter binary_tuple_* --iterations 20 --warmup-iterations 3" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		mkdir -p $(PCDTW_BINARY_TUPLE_REPEAT_DIR)/run$$run_id; \
		cp $(OUTPUT_DIR_BOARD)/analyze_bench_compare.log $(PCDTW_BINARY_TUPLE_REPEAT_DIR)/run$$run_id/analyze_bench_compare.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_std.log $(PCDTW_BINARY_TUPLE_REPEAT_DIR)/run$$run_id/run_bench_std.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_rvv.log $(PCDTW_BINARY_TUPLE_REPEAT_DIR)/run$$run_id/run_bench_rvv.log; \
	done

generate_board_pcdtw_binary_tuple_repeat_summary:
	@$(PYTHON_RUN) script/generate_pcdtw_repeat_summary.py \
		--repeat-dir $(PCDTW_BINARY_TUPLE_REPEAT_DIR) \
		--output $(PCDTW_BINARY_TUPLE_REPEAT_SUMMARY) \
		--title "pcd_io_templated_writer binary tuple/segment repeated board summary" \
		--evidence-role binary_tuple_segment_diagnostic

generate_board_pcdtw_binary_tuple_evidence_manifest: generate_board_pcdtw_binary_tuple_repeat_summary
	@$(PYTHON_RUN) script/generate_pcdtw_evidence_manifest.py \
		--repeat-dir $(PCDTW_BINARY_TUPLE_REPEAT_DIR) \
		--summary-log $(PCDTW_BINARY_TUPLE_REPEAT_SUMMARY) \
		--output $(PCDTW_EVIDENCE_MANIFEST) \
		--evidence-role binary_tuple_segment_diagnostic \
		--label "pcd_io_templated_writer board repeated phase090 binary tuple segment"

run_board_pcdtw_binary_tuple_evidence_doctor: generate_board_pcdtw_binary_tuple_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(PCDTW_EVIDENCE_MANIFEST) \
		--output $(PCDTW_EVIDENCE_DOCTOR) \
		--fail-on never
	@cp $(PCDTW_EVIDENCE_MANIFEST) $(PCDTW_BINARY_TUPLE_REPEAT_DIR)/evidence_manifest.json
	@cp $(PCDTW_EVIDENCE_DOCTOR) $(PCDTW_BINARY_TUPLE_REPEAT_DIR)/evidence_doctor.md
	@rm -f $(PCDTW_EVIDENCE_MANIFEST) $(PCDTW_EVIDENCE_DOCTOR)

record_board_pcdtw_binary_tuple_evidence_state: run_board_pcdtw_binary_tuple_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(PCDTW_EVIDENCE_REGISTRY) \
		--summary $(PCDTW_BINARY_TUPLE_REPEAT_SUMMARY) \
		--manifest $(PCDTW_BINARY_TUPLE_REPEAT_DIR)/evidence_manifest.json \
		--doctor $(PCDTW_BINARY_TUPLE_REPEAT_DIR)/evidence_doctor.md \
		--target run_board_pcdtw_binary_tuple_repeated \
		--backend board \
		--run-label board-pcdtw-binary-tuple-segment-repeat-phase090 \
		--case-filter "binary_tuple_*" \
		--evidence-role binary_tuple_segment_diagnostic \
		--doc-ref doc/phases/090-binary-writer-tuple-segment-diagnostic/result.zh.md \
		--doc-ref doc/pcd_io_templated_writer-evaluation.zh.md \
		--freshness-state fresh

run_board_pcdtw_binary_tuple_repeated: collect_board_pcdtw_binary_tuple_repeated record_board_pcdtw_binary_tuple_evidence_state

collect_board_pcdtw_production_binary_tuple_repeated:
	@mkdir -p $(PCDTW_PRODUCTION_BINARY_TUPLE_REPEAT_DIR)
	@set -e; for run_id in $$(seq 1 $(PCDTW_REPEAT_RUNS)); do \
		echo "[BOARD] pcd_io_templated_writer production binary tuple repeated run $$run_id/$(PCDTW_REPEAT_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter production_binary_tuple_* --iterations 20 --warmup-iterations 3" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		mkdir -p $(PCDTW_PRODUCTION_BINARY_TUPLE_REPEAT_DIR)/run$$run_id; \
		cp $(OUTPUT_DIR_BOARD)/analyze_bench_compare.log $(PCDTW_PRODUCTION_BINARY_TUPLE_REPEAT_DIR)/run$$run_id/analyze_bench_compare.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_std.log $(PCDTW_PRODUCTION_BINARY_TUPLE_REPEAT_DIR)/run$$run_id/run_bench_std.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_rvv.log $(PCDTW_PRODUCTION_BINARY_TUPLE_REPEAT_DIR)/run$$run_id/run_bench_rvv.log; \
	done

generate_board_pcdtw_production_binary_tuple_repeat_summary:
	@$(PYTHON_RUN) script/generate_pcdtw_repeat_summary.py \
		--repeat-dir $(PCDTW_PRODUCTION_BINARY_TUPLE_REPEAT_DIR) \
		--output $(PCDTW_PRODUCTION_BINARY_TUPLE_REPEAT_SUMMARY) \
		--title "pcd_io_templated_writer production binary tuple repeated board summary" \
		--evidence-role production_public

generate_board_pcdtw_production_binary_tuple_evidence_manifest: generate_board_pcdtw_production_binary_tuple_repeat_summary
	@$(PYTHON_RUN) script/generate_pcdtw_evidence_manifest.py \
		--repeat-dir $(PCDTW_PRODUCTION_BINARY_TUPLE_REPEAT_DIR) \
		--summary-log $(PCDTW_PRODUCTION_BINARY_TUPLE_REPEAT_SUMMARY) \
		--output $(PCDTW_EVIDENCE_MANIFEST) \
		--evidence-role production_public \
		--label "pcd_io_templated_writer board repeated phase100 production binary tuple"

run_board_pcdtw_production_binary_tuple_evidence_doctor: generate_board_pcdtw_production_binary_tuple_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(PCDTW_EVIDENCE_MANIFEST) \
		--output $(PCDTW_EVIDENCE_DOCTOR) \
		--fail-on never
	@cp $(PCDTW_EVIDENCE_MANIFEST) $(PCDTW_PRODUCTION_BINARY_TUPLE_REPEAT_DIR)/evidence_manifest.json
	@cp $(PCDTW_EVIDENCE_DOCTOR) $(PCDTW_PRODUCTION_BINARY_TUPLE_REPEAT_DIR)/evidence_doctor.md
	@rm -f $(PCDTW_EVIDENCE_MANIFEST) $(PCDTW_EVIDENCE_DOCTOR)

record_board_pcdtw_production_binary_tuple_evidence_state: run_board_pcdtw_production_binary_tuple_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(PCDTW_EVIDENCE_REGISTRY) \
		--summary $(PCDTW_PRODUCTION_BINARY_TUPLE_REPEAT_SUMMARY) \
		--manifest $(PCDTW_PRODUCTION_BINARY_TUPLE_REPEAT_DIR)/evidence_manifest.json \
		--doctor $(PCDTW_PRODUCTION_BINARY_TUPLE_REPEAT_DIR)/evidence_doctor.md \
		--target run_board_pcdtw_production_binary_tuple_repeated \
		--backend board \
		--run-label board-pcdtw-production-binary-tuple-repeat-phase100 \
		--case-filter "production_binary_tuple_*" \
		--evidence-role production_public \
		--doc-ref doc/phases/100-binary-writer-tuple-segment-production-probe/result.zh.md \
		--doc-ref doc/pcd_io_templated_writer-evaluation.zh.md \
		--freshness-state fresh

run_board_pcdtw_production_binary_tuple_repeated: collect_board_pcdtw_production_binary_tuple_repeated record_board_pcdtw_production_binary_tuple_evidence_state

.PHONY: run_qemu_smoke run_board_pcdtw_smoke collect_board_pcdtw_repeated generate_board_pcdtw_repeat_summary generate_board_pcdtw_evidence_manifest run_board_pcdtw_evidence_doctor record_board_pcdtw_evidence_state run_board_pcdtw_repeated collect_board_pcdtw_shaped_repeated generate_board_pcdtw_shaped_repeat_summary generate_board_pcdtw_shaped_evidence_manifest run_board_pcdtw_shaped_evidence_doctor record_board_pcdtw_shaped_evidence_state run_board_pcdtw_shaped_repeated collect_board_pcdtw_production_compressed_repeated generate_board_pcdtw_production_compressed_repeat_summary generate_board_pcdtw_production_compressed_evidence_manifest run_board_pcdtw_production_compressed_evidence_doctor record_board_pcdtw_production_compressed_evidence_state run_board_pcdtw_production_compressed_repeated collect_board_pcdtw_binary_repeated generate_board_pcdtw_binary_repeat_summary generate_board_pcdtw_binary_evidence_manifest run_board_pcdtw_binary_evidence_doctor record_board_pcdtw_binary_evidence_state run_board_pcdtw_binary_repeated collect_board_pcdtw_binary_tuple_repeated generate_board_pcdtw_binary_tuple_repeat_summary generate_board_pcdtw_binary_tuple_evidence_manifest run_board_pcdtw_binary_tuple_evidence_doctor record_board_pcdtw_binary_tuple_evidence_state run_board_pcdtw_binary_tuple_repeated collect_board_pcdtw_production_binary_tuple_repeated generate_board_pcdtw_production_binary_tuple_repeat_summary generate_board_pcdtw_production_binary_tuple_evidence_manifest run_board_pcdtw_production_binary_tuple_evidence_doctor record_board_pcdtw_production_binary_tuple_evidence_state run_board_pcdtw_production_binary_tuple_repeated
