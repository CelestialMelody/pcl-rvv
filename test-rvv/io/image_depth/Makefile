# =============================================================================
#  PCL io/image_depth RVV diagnostic Makefile
#
#  本 topic 先建立 production-shaped diagnostic（生产形态诊断）：
#  测试专用 helper 复刻 DepthImage::fillDepthImage() / fillDisparityImage()
#  的像素语义和 padding 语义，用于筛选 RVV 候选。QEMU 只用于正确性
#  和日志形状；性能结论必须来自板卡或目标硬件。
# =============================================================================

TOPIC  := image_depth
MODULE := io

FOCUS_DIR      := pcl/io/src/image_depth.cpp
FILTER_REPORT  := log/filtered_image_depth.log
ANALYZE_REPORT := log/analyze_image_depth.log

TARGET_TEST     := test_image_depth
TARGET_TEST_STD := test_image_depth_std
TARGET_TEST_RVV := test_image_depth_rvv
SRCS_PRODUCTION = $(PCL_SOURCE_ROOT)/io/src/image_depth.cpp $(PCL_SOURCE_ROOT)/io/src/io_exception.cpp
SRCS_TEST       = src/test_image_depth.cpp src/image_depth_test_hook.cpp $(PCL_SOURCE_ROOT)/io/src/io_exception.cpp

TARGET_BENCH     := bench_image_depth
TARGET_BENCH_STD := bench_image_depth_std
TARGET_BENCH_RVV := bench_image_depth_rvv
SRCS_BENCH       = src/bench_image_depth.cpp $(SRCS_PRODUCTION)

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

LIBS_TEST  := -lpcl_common -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lpcl_common -lm

IMAGE_DEPTH_BOARD_MANIFEST_JSON := log/board/evidence_manifest.json
IMAGE_DEPTH_BOARD_DOCTOR_MD := log/board/evidence_doctor.md
IMAGE_DEPTH_REPEATED_DIR := log/board/repeated_contiguous
IMAGE_DEPTH_REPEATED_SUMMARY_MD := $(IMAGE_DEPTH_REPEATED_DIR)/summary.md
IMAGE_DEPTH_REPEATED_MANIFEST_JSON := $(IMAGE_DEPTH_REPEATED_DIR)/evidence_manifest.json
IMAGE_DEPTH_REPEATED_DOCTOR_MD := $(IMAGE_DEPTH_REPEATED_DIR)/evidence_doctor.md
IMAGE_DEPTH_DOWNSAMPLE_REPEATED_DIR := log/board/repeated_downsample
IMAGE_DEPTH_DOWNSAMPLE_SUMMARY_MD := $(IMAGE_DEPTH_DOWNSAMPLE_REPEATED_DIR)/summary.md
IMAGE_DEPTH_DOWNSAMPLE_MANIFEST_JSON := $(IMAGE_DEPTH_DOWNSAMPLE_REPEATED_DIR)/evidence_manifest.json
IMAGE_DEPTH_DOWNSAMPLE_DOCTOR_MD := $(IMAGE_DEPTH_DOWNSAMPLE_REPEATED_DIR)/evidence_doctor.md
IMAGE_DEPTH_PRODUCTION_REPEATED_DIR := log/board/repeated_production_public
IMAGE_DEPTH_PRODUCTION_SUMMARY_MD := $(IMAGE_DEPTH_PRODUCTION_REPEATED_DIR)/summary.md
IMAGE_DEPTH_PRODUCTION_MANIFEST_JSON := $(IMAGE_DEPTH_PRODUCTION_REPEATED_DIR)/evidence_manifest.json
IMAGE_DEPTH_PRODUCTION_DOCTOR_MD := $(IMAGE_DEPTH_PRODUCTION_REPEATED_DIR)/evidence_doctor.md
IMAGE_DEPTH_EVIDENCE_REGISTRY_JSON := log/evidence_registry.json
IMAGE_DEPTH_REPEATED_COMPARE_LOGS := $(wildcard $(IMAGE_DEPTH_REPEATED_DIR)/analyze_bench_compare_*.log)
IMAGE_DEPTH_REPEATED_COMPARE_ARGS := $(foreach f,$(IMAGE_DEPTH_REPEATED_COMPARE_LOGS),--compare-log $(f))
IMAGE_DEPTH_DOWNSAMPLE_COMPARE_LOGS := $(wildcard $(IMAGE_DEPTH_DOWNSAMPLE_REPEATED_DIR)/analyze_bench_compare_*.log)
IMAGE_DEPTH_DOWNSAMPLE_COMPARE_ARGS := $(foreach f,$(IMAGE_DEPTH_DOWNSAMPLE_COMPARE_LOGS),--compare-log $(f))
IMAGE_DEPTH_PRODUCTION_COMPARE_LOGS := $(wildcard $(IMAGE_DEPTH_PRODUCTION_REPEATED_DIR)/analyze_bench_compare_*.log)
IMAGE_DEPTH_PRODUCTION_COMPARE_ARGS := $(foreach f,$(IMAGE_DEPTH_PRODUCTION_COMPARE_LOGS),--compare-log $(f))
IMAGE_DEPTH_DOWNSAMPLE_REPEATED_RUNS ?= 5
IMAGE_DEPTH_PRODUCTION_REPEATED_RUNS ?= 5

include ../../mk/rvv-topic.mk

run_qemu_smoke: run_test_compare run_bench_rvv

run_board_depth_full:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter depth_full_640x480 --iterations 20 --warmup-iterations 3"

run_board_disparity_full:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter disparity_full_640x480 --iterations 20 --warmup-iterations 3"

run_board_depth_conversion_smoke:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter all --iterations 10 --warmup-iterations 2"

generate_board_evidence_manifest:
	$(PYTHON) script/generate_image_depth_evidence_manifest.py \
		--std-log log/board/run_bench_std.log \
		--rvv-log log/board/run_bench_rvv.log \
		--summary log/board/analyze_bench_compare.log \
		--output $(IMAGE_DEPTH_BOARD_MANIFEST_JSON)

run_board_evidence_doctor: generate_board_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(IMAGE_DEPTH_BOARD_MANIFEST_JSON) \
		--output $(IMAGE_DEPTH_BOARD_DOCTOR_MD) \
		--fail-on never

generate_board_repeated_summary:
	$(PYTHON) script/generate_image_depth_repeated_summary.py \
		--input-dir $(IMAGE_DEPTH_REPEATED_DIR) \
		--output $(IMAGE_DEPTH_REPEATED_SUMMARY_MD)

generate_board_repeated_evidence_manifest: generate_board_repeated_summary
	$(PYTHON) script/generate_image_depth_evidence_manifest.py \
		--std-log log/board/run_bench_std.log \
		--rvv-log log/board/run_bench_rvv.log \
		--summary $(IMAGE_DEPTH_REPEATED_SUMMARY_MD) \
		--output $(IMAGE_DEPTH_REPEATED_MANIFEST_JSON) \
		$(IMAGE_DEPTH_REPEATED_COMPARE_ARGS)

run_board_repeated_evidence_doctor: generate_board_repeated_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(IMAGE_DEPTH_REPEATED_MANIFEST_JSON) \
		--output $(IMAGE_DEPTH_REPEATED_DOCTOR_MD) \
		--fail-on never

collect_board_downsample_repeated:
	@mkdir -p $(IMAGE_DEPTH_DOWNSAMPLE_REPEATED_DIR)
	@for run_id in $$(seq 1 $(IMAGE_DEPTH_DOWNSAMPLE_REPEATED_RUNS)); do \
		echo "[BOARD] image_depth downsample repeated run $$run_id/$(IMAGE_DEPTH_DOWNSAMPLE_REPEATED_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter all --iterations 10 --warmup-iterations 2" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		cp log/board/analyze_bench_compare.log $(IMAGE_DEPTH_DOWNSAMPLE_REPEATED_DIR)/analyze_bench_compare_$$run_id.log; \
		cp log/board/run_bench_std.log $(IMAGE_DEPTH_DOWNSAMPLE_REPEATED_DIR)/run_bench_std_$$run_id.log; \
		cp log/board/run_bench_rvv.log $(IMAGE_DEPTH_DOWNSAMPLE_REPEATED_DIR)/run_bench_rvv_$$run_id.log; \
	done

generate_board_downsample_summary:
	$(PYTHON) script/generate_image_depth_repeated_summary.py \
		--input-dir $(IMAGE_DEPTH_DOWNSAMPLE_REPEATED_DIR) \
		--output $(IMAGE_DEPTH_DOWNSAMPLE_SUMMARY_MD) \
		--include-label depth_downsample_640x480_to_320x240 \
		--include-label disparity_downsample_640x480_to_320x240

generate_board_downsample_evidence_manifest: generate_board_downsample_summary
	$(PYTHON) script/generate_image_depth_evidence_manifest.py \
		--std-log log/board/run_bench_std.log \
		--rvv-log log/board/run_bench_rvv.log \
		--summary $(IMAGE_DEPTH_DOWNSAMPLE_SUMMARY_MD) \
		--output $(IMAGE_DEPTH_DOWNSAMPLE_MANIFEST_JSON) \
		--include-label depth_downsample_640x480_to_320x240 \
		--include-label disparity_downsample_640x480_to_320x240 \
		$(IMAGE_DEPTH_DOWNSAMPLE_COMPARE_ARGS)

run_board_downsample_evidence_doctor: generate_board_downsample_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(IMAGE_DEPTH_DOWNSAMPLE_MANIFEST_JSON) \
		--output $(IMAGE_DEPTH_DOWNSAMPLE_DOCTOR_MD) \
		--fail-on never

collect_board_production_repeated:
	@mkdir -p $(IMAGE_DEPTH_PRODUCTION_REPEATED_DIR)
	@for run_id in $$(seq 1 $(IMAGE_DEPTH_PRODUCTION_REPEATED_RUNS)); do \
		echo "[BOARD] image_depth production-public repeated run $$run_id/$(IMAGE_DEPTH_PRODUCTION_REPEATED_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter all --iterations 10 --warmup-iterations 2" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		cp log/board/analyze_bench_compare.log $(IMAGE_DEPTH_PRODUCTION_REPEATED_DIR)/analyze_bench_compare_$$run_id.log; \
		cp log/board/run_bench_std.log $(IMAGE_DEPTH_PRODUCTION_REPEATED_DIR)/run_bench_std_$$run_id.log; \
		cp log/board/run_bench_rvv.log $(IMAGE_DEPTH_PRODUCTION_REPEATED_DIR)/run_bench_rvv_$$run_id.log; \
	done

generate_board_production_summary:
	$(PYTHON) script/generate_image_depth_repeated_summary.py \
		--input-dir $(IMAGE_DEPTH_PRODUCTION_REPEATED_DIR) \
		--output $(IMAGE_DEPTH_PRODUCTION_SUMMARY_MD) \
		--include-label prod_depth_full_640x480 \
		--include-label prod_depth_full_padded_640x480 \
		--include-label prod_depth_downsample_640x480_to_320x240 \
		--include-label prod_disparity_full_640x480 \
		--include-label prod_disparity_downsample_640x480_to_320x240

generate_board_production_evidence_manifest: generate_board_production_summary
	$(PYTHON) script/generate_image_depth_evidence_manifest.py \
		--std-log log/board/run_bench_std.log \
		--rvv-log log/board/run_bench_rvv.log \
		--summary $(IMAGE_DEPTH_PRODUCTION_SUMMARY_MD) \
		--output $(IMAGE_DEPTH_PRODUCTION_MANIFEST_JSON) \
		--include-label prod_depth_full_640x480 \
		--include-label prod_depth_full_padded_640x480 \
		--include-label prod_depth_downsample_640x480_to_320x240 \
		--include-label prod_disparity_full_640x480 \
		--include-label prod_disparity_downsample_640x480_to_320x240 \
		$(IMAGE_DEPTH_PRODUCTION_COMPARE_ARGS)

run_board_production_evidence_doctor: generate_board_production_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(IMAGE_DEPTH_PRODUCTION_MANIFEST_JSON) \
		--output $(IMAGE_DEPTH_PRODUCTION_DOCTOR_MD) \
		--fail-on never

record_board_repeated_evidence_state: run_board_repeated_evidence_doctor
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(IMAGE_DEPTH_EVIDENCE_REGISTRY_JSON) \
		--summary $(IMAGE_DEPTH_REPEATED_SUMMARY_MD) \
		--manifest $(IMAGE_DEPTH_REPEATED_MANIFEST_JSON) \
		--doctor $(IMAGE_DEPTH_REPEATED_DOCTOR_MD) \
		--target run_board_repeated_evidence_doctor \
		--backend board \
		--run-label board-image-depth-contiguous-production-shaped-repeated-phase000 \
		--case-filter depth_full_640x480,depth_full_padded_640x480,disparity_full_640x480 \
		--evidence-role production_shaped_diagnostic \
		--doc-ref doc/phases/000-current-state-and-diagnostic-scaffold/result.zh.md \
		--doc-ref doc/image_depth-evaluation.zh.md \
		--freshness-state fresh

record_board_downsample_evidence_state: run_board_downsample_evidence_doctor
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(IMAGE_DEPTH_EVIDENCE_REGISTRY_JSON) \
		--summary $(IMAGE_DEPTH_DOWNSAMPLE_SUMMARY_MD) \
		--manifest $(IMAGE_DEPTH_DOWNSAMPLE_MANIFEST_JSON) \
		--doctor $(IMAGE_DEPTH_DOWNSAMPLE_DOCTOR_MD) \
		--target run_board_downsample_evidence_doctor \
		--backend board \
		--run-label board-image-depth-downsample-stride-load-repeated-phase020 \
		--case-filter depth_downsample_640x480_to_320x240,disparity_downsample_640x480_to_320x240 \
		--evidence-role production_shaped_diagnostic \
		--doc-ref doc/phases/020-stride-load-downsample-diagnostic/result.zh.md \
		--doc-ref doc/image_depth-evaluation.zh.md \
		--freshness-state fresh

record_board_production_evidence_state: run_board_production_evidence_doctor
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(IMAGE_DEPTH_EVIDENCE_REGISTRY_JSON) \
		--summary $(IMAGE_DEPTH_PRODUCTION_SUMMARY_MD) \
		--manifest $(IMAGE_DEPTH_PRODUCTION_MANIFEST_JSON) \
		--doctor $(IMAGE_DEPTH_PRODUCTION_DOCTOR_MD) \
		--target run_board_production_evidence_doctor \
		--backend board \
		--run-label board-image-depth-production-public-repeated-phase050 \
		--case-filter prod_depth_full_640x480,prod_depth_full_padded_640x480,prod_depth_downsample_640x480_to_320x240,prod_disparity_full_640x480,prod_disparity_downsample_640x480_to_320x240 \
		--evidence-role production-public \
		--doc-ref doc/phases/050-production-public-probe/result.zh.md \
		--doc-ref doc/image_depth-evaluation.zh.md \
		--doc-ref doc-rvv/io/image_depth-RVV.zh.md \
		--freshness-state fresh

check_evidence_freshness:
	$(PYTHON) ../../script/evidence_registry.py check \
		--registry $(IMAGE_DEPTH_EVIDENCE_REGISTRY_JSON) \
		--scan-glob 'test-rvv/io/image_depth/log/board/repeated_contiguous/summary.md' \
		--scan-glob 'test-rvv/io/image_depth/log/board/repeated_contiguous/evidence_manifest.json' \
		--scan-glob 'test-rvv/io/image_depth/log/board/repeated_contiguous/evidence_doctor.md' \
		--scan-glob 'test-rvv/io/image_depth/log/board/repeated_downsample/summary.md' \
		--scan-glob 'test-rvv/io/image_depth/log/board/repeated_downsample/evidence_manifest.json' \
		--scan-glob 'test-rvv/io/image_depth/log/board/repeated_downsample/evidence_doctor.md' \
		--scan-glob 'test-rvv/io/image_depth/log/board/repeated_production_public/summary.md' \
		--scan-glob 'test-rvv/io/image_depth/log/board/repeated_production_public/evidence_manifest.json' \
		--scan-glob 'test-rvv/io/image_depth/log/board/repeated_production_public/evidence_doctor.md' \
		--doc test-rvv/io/image_depth/doc/phases/000-current-state-and-diagnostic-scaffold/result.zh.md \
		--doc test-rvv/io/image_depth/doc/phases/020-stride-load-downsample-diagnostic/result.zh.md \
		--doc test-rvv/io/image_depth/doc/phases/050-production-public-probe/result.zh.md \
		--doc test-rvv/io/image_depth/doc/image_depth-evaluation.zh.md \
		--doc doc-rvv/io/image_depth-RVV.zh.md \
		--require-doc-ref \
		--fail-on any

evidence_status: check_evidence_freshness

.PHONY: run_qemu_smoke run_board_depth_full run_board_disparity_full run_board_depth_conversion_smoke generate_board_evidence_manifest run_board_evidence_doctor generate_board_repeated_summary generate_board_repeated_evidence_manifest run_board_repeated_evidence_doctor collect_board_downsample_repeated generate_board_downsample_summary generate_board_downsample_evidence_manifest run_board_downsample_evidence_doctor collect_board_production_repeated generate_board_production_summary generate_board_production_evidence_manifest run_board_production_evidence_doctor record_board_repeated_evidence_state record_board_downsample_evidence_state record_board_production_evidence_state check_evidence_freshness evidence_status
