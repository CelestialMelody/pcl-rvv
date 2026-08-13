# =============================================================================
#  PCL registration/correspondence_types RVV diagnostic Makefile
# =============================================================================

TOPIC  := correspondence_types
MODULE := registration

FOCUS_DIR      := pcl/registration/include/pcl/registration/impl/correspondence_types.hpp
FILTER_REPORT  := log/filtered_correspondence_types.log
ANALYZE_REPORT := log/analyze_correspondence_types.log

TARGET_BENCH     := bench_correspondence_types
TARGET_BENCH_STD := bench_correspondence_types_std
TARGET_BENCH_RVV := bench_correspondence_types_rvv
SRCS_BENCH       := src/bench_correspondence_types.cpp

TARGET_TEST     := test_correspondence_types
TARGET_TEST_STD := test_correspondence_types_std
TARGET_TEST_RVV := test_correspondence_types_rvv
SRCS_TEST       := src/test_correspondence_types.cpp

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

CORRESPONDENCE_TYPES_BENCH_WARMUP_ITERATIONS ?= 5

include ../../mk/rvv-topic.mk

run_bench_index_extract: run_bench_index_extract_compare
run_bench_index_extract_compare: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_compare BENCH_ARGS="--case-filter index-extract --warmup-iterations $(CORRESPONDENCE_TYPES_BENCH_WARMUP_ITERATIONS)" BENCH_STD_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_index_extract_std.log BENCH_RVV_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_index_extract_rvv.log BENCH_COMPARE_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/analyze_bench_compare_index_extract.log

# This target calls the real production helper. It is a production probe only
# when the phase applies a temporary production RVV patch; with the current
# no-production source it is just a public-helper scalar boundary smoke.
run_bench_production_index_extract: run_bench_production_index_extract_compare
run_bench_production_index_extract_compare: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_compare BENCH_ARGS="--case-filter production-index-extract --warmup-iterations $(CORRESPONDENCE_TYPES_BENCH_WARMUP_ITERATIONS)" BENCH_STD_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_production_index_extract_std.log BENCH_RVV_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_production_index_extract_rvv.log BENCH_COMPARE_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/analyze_bench_compare_production_index_extract.log

run_bench_distance_stats: run_bench_distance_stats_compare
run_bench_distance_stats_compare: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_compare BENCH_ARGS="--case-filter distance-stats --warmup-iterations $(CORRESPONDENCE_TYPES_BENCH_WARMUP_ITERATIONS)" BENCH_STD_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_distance_stats_std.log BENCH_RVV_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_distance_stats_rvv.log BENCH_COMPARE_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/analyze_bench_compare_distance_stats.log

generate_qemu_evidence_manifest: | $(OUTPUT_DIR_QEMU)
	$(PYTHON) script/generate_qemu_evidence_manifest.py --std-log $(BENCH_STD_OUTPUT_FILE) --rvv-log $(BENCH_RVV_OUTPUT_FILE) --summary-md $(BENCH_COMPARE_OUTPUT_FILE) --asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm --output $(OUTPUT_DIR_QEMU)/evidence_manifest.json

run_evidence_doctor_qemu: generate_qemu_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py --manifest $(OUTPUT_DIR_QEMU)/evidence_manifest.json --output $(OUTPUT_DIR_QEMU)/evidence_doctor.md --fail-on never

BOARD_INDEX_EXTRACT_EVIDENCE_DIR ?= $(OUTPUT_DIR_BOARD)/010-board-diagnostic/index-extract
BOARD_INDEX_EXTRACT_SUMMARY ?= $(BOARD_INDEX_EXTRACT_EVIDENCE_DIR)/summary.md
BOARD_INDEX_EXTRACT_MANIFEST ?= $(BOARD_INDEX_EXTRACT_EVIDENCE_DIR)/evidence_manifest.json
BOARD_INDEX_EXTRACT_DOCTOR ?= $(BOARD_INDEX_EXTRACT_EVIDENCE_DIR)/evidence_doctor.md
BOARD_PRODUCTION_INDEX_EXTRACT_EVIDENCE_DIR ?= $(OUTPUT_DIR_BOARD)/011-production-probe-boundary-check/production-index-extract
BOARD_PRODUCTION_INDEX_EXTRACT_SUMMARY ?= $(BOARD_PRODUCTION_INDEX_EXTRACT_EVIDENCE_DIR)/summary.md
BOARD_PRODUCTION_INDEX_EXTRACT_MANIFEST ?= $(BOARD_PRODUCTION_INDEX_EXTRACT_EVIDENCE_DIR)/evidence_manifest.json
BOARD_PRODUCTION_INDEX_EXTRACT_DOCTOR ?= $(BOARD_PRODUCTION_INDEX_EXTRACT_EVIDENCE_DIR)/evidence_doctor.md

generate_board_index_extract_summary:
	$(PYTHON) script/generate_board_repeated_summary.py --run-root $(BOARD_INDEX_EXTRACT_EVIDENCE_DIR) --summary $(BOARD_INDEX_EXTRACT_SUMMARY) --manifest $(BOARD_INDEX_EXTRACT_MANIFEST) --doctor $(BOARD_INDEX_EXTRACT_DOCTOR)

run_evidence_doctor_board_index_extract: generate_board_index_extract_summary
	$(PYTHON) ../../script/evidence_doctor.py --manifest $(BOARD_INDEX_EXTRACT_MANIFEST) --output $(BOARD_INDEX_EXTRACT_DOCTOR) --fail-on never

generate_board_production_index_extract_summary:
	$(PYTHON) script/generate_board_repeated_summary.py --run-root $(BOARD_PRODUCTION_INDEX_EXTRACT_EVIDENCE_DIR) --summary $(BOARD_PRODUCTION_INDEX_EXTRACT_SUMMARY) --manifest $(BOARD_PRODUCTION_INDEX_EXTRACT_MANIFEST) --doctor $(BOARD_PRODUCTION_INDEX_EXTRACT_DOCTOR) --case-filter production-index-extract --group board-production-index-extract --evidence-role production_direct_probe --case-kind board_production_direct_probe --boundary production_public_helper --decision-summary production-probe

run_evidence_doctor_board_production_index_extract: generate_board_production_index_extract_summary
	$(PYTHON) ../../script/evidence_doctor.py --manifest $(BOARD_PRODUCTION_INDEX_EXTRACT_MANIFEST) --output $(BOARD_PRODUCTION_INDEX_EXTRACT_DOCTOR) --fail-on never
