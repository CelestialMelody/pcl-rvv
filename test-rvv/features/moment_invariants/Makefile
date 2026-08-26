# =============================================================================
# PCL features/moment_invariants RVV diagnostic Makefile
#
# 本 topic 先隔离 MomentInvariantsEstimation 中 centroid 之后的中心矩累加。
# QEMU 只用于 correctness（正确性）和日志形状；性能结论必须来自板卡。
# =============================================================================

TOPIC  := moment_invariants
MODULE := features

FOCUS_DIR      := pcl/features/include/pcl/features/impl/moment_invariants.hpp
FILTER_REPORT  := log/filtered_moment_invariants.log
ANALYZE_REPORT := log/analyze_moment_invariants.log

TARGET_TEST     := test_moment_invariants
TARGET_TEST_STD := test_moment_invariants_std
TARGET_TEST_RVV := test_moment_invariants_rvv
BOARD_TARGET_TEST := test_moment_invariants_rvv
SRCS_TEST       := src/test_moment_invariants.cpp

TARGET_BENCH     := bench_moment_invariants
TARGET_BENCH_STD := bench_moment_invariants_std
TARGET_BENCH_RVV := bench_moment_invariants_rvv
SRCS_BENCH       := src/bench_moment_invariants.cpp

INCLUDES = \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/features/include \
	-I$(PCL_SOURCE_ROOT)/search/include \
	-I$(PCL_SOURCE_ROOT)/kdtree/include \
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

LIBS_BENCH := \
	-lpcl_features \
	-lpcl_search \
	-lpcl_kdtree \
	-lpcl_common \
	-lflann_cpp \
	-lboost_system \
	-llz4 \
	-lm

REMOTE_DIR ?= /root/pcl-test/features/moment_invariants

MI_REPEATED_RUNS ?= 5
MI_REPEATED_DIR ?= log/board/repeated_phase000_moment_accumulation_diagnostic
MI_REPEATED_LABEL ?= moment_invariants_accumulation_diagnostic_phase000
MI_REPEATED_CASE ?= mi_accumulation_indexed
MI_REPEATED_ARGS ?= --case-filter mi_accumulation_indexed --points 262144 --iterations 30 --warmup-iterations 4
MI_REPEATED_TITLE ?= Moment invariants accumulation diagnostic repeated board summary
MI_REPEATED_EVIDENCE_ROLE ?= diagnostic
MI_REPEATED_AB_BOUNDARY ?= test helper
MI_REPEATED_BOUNDARY ?= test_helper
MI_REPEATED_TIMER_BOUNDARY ?= helper_only_moment_accumulation_after_centroid
MI_REPEATED_ROW_SOURCE ?= indexed_cloud
MI_REPEATED_CASE_KIND ?= component-candidate
MI_REPEATED_GROUP ?= moment-invariants-accumulation-diagnostic
MI_REPEATED_POINT_TYPE ?= PointXYZ
MI_REPEATED_GATE ?= PointXYZ indexed AoS diagnostic
MI_REPEATED_ASM_BOUNDARY ?= computeMomentSummaryRVV inlined into bench_moment_invariants
MI_REPEATED_BOUNDARY_NOTE ?= 这是未接 production 的 diagnostic（诊断）性能证据。它证明 test helper boundary（测试 helper 边界）下中心矩累加的板卡性能信号，不能替代真实 `MomentInvariantsEstimation::computeFeature` production direct（真实生产路径）证据。
MI_REPEATED_REGISTRY_LABEL ?= board-mi-accumulation-diagnostic-phase000
MI_REPEATED_REGISTRY_TARGET ?= collect_board_mi_repeated
MI_REPEATED_DOC_REF ?= test-rvv/features/moment_invariants/doc/phases/000-current-state-and-diagnostic-plan/result.zh.md
MI_REPEATED_SUMMARY := $(MI_REPEATED_DIR)/summary.md
MI_REPEATED_MANIFEST := $(MI_REPEATED_DIR)/evidence_manifest.json
MI_REPEATED_DOCTOR_MD := $(MI_REPEATED_DIR)/evidence_doctor.md
MI_REPEATED_DOCTOR_JSON := $(MI_REPEATED_DIR)/evidence_doctor.json

MI_PHASE010_REPEATED_DIR ?= log/board/repeated_phase010_public_search_shape_diagnostic
MI_PHASE010_REPEATED_LABEL ?= moment_invariants_public_search_shape_diagnostic_phase010
MI_PHASE010_REPEATED_CASE ?= mi_public_search_shape
MI_PHASE010_REPEATED_ARGS ?= --case-filter mi_public_search_shape --points 4096 --iterations 8 --warmup-iterations 2
MI_PHASE010_REPEATED_TITLE ?= Moment invariants public-search-shaped diagnostic repeated board summary
MI_PHASE010_REPEATED_EVIDENCE_ROLE ?= production_shaped_diagnostic
MI_PHASE010_REPEATED_AB_BOUNDARY ?= production-shaped helper
MI_PHASE010_REPEATED_BOUNDARY ?= production_shaped_helper
MI_PHASE010_REPEATED_TIMER_BOUNDARY ?= kd_tree_search_plus_moment_accumulation_helper_replacement
MI_PHASE010_REPEATED_ROW_SOURCE ?= kd_tree_k_neighbor_query
MI_PHASE010_REPEATED_CASE_KIND ?= production-shaped-diagnostic
MI_PHASE010_REPEATED_GROUP ?= moment-invariants-public-search-shape-diagnostic
MI_PHASE010_REPEATED_GATE ?= PointXYZ KdTree nearestKSearch plus test-only helper replacement
MI_PHASE010_REPEATED_BOUNDARY_NOTE ?= 这是未接 production 的 production-shaped diagnostic（生产形态诊断）性能证据。它把真实 `pcl::search::KdTree<PointXYZ>` nearestKSearch（近邻搜索）外层和测试专用 Std/RVV helper replacement（helper 替换）放进同一计时边界，仍不能替代 `MomentInvariantsEstimation::computeFeature` production direct（真实生产路径）证据。
MI_PHASE010_REPEATED_REGISTRY_LABEL ?= board-mi-public-search-shape-diagnostic-phase010
MI_PHASE010_REPEATED_DOC_REF ?= test-rvv/features/moment_invariants/doc/phases/010-public-search-dilution-check/result.zh.md

MI_PRODUCTION_REPEATED_DIR ?= log/board/repeated_phase030_production_compute_feature
MI_PRODUCTION_REPEATED_LABEL ?= moment_invariants_production_compute_feature_phase030
MI_PRODUCTION_REPEATED_CASE ?= mi_production_compute_feature
MI_PRODUCTION_REPEATED_ARGS ?= --case-filter mi_production_compute_feature --points 4096 --iterations 8 --warmup-iterations 2
MI_PRODUCTION_REPEATED_TITLE ?= Moment invariants production computeFeature repeated board summary
MI_PRODUCTION_REPEATED_EVIDENCE_ROLE ?= production-public
MI_PRODUCTION_REPEATED_AB_BOUNDARY ?= public overload
MI_PRODUCTION_REPEATED_BOUNDARY ?= public_overload
MI_PRODUCTION_REPEATED_TIMER_BOUNDARY ?= public_compute_feature_with_kdtree_search_and_output_write
MI_PRODUCTION_REPEATED_ROW_SOURCE ?= kd_tree_k_neighbor_query
MI_PRODUCTION_REPEATED_CASE_KIND ?= production-direct
MI_PRODUCTION_REPEATED_GROUP ?= moment-invariants-production-compute-feature
MI_PRODUCTION_REPEATED_POINT_TYPE ?= PointXYZ
MI_PRODUCTION_REPEATED_GATE ?= __RVV10__ + exact PointXYZ + exact MomentInvariants output + k-neighbor indexed moment accumulation
MI_PRODUCTION_REPEATED_ASM_BOUNDARY ?= momentInvariantsIndexedMomentsRVV / computePointMomentInvariants production symbols
MI_PRODUCTION_REPEATED_BOUNDARY_NOTE ?= 这是 production-public（生产公开入口）性能证据。Std/RVV 两侧都调用真实 `MomentInvariantsEstimation<PointXYZ, MomentInvariants>::compute`，计时边界包含 KdTree nearestKSearch（近邻搜索）、indexed moment accumulation（索引矩累加）和 output write（输出写回），不包含点云构造或 KdTree 初始化。
MI_PRODUCTION_REPEATED_REGISTRY_LABEL ?= board-mi-production-compute-feature-phase030
MI_PRODUCTION_REPEATED_DOC_REF ?= test-rvv/features/moment_invariants/doc/phases/030-production-integration-probe/result.zh.md

include ../../mk/rvv-topic.mk

dump_test_rvv: | $(ASM_DIR)/$(ARCH)
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_TEST=$(TARGET_TEST_RVV) $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_RVV)
	$(OBJDUMP) -d -C $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_RVV) > $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).full.asm
	grep -E $(VEC_REGEX_STR) $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).full.asm > $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm || true
	@echo "[DONE] RVV test asm dump: $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm"

$(MI_REPEATED_DIR):
	mkdir -p $(MI_REPEATED_DIR)

collect_board_mi_repeated: deploy_board | $(MI_REPEATED_DIR)
	@rm -rf "$(MI_REPEATED_DIR)"/run-*
	@set -e; for run in $$(seq 1 $(MI_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(MI_REPEATED_LABEL)/run-$$run_id"; \
		local_dir="$(MI_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] moment_invariants repeated run $$run_id/$(MI_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(MI_REPEATED_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

summarize_board_mi_repeated:
	@$(PYTHON_RUN) script/generate_mi_repeated_summary.py \
		--input-dir $(MI_REPEATED_DIR) \
		--output $(MI_REPEATED_SUMMARY) \
		--manifest $(MI_REPEATED_MANIFEST) \
		--run-label $(MI_REPEATED_LABEL) \
		--case-label $(MI_REPEATED_CASE) \
		--expected-runs $(MI_REPEATED_RUNS) \
		--asm build/asm/riscv/bench_moment_invariants_rvv.asm \
		--title '$(MI_REPEATED_TITLE)' \
		--evidence-role '$(MI_REPEATED_EVIDENCE_ROLE)' \
		--ab-boundary '$(MI_REPEATED_AB_BOUNDARY)' \
		--boundary '$(MI_REPEATED_BOUNDARY)' \
		--timer-boundary '$(MI_REPEATED_TIMER_BOUNDARY)' \
		--row-source '$(MI_REPEATED_ROW_SOURCE)' \
		--case-kind '$(MI_REPEATED_CASE_KIND)' \
		--group '$(MI_REPEATED_GROUP)' \
		--point-type '$(MI_REPEATED_POINT_TYPE)' \
		--gate '$(MI_REPEATED_GATE)' \
		--asm-boundary '$(MI_REPEATED_ASM_BOUNDARY)' \
		--boundary-note '$(MI_REPEATED_BOUNDARY_NOTE)'

run_board_mi_repeated_evidence_doctor: summarize_board_mi_repeated
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(MI_REPEATED_MANIFEST) \
		--output $(MI_REPEATED_DOCTOR_MD) \
		--json-output $(MI_REPEATED_DOCTOR_JSON) \
		--fail-on never

record_board_mi_repeated_state: run_board_mi_repeated_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry log/evidence_registry.json \
		--summary $(MI_REPEATED_SUMMARY) \
		--manifest $(MI_REPEATED_MANIFEST) \
		--doctor $(MI_REPEATED_DOCTOR_MD) \
		--target $(MI_REPEATED_REGISTRY_TARGET) \
		--backend board \
		--run-label $(MI_REPEATED_REGISTRY_LABEL) \
		--case-filter $(MI_REPEATED_CASE) \
		--evidence-role '$(MI_REPEATED_EVIDENCE_ROLE)' \
		--doc-ref $(MI_REPEATED_DOC_REF) \
		--doc-ref test-rvv/features/moment_invariants/doc/moment_invariants-evaluation.zh.md \
		--freshness-state fresh

run_board_mi_repeated: collect_board_mi_repeated record_board_mi_repeated_state

record_board_mi_phase010_repeated_state:
	@$(MAKE) record_board_mi_repeated_state \
		MI_REPEATED_DIR=$(MI_PHASE010_REPEATED_DIR) \
		MI_REPEATED_LABEL=$(MI_PHASE010_REPEATED_LABEL) \
		MI_REPEATED_CASE=$(MI_PHASE010_REPEATED_CASE) \
		MI_REPEATED_TITLE='$(MI_PHASE010_REPEATED_TITLE)' \
		MI_REPEATED_EVIDENCE_ROLE='$(MI_PHASE010_REPEATED_EVIDENCE_ROLE)' \
		MI_REPEATED_AB_BOUNDARY='$(MI_PHASE010_REPEATED_AB_BOUNDARY)' \
		MI_REPEATED_BOUNDARY='$(MI_PHASE010_REPEATED_BOUNDARY)' \
		MI_REPEATED_TIMER_BOUNDARY='$(MI_PHASE010_REPEATED_TIMER_BOUNDARY)' \
		MI_REPEATED_ROW_SOURCE='$(MI_PHASE010_REPEATED_ROW_SOURCE)' \
		MI_REPEATED_CASE_KIND='$(MI_PHASE010_REPEATED_CASE_KIND)' \
		MI_REPEATED_GROUP='$(MI_PHASE010_REPEATED_GROUP)' \
		MI_REPEATED_GATE='$(MI_PHASE010_REPEATED_GATE)' \
		MI_REPEATED_BOUNDARY_NOTE='$(MI_PHASE010_REPEATED_BOUNDARY_NOTE)' \
		MI_REPEATED_REGISTRY_LABEL=$(MI_PHASE010_REPEATED_REGISTRY_LABEL) \
		MI_REPEATED_REGISTRY_TARGET=collect_board_mi_phase010_repeated \
		MI_REPEATED_DOC_REF=$(MI_PHASE010_REPEATED_DOC_REF)

run_board_mi_phase010_repeated:
	@$(MAKE) run_board_mi_repeated \
		MI_REPEATED_DIR=$(MI_PHASE010_REPEATED_DIR) \
		MI_REPEATED_LABEL=$(MI_PHASE010_REPEATED_LABEL) \
		MI_REPEATED_CASE=$(MI_PHASE010_REPEATED_CASE) \
		MI_REPEATED_ARGS='$(MI_PHASE010_REPEATED_ARGS)' \
		MI_REPEATED_TITLE='$(MI_PHASE010_REPEATED_TITLE)' \
		MI_REPEATED_EVIDENCE_ROLE='$(MI_PHASE010_REPEATED_EVIDENCE_ROLE)' \
		MI_REPEATED_AB_BOUNDARY='$(MI_PHASE010_REPEATED_AB_BOUNDARY)' \
		MI_REPEATED_BOUNDARY='$(MI_PHASE010_REPEATED_BOUNDARY)' \
		MI_REPEATED_TIMER_BOUNDARY='$(MI_PHASE010_REPEATED_TIMER_BOUNDARY)' \
		MI_REPEATED_ROW_SOURCE='$(MI_PHASE010_REPEATED_ROW_SOURCE)' \
		MI_REPEATED_CASE_KIND='$(MI_PHASE010_REPEATED_CASE_KIND)' \
		MI_REPEATED_GROUP='$(MI_PHASE010_REPEATED_GROUP)' \
		MI_REPEATED_GATE='$(MI_PHASE010_REPEATED_GATE)' \
		MI_REPEATED_BOUNDARY_NOTE='$(MI_PHASE010_REPEATED_BOUNDARY_NOTE)' \
		MI_REPEATED_REGISTRY_LABEL=$(MI_PHASE010_REPEATED_REGISTRY_LABEL) \
		MI_REPEATED_REGISTRY_TARGET=collect_board_mi_phase010_repeated \
		MI_REPEATED_DOC_REF=$(MI_PHASE010_REPEATED_DOC_REF)

check_production_rvv_asm: clean_bench_rvv dump_bench_rvv
	@$(PYTHON_RUN) script/check_mi_production_asm.py $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm

check_production_pointxyzi_rvv_asm: clean_bench_rvv dump_bench_rvv
	@$(PYTHON_RUN) script/check_mi_production_asm.py $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm \
		--symbol-fragment 'pcl::detail::momentInvariantsIndexedMomentsRVV<pcl::PointXYZI>' \
		--symbol-fragment 'pcl::MomentInvariantsEstimation<pcl::PointXYZI, pcl::MomentInvariants>::computePointMomentInvariants' \
		--symbol-fragment 'pcl::MomentInvariantsEstimation<pcl::PointXYZI, pcl::MomentInvariants>::computeFeature'

check_production_pointxyzrgb_rvv_asm: clean_bench_rvv dump_bench_rvv
	@$(PYTHON_RUN) script/check_mi_production_asm.py $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm \
		--symbol-fragment 'pcl::detail::momentInvariantsIndexedMomentsRVV<pcl::PointXYZRGB>' \
		--symbol-fragment 'pcl::MomentInvariantsEstimation<pcl::PointXYZRGB, pcl::MomentInvariants>::computePointMomentInvariants' \
		--symbol-fragment 'pcl::MomentInvariantsEstimation<pcl::PointXYZRGB, pcl::MomentInvariants>::computeFeature'

check_production_pointxyzrgba_rvv_asm: clean_bench_rvv dump_bench_rvv
	@$(PYTHON_RUN) script/check_mi_production_asm.py $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm \
		--symbol-fragment 'pcl::detail::momentInvariantsIndexedMomentsRVV<pcl::PointXYZRGBA>' \
		--symbol-fragment 'pcl::MomentInvariantsEstimation<pcl::PointXYZRGBA, pcl::MomentInvariants>::computePointMomentInvariants' \
		--symbol-fragment 'pcl::MomentInvariantsEstimation<pcl::PointXYZRGBA, pcl::MomentInvariants>::computeFeature'

record_board_mi_production_repeated_state:
	@$(MAKE) record_board_mi_repeated_state \
		MI_REPEATED_DIR=$(MI_PRODUCTION_REPEATED_DIR) \
		MI_REPEATED_LABEL=$(MI_PRODUCTION_REPEATED_LABEL) \
		MI_REPEATED_CASE=$(MI_PRODUCTION_REPEATED_CASE) \
		MI_REPEATED_TITLE='$(MI_PRODUCTION_REPEATED_TITLE)' \
		MI_REPEATED_EVIDENCE_ROLE='$(MI_PRODUCTION_REPEATED_EVIDENCE_ROLE)' \
		MI_REPEATED_AB_BOUNDARY='$(MI_PRODUCTION_REPEATED_AB_BOUNDARY)' \
		MI_REPEATED_BOUNDARY='$(MI_PRODUCTION_REPEATED_BOUNDARY)' \
		MI_REPEATED_TIMER_BOUNDARY='$(MI_PRODUCTION_REPEATED_TIMER_BOUNDARY)' \
		MI_REPEATED_ROW_SOURCE='$(MI_PRODUCTION_REPEATED_ROW_SOURCE)' \
		MI_REPEATED_CASE_KIND='$(MI_PRODUCTION_REPEATED_CASE_KIND)' \
		MI_REPEATED_GROUP='$(MI_PRODUCTION_REPEATED_GROUP)' \
		MI_REPEATED_POINT_TYPE='$(MI_PRODUCTION_REPEATED_POINT_TYPE)' \
		MI_REPEATED_GATE='$(MI_PRODUCTION_REPEATED_GATE)' \
		MI_REPEATED_ASM_BOUNDARY='$(MI_PRODUCTION_REPEATED_ASM_BOUNDARY)' \
		MI_REPEATED_BOUNDARY_NOTE='$(MI_PRODUCTION_REPEATED_BOUNDARY_NOTE)' \
		MI_REPEATED_REGISTRY_LABEL=$(MI_PRODUCTION_REPEATED_REGISTRY_LABEL) \
		MI_REPEATED_REGISTRY_TARGET=collect_board_mi_production_repeated \
		MI_REPEATED_DOC_REF=$(MI_PRODUCTION_REPEATED_DOC_REF)

run_board_mi_production_repeated:
	@$(MAKE) run_board_mi_repeated \
		MI_REPEATED_DIR=$(MI_PRODUCTION_REPEATED_DIR) \
		MI_REPEATED_LABEL=$(MI_PRODUCTION_REPEATED_LABEL) \
		MI_REPEATED_CASE=$(MI_PRODUCTION_REPEATED_CASE) \
		MI_REPEATED_ARGS='$(MI_PRODUCTION_REPEATED_ARGS)' \
		MI_REPEATED_TITLE='$(MI_PRODUCTION_REPEATED_TITLE)' \
		MI_REPEATED_EVIDENCE_ROLE='$(MI_PRODUCTION_REPEATED_EVIDENCE_ROLE)' \
		MI_REPEATED_AB_BOUNDARY='$(MI_PRODUCTION_REPEATED_AB_BOUNDARY)' \
		MI_REPEATED_BOUNDARY='$(MI_PRODUCTION_REPEATED_BOUNDARY)' \
		MI_REPEATED_TIMER_BOUNDARY='$(MI_PRODUCTION_REPEATED_TIMER_BOUNDARY)' \
		MI_REPEATED_ROW_SOURCE='$(MI_PRODUCTION_REPEATED_ROW_SOURCE)' \
		MI_REPEATED_CASE_KIND='$(MI_PRODUCTION_REPEATED_CASE_KIND)' \
		MI_REPEATED_GROUP='$(MI_PRODUCTION_REPEATED_GROUP)' \
		MI_REPEATED_POINT_TYPE='$(MI_PRODUCTION_REPEATED_POINT_TYPE)' \
		MI_REPEATED_GATE='$(MI_PRODUCTION_REPEATED_GATE)' \
		MI_REPEATED_ASM_BOUNDARY='$(MI_PRODUCTION_REPEATED_ASM_BOUNDARY)' \
		MI_REPEATED_BOUNDARY_NOTE='$(MI_PRODUCTION_REPEATED_BOUNDARY_NOTE)' \
		MI_REPEATED_REGISTRY_LABEL=$(MI_PRODUCTION_REPEATED_REGISTRY_LABEL) \
		MI_REPEATED_REGISTRY_TARGET=collect_board_mi_production_repeated \
		MI_REPEATED_DOC_REF=$(MI_PRODUCTION_REPEATED_DOC_REF)

run_board_mi_production_pointxyzi_repeated:
	@$(MAKE) run_board_mi_repeated \
		MI_REPEATED_DIR=log/board/repeated_phase040_production_pointxyzi_compute_feature \
		MI_REPEATED_LABEL=moment_invariants_production_pointxyzi_compute_feature_phase040 \
		MI_REPEATED_CASE=mi_production_compute_feature_pointxyzi \
		MI_REPEATED_ARGS='--case-filter mi_production_compute_feature_pointxyzi --points 4096 --iterations 8 --warmup-iterations 2' \
		MI_REPEATED_TITLE='Moment invariants production PointXYZI computeFeature repeated board summary' \
		MI_REPEATED_EVIDENCE_ROLE='production-public' \
		MI_REPEATED_AB_BOUNDARY='public overload' \
		MI_REPEATED_BOUNDARY='public_overload' \
		MI_REPEATED_TIMER_BOUNDARY='public_compute_feature_with_kdtree_search_and_output_write' \
		MI_REPEATED_ROW_SOURCE='kd_tree_k_neighbor_query' \
		MI_REPEATED_CASE_KIND='production-direct' \
		MI_REPEATED_GROUP='moment-invariants-production-typed-compute-feature' \
		MI_REPEATED_POINT_TYPE='PointXYZI' \
		MI_REPEATED_GATE='__RVV10__ + RVVXYZAoSFloatLayout<PointXYZI> + exact MomentInvariants output + k-neighbor indexed moment accumulation' \
		MI_REPEATED_ASM_BOUNDARY='momentInvariantsIndexedMomentsRVV / computePointMomentInvariants PointXYZI production symbols' \
		MI_REPEATED_BOUNDARY_NOTE='这是 production-public（生产公开入口）性能证据。Std/RVV 两侧都调用真实 `MomentInvariantsEstimation<PointXYZI, MomentInvariants>::compute`，计时边界包含 KdTree nearestKSearch（近邻搜索）、indexed moment accumulation（索引矩累加）和 output write（输出写回），不包含点云构造或 KdTree 初始化。' \
		MI_REPEATED_REGISTRY_LABEL=board-mi-production-pointxyzi-compute-feature-phase040 \
		MI_REPEATED_REGISTRY_TARGET=collect_board_mi_production_pointxyzi_repeated \
		MI_REPEATED_DOC_REF=test-rvv/features/moment_invariants/doc/phases/040-pointxyz-like-production-expansion/result.zh.md

run_board_mi_production_pointxyzrgb_repeated:
	@$(MAKE) run_board_mi_repeated \
		MI_REPEATED_DIR=log/board/repeated_phase040_production_pointxyzrgb_compute_feature \
		MI_REPEATED_LABEL=moment_invariants_production_pointxyzrgb_compute_feature_phase040 \
		MI_REPEATED_CASE=mi_production_compute_feature_pointxyzrgb \
		MI_REPEATED_ARGS='--case-filter mi_production_compute_feature_pointxyzrgb --points 4096 --iterations 8 --warmup-iterations 2' \
		MI_REPEATED_TITLE='Moment invariants production PointXYZRGB computeFeature repeated board summary' \
		MI_REPEATED_EVIDENCE_ROLE='production-public' \
		MI_REPEATED_AB_BOUNDARY='public overload' \
		MI_REPEATED_BOUNDARY='public_overload' \
		MI_REPEATED_TIMER_BOUNDARY='public_compute_feature_with_kdtree_search_and_output_write' \
		MI_REPEATED_ROW_SOURCE='kd_tree_k_neighbor_query' \
		MI_REPEATED_CASE_KIND='production-direct' \
		MI_REPEATED_GROUP='moment-invariants-production-typed-compute-feature' \
		MI_REPEATED_POINT_TYPE='PointXYZRGB' \
		MI_REPEATED_GATE='__RVV10__ + RVVXYZAoSFloatLayout<PointXYZRGB> + exact MomentInvariants output + k-neighbor indexed moment accumulation' \
		MI_REPEATED_ASM_BOUNDARY='momentInvariantsIndexedMomentsRVV / computePointMomentInvariants PointXYZRGB production symbols' \
		MI_REPEATED_BOUNDARY_NOTE='这是 production-public（生产公开入口）性能证据。Std/RVV 两侧都调用真实 `MomentInvariantsEstimation<PointXYZRGB, MomentInvariants>::compute`，计时边界包含 KdTree nearestKSearch（近邻搜索）、indexed moment accumulation（索引矩累加）和 output write（输出写回），不包含点云构造或 KdTree 初始化。' \
		MI_REPEATED_REGISTRY_LABEL=board-mi-production-pointxyzrgb-compute-feature-phase040 \
		MI_REPEATED_REGISTRY_TARGET=collect_board_mi_production_pointxyzrgb_repeated \
		MI_REPEATED_DOC_REF=test-rvv/features/moment_invariants/doc/phases/040-pointxyz-like-production-expansion/result.zh.md

run_board_mi_production_pointxyzrgba_repeated:
	@$(MAKE) run_board_mi_repeated \
		MI_REPEATED_DIR=log/board/repeated_phase040_production_pointxyzrgba_compute_feature \
		MI_REPEATED_LABEL=moment_invariants_production_pointxyzrgba_compute_feature_phase040 \
		MI_REPEATED_CASE=mi_production_compute_feature_pointxyzrgba \
		MI_REPEATED_ARGS='--case-filter mi_production_compute_feature_pointxyzrgba --points 4096 --iterations 8 --warmup-iterations 2' \
		MI_REPEATED_TITLE='Moment invariants production PointXYZRGBA computeFeature repeated board summary' \
		MI_REPEATED_EVIDENCE_ROLE='production-public' \
		MI_REPEATED_AB_BOUNDARY='public overload' \
		MI_REPEATED_BOUNDARY='public_overload' \
		MI_REPEATED_TIMER_BOUNDARY='public_compute_feature_with_kdtree_search_and_output_write' \
		MI_REPEATED_ROW_SOURCE='kd_tree_k_neighbor_query' \
		MI_REPEATED_CASE_KIND='production-direct' \
		MI_REPEATED_GROUP='moment-invariants-production-typed-compute-feature' \
		MI_REPEATED_POINT_TYPE='PointXYZRGBA' \
		MI_REPEATED_GATE='__RVV10__ + RVVXYZAoSFloatLayout<PointXYZRGBA> + exact MomentInvariants output + k-neighbor indexed moment accumulation' \
		MI_REPEATED_ASM_BOUNDARY='momentInvariantsIndexedMomentsRVV / computePointMomentInvariants PointXYZRGBA production symbols' \
		MI_REPEATED_BOUNDARY_NOTE='这是 production-public（生产公开入口）性能证据。Std/RVV 两侧都调用真实 `MomentInvariantsEstimation<PointXYZRGBA, MomentInvariants>::compute`，计时边界包含 KdTree nearestKSearch（近邻搜索）、indexed moment accumulation（索引矩累加）和 output write（输出写回），不包含点云构造或 KdTree 初始化。' \
		MI_REPEATED_REGISTRY_LABEL=board-mi-production-pointxyzrgba-compute-feature-phase040 \
		MI_REPEATED_REGISTRY_TARGET=collect_board_mi_production_pointxyzrgba_repeated \
		MI_REPEATED_DOC_REF=test-rvv/features/moment_invariants/doc/phases/040-pointxyz-like-production-expansion/result.zh.md

.PHONY: dump_test_rvv collect_board_mi_repeated summarize_board_mi_repeated run_board_mi_repeated_evidence_doctor record_board_mi_repeated_state run_board_mi_repeated record_board_mi_phase010_repeated_state run_board_mi_phase010_repeated check_production_rvv_asm check_production_pointxyzi_rvv_asm check_production_pointxyzrgb_rvv_asm check_production_pointxyzrgba_rvv_asm record_board_mi_production_repeated_state run_board_mi_production_repeated run_board_mi_production_pointxyzi_repeated run_board_mi_production_pointxyzrgb_repeated run_board_mi_production_pointxyzrgba_repeated
