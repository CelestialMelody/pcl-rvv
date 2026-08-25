# =============================================================================
# PCL features/moment_of_inertia_estimation RVV diagnostic Makefile
#
# 本 topic 先评估 MomentOfInertiaEstimation 的逐点规约热点。QEMU 只用于
# correctness（正确性）和日志形状；性能结论必须来自板卡或目标硬件。
# =============================================================================

TOPIC  := moment_of_inertia_estimation
MODULE := features

FOCUS_DIR      := pcl/features/include/pcl/features/impl/moment_of_inertia_estimation.hpp
FILTER_REPORT  := log/filtered_moi.log
ANALYZE_REPORT := log/analyze_moi.log

TARGET_TEST     := test_moi
TARGET_TEST_STD := test_moi_std
TARGET_TEST_RVV := test_moi_rvv
SRCS_TEST       := src/test_moi.cpp

TARGET_BENCH     := bench_moi
TARGET_BENCH_STD := bench_moi_std
TARGET_BENCH_RVV := bench_moi_rvv
SRCS_BENCH       := src/bench_moi.cpp

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

LIBS_TEST  := -lpcl_features -lpcl_common -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lpcl_features -lpcl_common -lm

MOI_EVIDENCE_REGISTRY := log/evidence_registry.json
MOI_BOARD_REPEATED_DIR ?= log/board/repeated_phase000_reduction_diagnostic
MOI_BOARD_REPEATED_RUNS ?= 5
MOI_BOARD_REPEATED_LABEL ?= moi_reduction_diagnostic_repeated_phase000
MOI_BOARD_REPEATED_CASE ?= moi_reductions
MOI_BOARD_REPEATED_GROUP ?= moi-reduction-diagnostic
MOI_BOARD_REPEATED_TITLE ?= MOI reduction diagnostic repeated board summary
MOI_BOARD_REPEATED_EVIDENCE_ROLE ?= diagnostic
MOI_BOARD_REPEATED_AB_BOUNDARY ?= test helper
MOI_BOARD_REPEATED_BOUNDARY ?= test_helper
MOI_BOARD_REPEATED_CASE_KIND ?= component-candidate
MOI_BOARD_REPEATED_POINT_TYPE ?= PointXYZ
MOI_BOARD_REPEATED_TIMER_BOUNDARY ?= helper_only_reduction_summary_no_full_compute
MOI_BOARD_REPEATED_REDUCTION_LABEL ?= mean_aabb_covariance_moment_obb_extrema
MOI_BOARD_REPEATED_ASM_BOUNDARY ?= computeReductionSummaryRVV inlined into bench_moi
MOI_BOARD_REPEATED_GATE ?= PointXYZ indexed AoS diagnostic
MOI_BOARD_REPEATED_DOC_REF ?= doc/phases/000-current-state-and-reduction-diagnostic/result.zh.md
MOI_BOARD_REPEATED_RUN_LABEL_REGISTRY ?= board-moi-reduction-diagnostic-repeated-phase000
MOI_BOARD_REPEATED_SUMMARY := $(MOI_BOARD_REPEATED_DIR)/summary.md
MOI_BOARD_REPEATED_MANIFEST := $(MOI_BOARD_REPEATED_DIR)/evidence_manifest.json
MOI_BOARD_REPEATED_DOCTOR_MD := $(MOI_BOARD_REPEATED_DIR)/evidence_doctor.md
MOI_REPEATED_BENCH_ARGS ?= --case-filter moi_reductions --points 262144 --iterations 20 --warmup-iterations 3

MOI_PHASE010_BOARD_REPEATED_DIR ?= log/board/repeated_phase010_projected_covariance_diagnostic
MOI_PHASE010_BOARD_REPEATED_LABEL ?= moi_projected_covariance_diagnostic_repeated_phase010
MOI_PHASE010_REPEATED_BENCH_ARGS ?= --case-filter moi_projected_covariance --points 262144 --iterations 20 --warmup-iterations 3

MOI_PHASE030_BOARD_REPEATED_DIR ?= log/board/repeated_phase030_public_compute_production
MOI_PHASE030_BOARD_REPEATED_LABEL ?= moi_public_compute_production_repeated_phase030
MOI_PHASE030_REPEATED_BENCH_ARGS ?= --case-filter moi_public_compute --points 65536 --iterations 5 --warmup-iterations 1

MOI_PHASE040_BOARD_REPEATED_DIR ?= log/board/repeated_phase040_projected_covariance_production
MOI_PHASE040_BOARD_REPEATED_LABEL ?= moi_projected_covariance_production_repeated_phase040
MOI_PHASE040_REPEATED_BENCH_ARGS ?= --case-filter moi_public_compute --points 65536 --iterations 5 --warmup-iterations 1

MOI_PHASE050_POINTXYZI_BOARD_REPEATED_DIR ?= log/board/repeated_phase050_pointxyzi_production
MOI_PHASE050_POINTXYZI_BOARD_REPEATED_LABEL ?= moi_pointxyzi_production_repeated_phase050
MOI_PHASE050_POINTXYZI_REPEATED_BENCH_ARGS ?= --case-filter moi_public_compute_pointxyzi --points 65536 --iterations 5 --warmup-iterations 1

MOI_PHASE050_POINTXYZRGB_BOARD_REPEATED_DIR ?= log/board/repeated_phase050_pointxyzrgb_production
MOI_PHASE050_POINTXYZRGB_BOARD_REPEATED_LABEL ?= moi_pointxyzrgb_production_repeated_phase050
MOI_PHASE050_POINTXYZRGB_REPEATED_BENCH_ARGS ?= --case-filter moi_public_compute_pointxyzrgb --points 65536 --iterations 5 --warmup-iterations 1

MOI_PHASE050_POINTXYZRGBA_BOARD_REPEATED_DIR ?= log/board/repeated_phase050_pointxyzrgba_production
MOI_PHASE050_POINTXYZRGBA_BOARD_REPEATED_LABEL ?= moi_pointxyzrgba_production_repeated_phase050
MOI_PHASE050_POINTXYZRGBA_REPEATED_BENCH_ARGS ?= --case-filter moi_public_compute_pointxyzrgba --points 65536 --iterations 5 --warmup-iterations 1

MOI_PHASE050_POINTXYZRGBNORMAL_BOARD_REPEATED_DIR ?= log/board/repeated_phase050_pointxyzrgbnormal_production
MOI_PHASE050_POINTXYZRGBNORMAL_BOARD_REPEATED_LABEL ?= moi_pointxyzrgbnormal_production_repeated_phase050
MOI_PHASE050_POINTXYZRGBNORMAL_REPEATED_BENCH_ARGS ?= --case-filter moi_public_compute_pointxyzrgbnormal --points 65536 --iterations 5 --warmup-iterations 1

include ../../mk/rvv-topic.mk

dump_test_rvv: | $(ASM_DIR)/$(ARCH)
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_TEST=$(TARGET_TEST_RVV) $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_RVV)
	$(OBJDUMP) -d -C $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_RVV) > $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).full.asm
	grep -E $(VEC_REGEX_STR) $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).full.asm > $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm || true
	@echo "[DONE] RVV test asm dump: $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm"

$(MOI_BOARD_REPEATED_DIR):
	mkdir -p $(MOI_BOARD_REPEATED_DIR)

collect_board_moi_repeated: deploy_board | $(MOI_BOARD_REPEATED_DIR)
	@rm -rf "$(MOI_BOARD_REPEATED_DIR)"/run-*
	@set -e; for run in $$(seq 1 $(MOI_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(MOI_BOARD_REPEATED_LABEL)/run-$$run_id"; \
		local_dir="$(MOI_BOARD_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] MOI $(MOI_BOARD_REPEATED_CASE) diagnostic repeated run $$run_id/$(MOI_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(MOI_REPEATED_BENCH_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

summarize_board_moi_repeated:
	@$(PYTHON_RUN) script/generate_moi_repeated_summary.py \
		--input-dir $(MOI_BOARD_REPEATED_DIR) \
		--output $(MOI_BOARD_REPEATED_SUMMARY) \
		--manifest $(MOI_BOARD_REPEATED_MANIFEST) \
		--run-label $(MOI_BOARD_REPEATED_LABEL) \
		--case-label $(MOI_BOARD_REPEATED_CASE) \
		--group $(MOI_BOARD_REPEATED_GROUP) \
		--title '$(MOI_BOARD_REPEATED_TITLE)' \
		--evidence-role $(MOI_BOARD_REPEATED_EVIDENCE_ROLE) \
		--ab-boundary '$(MOI_BOARD_REPEATED_AB_BOUNDARY)' \
		--boundary $(MOI_BOARD_REPEATED_BOUNDARY) \
		--case-kind $(MOI_BOARD_REPEATED_CASE_KIND) \
		--point-type '$(MOI_BOARD_REPEATED_POINT_TYPE)' \
		--timer-boundary $(MOI_BOARD_REPEATED_TIMER_BOUNDARY) \
		--reduction-label $(MOI_BOARD_REPEATED_REDUCTION_LABEL) \
		--asm-boundary '$(MOI_BOARD_REPEATED_ASM_BOUNDARY)' \
		--gate '$(MOI_BOARD_REPEATED_GATE)' \
		--expected-runs $(MOI_BOARD_REPEATED_RUNS) \
		--asm build/asm/riscv/bench_moi_rvv.asm

run_board_moi_repeated_evidence_doctor: summarize_board_moi_repeated
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(MOI_BOARD_REPEATED_MANIFEST) \
		--output $(MOI_BOARD_REPEATED_DOCTOR_MD) \
		--fail-on never

record_board_moi_repeated_state: run_board_moi_repeated_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(MOI_EVIDENCE_REGISTRY) \
		--summary $(MOI_BOARD_REPEATED_SUMMARY) \
		--manifest $(MOI_BOARD_REPEATED_MANIFEST) \
		--doctor $(MOI_BOARD_REPEATED_DOCTOR_MD) \
		--target collect_board_moi_repeated \
		--backend board \
		--run-label $(MOI_BOARD_REPEATED_RUN_LABEL_REGISTRY) \
		--case-filter $(MOI_BOARD_REPEATED_CASE) \
		--evidence-role $(MOI_BOARD_REPEATED_EVIDENCE_ROLE) \
		--doc-ref $(MOI_BOARD_REPEATED_DOC_REF) \
		--doc-ref doc/moment_of_inertia_estimation-evaluation.zh.md \
		--freshness-state fresh

run_board_moi_repeated: collect_board_moi_repeated record_board_moi_repeated_state

run_board_moi_phase010_repeated:
	@$(MAKE) run_board_moi_repeated \
		MOI_BOARD_REPEATED_DIR=$(MOI_PHASE010_BOARD_REPEATED_DIR) \
		MOI_BOARD_REPEATED_LABEL=$(MOI_PHASE010_BOARD_REPEATED_LABEL) \
		MOI_REPEATED_BENCH_ARGS='$(MOI_PHASE010_REPEATED_BENCH_ARGS)' \
		MOI_BOARD_REPEATED_CASE=moi_projected_covariance \
		MOI_BOARD_REPEATED_GROUP=moi-projected-covariance-diagnostic \
		MOI_BOARD_REPEATED_TITLE='MOI projected covariance diagnostic repeated board summary' \
		MOI_BOARD_REPEATED_TIMER_BOUNDARY=helper_only_projected_covariance_no_full_compute \
		MOI_BOARD_REPEATED_REDUCTION_LABEL=projected_covariance_6_terms \
		MOI_BOARD_REPEATED_ASM_BOUNDARY='computeProjectedCovarianceRVV inlined into bench_moi' \
		MOI_BOARD_REPEATED_DOC_REF=doc/phases/010-projected-covariance-fusion-diagnostic/result.zh.md \
		MOI_BOARD_REPEATED_RUN_LABEL_REGISTRY=board-moi-projected-covariance-diagnostic-repeated-phase010

run_board_moi_phase030_public_compute_repeated:
	@$(MAKE) run_board_moi_repeated \
		MOI_BOARD_REPEATED_DIR=$(MOI_PHASE030_BOARD_REPEATED_DIR) \
		MOI_BOARD_REPEATED_LABEL=$(MOI_PHASE030_BOARD_REPEATED_LABEL) \
		MOI_REPEATED_BENCH_ARGS='$(MOI_PHASE030_REPEATED_BENCH_ARGS)' \
		MOI_BOARD_REPEATED_CASE=moi_public_compute \
		MOI_BOARD_REPEATED_GROUP=moi-public-compute-production \
		MOI_BOARD_REPEATED_TITLE='MOI public compute production repeated board summary' \
		MOI_BOARD_REPEATED_EVIDENCE_ROLE=production-public \
		MOI_BOARD_REPEATED_AB_BOUNDARY='public MomentOfInertiaEstimation::compute()' \
		MOI_BOARD_REPEATED_BOUNDARY=production_public \
		MOI_BOARD_REPEATED_CASE_KIND=production-public \
		MOI_BOARD_REPEATED_POINT_TYPE='PointXYZ via RVVXYZAoSFloatLayout' \
		MOI_BOARD_REPEATED_TIMER_BOUNDARY=public_compute_angle_step_45_mean_aabb_rvv_only \
		MOI_BOARD_REPEATED_REDUCTION_LABEL=public_compute_with_mean_aabb_rvv_dispatch \
		MOI_BOARD_REPEATED_ASM_BOUNDARY='computeMeanValueRVV inlined into public compute bench' \
		MOI_BOARD_REPEATED_GATE='PointXYZ-like RVVXYZAoSFloatLayout indexed AoS production dispatch' \
		MOI_BOARD_REPEATED_DOC_REF=doc/phases/030-production-mean-aabb-pi2-pi5/result.zh.md \
		MOI_BOARD_REPEATED_RUN_LABEL_REGISTRY=board-moi-public-compute-production-repeated-phase030

run_board_moi_phase040_projected_covariance_repeated:
	@$(MAKE) run_board_moi_repeated \
		MOI_BOARD_REPEATED_DIR=$(MOI_PHASE040_BOARD_REPEATED_DIR) \
		MOI_BOARD_REPEATED_LABEL=$(MOI_PHASE040_BOARD_REPEATED_LABEL) \
		MOI_REPEATED_BENCH_ARGS='$(MOI_PHASE040_REPEATED_BENCH_ARGS)' \
		MOI_BOARD_REPEATED_CASE=moi_public_compute \
		MOI_BOARD_REPEATED_GROUP=moi-projected-covariance-production \
		MOI_BOARD_REPEATED_TITLE='MOI projected covariance production repeated board summary' \
		MOI_BOARD_REPEATED_EVIDENCE_ROLE=production-public \
		MOI_BOARD_REPEATED_AB_BOUNDARY='public MomentOfInertiaEstimation::compute()' \
		MOI_BOARD_REPEATED_BOUNDARY=production_public \
		MOI_BOARD_REPEATED_CASE_KIND=production-public \
		MOI_BOARD_REPEATED_POINT_TYPE='PointXYZ via RVVXYZAoSFloatLayout' \
		MOI_BOARD_REPEATED_TIMER_BOUNDARY=public_compute_angle_step_45_projected_covariance_rvv \
		MOI_BOARD_REPEATED_REDUCTION_LABEL=public_compute_with_projected_covariance_rvv_dispatch \
		MOI_BOARD_REPEATED_ASM_BOUNDARY='computeProjectedCovarianceRVV inlined into public compute bench' \
		MOI_BOARD_REPEATED_GATE='PointXYZ-like RVVXYZAoSFloatLayout indexed AoS production dispatch' \
		MOI_BOARD_REPEATED_DOC_REF=doc/phases/040-projected-covariance-production-probe/result.zh.md \
		MOI_BOARD_REPEATED_RUN_LABEL_REGISTRY=board-moi-projected-covariance-production-repeated-phase040

run_board_moi_phase050_pointxyzi_repeated:
	@$(MAKE) run_board_moi_repeated \
		MOI_BOARD_REPEATED_DIR=$(MOI_PHASE050_POINTXYZI_BOARD_REPEATED_DIR) \
		MOI_BOARD_REPEATED_LABEL=$(MOI_PHASE050_POINTXYZI_BOARD_REPEATED_LABEL) \
		MOI_REPEATED_BENCH_ARGS='$(MOI_PHASE050_POINTXYZI_REPEATED_BENCH_ARGS)' \
		MOI_BOARD_REPEATED_CASE=moi_public_compute_pointxyzi \
		MOI_BOARD_REPEATED_GROUP=moi-pointxyzi-production \
		MOI_BOARD_REPEATED_TITLE='MOI PointXYZI projected covariance production repeated board summary' \
		MOI_BOARD_REPEATED_EVIDENCE_ROLE=production-public \
		MOI_BOARD_REPEATED_AB_BOUNDARY='public MomentOfInertiaEstimation<PointXYZI>::compute()' \
		MOI_BOARD_REPEATED_BOUNDARY=production_public \
		MOI_BOARD_REPEATED_CASE_KIND=production-public \
		MOI_BOARD_REPEATED_POINT_TYPE='PointXYZI via RVVXYZAoSFloatLayout' \
		MOI_BOARD_REPEATED_TIMER_BOUNDARY=public_compute_pointxyzi_projected_covariance_rvv \
		MOI_BOARD_REPEATED_REDUCTION_LABEL=public_compute_pointxyzi_with_projected_covariance_rvv_dispatch \
		MOI_BOARD_REPEATED_ASM_BOUNDARY='computeProjectedCovarianceRVV inlined into PointXYZI public compute bench' \
		MOI_BOARD_REPEATED_GATE='PointXYZI-like RVVXYZAoSFloatLayout indexed AoS production dispatch' \
		MOI_BOARD_REPEATED_DOC_REF=doc/phases/050-point-type-expansion/result.zh.md \
		MOI_BOARD_REPEATED_RUN_LABEL_REGISTRY=board-moi-pointxyzi-production-repeated-phase050

run_board_moi_phase050_pointxyzrgb_repeated:
	@$(MAKE) run_board_moi_repeated \
		MOI_BOARD_REPEATED_DIR=$(MOI_PHASE050_POINTXYZRGB_BOARD_REPEATED_DIR) \
		MOI_BOARD_REPEATED_LABEL=$(MOI_PHASE050_POINTXYZRGB_BOARD_REPEATED_LABEL) \
		MOI_REPEATED_BENCH_ARGS='$(MOI_PHASE050_POINTXYZRGB_REPEATED_BENCH_ARGS)' \
		MOI_BOARD_REPEATED_CASE=moi_public_compute_pointxyzrgb \
		MOI_BOARD_REPEATED_GROUP=moi-pointxyzrgb-production \
		MOI_BOARD_REPEATED_TITLE='MOI PointXYZRGB projected covariance production repeated board summary' \
		MOI_BOARD_REPEATED_EVIDENCE_ROLE=production-public \
		MOI_BOARD_REPEATED_AB_BOUNDARY='public MomentOfInertiaEstimation<PointXYZRGB>::compute()' \
		MOI_BOARD_REPEATED_BOUNDARY=production_public \
		MOI_BOARD_REPEATED_CASE_KIND=production-public \
		MOI_BOARD_REPEATED_POINT_TYPE='PointXYZRGB via RVVXYZAoSFloatLayout' \
		MOI_BOARD_REPEATED_TIMER_BOUNDARY=public_compute_pointxyzrgb_projected_covariance_rvv \
		MOI_BOARD_REPEATED_REDUCTION_LABEL=public_compute_pointxyzrgb_with_projected_covariance_rvv_dispatch \
		MOI_BOARD_REPEATED_ASM_BOUNDARY='computeProjectedCovarianceRVV inlined into PointXYZRGB public compute bench' \
		MOI_BOARD_REPEATED_GATE='PointXYZRGB-like RVVXYZAoSFloatLayout indexed AoS production dispatch' \
		MOI_BOARD_REPEATED_DOC_REF=doc/phases/050-point-type-expansion/result.zh.md \
		MOI_BOARD_REPEATED_RUN_LABEL_REGISTRY=board-moi-pointxyzrgb-production-repeated-phase050

run_board_moi_phase050_pointxyzrgba_repeated:
	@$(MAKE) run_board_moi_repeated \
		MOI_BOARD_REPEATED_DIR=$(MOI_PHASE050_POINTXYZRGBA_BOARD_REPEATED_DIR) \
		MOI_BOARD_REPEATED_LABEL=$(MOI_PHASE050_POINTXYZRGBA_BOARD_REPEATED_LABEL) \
		MOI_REPEATED_BENCH_ARGS='$(MOI_PHASE050_POINTXYZRGBA_REPEATED_BENCH_ARGS)' \
		MOI_BOARD_REPEATED_CASE=moi_public_compute_pointxyzrgba \
		MOI_BOARD_REPEATED_GROUP=moi-pointxyzrgba-production \
		MOI_BOARD_REPEATED_TITLE='MOI PointXYZRGBA projected covariance production repeated board summary' \
		MOI_BOARD_REPEATED_EVIDENCE_ROLE=production-public \
		MOI_BOARD_REPEATED_AB_BOUNDARY='public MomentOfInertiaEstimation<PointXYZRGBA>::compute()' \
		MOI_BOARD_REPEATED_BOUNDARY=production_public \
		MOI_BOARD_REPEATED_CASE_KIND=production-public \
		MOI_BOARD_REPEATED_POINT_TYPE='PointXYZRGBA via RVVXYZAoSFloatLayout' \
		MOI_BOARD_REPEATED_TIMER_BOUNDARY=public_compute_pointxyzrgba_projected_covariance_rvv \
		MOI_BOARD_REPEATED_REDUCTION_LABEL=public_compute_pointxyzrgba_with_projected_covariance_rvv_dispatch \
		MOI_BOARD_REPEATED_ASM_BOUNDARY='computeProjectedCovarianceRVV inlined into PointXYZRGBA public compute bench' \
		MOI_BOARD_REPEATED_GATE='PointXYZRGBA-like RVVXYZAoSFloatLayout indexed AoS production dispatch' \
		MOI_BOARD_REPEATED_DOC_REF=doc/phases/050-point-type-expansion/result.zh.md \
		MOI_BOARD_REPEATED_RUN_LABEL_REGISTRY=board-moi-pointxyzrgba-production-repeated-phase050

run_board_moi_phase050_pointxyzrgbnormal_repeated:
	@$(MAKE) run_board_moi_repeated \
		MOI_BOARD_REPEATED_DIR=$(MOI_PHASE050_POINTXYZRGBNORMAL_BOARD_REPEATED_DIR) \
		MOI_BOARD_REPEATED_LABEL=$(MOI_PHASE050_POINTXYZRGBNORMAL_BOARD_REPEATED_LABEL) \
		MOI_REPEATED_BENCH_ARGS='$(MOI_PHASE050_POINTXYZRGBNORMAL_REPEATED_BENCH_ARGS)' \
		MOI_BOARD_REPEATED_CASE=moi_public_compute_pointxyzrgbnormal \
		MOI_BOARD_REPEATED_GROUP=moi-pointxyzrgbnormal-production \
		MOI_BOARD_REPEATED_TITLE='MOI PointXYZRGBNormal projected covariance production repeated board summary' \
		MOI_BOARD_REPEATED_EVIDENCE_ROLE=production-public \
		MOI_BOARD_REPEATED_AB_BOUNDARY='public MomentOfInertiaEstimation<PointXYZRGBNormal>::compute()' \
		MOI_BOARD_REPEATED_BOUNDARY=production_public \
		MOI_BOARD_REPEATED_CASE_KIND=production-public \
		MOI_BOARD_REPEATED_POINT_TYPE='PointXYZRGBNormal via RVVXYZAoSFloatLayout' \
		MOI_BOARD_REPEATED_TIMER_BOUNDARY=public_compute_pointxyzrgbnormal_projected_covariance_rvv \
		MOI_BOARD_REPEATED_REDUCTION_LABEL=public_compute_pointxyzrgbnormal_with_projected_covariance_rvv_dispatch \
		MOI_BOARD_REPEATED_ASM_BOUNDARY='computeProjectedCovarianceRVV inlined into PointXYZRGBNormal public compute bench' \
		MOI_BOARD_REPEATED_GATE='PointXYZRGBNormal-like RVVXYZAoSFloatLayout indexed AoS production dispatch' \
		MOI_BOARD_REPEATED_DOC_REF=doc/phases/050-point-type-expansion/result.zh.md \
		MOI_BOARD_REPEATED_RUN_LABEL_REGISTRY=board-moi-pointxyzrgbnormal-production-repeated-phase050

evidence_status:
	@$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry $(MOI_EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase000_reduction_diagnostic/summary.md' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase000_reduction_diagnostic/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase000_reduction_diagnostic/evidence_doctor.md' \
		--doc test-rvv/features/moment_of_inertia_estimation/doc/phases/000-current-state-and-reduction-diagnostic/result.zh.md \
		--doc test-rvv/features/moment_of_inertia_estimation/doc/moment_of_inertia_estimation-evaluation.zh.md \
		--fail-on never

evidence_status_phase010:
	@$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry $(MOI_EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase010_projected_covariance_diagnostic/summary.md' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase010_projected_covariance_diagnostic/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase010_projected_covariance_diagnostic/evidence_doctor.md' \
		--doc test-rvv/features/moment_of_inertia_estimation/doc/phases/010-projected-covariance-fusion-diagnostic/result.zh.md \
		--doc test-rvv/features/moment_of_inertia_estimation/doc/moment_of_inertia_estimation-evaluation.zh.md \
		--fail-on never

evidence_status_phase030:
	@$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry $(MOI_EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase030_public_compute_production/summary.md' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase030_public_compute_production/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase030_public_compute_production/evidence_doctor.md' \
		--doc test-rvv/features/moment_of_inertia_estimation/doc/phases/030-production-mean-aabb-pi2-pi5/result.zh.md \
		--doc test-rvv/features/moment_of_inertia_estimation/doc/moment_of_inertia_estimation-evaluation.zh.md \
		--fail-on never

evidence_status_phase040:
	@$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry $(MOI_EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase040_projected_covariance_production/summary.md' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase040_projected_covariance_production/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase040_projected_covariance_production/evidence_doctor.md' \
		--doc test-rvv/features/moment_of_inertia_estimation/doc/phases/040-projected-covariance-production-probe/result.zh.md \
		--doc test-rvv/features/moment_of_inertia_estimation/doc/moment_of_inertia_estimation-evaluation.zh.md \
		--fail-on never

evidence_status_phase050:
	@$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry $(MOI_EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase050_pointxyzi_production/summary.md' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase050_pointxyzi_production/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase050_pointxyzi_production/evidence_doctor.md' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase050_pointxyzrgb_production/summary.md' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase050_pointxyzrgb_production/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase050_pointxyzrgb_production/evidence_doctor.md' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase050_pointxyzrgba_production/summary.md' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase050_pointxyzrgba_production/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase050_pointxyzrgba_production/evidence_doctor.md' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase050_pointxyzrgbnormal_production/summary.md' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase050_pointxyzrgbnormal_production/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase050_pointxyzrgbnormal_production/evidence_doctor.md' \
		--doc test-rvv/features/moment_of_inertia_estimation/doc/phases/050-point-type-expansion/result.zh.md \
		--doc test-rvv/features/moment_of_inertia_estimation/doc/moment_of_inertia_estimation-evaluation.zh.md \
		--fail-on never

.PHONY: dump_test_rvv collect_board_moi_repeated summarize_board_moi_repeated run_board_moi_repeated_evidence_doctor record_board_moi_repeated_state run_board_moi_repeated run_board_moi_phase010_repeated run_board_moi_phase030_public_compute_repeated run_board_moi_phase040_projected_covariance_repeated run_board_moi_phase050_pointxyzi_repeated run_board_moi_phase050_pointxyzrgb_repeated run_board_moi_phase050_pointxyzrgba_repeated run_board_moi_phase050_pointxyzrgbnormal_repeated evidence_status evidence_status_phase010 evidence_status_phase030 evidence_status_phase040 evidence_status_phase050
