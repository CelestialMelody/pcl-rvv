# =============================================================================
#  PCL io/color_coding RVV component diagnostic Makefile
#
#  本 topic 先建立 component diagnostic（组件诊断）：测试专用 helper
#  隔离 ColorCoding 的 color average / diff / decode / default-color 语义。
#  QEMU 只用于 correctness（正确性）和日志形状；性能结论必须来自板卡。
# =============================================================================

TOPIC  := color_coding
MODULE := io

FOCUS_DIR      := pcl/io/include/pcl/compression/color_coding.h
FILTER_REPORT  := log/filtered_color_coding.log
ANALYZE_REPORT := log/analyze_color_coding.log

TARGET_TEST     := test_color_coding
TARGET_TEST_STD := test_color_coding_std
TARGET_TEST_RVV := test_color_coding_rvv
SRCS_TEST       := src/test_color_coding.cpp

TARGET_BENCH     := bench_color_coding
TARGET_BENCH_STD := bench_color_coding_std
TARGET_BENCH_RVV := bench_color_coding_rvv
SRCS_BENCH       := src/bench_color_coding.cpp

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/io/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include

LIBS_TEST  := -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lm

include ../../mk/rvv-topic.mk

COLOR_CODING_REPEAT_RUNS ?= 5
COLOR_CODING_REPEAT_DIR := $(OUTPUT_DIR_BOARD)/component_repeat_5
COLOR_CODING_REPEAT_SUMMARY := $(COLOR_CODING_REPEAT_DIR)/summary.md
COLOR_CODING_EVIDENCE_MANIFEST := $(COLOR_CODING_REPEAT_DIR)/evidence_manifest.json
COLOR_CODING_EVIDENCE_DOCTOR := $(COLOR_CODING_REPEAT_DIR)/evidence_doctor.md
COLOR_CODING_EVIDENCE_REGISTRY := log/evidence_registry.json
COLOR_CODING_PRODUCTION_REPEAT_DIR := $(OUTPUT_DIR_BOARD)/production_repeat_5
COLOR_CODING_PRODUCTION_REPEAT_SUMMARY := $(COLOR_CODING_PRODUCTION_REPEAT_DIR)/summary.md
COLOR_CODING_PRODUCTION_EVIDENCE_MANIFEST := $(COLOR_CODING_PRODUCTION_REPEAT_DIR)/evidence_manifest.json
COLOR_CODING_PRODUCTION_EVIDENCE_DOCTOR := $(COLOR_CODING_PRODUCTION_REPEAT_DIR)/evidence_doctor.md

run_qemu_smoke: run_test_compare run_bench_rvv

run_board_color_coding_smoke:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter all --iterations 10 --warmup-iterations 2"

collect_board_color_coding_repeated:
	@mkdir -p $(COLOR_CODING_REPEAT_DIR)
	@set -e; for run_id in $$(seq 1 $(COLOR_CODING_REPEAT_RUNS)); do \
		echo "[BOARD] color_coding component repeated run $$run_id/$(COLOR_CODING_REPEAT_RUNS)"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter all --iterations 20 --warmup-iterations 3" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		mkdir -p $(COLOR_CODING_REPEAT_DIR)/run$$run_id; \
		cp $(OUTPUT_DIR_BOARD)/analyze_bench_compare.log $(COLOR_CODING_REPEAT_DIR)/run$$run_id/analyze_bench_compare.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_std.log $(COLOR_CODING_REPEAT_DIR)/run$$run_id/run_bench_std.log; \
		cp $(OUTPUT_DIR_BOARD)/run_bench_rvv.log $(COLOR_CODING_REPEAT_DIR)/run$$run_id/run_bench_rvv.log; \
	done

run_board_color_coding_repeated: collect_board_color_coding_repeated
	@$(PYTHON_RUN) script/generate_color_coding_repeat_summary.py \
		--repeat-dir $(COLOR_CODING_REPEAT_DIR) \
		--output $(COLOR_CODING_REPEAT_SUMMARY) \
		--title "color_coding component repeated board summary" \
		--evidence-role mixed_component_production_shaped_and_decode_shape_diagnostic
	@$(PYTHON_RUN) script/generate_color_coding_evidence_manifest.py \
		--repeat-dir $(COLOR_CODING_REPEAT_DIR) \
		--summary-log $(COLOR_CODING_REPEAT_SUMMARY) \
		--output $(COLOR_CODING_EVIDENCE_MANIFEST)
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(COLOR_CODING_EVIDENCE_MANIFEST) \
		--output $(COLOR_CODING_EVIDENCE_DOCTOR) \
		--fail-on never
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(COLOR_CODING_EVIDENCE_REGISTRY) \
		--summary $(COLOR_CODING_REPEAT_SUMMARY) \
		--manifest $(COLOR_CODING_EVIDENCE_MANIFEST) \
		--doctor $(COLOR_CODING_EVIDENCE_DOCTOR) \
		--target run_board_color_coding_repeated \
		--backend board \
		--run-label board-color-coding-component-repeat-phase050 \
		--case-filter all \
		--evidence-role mixed_component_production_shaped_and_decode_shape_diagnostic \
		--doc-ref doc/phases/050-decode-implementation-shape-audit/result.zh.md \
		--doc-ref doc/color_coding-evaluation.zh.md \
		--freshness-state fresh

collect_board_color_coding_production_repeated:
	@echo "[SKIP] color_coding has no current production RVV case after phase 080 full rollback."
	@echo "[SKIP] Historical phase 070 production evidence remains in $(COLOR_CODING_PRODUCTION_REPEAT_DIR)."

run_board_color_coding_production_repeated: collect_board_color_coding_production_repeated
	@echo "[SKIP] No production summary generated; phase 070 evidence is the no-adoption decision input."

check_evidence_freshness:
	@$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry $(COLOR_CODING_EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/io/color_coding/log/board/component_repeat_5/summary.md' \
		--scan-glob 'test-rvv/io/color_coding/log/board/component_repeat_5/evidence_manifest.json' \
		--scan-glob 'test-rvv/io/color_coding/log/board/component_repeat_5/evidence_doctor.md' \
		--scan-glob 'test-rvv/io/color_coding/log/board/production_repeat_5/summary.md' \
		--scan-glob 'test-rvv/io/color_coding/log/board/production_repeat_5/evidence_manifest.json' \
		--scan-glob 'test-rvv/io/color_coding/log/board/production_repeat_5/evidence_doctor.md' \
		--doc doc/phases/050-decode-implementation-shape-audit/result.zh.md \
		--doc doc/phases/060-pi2-production-patch-and-direct-evidence/result.zh.md \
		--doc doc/phases/070-pi5-partial-rollback-default-only/result.zh.md \
		--doc doc/color_coding-evaluation.zh.md \
		--fail-on never

.PHONY: run_qemu_smoke run_board_color_coding_smoke collect_board_color_coding_repeated run_board_color_coding_repeated collect_board_color_coding_production_repeated run_board_color_coding_production_repeated check_evidence_freshness
