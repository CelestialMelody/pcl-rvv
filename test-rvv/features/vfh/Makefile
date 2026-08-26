# =============================================================================
# PCL features/vfh RVV diagnostic Makefile
#
# 本 topic 先评估 VFH（Viewpoint Feature Histogram，视点特征直方图）
# 的 caller-shaped diagnostic（调用方形态诊断）。QEMU（仿真器）只用于
# correctness（正确性）和日志形状；性能结论必须来自 board（板卡）或目标硬件。
# =============================================================================

TOPIC  := vfh
MODULE := features

FOCUS_DIR      := pcl/features/include/pcl/features/impl/vfh.hpp
FILTER_REPORT  := log/filtered_vfh.log
ANALYZE_REPORT := log/analyze_vfh.log

TARGET_BENCH     := bench_vfh
TARGET_BENCH_STD := bench_vfh_std
TARGET_BENCH_RVV := bench_vfh_rvv
SRCS_BENCH       := src/bench_vfh.cpp

TARGET_TEST     := test_vfh
TARGET_TEST_STD := test_vfh_std
TARGET_TEST_RVV := test_vfh_rvv
BOARD_TARGET_TEST := test_vfh_rvv
SRCS_TEST       := src/test_vfh.cpp

INCLUDES = \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/features/include \
	-I$(PCL_SOURCE_ROOT)/search/include \
	-I$(PCL_SOURCE_ROOT)/kdtree/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/filters/include \
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

REMOTE_DIR ?= /root/pcl-test/features/vfh

REPEATED_BOARD_RUNS ?= 5
REPEATED_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated
EVIDENCE_MANIFEST_REPEATED ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_manifest.json
EVIDENCE_DOCTOR_REPEATED_MD ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.md
EVIDENCE_DOCTOR_REPEATED_JSON ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.json

LIBS_BENCH := \
	-lpcl_features \
	-lpcl_search \
	-lpcl_kdtree \
	-lpcl_common \
	-lflann_cpp \
	-lboost_system \
	-llz4 \
	-lm

LIBS_TEST := \
	-lpcl_features \
	-lpcl_search \
	-lpcl_kdtree \
	-lpcl_common \
	-lflann_cpp \
	-lboost_system \
	-llz4 \
	-lgtest -lgtest_main -lpthread \
	-lm

include ../../mk/rvv-topic.mk

board_repeated:
	@set -eu; \
	for i in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
			OUTPUT_DIR_BOARD="$(REPEATED_BOARD_OUTPUT_DIR)/$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated/$$run"; \
	done

evidence_manifest_repeated:
	python3 script/generate_vfh_evidence_manifest.py \
		--summary-dir "$(REPEATED_BOARD_OUTPUT_DIR)" \
		--output "$(EVIDENCE_MANIFEST_REPEATED)"

evidence_doctor_repeated: evidence_manifest_repeated
	python3 ../../script/evidence_doctor.py \
		--manifest "$(EVIDENCE_MANIFEST_REPEATED)" \
		--output "$(EVIDENCE_DOCTOR_REPEATED_MD)" \
		--json-output "$(EVIDENCE_DOCTOR_REPEATED_JSON)" \
		--fail-on never

evidence_doctor_phase030:
	$(MAKE) -C $(CURDIR) evidence_doctor_repeated \
		REPEATED_BOARD_OUTPUT_DIR=log/board/repeated-phase030

.PHONY: board_repeated evidence_manifest_repeated evidence_doctor_repeated evidence_doctor_phase030
