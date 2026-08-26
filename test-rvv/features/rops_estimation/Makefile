# =============================================================================
# PCL features/rops_estimation RVV diagnostic Makefile
#
# 本 topic 先评估 RoPS（Rotational Projection Statistics，旋转投影统计）
# 描述子里的 distribution matrix（分布矩阵）和 central moments（中心矩）
# 组件。QEMU 只用于 correctness（正确性）和日志形状；性能结论必须来自
# 板卡或目标硬件。
# =============================================================================

TOPIC  := rops_estimation
MODULE := features

FOCUS_DIR      := pcl/features/include/pcl/features/impl/rops_estimation.hpp
FILTER_REPORT  := log/filtered_rops.log
ANALYZE_REPORT := log/analyze_rops.log

TARGET_TEST     := test_rops_estimation
TARGET_TEST_STD := test_rops_estimation_std
TARGET_TEST_RVV := test_rops_estimation_rvv
SRCS_TEST       := src/test_rops_estimation.cpp

TARGET_BENCH     := bench_rops_estimation
TARGET_BENCH_STD := bench_rops_estimation_std
TARGET_BENCH_RVV := bench_rops_estimation_rvv
SRCS_BENCH       := src/bench_rops_estimation.cpp

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/features/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/search/include \
	-I$(PCL_SOURCE_ROOT)/kdtree/include \
	-I$(PCL_SOURCE_ROOT)/octree/include \
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

LIBS_TEST := -lpcl_features -lpcl_search -lpcl_kdtree -lpcl_common -lflann_cpp -llz4 -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lpcl_features -lpcl_search -lpcl_kdtree -lpcl_common -lflann_cpp -llz4 -lm

include ../../mk/rvv-topic.mk

dump_test_rvv: | $(ASM_DIR)/$(ARCH)
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_TEST=$(TARGET_TEST_RVV) $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_RVV)
	$(OBJDUMP) -d -C $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_RVV) > $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).full.asm
	grep -E $(VEC_REGEX_STR) $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).full.asm > $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm || true
	@echo "[DONE] ROPS RVV test asm dump: $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm"

run_qemu_smoke: run_test_compare dump_test_rvv

generate_phase010_board_summary:
	$(PYTHON_RUN) script/generate_rops_repeated_summary.py \
		--input-dir log/board/phase010_distribution_matrix_repeated \
		--output log/board/phase010_distribution_matrix_repeated/summary.md \
		--manifest log/board/phase010_distribution_matrix_repeated/evidence_manifest.json \
		--run-label phase010_distribution_matrix_repeated \
		--expected-runs 5 \
		--asm build/asm/riscv/bench_rops_estimation_rvv.full.asm

doctor_phase010_board_summary: generate_phase010_board_summary
	$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest log/board/phase010_distribution_matrix_repeated/evidence_manifest.json \
		--output log/board/phase010_distribution_matrix_repeated/evidence_doctor.md \
		--json-output log/board/phase010_distribution_matrix_repeated/evidence_doctor.json \
		--fail-on never

generate_phase020_board_summary:
	$(PYTHON_RUN) script/generate_rops_repeated_summary.py \
		--input-dir log/board/phase020_rotate_cloud_repeated \
		--output log/board/phase020_rotate_cloud_repeated/summary.md \
		--manifest log/board/phase020_rotate_cloud_repeated/evidence_manifest.json \
		--run-label phase020_rotate_cloud_repeated \
		--expected-runs 5 \
		--asm build/asm/riscv/bench_rops_estimation_rvv.full.asm \
		--case-label rops_rotate_cloud_aabb

doctor_phase020_board_summary: generate_phase020_board_summary
	$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest log/board/phase020_rotate_cloud_repeated/evidence_manifest.json \
		--output log/board/phase020_rotate_cloud_repeated/evidence_doctor.md \
		--json-output log/board/phase020_rotate_cloud_repeated/evidence_doctor.json \
		--fail-on never

generate_phase030_board_summary:
	$(PYTHON_RUN) script/generate_rops_repeated_summary.py \
		--input-dir log/board/phase030_combined_rotate_distribution_repeated \
		--output log/board/phase030_combined_rotate_distribution_repeated/summary.md \
		--manifest log/board/phase030_combined_rotate_distribution_repeated/evidence_manifest.json \
		--run-label phase030_combined_rotate_distribution_repeated \
		--expected-runs 5 \
		--asm build/asm/riscv/bench_rops_estimation_rvv.full.asm \
		--case-label rops_rotate_distribution_pipeline

doctor_phase030_board_summary: generate_phase030_board_summary
	$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest log/board/phase030_combined_rotate_distribution_repeated/evidence_manifest.json \
		--output log/board/phase030_combined_rotate_distribution_repeated/evidence_doctor.md \
		--json-output log/board/phase030_combined_rotate_distribution_repeated/evidence_doctor.json \
		--fail-on never

generate_phase040_board_summary:
	$(PYTHON_RUN) script/generate_rops_repeated_summary.py \
		--input-dir log/board/phase040_production_direct_repeated \
		--output log/board/phase040_production_direct_repeated/summary.md \
		--manifest log/board/phase040_production_direct_repeated/evidence_manifest.json \
		--run-label phase040_production_direct_repeated \
		--expected-runs 5 \
		--asm build/asm/riscv/bench_rops_estimation_rvv.full.asm \
		--case-label rops_production_rotate_distribution_pipeline

doctor_phase040_board_summary: generate_phase040_board_summary
	$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest log/board/phase040_production_direct_repeated/evidence_manifest.json \
		--output log/board/phase040_production_direct_repeated/evidence_doctor.md \
		--json-output log/board/phase040_production_direct_repeated/evidence_doctor.json \
		--fail-on never

generate_phase050_pointxyzi_board_summary:
	$(PYTHON_RUN) script/generate_rops_repeated_summary.py \
		--input-dir log/board/phase050_pointxyzi_production_detail_repeated \
		--output log/board/phase050_pointxyzi_production_detail_repeated/summary.md \
		--manifest log/board/phase050_pointxyzi_production_detail_repeated/evidence_manifest.json \
		--run-label phase050_pointxyzi_production_detail_repeated \
		--expected-runs 5 \
		--asm build/asm/riscv/bench_rops_estimation_rvv.full.asm \
		--case-label rops_production_pointxyzi_rotate_distribution_pipeline

doctor_phase050_pointxyzi_board_summary: generate_phase050_pointxyzi_board_summary
	$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest log/board/phase050_pointxyzi_production_detail_repeated/evidence_manifest.json \
		--output log/board/phase050_pointxyzi_production_detail_repeated/evidence_doctor.md \
		--json-output log/board/phase050_pointxyzi_production_detail_repeated/evidence_doctor.json \
		--fail-on never

generate_phase050_pointnormal_board_summary:
	$(PYTHON_RUN) script/generate_rops_repeated_summary.py \
		--input-dir log/board/phase050_pointnormal_production_detail_repeated \
		--output log/board/phase050_pointnormal_production_detail_repeated/summary.md \
		--manifest log/board/phase050_pointnormal_production_detail_repeated/evidence_manifest.json \
		--run-label phase050_pointnormal_production_detail_repeated \
		--expected-runs 5 \
		--asm build/asm/riscv/bench_rops_estimation_rvv.full.asm \
		--case-label rops_production_pointnormal_rotate_distribution_pipeline

doctor_phase050_pointnormal_board_summary: generate_phase050_pointnormal_board_summary
	$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest log/board/phase050_pointnormal_production_detail_repeated/evidence_manifest.json \
		--output log/board/phase050_pointnormal_production_detail_repeated/evidence_doctor.md \
		--json-output log/board/phase050_pointnormal_production_detail_repeated/evidence_doctor.json \
		--fail-on never

.PHONY: dump_test_rvv run_qemu_smoke generate_phase010_board_summary doctor_phase010_board_summary generate_phase020_board_summary doctor_phase020_board_summary generate_phase030_board_summary doctor_phase030_board_summary generate_phase040_board_summary doctor_phase040_board_summary generate_phase050_pointxyzi_board_summary doctor_phase050_pointxyzi_board_summary generate_phase050_pointnormal_board_summary doctor_phase050_pointnormal_board_summary
