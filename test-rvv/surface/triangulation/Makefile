# =============================================================================
#  PCL surface/triangulation RVV diagnostic Makefile
#
#  本 topic 只构建 test-rvv 下的接入前诊断（pre-production diagnostic）。
#  production 源码 surface/src/on_nurbs/triangulation.cpp 不在本 Makefile 中
#  修改或覆盖。
# =============================================================================

TOPIC  := triangulation
MODULE := surface

FOCUS_DIR      := pcl/surface/on_nurbs/triangulation.cpp
FILTER_REPORT  := log/filtered_triangulation.log
ANALYZE_REPORT := log/analyze_triangulation.log

TARGET_BENCH     := bench_triangulation
TARGET_BENCH_STD := bench_triangulation_std
TARGET_BENCH_RVV := bench_triangulation_rvv
SRCS_BENCH       := src/bench_triangulation.cpp

TARGET_TEST     := test_triangulation
TARGET_TEST_STD := test_triangulation_std
TARGET_TEST_RVV := test_triangulation_rvv
SRCS_TEST       := src/test_triangulation.cpp

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/surface/include \
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

LIBS_CORE := -lpcl_common -lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread

TRI_BOARD_REPEATED_DIR ?= log/board/grid_sampling_repeated
TRI_BOARD_SUMMARY_MD := $(TRI_BOARD_REPEATED_DIR)/summary.md
TRI_BOARD_DOCTOR_MD := $(TRI_BOARD_REPEATED_DIR)/evidence_doctor.md
TRI_MANIFEST_SCRIPT := script/generate_triangulation_board_manifest.py

include ../../mk/rvv-topic.mk

run_bench_param_grid_rvv:
	@$(MAKE) -C $(CURDIR) run_bench_rvv BENCH_ARGS="--case-filter tri_param_grid_512 --iterations 10 --warmup-iterations 2"

run_bench_surface_eval_rvv:
	@$(MAKE) -C $(CURDIR) run_bench_rvv BENCH_ARGS="--case-filter tri_surface_eval_256 --iterations 5 --warmup-iterations 1"

run_board_bench_param_grid:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter tri_param_grid_512 --iterations 20 --warmup-iterations 3"

run_board_bench_surface_eval:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter tri_surface_eval_256 --iterations 8 --warmup-iterations 2"

run_board_grid_sampling_evidence_doctor:
	@mkdir -p $(TRI_BOARD_REPEATED_DIR)
	$(PYTHON) ../../script/evidence_doctor.py \
		--summary-md $(OUTPUT_DIR_BOARD)/analyze_bench_compare.log \
		--output $(TRI_BOARD_DOCTOR_MD) \
		--fail-on never

generate_board_smoke_manifests:
	@test -d log/board/param_grid_512_smoke && $(PYTHON) $(TRI_MANIFEST_SCRIPT) log/board/param_grid_512_smoke
	@test -d log/board/surface_eval_256_smoke && $(PYTHON) $(TRI_MANIFEST_SCRIPT) log/board/surface_eval_256_smoke

doctor_board_smoke_manifests: generate_board_smoke_manifests
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest log/board/param_grid_512_smoke/evidence_manifest.json \
		--output log/board/param_grid_512_smoke/evidence_doctor.md \
		--fail-on never
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest log/board/surface_eval_256_smoke/evidence_manifest.json \
		--output log/board/surface_eval_256_smoke/evidence_doctor.md \
		--fail-on never

.PHONY: run_bench_param_grid_rvv run_bench_surface_eval_rvv run_board_bench_param_grid run_board_bench_surface_eval run_board_grid_sampling_evidence_doctor generate_board_smoke_manifests doctor_board_smoke_manifests
