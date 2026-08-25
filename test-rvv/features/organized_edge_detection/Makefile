# =============================================================================
# PCL features/organized_edge_detection RVV diagnostic Makefile
#
# 本 topic 先评估 OrganizedEdgeBase depth-label path（深度标签路径）的
# test-only candidate。QEMU（仿真器）只用于 correctness（正确性）和日志形状；
# 性能结论必须来自 board（板卡）或目标硬件 repeated benchmark（重复性能测试）。
# =============================================================================

TOPIC  := organized_edge_detection
MODULE := features

FOCUS_DIR      := pcl/features/include/pcl/features/impl/organized_edge_detection.hpp
FILTER_REPORT  := log/filtered_organized_edge_detection.log
ANALYZE_REPORT := log/analyze_organized_edge_detection.log

TARGET_TEST     := test_organized_edge_detection
TARGET_TEST_STD := test_organized_edge_detection_std
TARGET_TEST_RVV := test_organized_edge_detection_rvv
BOARD_TARGET_TEST := test_organized_edge_detection_rvv
SRCS_TEST       := src/test_organized_edge_detection.cpp

TARGET_BENCH     := bench_organized_edge_detection
TARGET_BENCH_STD := bench_organized_edge_detection_std
TARGET_BENCH_RVV := bench_organized_edge_detection_rvv
SRCS_BENCH       := src/bench_organized_edge_detection.cpp

TARGET_PROD_BENCH     := bench_organized_edge_detection_production
TARGET_PROD_BENCH_STD := bench_organized_edge_detection_production_std
TARGET_PROD_BENCH_RVV := bench_organized_edge_detection_production_rvv
SRCS_PROD_BENCH       := src/bench_organized_edge_detection_production.cpp
PROD_BENCH_STD_OUTPUT_FILE     ?= log/qemu/run_bench_production_std.log
PROD_BENCH_RVV_OUTPUT_FILE     ?= log/qemu/run_bench_production_rvv.log
PROD_BENCH_COMPARE_OUTPUT_FILE ?= log/qemu/analyze_bench_production_compare.log
PROD_ASM_FULL := build/asm/riscv/bench_organized_edge_detection_production_rvv.full.asm
PROD_ASM_FILTERED := build/asm/riscv/bench_organized_edge_detection_production_rvv.asm
OED_PRODUCTION_HEADER := $(abspath $(CURDIR)/../../../features/include/pcl/features/impl/organized_edge_detection.hpp)

OED_BOARD_MANIFEST_JSON := log/board/evidence_manifest.json
OED_BOARD_DOCTOR_MD := log/board/evidence_doctor.md
OED_EVIDENCE_REGISTRY := log/evidence_registry.json
OED_BOARD_REPEATED_SUMMARY := log/board/repeated-summary.md
OED_BOARD_PROD_MANIFEST_JSON := log/board/production-evidence_manifest.json
OED_BOARD_PROD_DOCTOR_MD := log/board/production-evidence_doctor.md
OED_BOARD_PROD_REPEATED_SUMMARY := log/board/production-repeated-summary.md
OED_BOARD_PROD_PT_MANIFEST_JSON := log/board/point-type-evidence_manifest.json
OED_BOARD_PROD_PT_DOCTOR_MD := log/board/point-type-evidence_doctor.md
OED_BOARD_PROD_PT_REPEATED_SUMMARY := log/board/point-type-repeated-summary.md
OED_BOARD_REPEATED_RUNS ?= 5
OED_BOARD_REPEATED_LABEL ?= organized_edge_detection_phase000_depth_labels
OED_BOARD_PROD_REPEATED_LABEL ?= organized_edge_detection_phase010_production_depth_labels
OED_BOARD_PROD_PT_REPEATED_LABEL ?= organized_edge_detection_phase020_point_type_expansion
OED_POINT_TYPE_CASE_FILTER := prod_depth_pointxyzi_finite_320x240,prod_depth_pointxyzrgb_finite_320x240,prod_depth_pointxyzrgbnormal_finite_320x240,prod_depth_pointxyzrgb_nan_boundary_320x240
OED_BOARD_REPEATED_RUN_DIRS := \
	log/board/repeated_001 \
	log/board/repeated_002 \
	log/board/repeated_003 \
	log/board/repeated_004 \
	log/board/repeated_005
OED_BOARD_PROD_REPEATED_RUN_DIRS := \
	log/board/production_repeated_001 \
	log/board/production_repeated_002 \
	log/board/production_repeated_003 \
	log/board/production_repeated_004 \
	log/board/production_repeated_005
OED_BOARD_PROD_PT_REPEATED_RUN_DIRS := \
	log/board/point_type_repeated_001 \
	log/board/point_type_repeated_002 \
	log/board/point_type_repeated_003 \
	log/board/point_type_repeated_004 \
	log/board/point_type_repeated_005

INCLUDES = \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/features/include \
	-I$(PCL_SOURCE_ROOT)/2d/include \
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
	-lpcl_common \
	-lgtest -lgtest_main -lpthread \
	-lm

LIBS_BENCH := -lm
LIBS_PROD_BENCH := -lpcl_common -lm

include ../../mk/rvv-topic.mk

$(BUILD_DIR)/$(ARCH)/$(TARGET_PROD_BENCH): $(SRCS_PROD_BENCH) $(OED_PRODUCTION_HEADER) | $(LOG_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling production Benchmark ($(ARCH), USE_PCL_RVV10=$(USE_PCL_RVV10))..."
	$(CXX) $(CXXFLAGS) $(SRCS_PROD_BENCH) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_PROD_BENCH) -o $(BUILD_DIR)/$(ARCH)/$(TARGET_PROD_BENCH)

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
	@grep -Eq 'organizedEdgeDepthLabelsRVV' $(PROD_ASM_FULL)
	@grep -Eq 'vle32[.]v' $(PROD_ASM_FILTERED)
	@grep -Eq 'vfsub[.]vv' $(PROD_ASM_FILTERED)
	@grep -Eq 'vfmin[.]vv' $(PROD_ASM_FILTERED)
	@grep -Eq 'vfmax[.]vv' $(PROD_ASM_FILTERED)
	@grep -Eq 'vse32[.]v' $(PROD_ASM_FILTERED)
	@echo "[PASS] production organized_edge_detection RVV asm gate matched expected instructions."

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

deploy_board_production: deploy_files deploy_production_bench_std deploy_production_bench_rvv

collect_board_organized_edge_detection_repeated: deploy_board
	@rm -rf $(OED_BOARD_REPEATED_RUN_DIRS)
	@set -e; for run in $$(seq 1 $(OED_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%03d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(OED_BOARD_REPEATED_LABEL)/repeated_$$run_id"; \
		local_dir="log/board/repeated_$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] organized_edge_detection repeated run $$run_id/$(OED_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(BENCH_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest:
	@$(PYTHON_RUN) script/generate_organized_edge_detection_evidence_manifest.py \
		$(foreach run_dir,$(OED_BOARD_REPEATED_RUN_DIRS),--run-dir $(run_dir)) \
		--summary $(OED_BOARD_REPEATED_SUMMARY) \
		--output $(OED_BOARD_MANIFEST_JSON)

run_board_evidence_doctor: generate_board_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(OED_BOARD_MANIFEST_JSON) \
		--output $(OED_BOARD_DOCTOR_MD) \
		--fail-on never

record_board_evidence_state: run_board_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(OED_EVIDENCE_REGISTRY) \
		--summary $(OED_BOARD_REPEATED_SUMMARY) \
		--manifest $(OED_BOARD_MANIFEST_JSON) \
		--doctor $(OED_BOARD_DOCTOR_MD) \
		--target collect_board_organized_edge_detection_repeated \
		--backend board \
		--run-label board-organized-edge-detection-depth-labels-phase000 \
		--case-filter depth_labels_finite_320x240,depth_labels_finite_641x481_tail,depth_labels_nan_boundary_320x240 \
		--evidence-role diagnostic \
		--doc-ref doc/phases/000-current-state-and-label-equivalence/result.zh.md \
		--doc-ref doc/organized_edge_detection-evaluation.zh.md \
		--freshness-state fresh

run_board_organized_edge_detection_repeated: collect_board_organized_edge_detection_repeated record_board_evidence_state

collect_board_organized_edge_detection_production_repeated: deploy_board_production
	@rm -rf $(OED_BOARD_PROD_REPEATED_RUN_DIRS)
	@set -e; for run in $$(seq 1 $(OED_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%03d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(OED_BOARD_PROD_REPEATED_LABEL)/repeated_$$run_id"; \
		local_dir="log/board/production_repeated_$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] organized_edge_detection production repeated run $$run_id/$(OED_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_BENCH_STD='$(TARGET_PROD_BENCH_STD)' REMOTE_BENCH_RVV='$(TARGET_PROD_BENCH_RVV)' REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(BENCH_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_production_evidence_manifest:
	@$(PYTHON_RUN) script/generate_organized_edge_detection_evidence_manifest.py \
		$(foreach run_dir,$(OED_BOARD_PROD_REPEATED_RUN_DIRS),--run-dir $(run_dir)) \
		--summary $(OED_BOARD_PROD_REPEATED_SUMMARY) \
		--output $(OED_BOARD_PROD_MANIFEST_JSON) \
		--title "organized_edge_detection Phase 010 production repeated board summary" \
		--std-binary build/riscv/$(TARGET_PROD_BENCH_STD) \
		--rvv-binary build/riscv/$(TARGET_PROD_BENCH_RVV)

run_board_production_evidence_doctor: generate_board_production_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(OED_BOARD_PROD_MANIFEST_JSON) \
		--output $(OED_BOARD_PROD_DOCTOR_MD) \
		--fail-on never

record_board_production_evidence_state: run_board_production_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(OED_EVIDENCE_REGISTRY) \
		--summary $(OED_BOARD_PROD_REPEATED_SUMMARY) \
		--manifest $(OED_BOARD_PROD_MANIFEST_JSON) \
		--doctor $(OED_BOARD_PROD_DOCTOR_MD) \
		--target collect_board_organized_edge_detection_production_repeated \
		--backend board \
		--run-label board-organized-edge-detection-production-depth-labels-phase010 \
		--case-filter prod_depth_finite_320x240,prod_depth_finite_641x481_tail,prod_depth_nan_boundary_320x240 \
		--evidence-role production-public \
		--doc-ref doc/phases/010-production-depth-label-probe/result.zh.md \
		--doc-ref doc/organized_edge_detection-evaluation.zh.md \
		--doc-ref ../../../doc-rvv/features/organized_edge_detection-RVV.zh.md \
		--freshness-state fresh

run_board_organized_edge_detection_production_repeated: collect_board_organized_edge_detection_production_repeated record_board_production_evidence_state

collect_board_organized_edge_detection_point_type_repeated: deploy_board_production
	@rm -rf $(OED_BOARD_PROD_PT_REPEATED_RUN_DIRS)
	@set -e; for run in $$(seq 1 $(OED_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%03d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(OED_BOARD_PROD_PT_REPEATED_LABEL)/repeated_$$run_id"; \
		local_dir="log/board/point_type_repeated_$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] organized_edge_detection point-type repeated run $$run_id/$(OED_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_BENCH_STD='$(TARGET_PROD_BENCH_STD)' REMOTE_BENCH_RVV='$(TARGET_PROD_BENCH_RVV)' REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter $(OED_POINT_TYPE_CASE_FILTER) $(BENCH_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_point_type_evidence_manifest:
	@$(PYTHON_RUN) script/generate_organized_edge_detection_evidence_manifest.py \
		$(foreach run_dir,$(OED_BOARD_PROD_PT_REPEATED_RUN_DIRS),--run-dir $(run_dir)) \
		--summary $(OED_BOARD_PROD_PT_REPEATED_SUMMARY) \
		--output $(OED_BOARD_PROD_PT_MANIFEST_JSON) \
		--title "organized_edge_detection Phase 020 point-type production repeated board summary" \
		--std-binary build/riscv/$(TARGET_PROD_BENCH_STD) \
		--rvv-binary build/riscv/$(TARGET_PROD_BENCH_RVV)

run_board_point_type_evidence_doctor: generate_board_point_type_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(OED_BOARD_PROD_PT_MANIFEST_JSON) \
		--output $(OED_BOARD_PROD_PT_DOCTOR_MD) \
		--fail-on never

record_board_point_type_evidence_state: run_board_point_type_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(OED_EVIDENCE_REGISTRY) \
		--summary $(OED_BOARD_PROD_PT_REPEATED_SUMMARY) \
		--manifest $(OED_BOARD_PROD_PT_MANIFEST_JSON) \
		--doctor $(OED_BOARD_PROD_PT_DOCTOR_MD) \
		--target collect_board_organized_edge_detection_point_type_repeated \
		--backend board \
		--run-label board-organized-edge-detection-point-type-expansion-phase020 \
		--case-filter $(OED_POINT_TYPE_CASE_FILTER) \
		--evidence-role production-public \
		--doc-ref doc/phases/020-point-type-expansion/result.zh.md \
		--doc-ref doc/organized_edge_detection-evaluation.zh.md \
		--doc-ref ../../../doc-rvv/features/organized_edge_detection-RVV.zh.md \
		--freshness-state fresh

run_board_organized_edge_detection_point_type_repeated: collect_board_organized_edge_detection_point_type_repeated record_board_point_type_evidence_state

evidence_status:
	@$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry $(OED_EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/features/organized_edge_detection/log/board/repeated-summary.md' \
		--scan-glob 'test-rvv/features/organized_edge_detection/log/board/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/organized_edge_detection/log/board/evidence_doctor.md' \
		--scan-glob 'test-rvv/features/organized_edge_detection/log/board/production-repeated-summary.md' \
		--scan-glob 'test-rvv/features/organized_edge_detection/log/board/production-evidence_manifest.json' \
		--scan-glob 'test-rvv/features/organized_edge_detection/log/board/production-evidence_doctor.md' \
		--scan-glob 'test-rvv/features/organized_edge_detection/log/board/point-type-repeated-summary.md' \
		--scan-glob 'test-rvv/features/organized_edge_detection/log/board/point-type-evidence_manifest.json' \
		--scan-glob 'test-rvv/features/organized_edge_detection/log/board/point-type-evidence_doctor.md' \
		--doc test-rvv/features/organized_edge_detection/doc/phases/000-current-state-and-label-equivalence/result.zh.md \
		--doc test-rvv/features/organized_edge_detection/doc/phases/010-production-depth-label-probe/result.zh.md \
		--doc test-rvv/features/organized_edge_detection/doc/phases/020-point-type-expansion/result.zh.md \
		--doc test-rvv/features/organized_edge_detection/doc/organized_edge_detection-evaluation.zh.md \
		--fail-on never

.PHONY: run_bench_production run_bench_production_rvv run_bench_production_std analyze_bench_production_compare dump_bench_production_rvv check_production_rvv_asm deploy_production_bench_rvv deploy_production_bench_std deploy_board_production collect_board_organized_edge_detection_repeated generate_board_evidence_manifest run_board_evidence_doctor record_board_evidence_state run_board_organized_edge_detection_repeated collect_board_organized_edge_detection_production_repeated generate_board_production_evidence_manifest run_board_production_evidence_doctor record_board_production_evidence_state run_board_organized_edge_detection_production_repeated collect_board_organized_edge_detection_point_type_repeated generate_board_point_type_evidence_manifest run_board_point_type_evidence_doctor record_board_point_type_evidence_state run_board_organized_edge_detection_point_type_repeated evidence_status
