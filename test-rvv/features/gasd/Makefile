# =============================================================================
# PCL features/gasd RVV diagnostic Makefile
#
# 先评估 GASD（Globally Aligned Spatial Distribution，全球对齐空间分布）
# 里 shared fixed-grid copy（共享固定网格直拷贝）是否值得继续。QEMU 只用于
# correctness（正确性）和日志形状；性能结论必须来自板卡或目标硬件。
# =============================================================================

TOPIC  := gasd
MODULE := features

FOCUS_DIR      := pcl/features/include/pcl/features/impl/gasd.hpp
FILTER_REPORT  := log/filtered_gasd.log
ANALYZE_REPORT := log/analyze_gasd.log

TARGET_TEST     := test_gasd
TARGET_TEST_STD := test_gasd_std
TARGET_TEST_RVV := test_gasd_rvv
SRCS_TEST       := src/test_gasd.cpp

TARGET_BENCH     := bench_gasd
TARGET_BENCH_STD := bench_gasd_std
TARGET_BENCH_RVV := bench_gasd_rvv
SRCS_BENCH       := src/bench_gasd.cpp

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/features/include \
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

LIBS_TEST  := -lpcl_features -lpcl_common -llz4 -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lpcl_features -lpcl_common -llz4 -lm

REMOTE_DIR ?= /root/pcl-test/features/gasd

REPEATED_BOARD_RUNS ?= 5
REPEATED_BOARD_TAG ?= phase000_shape_copy
REPEATED_BOARD_TITLE ?= GASD fixed-grid copy diagnostic repeated board summary
REPEATED_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated_$(REPEATED_BOARD_TAG)
REPEATED_BOARD_REMOTE_TAG ?= phase000_shape_copy
REPEATED_BOARD_RUN_LABEL ?= gasd_phase000_shape_copy_repeated
REPEATED_BOARD_CASE_NAME ?= candidate_shape_copy_rvv
EVIDENCE_MANIFEST_REPEATED ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_manifest.json
EVIDENCE_DOCTOR_REPEATED_MD ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.md
EVIDENCE_DOCTOR_REPEATED_JSON ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.json
GASD_REPEATED_BENCH_ARGS ?= --case-filter $(REPEATED_BOARD_CASE_NAME) --shape-half-grid 6 --hists-size 16 --repeat 256 --iterations 10 --warmup 2

include ../../mk/rvv-topic.mk

dump_test_rvv: | $(ASM_DIR)/$(ARCH)
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_TEST=$(TARGET_TEST_RVV) $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_RVV)
	$(OBJDUMP) -d -C $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_RVV) > $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).full.asm
	grep -E $(VEC_REGEX_STR) $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).full.asm > $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm || true
	@echo "[DONE] GASD RVV test asm dump: $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm"

run_qemu_smoke: run_test_compare

board_repeated:
	@set -eu; \
	for i in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
			BENCH_ARGS="$(GASD_REPEATED_BENCH_ARGS)" \
			OUTPUT_DIR_BOARD="$(REPEATED_BOARD_OUTPUT_DIR)/$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated_$(REPEATED_BOARD_REMOTE_TAG)/$$run"; \
	done

evidence_manifest_repeated:
	python3 script/generate_gasd_evidence_manifest.py \
		--summary-dir "$(REPEATED_BOARD_OUTPUT_DIR)" \
		--output "$(EVIDENCE_MANIFEST_REPEATED)" \
		--summary-output "$(REPEATED_BOARD_OUTPUT_DIR)/summary.md" \
		--title "$(REPEATED_BOARD_TITLE)" \
		--run-label "$(REPEATED_BOARD_RUN_LABEL)" \
		--case-name "$(REPEATED_BOARD_CASE_NAME)" \
		--expected-runs "$(REPEATED_BOARD_RUNS)" \
		--asm "$(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm"

evidence_doctor_repeated: evidence_manifest_repeated
	python3 ../../script/evidence_doctor.py \
		--manifest "$(EVIDENCE_MANIFEST_REPEATED)" \
		--output "$(EVIDENCE_DOCTOR_REPEATED_MD)" \
		--json-output "$(EVIDENCE_DOCTOR_REPEATED_JSON)" \
		--fail-on never

.PHONY: dump_test_rvv run_qemu_smoke board_repeated evidence_manifest_repeated evidence_doctor_repeated
