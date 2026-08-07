# =============================================================================
#  PCL registration/transformation_estimation_point_to_plane_lls_weighted RVV diagnostic Makefile
# =============================================================================

TOPIC  := transformation_estimation_point_to_plane_lls_weighted
MODULE := registration

FOCUS_DIR      := pcl/registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp
FILTER_REPORT  := log/filtered_transformation_estimation_point_to_plane_lls_weighted.log
ANALYZE_REPORT := log/analyze_transformation_estimation_point_to_plane_lls_weighted.log

TARGET_BENCH     := bench_transformation_estimation_point_to_plane_lls_weighted
TARGET_BENCH_STD := bench_transformation_estimation_point_to_plane_lls_weighted_std
TARGET_BENCH_RVV := bench_transformation_estimation_point_to_plane_lls_weighted_rvv
SRCS_BENCH       := src/bench_teptplw.cpp

TARGET_TEST     := test_transformation_estimation_point_to_plane_lls_weighted
TARGET_TEST_STD := test_transformation_estimation_point_to_plane_lls_weighted_std
TARGET_TEST_RVV := test_transformation_estimation_point_to_plane_lls_weighted_rvv
SRCS_TEST       := \
	src/test_teptplw_public_semantics.cpp \
	src/test_teptplw_candidates.cpp \
	src/test_teptplw_row_sources.cpp \
	src/test_teptplw_production_direct.cpp

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/registration/include \
	-I$(PCL_SOURCE_ROOT)/test/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include \
	-I$(GTEST_ROOT)/include

LIBS_CORE := \
	-lpcl_registration \
	-lpcl_common \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread

TEPTPLW_SCRIPT_DIR := script
TEPTPLW_BOARD_COLLECT_SCRIPT := $(TEPTPLW_SCRIPT_DIR)/collect_teptplw_board_rvv_ba.py
TEPTPLW_TRACE_SUMMARY_SCRIPT := $(TEPTPLW_SCRIPT_DIR)/summarize_teptplw_trace.py
TEPTPLW_ASM_ATTRIBUTION_SCRIPT := $(TEPTPLW_SCRIPT_DIR)/generate_teptplw_asm_attribution.py
TEPTPLW_COMPARE_SCRIPT := $(TEPTPLW_SCRIPT_DIR)/collect_teptplw_board_compare_repeated.py
TEPTPLW_BOARD_EVIDENCE_DIR ?= $(OUTPUT_DIR_BOARD)/production_default_fused_abcd_ilp
TEPTPLW_COMPARE_EVIDENCE_DIR ?= $(OUTPUT_DIR_BOARD)/production_dispatch_fused_abcd_ilp
TEPTPLW_BOARD_SIZE ?= 262144
TEPTPLW_BOARD_RUNS ?= 5
TEPTPLW_BOARD_ITERATIONS ?= 20
TEPTPLW_BOARD_WARMUP_ITERATIONS ?= 5
TEPTPLW_COMPARE_CASE_FILTER ?= production-dispatch
TEPTPLW_COMPARE_SIZE ?= 262144
TEPTPLW_COMPARE_RUNS ?= 5
TEPTPLW_COMPARE_ITERATIONS ?= 20
TEPTPLW_COMPARE_WARMUP_ITERATIONS ?= 5
TEPTPLW_TRACE_LOGS ?= $(wildcard $(TEPTPLW_BOARD_EVIDENCE_DIR)/run*_rvv.log)
TEPTPLW_ASM_EVIDENCE ?= $(TEPTPLW_BOARD_EVIDENCE_DIR)/asm_production_symbol_attribution.md

include ../../mk/rvv-topic.mk

collect_board_production_default_fused_abcd_ilp:
	$(PYTHON_RUN) $(TEPTPLW_BOARD_COLLECT_SCRIPT) \
		--case-filter production-default-fused-abcd-ilp \
		--size $(TEPTPLW_BOARD_SIZE) \
		--runs $(TEPTPLW_BOARD_RUNS) \
		--iterations $(TEPTPLW_BOARD_ITERATIONS) \
		--warmup-iterations $(TEPTPLW_BOARD_WARMUP_ITERATIONS) \
		--output-dir $(TEPTPLW_BOARD_EVIDENCE_DIR)

clean_board_production_default_fused_abcd_ilp:
	rm -rf $(TEPTPLW_BOARD_EVIDENCE_DIR)

refresh_board_production_default_fused_abcd_ilp: clean_board_production_default_fused_abcd_ilp collect_board_production_default_fused_abcd_ilp

summarize_board_production_default_fused_abcd_ilp:
	@test -n "$(strip $(TEPTPLW_TRACE_LOGS))" || \
		(echo "[trace] No run*_rvv.log under $(TEPTPLW_BOARD_EVIDENCE_DIR)." >&2; exit 1)
	$(PYTHON_RUN) $(TEPTPLW_TRACE_SUMMARY_SCRIPT) \
		$(TEPTPLW_TRACE_LOGS) \
		--output $(TEPTPLW_BOARD_EVIDENCE_DIR)/trace_summary.md

asm_production_default_fused_abcd_ilp:
	$(MAKE) dump_bench_rvv
	$(PYTHON_RUN) $(TEPTPLW_ASM_ATTRIBUTION_SCRIPT) \
		--profile production-default-fused-abcd-ilp \
		--asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm \
		--output $(TEPTPLW_ASM_EVIDENCE)

collect_board_production_dispatch_repeated:
	$(PYTHON_RUN) $(TEPTPLW_COMPARE_SCRIPT) \
		--case-filter $(TEPTPLW_COMPARE_CASE_FILTER) \
		--size $(TEPTPLW_COMPARE_SIZE) \
		--runs $(TEPTPLW_COMPARE_RUNS) \
		--iterations $(TEPTPLW_COMPARE_ITERATIONS) \
		--warmup-iterations $(TEPTPLW_COMPARE_WARMUP_ITERATIONS) \
		--output-dir $(TEPTPLW_COMPARE_EVIDENCE_DIR)

clean_board_production_dispatch_repeated:
	rm -rf $(TEPTPLW_COMPARE_EVIDENCE_DIR)

refresh_board_production_dispatch_repeated: clean_board_production_dispatch_repeated collect_board_production_dispatch_repeated

.PHONY: collect_board_production_default_fused_abcd_ilp clean_board_production_default_fused_abcd_ilp refresh_board_production_default_fused_abcd_ilp summarize_board_production_default_fused_abcd_ilp asm_production_default_fused_abcd_ilp collect_board_production_dispatch_repeated clean_board_production_dispatch_repeated refresh_board_production_dispatch_repeated
