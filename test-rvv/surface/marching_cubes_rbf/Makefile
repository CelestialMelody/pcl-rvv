# =============================================================================
#  PCL surface/marching_cubes_rbf RVV diagnostic Makefile
#
#  本 topic 同时保留 component ablation（组件消融）诊断 case，并为
#  MarchingCubesRBF<PointNormal>::voxelizeData() 提供 production direct
#  （真实生产入口证据）case。QEMU target 只用于 correctness（正确性）和日志形状；
#  性能结论来自板卡。
# =============================================================================

TOPIC  := marching_cubes_rbf
MODULE := surface

FOCUS_DIR      := pcl/surface/include/pcl/surface/impl/marching_cubes_rbf.hpp
FILTER_REPORT  := log/filtered_marching_cubes_rbf.log
ANALYZE_REPORT := log/analyze_marching_cubes_rbf.log

TARGET_BENCH     := bench_marching_cubes_rbf
TARGET_BENCH_STD := bench_marching_cubes_rbf_std
TARGET_BENCH_RVV := bench_marching_cubes_rbf_rvv
SRCS_BENCH       := src/bench_marching_cubes_rbf.cpp

TARGET_TEST     := test_marching_cubes_rbf
TARGET_TEST_STD := test_marching_cubes_rbf_std
TARGET_TEST_RVV := test_marching_cubes_rbf_rvv
SRCS_TEST       := src/test_marching_cubes_rbf.cpp

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

LIBS_BENCH := -lpcl_common -llz4 -lm
LIBS_TEST  := -lpcl_common -llz4 -lgtest -lgtest_main -lpthread -lm

MCRBF_BOARD_MANIFEST_JSON := log/board/evidence_manifest.json
MCRBF_BOARD_DOCTOR_MD := log/board/evidence_doctor.md

include ../../mk/rvv-topic.mk

run_qemu_smoke: run_test_compare run_bench_rvv

run_board_matrix_n40:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter mcrbf_matrix_fill_n40 --iterations 10 --warmup-iterations 2"

run_board_full_n40:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter mcrbf_full_pipeline_n40_r20 --iterations 6 --warmup-iterations 2"

run_board_component_smoke:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter all --iterations 5 --warmup-iterations 2"

run_board_production_pointnormal_smoke:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter mcrbf_prod_pointnormal_n40_r20 --iterations 5 --warmup-iterations 2"

run_board_production_smoke:
	@$(MAKE) -C $(CURDIR) run_board_bench_compare BENCH_ARGS="--case-filter all --iterations 5 --warmup-iterations 2"

generate_board_evidence_manifest:
	$(PYTHON) script/generate_marching_cubes_rbf_evidence_manifest.py \
		--std-log log/board/run_bench_std.log \
		--rvv-log log/board/run_bench_rvv.log \
		--summary log/board/analyze_bench_compare.log \
		--output $(MCRBF_BOARD_MANIFEST_JSON)

run_board_evidence_doctor: generate_board_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(MCRBF_BOARD_MANIFEST_JSON) \
		--output $(MCRBF_BOARD_DOCTOR_MD) \
		--fail-on never

.PHONY: run_qemu_smoke run_board_matrix_n40 run_board_full_n40 run_board_component_smoke run_board_production_pointnormal_smoke run_board_production_smoke generate_board_evidence_manifest run_board_evidence_doctor
