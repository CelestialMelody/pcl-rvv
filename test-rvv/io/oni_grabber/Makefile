# =============================================================================
#  PCL io/oni_grabber RVV diagnostic Makefile
#
#  本 topic 首阶段只做 production-shaped diagnostic（生产形态诊断）：
#  测试专用 helper 复刻 ONIGrabber depth-only frame-to-cloud（帧到点云）
#  转换循环。QEMU 只用于 correctness（正确性）和日志形状；性能结论
#  必须来自板卡或目标硬件。
# =============================================================================

TOPIC  := oni_grabber
MODULE := io

FOCUS_DIR      := pcl/io/src/oni_grabber.cpp
FILTER_REPORT  := log/filtered_oni_grabber.log
ANALYZE_REPORT := log/analyze_oni_grabber.log

TARGET_TEST     := test_oni_grabber
TARGET_TEST_STD := test_oni_grabber_std
TARGET_TEST_RVV := test_oni_grabber_rvv
SRCS_TEST       := src/test_oni_grabber.cpp \
	src/oni_grabber_production_detail_test.cpp \
	src/oni_grabber_test_hook.cpp

TARGET_BENCH     := bench_oni_grabber
TARGET_BENCH_STD := bench_oni_grabber_std
TARGET_BENCH_RVV := bench_oni_grabber_rvv
SRCS_BENCH       := src/bench_oni_grabber.cpp \
	src/oni_grabber_test_hook.cpp

LIBS_TEST := -lpcl_common -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lpcl_common -lm

ONI_GRABBER_REPEATED_DIR := log/board/repeated_diagnostic
ONI_GRABBER_REPEATED_SUMMARY_MD := $(ONI_GRABBER_REPEATED_DIR)/summary.md
ONI_GRABBER_PRODUCTION_REPEATED_DIR := log/board/repeated_production_depth
ONI_GRABBER_PRODUCTION_SUMMARY_MD := $(ONI_GRABBER_PRODUCTION_REPEATED_DIR)/summary.md
ONI_GRABBER_PRODUCTION_DOCTOR_MD := $(ONI_GRABBER_PRODUCTION_REPEATED_DIR)/evidence_doctor.md
ONI_GRABBER_PRODUCTION_DOCTOR_JSON := $(ONI_GRABBER_PRODUCTION_REPEATED_DIR)/evidence_doctor.json
ONI_GRABBER_RVV_ASM := build/asm/riscv/bench_oni_grabber_rvv.asm
ONI_GRABBER_RVV_FULL_ASM := build/asm/riscv/bench_oni_grabber_rvv.full.asm

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

run_board_oni_grabber_smoke:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter xyz_depth_full_640x480 --iterations 10 --warmup-iterations 2"

run_board_oni_grabber_production_smoke:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter prod_xyz_depth_full_640x480 --iterations 10 --warmup-iterations 2"

collect_board_oni_grabber_repeated:
	@mkdir -p $(ONI_GRABBER_REPEATED_DIR)
	@for run_id in $$(seq 1 5); do \
		echo "[BOARD] oni_grabber diagnostic repeated run $$run_id/5"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter xyz_depth_full_640x480 --iterations 10 --warmup-iterations 2" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		cp log/board/analyze_bench_compare.log $(ONI_GRABBER_REPEATED_DIR)/analyze_bench_compare_$$run_id.log; \
		cp log/board/run_bench_std.log $(ONI_GRABBER_REPEATED_DIR)/run_bench_std_$$run_id.log; \
		cp log/board/run_bench_rvv.log $(ONI_GRABBER_REPEATED_DIR)/run_bench_rvv_$$run_id.log; \
	done

generate_board_oni_grabber_repeated_summary:
	@set -- $(ONI_GRABBER_REPEATED_DIR)/analyze_bench_compare_*.log; \
	if [ "$$1" = "$(ONI_GRABBER_REPEATED_DIR)/analyze_bench_compare_*.log" ]; then \
		echo "No repeated board compare logs found under $(ONI_GRABBER_REPEATED_DIR)" >&2; \
		exit 1; \
	fi; \
	$(PYTHON) ../../script/analyze_bench_repeated.py "$$@" > $(ONI_GRABBER_REPEATED_SUMMARY_MD)

collect_board_oni_grabber_production_repeated:
	@mkdir -p $(ONI_GRABBER_PRODUCTION_REPEATED_DIR)
	@for run_id in $$(seq 1 5); do \
		echo "[BOARD] oni_grabber production-detail repeated run $$run_id/5"; \
		$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter prod_xyz_depth_full_640x480 --iterations 10 --warmup-iterations 2" || exit $$?; \
		$(MAKE) -C $(CURDIR) fetch_board_logs || exit $$?; \
		cp log/board/analyze_bench_compare.log $(ONI_GRABBER_PRODUCTION_REPEATED_DIR)/analyze_bench_compare_$$run_id.log; \
		cp log/board/run_bench_std.log $(ONI_GRABBER_PRODUCTION_REPEATED_DIR)/run_bench_std_$$run_id.log; \
		cp log/board/run_bench_rvv.log $(ONI_GRABBER_PRODUCTION_REPEATED_DIR)/run_bench_rvv_$$run_id.log; \
	done

generate_board_oni_grabber_production_summary:
	@set -- $(ONI_GRABBER_PRODUCTION_REPEATED_DIR)/analyze_bench_compare_*.log; \
	if [ "$$1" = "$(ONI_GRABBER_PRODUCTION_REPEATED_DIR)/analyze_bench_compare_*.log" ]; then \
		echo "No repeated production board compare logs found under $(ONI_GRABBER_PRODUCTION_REPEATED_DIR)" >&2; \
		exit 1; \
	fi; \
	$(PYTHON) ../../script/analyze_bench_repeated.py "$$@" > $(ONI_GRABBER_PRODUCTION_SUMMARY_MD)

run_board_oni_grabber_production_evidence_doctor: generate_board_oni_grabber_production_summary
	$(PYTHON) ../../script/evidence_doctor.py \
		--summary-md $(ONI_GRABBER_PRODUCTION_SUMMARY_MD) \
		--output $(ONI_GRABBER_PRODUCTION_DOCTOR_MD) \
		--json-output $(ONI_GRABBER_PRODUCTION_DOCTOR_JSON) \
		--fail-on never

check_oni_grabber_rvv_asm: dump_bench_rvv
	@grep -Eq 'vle16\.v' $(ONI_GRABBER_RVV_ASM)
	@grep -Eq 'vfcvt\.f\.xu\.v' $(ONI_GRABBER_RVV_ASM)
	@grep -Eq 'vfmul' $(ONI_GRABBER_RVV_ASM)
	@grep -Eq 'vmseq' $(ONI_GRABBER_RVV_ASM)
	@grep -Eq 'vsse32\.v' $(ONI_GRABBER_RVV_ASM)
	@echo "[ASM] oni_grabber RVV diagnostic instructions found in $(ONI_GRABBER_RVV_ASM)"

check_oni_grabber_production_rvv_asm: dump_bench_rvv
	@grep -Eq 'pcl_rvv_oni_grabber_fill_xyz_candidate_test_hook' $(ONI_GRABBER_RVV_FULL_ASM)
	@grep -Eq 'vle16\.v' $(ONI_GRABBER_RVV_ASM)
	@grep -Eq 'vfcvt\.f\.xu\.v' $(ONI_GRABBER_RVV_ASM)
	@grep -Eq 'vfmul' $(ONI_GRABBER_RVV_ASM)
	@grep -Eq 'vmseq' $(ONI_GRABBER_RVV_ASM)
	@grep -Eq 'vsse32\.v' $(ONI_GRABBER_RVV_ASM)
	@echo "[ASM] oni_grabber production-detail RVV instructions found in $(ONI_GRABBER_RVV_ASM)"

.PHONY: run_qemu_smoke run_board_oni_grabber_smoke run_board_oni_grabber_production_smoke collect_board_oni_grabber_repeated generate_board_oni_grabber_repeated_summary collect_board_oni_grabber_production_repeated generate_board_oni_grabber_production_summary run_board_oni_grabber_production_evidence_doctor check_oni_grabber_rvv_asm check_oni_grabber_production_rvv_asm
