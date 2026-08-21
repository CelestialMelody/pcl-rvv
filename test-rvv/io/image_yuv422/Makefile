# =============================================================================
#  PCL io/image_yuv422 RVV diagnostic Makefile
#
#  本 topic 先建立 production-shaped diagnostic（生产形态诊断）：测试专用
#  helper 复刻 ImageYUV422::fillRGB() / fillGrayscale() 的 YUYV 语义。
#  QEMU 只用于 correctness（正确性）和日志形状；性能结论必须来自板卡。
# =============================================================================

TOPIC  := image_yuv422
MODULE := io
PCL_REPO_ROOT := $(abspath $(CURDIR)/../../..)

FOCUS_DIR      := pcl/io/src/image_yuv422.cpp
FILTER_REPORT  := log/filtered_image_yuv422.log
ANALYZE_REPORT := log/analyze_image_yuv422.log

TARGET_TEST     := test_image_yuv422
TARGET_TEST_STD := test_image_yuv422_std
TARGET_TEST_RVV := test_image_yuv422_rvv
SRCS_TEST       := src/test_image_yuv422.cpp
SRCS_TEST       += $(PCL_REPO_ROOT)/io/src/image_yuv422.cpp
SRCS_TEST       += $(PCL_REPO_ROOT)/io/src/io_exception.cpp

TARGET_BENCH     := bench_image_yuv422
TARGET_BENCH_STD := bench_image_yuv422_std
TARGET_BENCH_RVV := bench_image_yuv422_rvv
SRCS_BENCH       := src/bench_image_yuv422.cpp
SRCS_BENCH       += $(PCL_REPO_ROOT)/io/src/image_yuv422.cpp
SRCS_BENCH       += $(PCL_REPO_ROOT)/io/src/io_exception.cpp

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

EVIDENCE_MANIFEST := $(OUTPUT_DIR_BOARD)/evidence_manifest.json
EVIDENCE_DOCTOR   := $(OUTPUT_DIR_BOARD)/evidence_doctor.md
PROD_REPEAT_DIR   := $(OUTPUT_DIR_BOARD)/prod_rgb_full_640x480_repeat_5
PROD_PADDED_REPEAT_DIR := $(OUTPUT_DIR_BOARD)/prod_rgb_full_padded_640x480_repeat_5
PROD_PADDED_REPEAT_SUMMARY := $(PROD_PADDED_REPEAT_DIR)/summary.md
DOWNSAMPLE_REPEAT_RUNS ?= 5
DOWNSAMPLE_REPEAT_DIR  := $(OUTPUT_DIR_BOARD)/rgb_downsample_640x480_to_320x240_repeat_5
DOWNSAMPLE_REPEAT_SUMMARY := $(DOWNSAMPLE_REPEAT_DIR)/summary.md
PROD_DOWNSAMPLE_REPEAT_DIR := $(OUTPUT_DIR_BOARD)/prod_rgb_downsample_640x480_to_320x240_repeat_5
PROD_DOWNSAMPLE_REPEAT_SUMMARY := $(PROD_DOWNSAMPLE_REPEAT_DIR)/summary.md

run_qemu_smoke: run_test_compare run_bench_rvv

generate_evidence_manifest:
	@$(PYTHON_RUN) script/generate_image_yuv422_evidence_manifest.py \
		--std-log $(OUTPUT_DIR_BOARD)/run_bench_std.log \
		--rvv-log $(OUTPUT_DIR_BOARD)/run_bench_rvv.log \
		--summary-log $(OUTPUT_DIR_BOARD)/analyze_bench_compare.log \
		--output $(EVIDENCE_MANIFEST)

run_evidence_doctor: generate_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(EVIDENCE_MANIFEST) \
		--output $(EVIDENCE_DOCTOR) \
		--fail-on never

generate_prod_repeat_evidence_manifest:
	@$(PYTHON_RUN) script/generate_image_yuv422_evidence_manifest.py \
		--repeat-dir $(PROD_REPEAT_DIR) \
		--summary-log $(PROD_REPEAT_DIR)/summary.md \
		--output $(EVIDENCE_MANIFEST)

run_evidence_doctor_prod_repeat: generate_prod_repeat_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(EVIDENCE_MANIFEST) \
		--output $(EVIDENCE_DOCTOR) \
		--fail-on never
	@cp $(EVIDENCE_MANIFEST) $(PROD_REPEAT_DIR)/evidence_manifest.json
	@cp $(EVIDENCE_DOCTOR) $(PROD_REPEAT_DIR)/evidence_doctor.md

collect_board_yuv422_prod_rgb_full_padded_repeated:
	@mkdir -p $(PROD_PADDED_REPEAT_DIR)
	@set -e; for run_id in $$(seq 1 $(DOWNSAMPLE_REPEAT_RUNS)); do \
		echo "[BOARD] image_yuv422 production RGB full padded repeated run $$run_id/$(DOWNSAMPLE_REPEAT_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_yuv422_prod_rgb_full_padded || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		mkdir -p $(PROD_PADDED_REPEAT_DIR)/run$$run_id; \
		cp $(OUTPUT_DIR_BOARD)/analyze_bench_compare.log $(PROD_PADDED_REPEAT_DIR)/run$$run_id/analyze_bench_compare.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_std.log $(PROD_PADDED_REPEAT_DIR)/run$$run_id/run_bench_std.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_rvv.log $(PROD_PADDED_REPEAT_DIR)/run$$run_id/run_bench_rvv.log; \
	done

generate_prod_padded_repeat_summary:
	@$(PYTHON_RUN) script/generate_image_yuv422_repeat_summary.py \
		--repeat-dir $(PROD_PADDED_REPEAT_DIR) \
		--output $(PROD_PADDED_REPEAT_SUMMARY) \
		--title "image_yuv422 production RGB full padded repeated board summary" \
		--evidence-role production_public

generate_prod_padded_repeat_evidence_manifest: generate_prod_padded_repeat_summary
	@$(PYTHON_RUN) script/generate_image_yuv422_evidence_manifest.py \
		--repeat-dir $(PROD_PADDED_REPEAT_DIR) \
		--summary-log $(PROD_PADDED_REPEAT_SUMMARY) \
		--output $(EVIDENCE_MANIFEST)

run_evidence_doctor_prod_padded_repeat: generate_prod_padded_repeat_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(EVIDENCE_MANIFEST) \
		--output $(EVIDENCE_DOCTOR) \
		--fail-on never
	@cp $(EVIDENCE_MANIFEST) $(PROD_PADDED_REPEAT_DIR)/evidence_manifest.json
	@cp $(EVIDENCE_DOCTOR) $(PROD_PADDED_REPEAT_DIR)/evidence_doctor.md

run_board_yuv422_prod_rgb_full_padded_repeated: collect_board_yuv422_prod_rgb_full_padded_repeated generate_prod_padded_repeat_summary run_evidence_doctor_prod_padded_repeat

collect_board_yuv422_rgb_downsample_repeated:
	@mkdir -p $(DOWNSAMPLE_REPEAT_DIR)
	@set -e; for run_id in $$(seq 1 $(DOWNSAMPLE_REPEAT_RUNS)); do \
		echo "[BOARD] image_yuv422 RGB downsample repeated run $$run_id/$(DOWNSAMPLE_REPEAT_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_yuv422_rgb_downsample || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		mkdir -p $(DOWNSAMPLE_REPEAT_DIR)/run$$run_id; \
		cp $(OUTPUT_DIR_BOARD)/analyze_bench_compare.log $(DOWNSAMPLE_REPEAT_DIR)/run$$run_id/analyze_bench_compare.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_std.log $(DOWNSAMPLE_REPEAT_DIR)/run$$run_id/run_bench_std.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_rvv.log $(DOWNSAMPLE_REPEAT_DIR)/run$$run_id/run_bench_rvv.log; \
	done

generate_downsample_repeat_summary:
	@$(PYTHON_RUN) script/generate_image_yuv422_repeat_summary.py \
		--repeat-dir $(DOWNSAMPLE_REPEAT_DIR) \
		--output $(DOWNSAMPLE_REPEAT_SUMMARY) \
		--title "image_yuv422 RGB downsample repeated board summary" \
		--evidence-role production_shaped_diagnostic

generate_downsample_repeat_evidence_manifest: generate_downsample_repeat_summary
	@$(PYTHON_RUN) script/generate_image_yuv422_evidence_manifest.py \
		--repeat-dir $(DOWNSAMPLE_REPEAT_DIR) \
		--summary-log $(DOWNSAMPLE_REPEAT_SUMMARY) \
		--output $(EVIDENCE_MANIFEST)

run_evidence_doctor_downsample_repeat: generate_downsample_repeat_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(EVIDENCE_MANIFEST) \
		--output $(EVIDENCE_DOCTOR) \
		--fail-on never
	@cp $(EVIDENCE_MANIFEST) $(DOWNSAMPLE_REPEAT_DIR)/evidence_manifest.json
	@cp $(EVIDENCE_DOCTOR) $(DOWNSAMPLE_REPEAT_DIR)/evidence_doctor.md

run_board_yuv422_rgb_downsample_repeated: collect_board_yuv422_rgb_downsample_repeated generate_downsample_repeat_summary run_evidence_doctor_downsample_repeat

collect_board_yuv422_prod_rgb_downsample_repeated:
	@mkdir -p $(PROD_DOWNSAMPLE_REPEAT_DIR)
	@set -e; for run_id in $$(seq 1 $(DOWNSAMPLE_REPEAT_RUNS)); do \
		echo "[BOARD] image_yuv422 production RGB downsample repeated run $$run_id/$(DOWNSAMPLE_REPEAT_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_yuv422_prod_rgb_downsample || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		mkdir -p $(PROD_DOWNSAMPLE_REPEAT_DIR)/run$$run_id; \
		cp $(OUTPUT_DIR_BOARD)/analyze_bench_compare.log $(PROD_DOWNSAMPLE_REPEAT_DIR)/run$$run_id/analyze_bench_compare.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_std.log $(PROD_DOWNSAMPLE_REPEAT_DIR)/run$$run_id/run_bench_std.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_rvv.log $(PROD_DOWNSAMPLE_REPEAT_DIR)/run$$run_id/run_bench_rvv.log; \
	done

generate_prod_downsample_repeat_summary:
	@$(PYTHON_RUN) script/generate_image_yuv422_repeat_summary.py \
		--repeat-dir $(PROD_DOWNSAMPLE_REPEAT_DIR) \
		--output $(PROD_DOWNSAMPLE_REPEAT_SUMMARY) \
		--title "image_yuv422 production RGB downsample repeated board summary" \
		--evidence-role production_public

generate_prod_downsample_repeat_evidence_manifest: generate_prod_downsample_repeat_summary
	@$(PYTHON_RUN) script/generate_image_yuv422_evidence_manifest.py \
		--repeat-dir $(PROD_DOWNSAMPLE_REPEAT_DIR) \
		--summary-log $(PROD_DOWNSAMPLE_REPEAT_SUMMARY) \
		--output $(EVIDENCE_MANIFEST)

run_evidence_doctor_prod_downsample_repeat: generate_prod_downsample_repeat_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(EVIDENCE_MANIFEST) \
		--output $(EVIDENCE_DOCTOR) \
		--fail-on never
	@cp $(EVIDENCE_MANIFEST) $(PROD_DOWNSAMPLE_REPEAT_DIR)/evidence_manifest.json
	@cp $(EVIDENCE_DOCTOR) $(PROD_DOWNSAMPLE_REPEAT_DIR)/evidence_doctor.md

run_board_yuv422_prod_rgb_downsample_repeated: collect_board_yuv422_prod_rgb_downsample_repeated generate_prod_downsample_repeat_summary run_evidence_doctor_prod_downsample_repeat

run_board_yuv422_rgb_full:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter rgb_full_640x480 --iterations 20 --warmup-iterations 3"

run_board_yuv422_prod_rgb_full:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter prod_rgb_full_640x480 --iterations 20 --warmup-iterations 3"

run_board_yuv422_prod_rgb_full_padded:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter prod_rgb_full_padded_640x480 --iterations 20 --warmup-iterations 3"

run_board_yuv422_rgb_downsample:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter rgb_downsample_640x480_to_320x240 --iterations 20 --warmup-iterations 3"

run_board_yuv422_prod_rgb_downsample:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter prod_rgb_downsample_640x480_to_320x240 --iterations 20 --warmup-iterations 3"

run_board_yuv422_gray_full:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter gray_full_640x480 --iterations 20 --warmup-iterations 3"

run_board_yuv422_smoke:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter all --iterations 10 --warmup-iterations 2"

	.PHONY: run_qemu_smoke generate_evidence_manifest run_evidence_doctor generate_prod_repeat_evidence_manifest run_evidence_doctor_prod_repeat collect_board_yuv422_prod_rgb_full_padded_repeated generate_prod_padded_repeat_summary generate_prod_padded_repeat_evidence_manifest run_evidence_doctor_prod_padded_repeat run_board_yuv422_prod_rgb_full_padded_repeated collect_board_yuv422_rgb_downsample_repeated generate_downsample_repeat_summary generate_downsample_repeat_evidence_manifest run_evidence_doctor_downsample_repeat run_board_yuv422_rgb_downsample_repeated collect_board_yuv422_prod_rgb_downsample_repeated generate_prod_downsample_repeat_summary generate_prod_downsample_repeat_evidence_manifest run_evidence_doctor_prod_downsample_repeat run_board_yuv422_prod_rgb_downsample_repeated run_board_yuv422_rgb_full run_board_yuv422_prod_rgb_full run_board_yuv422_prod_rgb_full_padded run_board_yuv422_rgb_downsample run_board_yuv422_prod_rgb_downsample run_board_yuv422_gray_full run_board_yuv422_smoke
