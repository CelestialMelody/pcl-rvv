# =============================================================================
#  PCL io/lzf_image_io RVV diagnostic Makefile
#
#  本 topic 先建立 production-shaped diagnostic（生产形态诊断）：测试专用
#  helper 复刻 PCLZF 解压后 depth/YUV/Bayer RGB buffer 到 cloud 的转换语义。
#  QEMU 只用于 correctness（正确性）和日志形状；性能结论必须来自板卡。
# =============================================================================

TOPIC  := lzf_image_io
MODULE := io

FOCUS_DIR      := pcl/io/include/pcl/io/impl/lzf_image_io.hpp
FILTER_REPORT  := log/filtered_lzf_image_io.log
ANALYZE_REPORT := log/analyze_lzf_image_io.log

TARGET_TEST     := test_lzf_image_io
TARGET_TEST_STD := test_lzf_image_io_std
TARGET_TEST_RVV := test_lzf_image_io_rvv
SRCS_TEST       := src/test_lzf_image_io.cpp

TARGET_BENCH     := bench_lzf_image_io
TARGET_BENCH_STD := bench_lzf_image_io_std
TARGET_BENCH_RVV := bench_lzf_image_io_rvv
SRCS_BENCH       := src/bench_lzf_image_io.cpp

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

LIBS_TEST  := -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lm

include ../../mk/rvv-topic.mk

LZF_REPEAT_RUNS ?= 5
LZF_REPEAT_DIR := $(OUTPUT_DIR_BOARD)/production_shaped_repeat_5
LZF_REPEAT_SUMMARY := $(LZF_REPEAT_DIR)/summary.md
LZF_EVIDENCE_MANIFEST := $(OUTPUT_DIR_BOARD)/evidence_manifest.json
LZF_EVIDENCE_DOCTOR := $(OUTPUT_DIR_BOARD)/evidence_doctor.md
LZF_EVIDENCE_REGISTRY := log/evidence_registry.json
LZF_PRODUCTION_REPEAT_DIR := $(OUTPUT_DIR_BOARD)/production_yuv422_repeat_5
LZF_PRODUCTION_REPEAT_SUMMARY := $(LZF_PRODUCTION_REPEAT_DIR)/summary.md
LZF_PRODUCTION_EVIDENCE_MANIFEST := $(LZF_PRODUCTION_REPEAT_DIR)/evidence_manifest.json
LZF_PRODUCTION_EVIDENCE_DOCTOR := $(LZF_PRODUCTION_REPEAT_DIR)/evidence_doctor.md

run_qemu_smoke: run_test_compare run_bench_rvv

run_board_lzf_depth_xyz:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter depth_xyz_640x480 --iterations 20 --warmup-iterations 3"

run_board_lzf_yuv422_rgb:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter yuv422_planar_rgb_640x480 --iterations 20 --warmup-iterations 3"

run_board_lzf_yuv422_production:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter yuv422_planar_rgb_production_640x480 --iterations 20 --warmup-iterations 3"

run_board_lzf_rgb_copy:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter rgb_buffer_to_cloud_640x480 --iterations 20 --warmup-iterations 3"

run_board_lzf_smoke:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter all --iterations 10 --warmup-iterations 2"

collect_board_lzf_repeated:
	@mkdir -p $(LZF_REPEAT_DIR)
	@set -e; for run_id in $$(seq 1 $(LZF_REPEAT_RUNS)); do \
		echo "[BOARD] lzf_image_io production-shaped repeated run $$run_id/$(LZF_REPEAT_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter all --iterations 20 --warmup-iterations 3" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		mkdir -p $(LZF_REPEAT_DIR)/run$$run_id; \
		cp $(OUTPUT_DIR_BOARD)/analyze_bench_compare.log $(LZF_REPEAT_DIR)/run$$run_id/analyze_bench_compare.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_std.log $(LZF_REPEAT_DIR)/run$$run_id/run_bench_std.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_rvv.log $(LZF_REPEAT_DIR)/run$$run_id/run_bench_rvv.log; \
	done

generate_board_lzf_repeat_summary:
	@$(PYTHON_RUN) script/generate_lzf_image_io_repeat_summary.py \
		--repeat-dir $(LZF_REPEAT_DIR) \
		--output $(LZF_REPEAT_SUMMARY) \
		--title "lzf_image_io production-shaped repeated board summary" \
		--evidence-role production_shaped_diagnostic

generate_board_lzf_evidence_manifest: generate_board_lzf_repeat_summary
	@$(PYTHON_RUN) script/generate_lzf_image_io_evidence_manifest.py \
		--repeat-dir $(LZF_REPEAT_DIR) \
		--summary-log $(LZF_REPEAT_SUMMARY) \
		--output $(LZF_EVIDENCE_MANIFEST)

run_board_lzf_evidence_doctor: generate_board_lzf_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(LZF_EVIDENCE_MANIFEST) \
		--output $(LZF_EVIDENCE_DOCTOR) \
		--fail-on never
	@cp $(LZF_EVIDENCE_MANIFEST) $(LZF_REPEAT_DIR)/evidence_manifest.json
	@cp $(LZF_EVIDENCE_DOCTOR) $(LZF_REPEAT_DIR)/evidence_doctor.md

record_board_lzf_evidence_state: run_board_lzf_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(LZF_EVIDENCE_REGISTRY) \
		--summary $(LZF_REPEAT_SUMMARY) \
		--manifest $(LZF_REPEAT_DIR)/evidence_manifest.json \
		--doctor $(LZF_REPEAT_DIR)/evidence_doctor.md \
		--target run_board_lzf_repeated \
		--backend board \
		--run-label board-lzf-image-io-production-shaped-repeat-phase000 \
		--case-filter all \
		--evidence-role production_shaped_diagnostic \
		--doc-ref doc/phases/000-current-state-and-diagnostic-scaffold/result.zh.md \
		--doc-ref doc/lzf_image_io-evaluation.zh.md \
		--freshness-state fresh

run_board_lzf_repeated: collect_board_lzf_repeated record_board_lzf_evidence_state

collect_board_lzf_yuv422_production_repeated:
	@mkdir -p $(LZF_PRODUCTION_REPEAT_DIR)
	@set -e; for run_id in $$(seq 1 $(LZF_REPEAT_RUNS)); do \
		echo "[BOARD] lzf_image_io production YUV422 repeated run $$run_id/$(LZF_REPEAT_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter yuv422_planar_rgb_production_640x480 --iterations 20 --warmup-iterations 3" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		mkdir -p $(LZF_PRODUCTION_REPEAT_DIR)/run$$run_id; \
		cp $(OUTPUT_DIR_BOARD)/analyze_bench_compare.log $(LZF_PRODUCTION_REPEAT_DIR)/run$$run_id/analyze_bench_compare.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_std.log $(LZF_PRODUCTION_REPEAT_DIR)/run$$run_id/run_bench_std.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_rvv.log $(LZF_PRODUCTION_REPEAT_DIR)/run$$run_id/run_bench_rvv.log; \
	done

generate_board_lzf_yuv422_production_repeat_summary:
	@$(PYTHON_RUN) script/generate_lzf_image_io_repeat_summary.py \
		--repeat-dir $(LZF_PRODUCTION_REPEAT_DIR) \
		--output $(LZF_PRODUCTION_REPEAT_SUMMARY) \
		--title "lzf_image_io production YUV422 repeated board summary" \
		--evidence-role production_detail

generate_board_lzf_yuv422_production_evidence_manifest: generate_board_lzf_yuv422_production_repeat_summary
	@$(PYTHON_RUN) script/generate_lzf_image_io_evidence_manifest.py \
		--repeat-dir $(LZF_PRODUCTION_REPEAT_DIR) \
		--summary-log $(LZF_PRODUCTION_REPEAT_SUMMARY) \
		--output $(LZF_PRODUCTION_EVIDENCE_MANIFEST)

run_board_lzf_yuv422_production_evidence_doctor: generate_board_lzf_yuv422_production_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(LZF_PRODUCTION_EVIDENCE_MANIFEST) \
		--output $(LZF_PRODUCTION_EVIDENCE_DOCTOR) \
		--fail-on never

record_board_lzf_yuv422_production_evidence_state: run_board_lzf_yuv422_production_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(LZF_EVIDENCE_REGISTRY) \
		--summary $(LZF_PRODUCTION_REPEAT_SUMMARY) \
		--manifest $(LZF_PRODUCTION_EVIDENCE_MANIFEST) \
		--doctor $(LZF_PRODUCTION_EVIDENCE_DOCTOR) \
		--target run_board_lzf_yuv422_production_repeated \
		--backend board \
		--run-label board-lzf-image-io-production-yuv422-repeat-phase020 \
		--case-filter yuv422_planar_rgb_production_640x480 \
		--evidence-role production_detail \
		--doc-ref doc/phases/020-yuv-planar-production-probe/result.zh.md \
		--doc-ref doc/lzf_image_io-evaluation.zh.md \
		--freshness-state fresh

run_board_lzf_yuv422_production_repeated: collect_board_lzf_yuv422_production_repeated record_board_lzf_yuv422_production_evidence_state

check_evidence_freshness:
	@$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry $(LZF_EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/io/lzf_image_io/log/board/production_shaped_repeat_5/summary.md' \
		--scan-glob 'test-rvv/io/lzf_image_io/log/board/production_shaped_repeat_5/evidence_manifest.json' \
		--scan-glob 'test-rvv/io/lzf_image_io/log/board/production_shaped_repeat_5/evidence_doctor.md' \
		--doc doc/phases/000-current-state-and-diagnostic-scaffold/result.zh.md \
		--doc doc/lzf_image_io-evaluation.zh.md \
		--fail-on never

check_production_evidence_freshness:
	@$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry $(LZF_EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/io/lzf_image_io/log/board/production_yuv422_repeat_5/summary.md' \
		--scan-glob 'test-rvv/io/lzf_image_io/log/board/production_yuv422_repeat_5/evidence_manifest.json' \
		--scan-glob 'test-rvv/io/lzf_image_io/log/board/production_yuv422_repeat_5/evidence_doctor.md' \
		--doc doc/phases/020-yuv-planar-production-probe/result.zh.md \
		--doc doc/lzf_image_io-evaluation.zh.md \
		--fail-on never

.PHONY: run_qemu_smoke run_board_lzf_depth_xyz run_board_lzf_yuv422_rgb run_board_lzf_yuv422_production run_board_lzf_rgb_copy run_board_lzf_smoke collect_board_lzf_repeated generate_board_lzf_repeat_summary generate_board_lzf_evidence_manifest run_board_lzf_evidence_doctor record_board_lzf_evidence_state run_board_lzf_repeated collect_board_lzf_yuv422_production_repeated generate_board_lzf_yuv422_production_repeat_summary generate_board_lzf_yuv422_production_evidence_manifest run_board_lzf_yuv422_production_evidence_doctor record_board_lzf_yuv422_production_evidence_state run_board_lzf_yuv422_production_repeated check_evidence_freshness check_production_evidence_freshness
