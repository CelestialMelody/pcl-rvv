# =============================================================================
#  PCL io/openni2_grabber RVV diagnostic Makefile
#
#  本 topic 首阶段使用 production-shaped diagnostic（生产形态诊断）：
#  测试专用 helper 复刻 OpenNI2Grabber frame-to-cloud（帧到点云）
#  转换循环。QEMU 只用于 correctness（正确性）和日志形状；性能结论
#  必须来自板卡或目标硬件。
# =============================================================================

TOPIC  := openni2_grabber
MODULE := io

FOCUS_DIR      := pcl/io/src/openni2_grabber.cpp
FILTER_REPORT  := log/filtered_openni2_grabber.log
ANALYZE_REPORT := log/analyze_openni2_grabber.log

TARGET_TEST     := test_openni2_grabber
TARGET_TEST_STD := test_openni2_grabber_std
TARGET_TEST_RVV := test_openni2_grabber_rvv
SRCS_TEST       := src/test_openni2_grabber.cpp \
	src/openni2_grabber_production_detail_test.cpp \
	src/openni2_grabber_test_hook.cpp

TARGET_BENCH     := bench_openni2_grabber
TARGET_BENCH_STD := bench_openni2_grabber_std
TARGET_BENCH_RVV := bench_openni2_grabber_rvv
SRCS_BENCH       := src/bench_openni2_grabber.cpp \
	src/openni2_grabber_test_hook.cpp

LIBS_TEST := -lpcl_common -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lpcl_common -lm

OPENNI2_GRABBER_REPEATED_DIR := log/board/repeated_diagnostic
OPENNI2_GRABBER_REPEATED_SUMMARY_MD := $(OPENNI2_GRABBER_REPEATED_DIR)/summary.md
OPENNI2_GRABBER_REPEATED_MANIFEST_JSON := $(OPENNI2_GRABBER_REPEATED_DIR)/evidence_manifest.json
OPENNI2_GRABBER_REPEATED_DOCTOR_MD := $(OPENNI2_GRABBER_REPEATED_DIR)/evidence_doctor.md
OPENNI2_GRABBER_REPEATED_DOCTOR_JSON := $(OPENNI2_GRABBER_REPEATED_DIR)/evidence_doctor.json
OPENNI2_GRABBER_EVIDENCE_REGISTRY_JSON := log/evidence_registry.json
OPENNI2_GRABBER_PRODUCTION_REPEATED_DIR := log/board/repeated_production_depth
OPENNI2_GRABBER_PRODUCTION_SUMMARY_MD := $(OPENNI2_GRABBER_PRODUCTION_REPEATED_DIR)/summary.md
OPENNI2_GRABBER_PRODUCTION_MANIFEST_JSON := $(OPENNI2_GRABBER_PRODUCTION_REPEATED_DIR)/evidence_manifest.json
OPENNI2_GRABBER_PRODUCTION_DOCTOR_MD := $(OPENNI2_GRABBER_PRODUCTION_REPEATED_DIR)/evidence_doctor.md
OPENNI2_GRABBER_PRODUCTION_DOCTOR_JSON := $(OPENNI2_GRABBER_PRODUCTION_REPEATED_DIR)/evidence_doctor.json
OPENNI2_GRABBER_PRODUCTION_COMPARE_LOGS := $(wildcard $(OPENNI2_GRABBER_PRODUCTION_REPEATED_DIR)/analyze_bench_compare_*.log)
OPENNI2_GRABBER_PRODUCTION_COMPARE_ARGS := $(foreach f,$(OPENNI2_GRABBER_PRODUCTION_COMPARE_LOGS),--compare-log $(f))
OPENNI2_GRABBER_REPEATED_RUNS ?= 5
OPENNI2_GRABBER_PRODUCTION_REPEATED_RUNS ?= 5
OPENNI2_GRABBER_RVV_ASM := build/asm/riscv/bench_openni2_grabber_rvv.asm
OPENNI2_GRABBER_RVV_FULL_ASM := build/asm/riscv/bench_openni2_grabber_rvv.full.asm

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

include ../../mk/rvv-topic.mk

run_qemu_smoke: run_test_compare run_bench_rvv

run_board_openni2_grabber_smoke:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter all --iterations 10 --warmup-iterations 2"

run_board_openni2_grabber_production_smoke:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter prod_xyz_depth_full_640x480 --iterations 10 --warmup-iterations 2"

collect_board_openni2_grabber_repeated:
	@mkdir -p $(OPENNI2_GRABBER_REPEATED_DIR)
	@for run_id in $$(seq 1 $(OPENNI2_GRABBER_REPEATED_RUNS)); do \
		echo "[BOARD] openni2_grabber diagnostic repeated run $$run_id/$(OPENNI2_GRABBER_REPEATED_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter all --iterations 10 --warmup-iterations 2" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		cp log/board/analyze_bench_compare.log $(OPENNI2_GRABBER_REPEATED_DIR)/analyze_bench_compare_$$run_id.log; \
		cp log/board/run_bench_std.log $(OPENNI2_GRABBER_REPEATED_DIR)/run_bench_std_$$run_id.log; \
		cp log/board/run_bench_rvv.log $(OPENNI2_GRABBER_REPEATED_DIR)/run_bench_rvv_$$run_id.log; \
	done

generate_board_openni2_grabber_repeated_summary:
	@set -- $(OPENNI2_GRABBER_REPEATED_DIR)/analyze_bench_compare_*.log; \
	if [ "$$1" = "$(OPENNI2_GRABBER_REPEATED_DIR)/analyze_bench_compare_*.log" ]; then \
		echo "No repeated board compare logs found under $(OPENNI2_GRABBER_REPEATED_DIR)" >&2; \
		exit 1; \
	fi; \
	$(PYTHON) ../../script/analyze_bench_repeated.py "$$@" > $(OPENNI2_GRABBER_REPEATED_SUMMARY_MD)

generate_board_openni2_grabber_repeated_evidence_manifest: generate_board_openni2_grabber_repeated_summary
	@set -- $(OPENNI2_GRABBER_REPEATED_DIR)/analyze_bench_compare_*.log; \
	if [ "$$1" = "$(OPENNI2_GRABBER_REPEATED_DIR)/analyze_bench_compare_*.log" ]; then \
		echo "No repeated board compare logs found under $(OPENNI2_GRABBER_REPEATED_DIR)" >&2; \
		exit 1; \
	fi; \
	$(PYTHON) script/generate_openni2_grabber_evidence_manifest.py \
		--std-log log/board/run_bench_std.log \
		--rvv-log log/board/run_bench_rvv.log \
		--summary $(OPENNI2_GRABBER_REPEATED_SUMMARY_MD) \
		--output $(OPENNI2_GRABBER_REPEATED_MANIFEST_JSON) \
		$$(printf ' --compare-log %s' "$$@")

run_board_openni2_grabber_evidence_doctor: generate_board_openni2_grabber_repeated_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(OPENNI2_GRABBER_REPEATED_MANIFEST_JSON) \
		--output $(OPENNI2_GRABBER_REPEATED_DOCTOR_MD) \
		--json-output $(OPENNI2_GRABBER_REPEATED_DOCTOR_JSON) \
		--fail-on never

record_board_openni2_grabber_repeated_evidence_state: run_board_openni2_grabber_evidence_doctor
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(OPENNI2_GRABBER_EVIDENCE_REGISTRY_JSON) \
		--summary $(OPENNI2_GRABBER_REPEATED_SUMMARY_MD) \
		--manifest $(OPENNI2_GRABBER_REPEATED_MANIFEST_JSON) \
		--doctor $(OPENNI2_GRABBER_REPEATED_DOCTOR_MD) \
		--target run_board_openni2_grabber_evidence_doctor \
		--backend board \
		--run-label board-openni2-grabber-production-shaped-repeated-phase000 \
		--case-filter xyz_depth_full_640x480,xyzrgba_depth_mismatch_320_to_640,rgba_overlay_full_640x480,xyzi_depth_ir_full_640x480 \
		--evidence-role production_shaped_diagnostic \
		--doc-ref doc/phases/000-current-state-and-diagnostic-scaffold/result.zh.md \
		--doc-ref doc/openni2_grabber-evaluation.zh.md \
		--freshness-state fresh

collect_board_openni2_grabber_production_repeated:
	@mkdir -p $(OPENNI2_GRABBER_PRODUCTION_REPEATED_DIR)
	@for run_id in $$(seq 1 $(OPENNI2_GRABBER_PRODUCTION_REPEATED_RUNS)); do \
		echo "[BOARD] openni2_grabber production-detail repeated run $$run_id/$(OPENNI2_GRABBER_PRODUCTION_REPEATED_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter prod_xyz_depth_full_640x480 --iterations 10 --warmup-iterations 2" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		cp log/board/analyze_bench_compare.log $(OPENNI2_GRABBER_PRODUCTION_REPEATED_DIR)/analyze_bench_compare_$$run_id.log; \
		cp log/board/run_bench_std.log $(OPENNI2_GRABBER_PRODUCTION_REPEATED_DIR)/run_bench_std_$$run_id.log; \
		cp log/board/run_bench_rvv.log $(OPENNI2_GRABBER_PRODUCTION_REPEATED_DIR)/run_bench_rvv_$$run_id.log; \
	done

generate_board_openni2_grabber_production_summary:
	@set -- $(OPENNI2_GRABBER_PRODUCTION_REPEATED_DIR)/analyze_bench_compare_*.log; \
	if [ "$$1" = "$(OPENNI2_GRABBER_PRODUCTION_REPEATED_DIR)/analyze_bench_compare_*.log" ]; then \
		echo "No repeated production board compare logs found under $(OPENNI2_GRABBER_PRODUCTION_REPEATED_DIR)" >&2; \
		exit 1; \
	fi; \
	$(PYTHON) ../../script/analyze_bench_repeated.py "$$@" > $(OPENNI2_GRABBER_PRODUCTION_SUMMARY_MD)

generate_board_openni2_grabber_production_evidence_manifest: generate_board_openni2_grabber_production_summary
	$(PYTHON) script/generate_openni2_grabber_evidence_manifest.py \
		--std-log log/board/run_bench_std.log \
		--rvv-log log/board/run_bench_rvv.log \
		--summary $(OPENNI2_GRABBER_PRODUCTION_SUMMARY_MD) \
		--output $(OPENNI2_GRABBER_PRODUCTION_MANIFEST_JSON) \
		--include-label prod_xyz_depth_full_640x480 \
		$(OPENNI2_GRABBER_PRODUCTION_COMPARE_ARGS)

run_board_openni2_grabber_production_evidence_doctor: generate_board_openni2_grabber_production_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(OPENNI2_GRABBER_PRODUCTION_MANIFEST_JSON) \
		--output $(OPENNI2_GRABBER_PRODUCTION_DOCTOR_MD) \
		--json-output $(OPENNI2_GRABBER_PRODUCTION_DOCTOR_JSON) \
		--fail-on never

record_board_openni2_grabber_production_evidence_state: run_board_openni2_grabber_production_evidence_doctor
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(OPENNI2_GRABBER_EVIDENCE_REGISTRY_JSON) \
		--summary $(OPENNI2_GRABBER_PRODUCTION_SUMMARY_MD) \
		--manifest $(OPENNI2_GRABBER_PRODUCTION_MANIFEST_JSON) \
		--doctor $(OPENNI2_GRABBER_PRODUCTION_DOCTOR_MD) \
		--target run_board_openni2_grabber_production_evidence_doctor \
		--backend board \
		--run-label board-openni2-grabber-production-detail-repeated-phase020 \
		--case-filter prod_xyz_depth_full_640x480 \
		--evidence-role production_detail \
		--doc-ref doc/phases/020-production-depth-connection/result.zh.md \
		--doc-ref doc/openni2_grabber-evaluation.zh.md \
		--freshness-state fresh

check_openni2_grabber_rvv_asm: dump_bench_rvv
	@grep -Eq 'vle16\.v' $(OPENNI2_GRABBER_RVV_ASM)
	@grep -Eq 'vfcvt\.f\.xu\.v' $(OPENNI2_GRABBER_RVV_ASM)
	@grep -Eq 'vfmul' $(OPENNI2_GRABBER_RVV_ASM)
	@grep -Eq 'vmseq' $(OPENNI2_GRABBER_RVV_ASM)
	@grep -Eq 'vsse32\.v' $(OPENNI2_GRABBER_RVV_ASM)
	@grep -Eq 'vlseg3e8\.v' $(OPENNI2_GRABBER_RVV_ASM)
	@echo "[ASM] openni2_grabber RVV diagnostic instructions found in $(OPENNI2_GRABBER_RVV_ASM)"

check_openni2_grabber_production_rvv_asm: dump_bench_rvv
	@grep -Eq 'pcl_rvv_openni2_grabber_fill_xyz_candidate_test_hook' $(OPENNI2_GRABBER_RVV_FULL_ASM)
	@grep -Eq 'vle16\.v' $(OPENNI2_GRABBER_RVV_ASM)
	@grep -Eq 'vfcvt\.f\.xu\.v' $(OPENNI2_GRABBER_RVV_ASM)
	@grep -Eq 'vfmul' $(OPENNI2_GRABBER_RVV_ASM)
	@grep -Eq 'vmseq' $(OPENNI2_GRABBER_RVV_ASM)
	@grep -Eq 'vsse32\.v' $(OPENNI2_GRABBER_RVV_ASM)
	@echo "[ASM] openni2_grabber production-detail RVV instructions found in $(OPENNI2_GRABBER_RVV_ASM)"

check_openni2_grabber_evidence_freshness:
	$(PYTHON) ../../script/evidence_registry.py check \
		--registry $(OPENNI2_GRABBER_EVIDENCE_REGISTRY_JSON) \
		--scan-glob 'test-rvv/io/openni2_grabber/log/board/repeated_diagnostic/summary.md' \
		--scan-glob 'test-rvv/io/openni2_grabber/log/board/repeated_diagnostic/evidence_manifest.json' \
		--scan-glob 'test-rvv/io/openni2_grabber/log/board/repeated_diagnostic/evidence_doctor.md' \
		--doc test-rvv/io/openni2_grabber/doc/phases/000-current-state-and-diagnostic-scaffold/result.zh.md \
		--doc test-rvv/io/openni2_grabber/doc/openni2_grabber-evaluation.zh.md \
		--require-doc-ref \
		--fail-on any

check_openni2_grabber_production_evidence_freshness:
	$(PYTHON) ../../script/evidence_registry.py check \
		--registry $(OPENNI2_GRABBER_EVIDENCE_REGISTRY_JSON) \
		--scan-glob 'test-rvv/io/openni2_grabber/log/board/repeated_production_depth/summary.md' \
		--scan-glob 'test-rvv/io/openni2_grabber/log/board/repeated_production_depth/evidence_manifest.json' \
		--scan-glob 'test-rvv/io/openni2_grabber/log/board/repeated_production_depth/evidence_doctor.md' \
		--doc test-rvv/io/openni2_grabber/doc/phases/020-production-depth-connection/result.zh.md \
		--doc test-rvv/io/openni2_grabber/doc/openni2_grabber-evaluation.zh.md \
		--require-doc-ref \
		--fail-on any

evidence_status: check_openni2_grabber_evidence_freshness

.PHONY: run_qemu_smoke run_board_openni2_grabber_smoke run_board_openni2_grabber_production_smoke collect_board_openni2_grabber_repeated generate_board_openni2_grabber_repeated_summary generate_board_openni2_grabber_repeated_evidence_manifest run_board_openni2_grabber_evidence_doctor record_board_openni2_grabber_repeated_evidence_state collect_board_openni2_grabber_production_repeated generate_board_openni2_grabber_production_summary generate_board_openni2_grabber_production_evidence_manifest run_board_openni2_grabber_production_evidence_doctor record_board_openni2_grabber_production_evidence_state check_openni2_grabber_rvv_asm check_openni2_grabber_production_rvv_asm check_openni2_grabber_evidence_freshness check_openni2_grabber_production_evidence_freshness evidence_status
