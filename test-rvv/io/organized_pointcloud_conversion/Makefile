# =============================================================================
#  PCL io/organized_pointcloud_conversion RVV diagnostic Makefile
#
# Topic-specific parameters live here; shared build, QEMU and board deploy rules
# are provided by test-rvv/mk/rvv-topic.mk.
# =============================================================================

TOPIC  := organized_pointcloud_conversion
MODULE := io

FOCUS_DIR      := pcl/compression/organized_pointcloud_conversion.h
FILTER_REPORT  := log/filtered_organized_pointcloud_conversion.log
ANALYZE_REPORT := log/analyze_organized_pointcloud_conversion.log

TARGET_BENCH     := bench_organized_pointcloud_conversion
TARGET_BENCH_STD := bench_organized_pointcloud_conversion_std
TARGET_BENCH_RVV := bench_organized_pointcloud_conversion_rvv
SRCS_BENCH       := src/bench_organized_pointcloud_conversion.cpp

TARGET_TEST     := test_organized_pointcloud_conversion
TARGET_TEST_STD := test_organized_pointcloud_conversion_std
TARGET_TEST_RVV := test_organized_pointcloud_conversion_rvv
SRCS_TEST       := src/test_organized_pointcloud_conversion.cpp

TARGET_PROD_ASM_RVV := prod_asm_organized_pointcloud_conversion_rvv
SRCS_PROD_ASM       := src/prod_asm_organized_pointcloud_conversion.cpp

BENCH_ARGS ?= --iterations 30 --warmup-iterations 5 --case-filter all

LIBS_CORE := \
	-lpcl_io \
	-lpcl_common \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-llz4 -lz -lpng \
	-lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/io/include \
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

BOARD_EVIDENCE_MANIFEST := log/board/evidence_manifest.json
BOARD_EVIDENCE_DOCTOR_MD := log/board/evidence_doctor.md
BOARD_EVIDENCE_DOCTOR_JSON := log/board/evidence_doctor.json
PRODUCTION_REPEATED_DIR ?= log/board/production_direct_repeated
PRODUCTION_REPEATED_RUN_DIRS := $(sort $(wildcard $(PRODUCTION_REPEATED_DIR)/run-*))
PRODUCTION_REPEATED_SUMMARY := $(PRODUCTION_REPEATED_DIR)/summary.md
PRODUCTION_REPEATED_MANIFEST := $(PRODUCTION_REPEATED_DIR)/evidence_manifest.json
PRODUCTION_REPEATED_DOCTOR_MD := $(PRODUCTION_REPEATED_DIR)/evidence_doctor.md
PRODUCTION_REPEATED_DOCTOR_JSON := $(PRODUCTION_REPEATED_DIR)/evidence_doctor.json

generate_board_evidence_manifest:
	$(PYTHON) script/generate_opc_board_evidence_manifest.py \
		--std-log log/board/run_bench_std.log \
		--rvv-log log/board/run_bench_rvv.log \
		--summary-log log/board/analyze_bench_compare.log \
		--output $(BOARD_EVIDENCE_MANIFEST)

run_board_evidence_doctor: generate_board_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(BOARD_EVIDENCE_MANIFEST) \
		--output $(BOARD_EVIDENCE_DOCTOR_MD) \
		--json-output $(BOARD_EVIDENCE_DOCTOR_JSON) \
		--fail-on never

generate_production_repeated_evidence_manifest:
	@test -n "$(PRODUCTION_REPEATED_RUN_DIRS)" || (echo "No repeated run dirs under $(PRODUCTION_REPEATED_DIR)" >&2; exit 2)
	$(PYTHON) script/generate_opc_board_evidence_manifest.py \
		$(foreach dir,$(PRODUCTION_REPEATED_RUN_DIRS),--run-dir $(dir)) \
		--production-only \
		--summary-log $(PRODUCTION_REPEATED_SUMMARY) \
		--output $(PRODUCTION_REPEATED_MANIFEST)

run_production_repeated_evidence_doctor: generate_production_repeated_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(PRODUCTION_REPEATED_MANIFEST) \
		--output $(PRODUCTION_REPEATED_DOCTOR_MD) \
		--json-output $(PRODUCTION_REPEATED_DOCTOR_JSON) \
		--fail-on never

PROD_ASM_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_PROD_ASM_RVV)
PROD_ASM_FULL := $(ASM_DIR)/$(ARCH)/$(TARGET_PROD_ASM_RVV).full.asm
PROD_ASM_RVV := $(ASM_DIR)/$(ARCH)/$(TARGET_PROD_ASM_RVV).asm

$(PROD_ASM_BIN): $(SRCS_PROD_ASM) $(PCL_SOURCE_ROOT)/io/include/pcl/compression/organized_pointcloud_conversion.h | $(LOG_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling production asm probe ($(ARCH), USE_PCL_RVV10=1)..."
	$(CXX) $(CXXFLAGS) -fno-tree-vectorize $(SRCS_PROD_ASM) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_BENCH) -o $(PROD_ASM_BIN)

dump_production_rvv_asm: USE_PCL_RVV10=1
dump_production_rvv_asm: | $(ASM_DIR)/$(ARCH)
dump_production_rvv_asm: $(PROD_ASM_BIN)
	$(OBJDUMP) -d -C $(PROD_ASM_BIN) > $(PROD_ASM_FULL)
	grep -E $(VEC_REGEX_STR) $(PROD_ASM_FULL) > $(PROD_ASM_RVV) || true
	@echo "[DONE] Production RVV asm dump: $(PROD_ASM_RVV)"

check_production_rvv_asm: dump_production_rvv_asm
	@if ! grep -Eq "vlse32|vlsseg|vfclass|vfrdiv|vs(e|s|u)x?e?32|vse32|vssseg" $(PROD_ASM_RVV); then \
		echo "[RED] production encode probe has no RVV instructions yet"; \
		exit 2; \
	fi
	@echo "[PASS] production encode probe contains RVV instructions"

.PHONY: generate_board_evidence_manifest run_board_evidence_doctor generate_production_repeated_evidence_manifest run_production_repeated_evidence_doctor
