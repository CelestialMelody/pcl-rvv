# =============================================================================
#  PCL features/don RVV diagnostic Makefile
#
#  本 topic 先评估 DifferenceOfNormalsEstimation::computeFeature() 的
#  normal 差和 curvature 写回热点。QEMU 只用于 correctness（正确性）
#  和日志形状；性能结论必须来自板卡或目标硬件。
# =============================================================================

TOPIC  := don
MODULE := features

FOCUS_DIR      := pcl/features/include/pcl/features/impl/don.hpp
FILTER_REPORT  := log/filtered_don.log
ANALYZE_REPORT := log/analyze_don.log

TARGET_TEST     := test_don
TARGET_TEST_STD := test_don_std
TARGET_TEST_RVV := test_don_rvv
SRCS_TEST       := src/test_don.cpp

TARGET_BENCH     := bench_don
TARGET_BENCH_STD := bench_don_std
TARGET_BENCH_RVV := bench_don_rvv
SRCS_BENCH       := src/bench_don.cpp

TARGET_PROD_BENCH     := bench_don_production
TARGET_PROD_BENCH_STD := bench_don_production_std
TARGET_PROD_BENCH_RVV := bench_don_production_rvv
SRCS_PROD_BENCH       := src/bench_don_production.cpp
PROD_BENCH_STD_OUTPUT_FILE     ?= log/qemu/run_bench_production_std.log
PROD_BENCH_RVV_OUTPUT_FILE     ?= log/qemu/run_bench_production_rvv.log
PROD_BENCH_COMPARE_OUTPUT_FILE ?= log/qemu/analyze_bench_production_compare.log
PROD_ASM_FULL := build/asm/riscv/bench_don_production_rvv.full.asm
PROD_ASM_FILTERED := build/asm/riscv/bench_don_production_rvv.asm
DON_PRODUCTION_HEADER := $(abspath $(CURDIR)/../../../features/include/pcl/features/impl/don.hpp)

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

LIBS_TEST  := -lpcl_features -lpcl_search -lpcl_kdtree -lpcl_common -lflann_cpp -llz4 -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lpcl_common -lm

DON_BOARD_MANIFEST_JSON := log/board/evidence_manifest.json
DON_BOARD_DOCTOR_MD := log/board/evidence_doctor.md
DON_EVIDENCE_REGISTRY := log/evidence_registry.json
DON_BOARD_REPEATED_DIR ?= log/board/repeated_phase010_diagnostic
DON_BOARD_REPEATED_RUNS ?= 5
DON_BOARD_REPEATED_LABEL ?= don_diagnostic_repeated_phase010
DON_BOARD_REPEATED_SUMMARY := $(DON_BOARD_REPEATED_DIR)/summary.md
DON_BOARD_REPEATED_DOCTOR_MD := $(DON_BOARD_REPEATED_DIR)/evidence_doctor.md
DON_BOARD_PROD_REPEATED_DIR ?= log/board/repeated_phase030_production_direct
DON_BOARD_PROD_REPEATED_RUNS ?= 5
DON_BOARD_PROD_REPEATED_LABEL ?= don_production_direct_repeated_phase030
DON_BOARD_PROD_REPEATED_SUMMARY := $(DON_BOARD_PROD_REPEATED_DIR)/summary.md
DON_BOARD_PROD_REPEATED_DOCTOR_MD := $(DON_BOARD_PROD_REPEATED_DIR)/evidence_doctor.md
DON_PROD_BENCH_ARGS ?= --case-filter don_production_direct
DON_BOARD_ABLATION_REPEATED_DIR ?= log/board/repeated_phase050_detail_ablation
DON_BOARD_ABLATION_REPEATED_RUNS ?= 5
DON_BOARD_ABLATION_REPEATED_LABEL ?= don_detail_ablation_repeated_phase050
DON_BOARD_ABLATION_REPEATED_SUMMARY := $(DON_BOARD_ABLATION_REPEATED_DIR)/summary.md
DON_BOARD_ABLATION_REPEATED_DOCTOR_MD := $(DON_BOARD_ABLATION_REPEATED_DIR)/evidence_doctor.md
DON_ABLATION_BENCH_ARGS ?= --case-filter don_ablate_all

include ../../mk/rvv-topic.mk

$(BUILD_DIR)/$(ARCH)/$(TARGET_PROD_BENCH): $(SRCS_PROD_BENCH) $(DON_PRODUCTION_HEADER) | $(LOG_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling production Benchmark ($(ARCH), USE_PCL_RVV10=$(USE_PCL_RVV10))..."
	$(CXX) $(CXXFLAGS) $(SRCS_PROD_BENCH) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_TEST) -o $(BUILD_DIR)/$(ARCH)/$(TARGET_PROD_BENCH)

run_bench_production: $(BUILD_DIR)/$(ARCH)/$(TARGET_PROD_BENCH)
	@echo "[RUN] Production Benchmark on $(ARCH) (USE_PCL_RVV10=$(USE_PCL_RVV10))..."
	$(RUN_CMD) ./$(BUILD_DIR)/$(ARCH)/$(TARGET_PROD_BENCH) $(BENCH_ARGS)

run_bench_production_rvv: | $(OUTPUT_DIR_QEMU)
	@rm -f $(BUILD_DIR)/$(ARCH)/$(TARGET_PROD_BENCH_RVV)
	@$(MAKE) -C $(CURDIR) run_bench_production USE_PCL_RVV10=1 TARGET_PROD_BENCH=$(TARGET_PROD_BENCH_RVV) BENCH_ARGS="$(BENCH_ARGS)" 2>&1 | tee $(PROD_BENCH_RVV_OUTPUT_FILE)

run_bench_production_std: | $(OUTPUT_DIR_QEMU)
	@rm -f $(BUILD_DIR)/$(ARCH)/$(TARGET_PROD_BENCH_STD)
	@$(MAKE) -C $(CURDIR) run_bench_production USE_PCL_RVV10=0 TARGET_PROD_BENCH=$(TARGET_PROD_BENCH_STD) BENCH_ARGS="$(BENCH_ARGS)" 2>&1 | tee $(PROD_BENCH_STD_OUTPUT_FILE)

analyze_bench_production_compare: $(PROD_BENCH_STD_OUTPUT_FILE) $(PROD_BENCH_RVV_OUTPUT_FILE) | $(OUTPUT_DIR_QEMU)
	@$(PYTHON_RUN) $(BENCH_COMPARE_SCRIPT) --std-log $(PROD_BENCH_STD_OUTPUT_FILE) --rvv-log $(PROD_BENCH_RVV_OUTPUT_FILE) 2>&1 | tee $(PROD_BENCH_COMPARE_OUTPUT_FILE)

dump_bench_production_rvv: | $(ASM_DIR)/$(ARCH)
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_PROD_BENCH=$(TARGET_PROD_BENCH_RVV) $(BUILD_DIR)/$(ARCH)/$(TARGET_PROD_BENCH_RVV)
	$(OBJDUMP) -d -C $(BUILD_DIR)/$(ARCH)/$(TARGET_PROD_BENCH_RVV) > $(PROD_ASM_FULL)
	grep -E $(VEC_REGEX_STR) $(PROD_ASM_FULL) > $(PROD_ASM_FILTERED) || true
	@echo "[DONE] production RVV asm dump: $(PROD_ASM_FILTERED)"

check_production_rvv_asm: dump_bench_production_rvv
	@grep -Eq 'vlse32[.]v' $(PROD_ASM_FILTERED)
	@grep -Eq 'vfsub[.]vv' $(PROD_ASM_FILTERED)
	@grep -Eq 'vmerge[.]vvm' $(PROD_ASM_FILTERED)
	@grep -Eq 'vfsqrt[.]v' $(PROD_ASM_FILTERED)
	@grep -Eq 'vsse32[.]v' $(PROD_ASM_FILTERED)
	@echo "[PASS] production DON RVV asm gate matched expected instructions."

run_qemu_smoke: run_test_compare run_bench_rvv

generate_board_evidence_manifest:
	@$(PYTHON_RUN) script/generate_don_evidence_manifest.py \
		--std-log log/board/run_bench_std.log \
		--rvv-log log/board/run_bench_rvv.log \
		--summary log/board/analyze_bench_compare.log \
		--asm build/asm/riscv/bench_don_rvv.asm \
		--output $(DON_BOARD_MANIFEST_JSON)

run_board_evidence_doctor: generate_board_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(DON_BOARD_MANIFEST_JSON) \
		--output $(DON_BOARD_DOCTOR_MD) \
		--fail-on never

record_board_evidence_state: run_board_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(DON_EVIDENCE_REGISTRY) \
		--summary log/board/analyze_bench_compare.log \
		--manifest $(DON_BOARD_MANIFEST_JSON) \
		--doctor $(DON_BOARD_DOCTOR_MD) \
		--target board_smoke \
		--backend board \
		--run-label board-don-diagnostic-smoke-phase000 \
		--case-filter don_normal_pair \
		--evidence-role diagnostic \
		--doc-ref doc/phases/000-current-state-and-diagnostic-plan/result.zh.md \
		--doc-ref doc/don-evaluation.zh.md \
		--freshness-state fresh

run_board_don_evidence: record_board_evidence_state

$(DON_BOARD_REPEATED_DIR):
	mkdir -p $(DON_BOARD_REPEATED_DIR)

$(DON_BOARD_PROD_REPEATED_DIR):
	mkdir -p $(DON_BOARD_PROD_REPEATED_DIR)

$(DON_BOARD_ABLATION_REPEATED_DIR):
	mkdir -p $(DON_BOARD_ABLATION_REPEATED_DIR)

collect_board_don_repeated: deploy_board | $(DON_BOARD_REPEATED_DIR)
	@rm -rf "$(DON_BOARD_REPEATED_DIR)"/run-*
	@set -e; for run in $$(seq 1 $(DON_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(DON_BOARD_REPEATED_LABEL)/run-$$run_id"; \
		local_dir="$(DON_BOARD_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] DON diagnostic repeated run $$run_id/$(DON_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(BENCH_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

summarize_board_don_repeated:
	@$(PYTHON_RUN) script/generate_don_repeated_summary.py \
		--input-dir $(DON_BOARD_REPEATED_DIR) \
		--output $(DON_BOARD_REPEATED_SUMMARY) \
		--run-label $(DON_BOARD_REPEATED_LABEL) \
		--expected-runs $(DON_BOARD_REPEATED_RUNS) \
		--asm build/asm/riscv/bench_don_rvv.asm

run_board_don_repeated_evidence_doctor: summarize_board_don_repeated
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(DON_BOARD_REPEATED_DIR)/evidence_manifest.json \
		--output $(DON_BOARD_REPEATED_DOCTOR_MD) \
		--fail-on never

record_board_don_repeated_state: run_board_don_repeated_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(DON_EVIDENCE_REGISTRY) \
		--summary $(DON_BOARD_REPEATED_SUMMARY) \
		--manifest $(DON_BOARD_REPEATED_DIR)/evidence_manifest.json \
		--doctor $(DON_BOARD_REPEATED_DOCTOR_MD) \
		--target collect_board_don_repeated \
		--backend board \
		--run-label board-don-diagnostic-repeated-phase010 \
		--case-filter don_normal_pair \
		--evidence-role diagnostic \
		--doc-ref doc/phases/010-diagnostic-repeated-board/result.zh.md \
		--doc-ref doc/don-evaluation.zh.md \
		--freshness-state fresh

run_board_don_repeated: collect_board_don_repeated record_board_don_repeated_state

collect_board_don_ablation_repeated: deploy_board | $(DON_BOARD_ABLATION_REPEATED_DIR)
	@rm -rf "$(DON_BOARD_ABLATION_REPEATED_DIR)"/run-*
	@set -e; for run in $$(seq 1 $(DON_BOARD_ABLATION_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(DON_BOARD_ABLATION_REPEATED_LABEL)/run-$$run_id"; \
		local_dir="$(DON_BOARD_ABLATION_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] DON detail ablation repeated run $$run_id/$(DON_BOARD_ABLATION_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(DON_ABLATION_BENCH_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

summarize_board_don_ablation_repeated:
	@$(PYTHON_RUN) script/generate_don_repeated_summary.py \
		--input-dir $(DON_BOARD_ABLATION_REPEATED_DIR) \
		--output $(DON_BOARD_ABLATION_REPEATED_SUMMARY) \
		--run-label $(DON_BOARD_ABLATION_REPEATED_LABEL) \
		--expected-runs $(DON_BOARD_ABLATION_REPEATED_RUNS) \
		--asm build/asm/riscv/bench_don_rvv.asm \
		--case-label don_ablate_finite_only_no_mask \
		--case-label don_ablate_no_sqrt_store_zero_curvature \
		--case-label don_ablate_normal_only \
		--evidence-role diagnostic \
		--ab-boundary "test helper detail ablation" \
		--wrapper "computeDoN detail ablation helpers" \
		--timer-boundary "helper_only_detail_ablation_no_setup" \
		--case-kind component_ablation \
		--title "DON detail ablation repeated board summary"

run_board_don_ablation_repeated_evidence_doctor: summarize_board_don_ablation_repeated
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(DON_BOARD_ABLATION_REPEATED_DIR)/evidence_manifest.json \
		--output $(DON_BOARD_ABLATION_REPEATED_DOCTOR_MD) \
		--fail-on never

record_board_don_ablation_repeated_state: run_board_don_ablation_repeated_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(DON_EVIDENCE_REGISTRY) \
		--summary $(DON_BOARD_ABLATION_REPEATED_SUMMARY) \
		--manifest $(DON_BOARD_ABLATION_REPEATED_DIR)/evidence_manifest.json \
		--doctor $(DON_BOARD_ABLATION_REPEATED_DOCTOR_MD) \
		--target collect_board_don_ablation_repeated \
		--backend board \
		--run-label board-don-detail-ablation-repeated-phase050 \
		--case-filter don_ablate_all \
		--evidence-role diagnostic \
		--doc-ref doc/phases/050-production-detail-ablation/result.zh.md \
		--doc-ref doc/don-evaluation.zh.md \
		--freshness-state fresh

run_board_don_ablation_repeated: collect_board_don_ablation_repeated record_board_don_ablation_repeated_state

deploy_production_bench_rvv: deploy_files
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_PROD_BENCH=$(TARGET_PROD_BENCH_RVV) $(BUILD_DIR)/$(ARCH)/$(TARGET_PROD_BENCH_RVV)
	@$(STRIP) -s ./$(BUILD_DIR)/$(ARCH)/$(TARGET_PROD_BENCH_RVV) -o ./$(TARGET_PROD_BENCH_RVV)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_PROD_BENCH_RVV)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_PROD_BENCH_RVV)
	@rm -f ./$(TARGET_PROD_BENCH_RVV)_stripped

deploy_production_bench_std: deploy_files
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=0 TARGET_PROD_BENCH=$(TARGET_PROD_BENCH_STD) $(BUILD_DIR)/$(ARCH)/$(TARGET_PROD_BENCH_STD)
	@$(STRIP) -s ./$(BUILD_DIR)/$(ARCH)/$(TARGET_PROD_BENCH_STD) -o ./$(TARGET_PROD_BENCH_STD)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_PROD_BENCH_STD)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_PROD_BENCH_STD)
	@rm -f ./$(TARGET_PROD_BENCH_STD)_stripped

deploy_board_production: deploy_production_bench_std deploy_production_bench_rvv

collect_board_don_production_repeated: deploy_board_production | $(DON_BOARD_PROD_REPEATED_DIR)
	@rm -rf "$(DON_BOARD_PROD_REPEATED_DIR)"/run-*
	@set -e; for run in $$(seq 1 $(DON_BOARD_PROD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(DON_BOARD_PROD_REPEATED_LABEL)/run-$$run_id"; \
		local_dir="$(DON_BOARD_PROD_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] DON production direct repeated run $$run_id/$(DON_BOARD_PROD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_BENCH_STD='$(TARGET_PROD_BENCH_STD)' REMOTE_BENCH_RVV='$(TARGET_PROD_BENCH_RVV)' REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(DON_PROD_BENCH_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

summarize_board_don_production_repeated:
	@$(PYTHON_RUN) script/generate_don_repeated_summary.py \
		--input-dir $(DON_BOARD_PROD_REPEATED_DIR) \
		--output $(DON_BOARD_PROD_REPEATED_SUMMARY) \
		--run-label $(DON_BOARD_PROD_REPEATED_LABEL) \
		--expected-runs $(DON_BOARD_PROD_REPEATED_RUNS) \
		--asm $(PROD_ASM_FILTERED) \
		--case-label don_production_direct \
		--evidence-role production-public \
		--ab-boundary "public overload" \
		--wrapper "Feature::compute -> DifferenceOfNormalsEstimation::computeFeature" \
		--timer-boundary "public_compute_no_input_construction" \
		--case-kind production_direct \
		--title "DON production direct repeated board summary"

run_board_don_production_repeated_evidence_doctor: summarize_board_don_production_repeated
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(DON_BOARD_PROD_REPEATED_DIR)/evidence_manifest.json \
		--output $(DON_BOARD_PROD_REPEATED_DOCTOR_MD) \
		--fail-on never

record_board_don_production_repeated_state: run_board_don_production_repeated_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(DON_EVIDENCE_REGISTRY) \
		--summary $(DON_BOARD_PROD_REPEATED_SUMMARY) \
		--manifest $(DON_BOARD_PROD_REPEATED_DIR)/evidence_manifest.json \
		--doctor $(DON_BOARD_PROD_REPEATED_DOCTOR_MD) \
		--target collect_board_don_production_repeated \
		--backend board \
		--run-label board-don-production-direct-repeated-phase030 \
		--case-filter don_production_direct \
		--evidence-role production-public \
		--doc-ref doc/phases/030-production-direct-probe/result.zh.md \
		--doc-ref doc/don-evaluation.zh.md \
		--freshness-state fresh

run_board_don_production_repeated: collect_board_don_production_repeated record_board_don_production_repeated_state

evidence_status:
	@$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry $(DON_EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/features/don/log/board/analyze_bench_compare.log' \
		--scan-glob 'test-rvv/features/don/log/board/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/don/log/board/evidence_doctor.md' \
		--scan-glob 'test-rvv/features/don/log/board/repeated_phase010_diagnostic/summary.md' \
		--scan-glob 'test-rvv/features/don/log/board/repeated_phase010_diagnostic/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/don/log/board/repeated_phase010_diagnostic/evidence_doctor.md' \
		--scan-glob 'test-rvv/features/don/log/board/repeated_phase030_production_direct/summary.md' \
		--scan-glob 'test-rvv/features/don/log/board/repeated_phase030_production_direct/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/don/log/board/repeated_phase030_production_direct/evidence_doctor.md' \
		--scan-glob 'test-rvv/features/don/log/board/repeated_phase050_detail_ablation/summary.md' \
		--scan-glob 'test-rvv/features/don/log/board/repeated_phase050_detail_ablation/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/don/log/board/repeated_phase050_detail_ablation/evidence_doctor.md' \
		--doc test-rvv/features/don/doc/phases/000-current-state-and-diagnostic-plan/result.zh.md \
		--doc test-rvv/features/don/doc/phases/010-diagnostic-repeated-board/result.zh.md \
		--doc test-rvv/features/don/doc/phases/030-production-direct-probe/result.zh.md \
		--doc test-rvv/features/don/doc/phases/050-production-detail-ablation/result.zh.md \
		--doc test-rvv/features/don/doc/don-evaluation.zh.md \
		--fail-on never

.PHONY: run_bench_production run_bench_production_rvv run_bench_production_std analyze_bench_production_compare dump_bench_production_rvv check_production_rvv_asm run_qemu_smoke generate_board_evidence_manifest run_board_evidence_doctor record_board_evidence_state run_board_don_evidence collect_board_don_repeated summarize_board_don_repeated run_board_don_repeated_evidence_doctor record_board_don_repeated_state run_board_don_repeated collect_board_don_ablation_repeated summarize_board_don_ablation_repeated run_board_don_ablation_repeated_evidence_doctor record_board_don_ablation_repeated_state run_board_don_ablation_repeated deploy_production_bench_rvv deploy_production_bench_std deploy_board_production collect_board_don_production_repeated summarize_board_don_production_repeated run_board_don_production_repeated_evidence_doctor record_board_don_production_repeated_state run_board_don_production_repeated evidence_status
