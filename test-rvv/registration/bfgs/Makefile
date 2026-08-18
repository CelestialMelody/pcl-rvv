# =============================================================================
#  PCL registration/bfgs RVV diagnostic Makefile
# =============================================================================

TOPIC  := bfgs
MODULE := registration

FOCUS_DIR      := pcl/registration/include/pcl/registration/bfgs.h
FILTER_REPORT  := log/filtered_bfgs.log
ANALYZE_REPORT := log/analyze_bfgs.log

TARGET_BENCH     := bench_bfgs
TARGET_BENCH_STD := bench_bfgs_std
TARGET_BENCH_RVV := bench_bfgs_rvv
SRCS_BENCH       := src/bench_bfgs.cpp

TARGET_TEST     := test_bfgs
TARGET_TEST_STD := test_bfgs_std
TARGET_TEST_RVV := test_bfgs_rvv
SRCS_TEST       := src/test_bfgs.cpp

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

BFGS_QEMU_DIRECTION_UPDATE_SMOKE_ARGS ?= --case-filter direction-update --iterations 2 --warmup-iterations 1
BFGS_QEMU_MOVE_TO_SLOPE_SMOKE_ARGS ?= --case-filter move-to-slope --iterations 2 --warmup-iterations 1
BFGS_QEMU_GICP_SHAPED_SMOKE_ARGS ?= --case-filter gicp-shaped-vector6 --iterations 2 --warmup-iterations 1
BFGS_QEMU_ALL_SMOKE_ARGS ?= --case-filter all --iterations 2 --warmup-iterations 1

include ../../mk/rvv-topic.mk

QEMU_DIRECTION_UPDATE_BENCH_LOG := $(OUTPUT_DIR_QEMU)/run_bench_direction_update_rvv.log
QEMU_MOVE_TO_SLOPE_BENCH_LOG := $(OUTPUT_DIR_QEMU)/run_bench_move_to_slope_rvv.log
QEMU_GICP_SHAPED_BENCH_LOG := $(OUTPUT_DIR_QEMU)/run_bench_gicp_shaped_vector6_rvv.log
QEMU_ALL_BENCH_LOG := $(OUTPUT_DIR_QEMU)/run_bench_all_rvv.log
QEMU_SMOKE_EVIDENCE_MANIFEST := $(OUTPUT_DIR_QEMU)/evidence_manifest.json
QEMU_SMOKE_EVIDENCE_DOCTOR_MD := $(OUTPUT_DIR_QEMU)/evidence_doctor.md
EVIDENCE_REGISTRY := $(LOG_DIR)/evidence_registry.json

BFGS_BOARD_REPEATED_RUNS ?= 5
BFGS_BOARD_BENCH_ITERATIONS ?= 20
BFGS_BOARD_BENCH_WARMUP_ITERATIONS ?= 5
BFGS_BOARD_RUN_LABEL ?= direction_update_repeated
BFGS_BOARD_REPEATED_DIR ?= $(OUTPUT_DIR_BOARD)/$(BFGS_BOARD_RUN_LABEL)
BFGS_BOARD_SUMMARY := $(BFGS_BOARD_REPEATED_DIR)/summary.md
BFGS_BOARD_EVIDENCE_MANIFEST := $(BFGS_BOARD_REPEATED_DIR)/evidence_manifest.json
BFGS_BOARD_EVIDENCE_DOCTOR_MD := $(BFGS_BOARD_REPEATED_DIR)/evidence_doctor.md

$(BFGS_BOARD_REPEATED_DIR) $(OUTPUT_DIR_BOARD)/test_smoke:
	mkdir -p $@

run_test_candidate_direction:
	@$(MAKE) -C $(CURDIR) run_test_compare TEST_ARGS="--gtest_filter=BFGSDiagnostic.DirectionUpdate*"

run_test_public_api_smoke:
	@$(MAKE) -C $(CURDIR) run_test_compare TEST_ARGS="--gtest_filter=BFGSPublicApiSmoke.*"

run_bench_direction_update_smoke: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_rvv BENCH_ARGS="$(BFGS_QEMU_DIRECTION_UPDATE_SMOKE_ARGS)" BENCH_RVV_OUTPUT_FILE=$(QEMU_DIRECTION_UPDATE_BENCH_LOG)

run_bench_move_to_slope_smoke: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_rvv BENCH_ARGS="$(BFGS_QEMU_MOVE_TO_SLOPE_SMOKE_ARGS)" BENCH_RVV_OUTPUT_FILE=$(QEMU_MOVE_TO_SLOPE_BENCH_LOG)

run_bench_gicp_shaped_vector6_smoke: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_rvv BENCH_ARGS="$(BFGS_QEMU_GICP_SHAPED_SMOKE_ARGS)" BENCH_RVV_OUTPUT_FILE=$(QEMU_GICP_SHAPED_BENCH_LOG)

run_bench_all_smoke: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_rvv BENCH_ARGS="$(BFGS_QEMU_ALL_SMOKE_ARGS)" BENCH_RVV_OUTPUT_FILE=$(QEMU_ALL_BENCH_LOG)

dump_bench_std: | $(ASM_DIR)/$(ARCH)
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=0 TARGET_BENCH=$(TARGET_BENCH_STD) $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD)
	$(OBJDUMP) -d -C $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) > $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_STD).full.asm
	grep -E $(VEC_REGEX_STR) $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_STD).full.asm > $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_STD).asm || true
	@echo "[DONE] Std asm dump: $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_STD).asm"

generate_qemu_smoke_evidence_manifest: run_bench_direction_update_smoke dump_bench_rvv
	$(PYTHON) script/generate_bfgs_qemu_evidence_manifest.py \
		--rvv-log $(QEMU_DIRECTION_UPDATE_BENCH_LOG) \
		--asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm \
		--output $(QEMU_SMOKE_EVIDENCE_MANIFEST)

run_qemu_smoke_evidence_doctor: generate_qemu_smoke_evidence_manifest
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(QEMU_SMOKE_EVIDENCE_MANIFEST) \
		--output $(QEMU_SMOKE_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_qemu_correctness_state: run_test_compare
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(TEST_STD_OUTPUT_FILE) \
		--path $(TEST_RVV_OUTPUT_FILE) \
		--target run_test_compare \
		--backend qemu \
		--run-label qemu-bfgs-correctness-phase010 \
		--case-filter all \
		--evidence-role qemu_correctness \
		--doc-ref README.zh.md \
		--doc-ref doc/testing-overview.zh.md \
		--doc-ref doc/correctness-tests.zh.md \
		--doc-ref doc/phases/010-diagnostic-scaffold-and-asm-probe/result.zh.md

record_qemu_smoke_evidence_state: run_qemu_smoke_evidence_doctor
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--path $(QEMU_DIRECTION_UPDATE_BENCH_LOG) \
		--path $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm \
		--manifest $(QEMU_SMOKE_EVIDENCE_MANIFEST) \
		--doctor $(QEMU_SMOKE_EVIDENCE_DOCTOR_MD) \
		--target run_bench_direction_update_smoke \
		--backend qemu \
		--run-label qemu-bfgs-direction-update-smoke-phase010 \
		--case-filter direction-update \
		--evidence-role qemu_smoke_only \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/test-support-code-map.zh.md \
		--doc-ref doc/bfgs-evaluation.zh.md \
		--doc-ref doc/phases/010-diagnostic-scaffold-and-asm-probe/result.zh.md

record_qemu_bench_smoke_state: record_qemu_smoke_evidence_state
generate_evidence_manifest: generate_qemu_smoke_evidence_manifest
run_evidence_doctor: run_qemu_smoke_evidence_doctor

run_board_test_smoke: deploy_board | $(OUTPUT_DIR_BOARD)/test_smoke
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_DIR)/log/board/test_smoke' REMOTE_TEST_ARGS='$(REMOTE_TEST_ARGS)'"
	@$(MAKE) -C $(CURDIR) fetch_board_logs OUTPUT_DIR_BOARD=$(OUTPUT_DIR_BOARD)/test_smoke REMOTE_BOARD_OUTPUT_DIR='$(REMOTE_DIR)/log/board/test_smoke'

collect_board_bfgs_direction_update_repeated: deploy_board | $(BFGS_BOARD_REPEATED_DIR)
	@rm -rf "$(BFGS_BOARD_REPEATED_DIR)"/run-*
	@set -e; for run in $$(seq 1 $(BFGS_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(BFGS_BOARD_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(BFGS_BOARD_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] bfgs direction-update repeated run $$run_id/$(BFGS_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter direction-update --iterations $(BFGS_BOARD_BENCH_ITERATIONS) --warmup-iterations $(BFGS_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_bfgs_direction_update:
	$(PYTHON) script/generate_bfgs_board_repeated_summary.py \
		--input-dir $(BFGS_BOARD_REPEATED_DIR) \
		--output-dir $(BFGS_BOARD_REPEATED_DIR) \
		--run-label $(BFGS_BOARD_RUN_LABEL) \
		--case-filter direction-update \
		--expected-runs $(BFGS_BOARD_REPEATED_RUNS) \
		--std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) \
		--rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) \
		--device-label "$(BOARD_LABEL)" \
		--vlen-desc "see SoC / ELF (board)"

run_board_evidence_doctor_bfgs_direction_update: generate_board_evidence_manifest_bfgs_direction_update
	$(PYTHON) ../../script/evidence_doctor.py \
		--manifest $(BFGS_BOARD_EVIDENCE_MANIFEST) \
		--output $(BFGS_BOARD_EVIDENCE_DOCTOR_MD) \
		--fail-on never

record_board_bfgs_direction_update_state: run_board_evidence_doctor_bfgs_direction_update
	$(PYTHON) ../../script/evidence_registry.py record \
		--registry $(EVIDENCE_REGISTRY) \
		--summary $(BFGS_BOARD_SUMMARY) \
		--manifest $(BFGS_BOARD_EVIDENCE_MANIFEST) \
		--doctor $(BFGS_BOARD_EVIDENCE_DOCTOR_MD) \
		--target collect_board_bfgs_direction_update_repeated \
		--backend board \
		--run-label board-bfgs-direction-update-repeated-phase020 \
		--case-filter direction-update \
		--evidence-role pre_production_diagnostic \
		--doc-ref README.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--doc-ref doc/optimization-evidence.zh.md \
		--doc-ref doc/test-support-code-map.zh.md \
		--doc-ref doc/bfgs-evaluation.zh.md \
		--doc-ref doc/phases/020-board-direction-update-diagnostic/plan.zh.md \
		--doc-ref doc/phases/020-board-direction-update-diagnostic/result.zh.md

run_board_bench_bfgs_direction_update_repeated: collect_board_bfgs_direction_update_repeated record_board_bfgs_direction_update_state
run_board_bench_repeated: run_board_bench_bfgs_direction_update_repeated

evidence_status:
	$(PYTHON) ../../script/evidence_registry.py check \
		--registry $(EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/registration/bfgs/log/qemu/*.log' \
		--scan-glob 'test-rvv/registration/bfgs/log/qemu/*.md' \
		--scan-glob 'test-rvv/registration/bfgs/log/qemu/*.json' \
		--scan-glob 'test-rvv/registration/bfgs/log/board/**/summary.md' \
		--scan-glob 'test-rvv/registration/bfgs/log/board/**/evidence_doctor.md' \
		--scan-glob 'test-rvv/registration/bfgs/log/board/**/evidence_manifest.json' \
		--scan-glob 'test-rvv/registration/bfgs/build/asm/riscv/*.asm' \
		--exclude-glob 'test-rvv/registration/bfgs/build/asm/riscv/*.full.asm' \
		--doc test-rvv/registration/bfgs/README.zh.md \
		--doc test-rvv/registration/bfgs/doc/testing-overview.zh.md \
		--doc test-rvv/registration/bfgs/doc/correctness-tests.zh.md \
		--doc test-rvv/registration/bfgs/doc/benchmark-and-evidence.zh.md \
		--doc test-rvv/registration/bfgs/doc/optimization-evidence.zh.md \
		--doc test-rvv/registration/bfgs/doc/test-support-code-map.zh.md \
		--doc test-rvv/registration/bfgs/doc/bfgs-evaluation.zh.md \
		--doc test-rvv/registration/bfgs/doc/phases/010-diagnostic-scaffold-and-asm-probe/result.zh.md \
		--doc test-rvv/registration/bfgs/doc/phases/020-board-direction-update-diagnostic/plan.zh.md \
		--doc test-rvv/registration/bfgs/doc/phases/020-board-direction-update-diagnostic/result.zh.md \
		--require-doc-ref \
		--fail-on never

.PHONY: run_test_candidate_direction run_test_public_api_smoke run_bench_direction_update_smoke run_bench_move_to_slope_smoke run_bench_gicp_shaped_vector6_smoke run_bench_all_smoke dump_bench_std generate_qemu_smoke_evidence_manifest run_qemu_smoke_evidence_doctor record_qemu_correctness_state record_qemu_smoke_evidence_state record_qemu_bench_smoke_state generate_evidence_manifest run_evidence_doctor run_board_test_smoke collect_board_bfgs_direction_update_repeated generate_board_evidence_manifest_bfgs_direction_update run_board_evidence_doctor_bfgs_direction_update record_board_bfgs_direction_update_state run_board_bench_bfgs_direction_update_repeated run_board_bench_repeated evidence_status
