# =============================================================================
#  PCL surface/marching_cubes RVV diagnostic Makefile
#
#  本 topic 构建 marching_cubes 的 RVV correctness / bench / board 证据。
#  Phase 050 后 production 源码已保留 traits-gated generic RVV prepass；本
#  Makefile 只负责编译和采集证据，不生成或覆盖 production patch。
# =============================================================================

TOPIC  := marching_cubes
MODULE := surface

FOCUS_DIR      := pcl/surface/include/pcl/surface/impl/marching_cubes.hpp
FILTER_REPORT  := log/filtered_marching_cubes.log
ANALYZE_REPORT := log/analyze_marching_cubes.log

TARGET_BENCH     := bench_marching_cubes
TARGET_BENCH_STD := bench_marching_cubes_std
TARGET_BENCH_RVV := bench_marching_cubes_rvv
SRCS_BENCH       := src/bench_marching_cubes.cpp

TARGET_TEST     := test_marching_cubes
TARGET_TEST_STD := test_marching_cubes_std
TARGET_TEST_RVV := test_marching_cubes_rvv
SRCS_TEST       := src/test_marching_cubes.cpp

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

LIBS_CORE := -lpcl_surface -lpcl_common -lm
LIBS_BENCH := $(LIBS_CORE) -llz4
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -llz4 -lpthread

MC_BOARD_REPEATED_DIR ?= log/board/edge_interpolation_repeated
MC_BOARD_SUMMARY_MD := $(MC_BOARD_REPEATED_DIR)/summary.md
MC_BOARD_MANIFEST_JSON := $(MC_BOARD_REPEATED_DIR)/evidence_manifest.json
MC_BOARD_DOCTOR_MD := $(MC_BOARD_REPEATED_DIR)/evidence_doctor.md

MC_PREPASS_REPEATED_DIR ?= log/board/prepass_repeated
MC_PREPASS_SUMMARY_MD := $(MC_PREPASS_REPEATED_DIR)/summary.md
MC_PREPASS_MANIFEST_JSON := $(MC_PREPASS_REPEATED_DIR)/evidence_manifest.json
MC_PREPASS_DOCTOR_MD := $(MC_PREPASS_REPEATED_DIR)/evidence_doctor.md

MC_PROD_REPEATED_DIR ?= log/board/production_direct_repeated
MC_PROD_SUMMARY_MD := $(MC_PROD_REPEATED_DIR)/summary.md
MC_PROD_MANIFEST_JSON := $(MC_PROD_REPEATED_DIR)/evidence_manifest.json
MC_PROD_DOCTOR_MD := $(MC_PROD_REPEATED_DIR)/evidence_doctor.md

MC_GENERIC_XYZ_REPEATED_DIR ?= log/board/generic_xyz_repeated
MC_GENERIC_XYZ_SUMMARY_MD := $(MC_GENERIC_XYZ_REPEATED_DIR)/summary.md
MC_GENERIC_XYZ_MANIFEST_JSON := $(MC_GENERIC_XYZ_REPEATED_DIR)/evidence_manifest.json
MC_GENERIC_XYZ_DOCTOR_MD := $(MC_GENERIC_XYZ_REPEATED_DIR)/evidence_doctor.md

MC_GENERIC_XYZI_REPEATED_DIR ?= log/board/generic_xyzi_repeated
MC_GENERIC_XYZI_SUMMARY_MD := $(MC_GENERIC_XYZI_REPEATED_DIR)/summary.md
MC_GENERIC_XYZI_MANIFEST_JSON := $(MC_GENERIC_XYZI_REPEATED_DIR)/evidence_manifest.json
MC_GENERIC_XYZI_DOCTOR_MD := $(MC_GENERIC_XYZI_REPEATED_DIR)/evidence_doctor.md

MC_GENERIC_XYZRGB_REPEATED_DIR ?= log/board/generic_xyzrgb_repeated
MC_GENERIC_XYZRGB_SUMMARY_MD := $(MC_GENERIC_XYZRGB_REPEATED_DIR)/summary.md
MC_GENERIC_XYZRGB_MANIFEST_JSON := $(MC_GENERIC_XYZRGB_REPEATED_DIR)/evidence_manifest.json
MC_GENERIC_XYZRGB_DOCTOR_MD := $(MC_GENERIC_XYZRGB_REPEATED_DIR)/evidence_doctor.md

MC_GENERIC_XYZRGBA_REPEATED_DIR ?= log/board/generic_xyzrgba_repeated
MC_GENERIC_XYZRGBA_SUMMARY_MD := $(MC_GENERIC_XYZRGBA_REPEATED_DIR)/summary.md
MC_GENERIC_XYZRGBA_MANIFEST_JSON := $(MC_GENERIC_XYZRGBA_REPEATED_DIR)/evidence_manifest.json
MC_GENERIC_XYZRGBA_DOCTOR_MD := $(MC_GENERIC_XYZRGBA_REPEATED_DIR)/evidence_doctor.md

MC_PHASE060_RVV_AB_DIR ?= log/board/phase060_rvv_ab
MC_PHASE060_RVV_AB_SUMMARY_MD := $(MC_PHASE060_RVV_AB_DIR)/summary.md
MC_PHASE060_RVV_AB_MANIFEST_JSON := $(MC_PHASE060_RVV_AB_DIR)/evidence_manifest.json
MC_PHASE060_RVV_AB_DOCTOR_MD := $(MC_PHASE060_RVV_AB_DIR)/evidence_doctor.md

include ../../mk/rvv-topic.mk

generate_board_edge_interpolation_summary:
	$(PYTHON) script/generate_marching_cubes_board_summary.py \
		--run-dir log/board \
		--run-dir log/board/edge_interpolation_smoke2/board \
		--summary $(MC_BOARD_SUMMARY_MD) \
		--manifest $(MC_BOARD_MANIFEST_JSON) \
		--case-prefix mc_edge_ \
		--run-label edge_interpolation_repeated

run_board_edge_interpolation_evidence_doctor: generate_board_edge_interpolation_summary
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(MC_BOARD_MANIFEST_JSON) \
		--output $(MC_BOARD_DOCTOR_MD) \
		--fail-on never

generate_board_prepass_summary:
	$(PYTHON) script/generate_marching_cubes_board_summary.py \
		--run-dir log/board/prepass_smoke1/board \
		--run-dir log/board/prepass_smoke2/board \
		--summary $(MC_PREPASS_SUMMARY_MD) \
		--manifest $(MC_PREPASS_MANIFEST_JSON) \
		--case-prefix mc_prepass_ \
		--run-label prepass_repeated \
		--summary-title "marching_cubes cube-index prepass repeated board diagnostic" \
		--decision-question "RVV-vs-scalar pre-production cube-index prepass diagnostic"

run_board_prepass_evidence_doctor: generate_board_prepass_summary
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(MC_PREPASS_MANIFEST_JSON) \
		--output $(MC_PREPASS_DOCTOR_MD) \
		--fail-on never

generate_board_production_direct_summary:
	$(PYTHON) script/generate_marching_cubes_board_summary.py \
		--run-dir log/board/production_direct_smoke1/board \
		--run-dir log/board/production_direct_smoke2/board \
		--run-dir log/board/production_direct_smoke3/board \
		--run-dir log/board/production_direct_smoke4/board \
		--run-dir log/board/production_direct_smoke5/board \
		--summary $(MC_PROD_SUMMARY_MD) \
		--manifest $(MC_PROD_MANIFEST_JSON) \
		--case-prefix mc_prod_ \
		--run-label production_direct_repeated \
		--summary-title "marching_cubes production-direct repeated board diagnostic" \
		--decision-question "RVV-vs-scalar production-direct PointNormal marching_cubes diagnostic"

run_board_production_direct_evidence_doctor: generate_board_production_direct_summary
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(MC_PROD_MANIFEST_JSON) \
		--output $(MC_PROD_DOCTOR_MD) \
		--fail-on never

generate_board_generic_xyz_summary:
	$(PYTHON) script/generate_marching_cubes_board_summary.py \
		--run-dir log/board/generic_xyz_repeated_smoke1/board \
		--run-dir log/board/generic_xyz_repeated_smoke2/board \
		--run-dir log/board/generic_xyz_repeated_smoke3/board \
		--run-dir log/board/generic_xyz_repeated_smoke4/board \
		--run-dir log/board/generic_xyz_repeated_smoke5/board \
		--summary $(MC_GENERIC_XYZ_SUMMARY_MD) \
		--manifest $(MC_GENERIC_XYZ_MANIFEST_JSON) \
		--case-prefix mc_prod_ \
		--run-label generic_xyz_repeated \
		--summary-title "marching_cubes production-direct generic xyz repeated board diagnostic" \
		--decision-question "RVV-vs-scalar production-direct mc_prod_xyz_64 marching_cubes generic diagnostic"

run_board_generic_xyz_evidence_doctor: generate_board_generic_xyz_summary
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(MC_GENERIC_XYZ_MANIFEST_JSON) \
		--output $(MC_GENERIC_XYZ_DOCTOR_MD) \
		--fail-on never

generate_board_generic_xyzi_summary:
	$(PYTHON) script/generate_marching_cubes_board_summary.py \
		--run-dir log/board/generic_xyzi_repeated_smoke1/board \
		--run-dir log/board/generic_xyzi_repeated_smoke2/board \
		--run-dir log/board/generic_xyzi_repeated_smoke3/board \
		--run-dir log/board/generic_xyzi_repeated_smoke4/board \
		--run-dir log/board/generic_xyzi_repeated_smoke5/board \
		--summary $(MC_GENERIC_XYZI_SUMMARY_MD) \
		--manifest $(MC_GENERIC_XYZI_MANIFEST_JSON) \
		--case-prefix mc_prod_ \
		--run-label generic_xyzi_repeated \
		--summary-title "marching_cubes production-direct generic xyzi repeated board diagnostic" \
		--decision-question "RVV-vs-scalar production-direct mc_prod_xyzi_64 marching_cubes generic diagnostic"

run_board_generic_xyzi_evidence_doctor: generate_board_generic_xyzi_summary
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(MC_GENERIC_XYZI_MANIFEST_JSON) \
		--output $(MC_GENERIC_XYZI_DOCTOR_MD) \
		--fail-on never

generate_board_generic_xyzrgb_summary:
	$(PYTHON) script/generate_marching_cubes_board_summary.py \
		--run-dir log/board/generic_xyzrgb_repeated_smoke1/board \
		--run-dir log/board/generic_xyzrgb_repeated_smoke2/board \
		--run-dir log/board/generic_xyzrgb_repeated_smoke3/board \
		--run-dir log/board/generic_xyzrgb_repeated_smoke4/board \
		--run-dir log/board/generic_xyzrgb_repeated_smoke5/board \
		--summary $(MC_GENERIC_XYZRGB_SUMMARY_MD) \
		--manifest $(MC_GENERIC_XYZRGB_MANIFEST_JSON) \
		--case-prefix mc_prod_ \
		--run-label generic_xyzrgb_repeated \
		--summary-title "marching_cubes production-direct generic xyzrgb repeated board diagnostic" \
		--decision-question "RVV-vs-scalar production-direct mc_prod_xyzrgb_64 marching_cubes generic diagnostic"

run_board_generic_xyzrgb_evidence_doctor: generate_board_generic_xyzrgb_summary
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(MC_GENERIC_XYZRGB_MANIFEST_JSON) \
		--output $(MC_GENERIC_XYZRGB_DOCTOR_MD) \
		--fail-on never

generate_board_generic_xyzrgba_summary:
	$(PYTHON) script/generate_marching_cubes_board_summary.py \
		--run-dir log/board/generic_xyzrgba_repeated_smoke1/board \
		--run-dir log/board/generic_xyzrgba_repeated_smoke2/board \
		--run-dir log/board/generic_xyzrgba_repeated_smoke3/board \
		--run-dir log/board/generic_xyzrgba_repeated_smoke4/board \
		--run-dir log/board/generic_xyzrgba_repeated_smoke5/board \
		--summary $(MC_GENERIC_XYZRGBA_SUMMARY_MD) \
		--manifest $(MC_GENERIC_XYZRGBA_MANIFEST_JSON) \
		--case-prefix mc_prod_ \
		--run-label generic_xyzrgba_repeated \
		--summary-title "marching_cubes production-direct generic xyzrgba repeated board diagnostic" \
		--decision-question "RVV-vs-scalar production-direct mc_prod_xyzrgba_64 marching_cubes generic diagnostic"

run_board_generic_xyzrgba_evidence_doctor: generate_board_generic_xyzrgba_summary
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(MC_GENERIC_XYZRGBA_MANIFEST_JSON) \
		--output $(MC_GENERIC_XYZRGBA_DOCTOR_MD) \
		--fail-on never

run_board_generic_repeated_evidence_doctor: \
	run_board_generic_xyz_evidence_doctor \
	run_board_generic_xyzi_evidence_doctor \
	run_board_generic_xyzrgb_evidence_doctor \
	run_board_generic_xyzrgba_evidence_doctor

generate_board_phase060_rvv_ab_summary:
	$(PYTHON) script/generate_marching_cubes_rvv_ab_summary.py \
		--baseline-dir log/board/phase060_baseline_rvv1/board \
		--baseline-dir log/board/phase060_baseline_rvv2/board \
		--baseline-dir log/board/phase060_baseline_rvv3/board \
		--candidate-dir log/board/phase060_candidate_rvv1/board \
		--candidate-dir log/board/phase060_candidate_rvv2/board \
		--candidate-dir log/board/phase060_candidate_rvv3/board \
		--summary $(MC_PHASE060_RVV_AB_SUMMARY_MD) \
		--manifest $(MC_PHASE060_RVV_AB_MANIFEST_JSON)

run_board_phase060_rvv_ab_evidence_doctor: generate_board_phase060_rvv_ab_summary
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(MC_PHASE060_RVV_AB_MANIFEST_JSON) \
		--output $(MC_PHASE060_RVV_AB_DOCTOR_MD) \
		--fail-on never

.PHONY: generate_board_edge_interpolation_summary run_board_edge_interpolation_evidence_doctor generate_board_prepass_summary run_board_prepass_evidence_doctor generate_board_production_direct_summary run_board_production_direct_evidence_doctor generate_board_generic_xyz_summary run_board_generic_xyz_evidence_doctor generate_board_generic_xyzi_summary run_board_generic_xyzi_evidence_doctor generate_board_generic_xyzrgb_summary run_board_generic_xyzrgb_evidence_doctor generate_board_generic_xyzrgba_summary run_board_generic_xyzrgba_evidence_doctor run_board_generic_repeated_evidence_doctor generate_board_phase060_rvv_ab_summary run_board_phase060_rvv_ab_evidence_doctor
