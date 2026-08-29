# =============================================================================
# PCL recognition/dotmod_template_matching RVV topic Makefile
#
# 本 topic 先评估 DOTMOD::detectTemplates 中 getSubMap 拷贝边界与 direct
# window load（直接窗口读取）的 production-shaped diagnostic（生产形态诊断）。
# QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状；性能结论必须
# 来自 board（板卡）或目标硬件 repeated benchmark（重复性能测试）。
# =============================================================================

TOPIC  := dotmod_template_matching
MODULE := recognition
TOPIC_DOC_PREFIX := test-rvv/recognition/dotmod_template_matching

FOCUS_DIR      := pcl/recognition/src/dotmod.cpp
FILTER_REPORT  := log/filtered_dotmod_template_matching.log
ANALYZE_REPORT := log/analyze_dotmod_template_matching.log

TARGET_TEST     := test_dotmod_template_matching
TARGET_TEST_STD := test_dotmod_template_matching_std
TARGET_TEST_RVV := test_dotmod_template_matching_rvv
SRCS_TEST       := src/test_dotmod_template_matching.cpp

TARGET_BENCH     := bench_dotmod_template_matching
TARGET_BENCH_STD := bench_dotmod_template_matching_std
TARGET_BENCH_RVV := bench_dotmod_template_matching_rvv
SRCS_BENCH       := src/bench_dotmod_template_matching.cpp
BENCH_ARGS       ?= 256 192 24 16 200 5

REMOTE_DIR ?= /root/pcl-test/recognition/dotmod_template_matching
REMOTE_BENCH_ARGS ?= $(BENCH_ARGS)
REMOTE_TEST_ARGS ?=
BENCH_ITERATIONS := $(word 5,$(BENCH_ARGS))

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/recognition/include \
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

LIBS_TEST  := -lpcl_recognition -lpcl_common -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lpcl_recognition -lpcl_common -lm

include ../../mk/rvv-topic.mk

PHASE000_REPEATED_BOARD_DIR ?= log/board/repeated_phase000_submap_direct_window
PHASE000_REPEATED_REMOTE_DIR ?= $(REMOTE_DIR)/log/board/repeated_phase000_submap_direct_window
PHASE000_REPEATED_RUNS ?= 01 02 03 04 05
PHASE000_EVIDENCE_SUMMARY ?= $(PHASE000_REPEATED_BOARD_DIR)/summary.md
PHASE000_EVIDENCE_MANIFEST ?= $(PHASE000_REPEATED_BOARD_DIR)/evidence_manifest.json
PHASE000_EVIDENCE_DOCTOR_MD ?= $(PHASE000_REPEATED_BOARD_DIR)/evidence_doctor.md
PHASE000_EVIDENCE_DOCTOR_JSON ?= $(PHASE000_REPEATED_BOARD_DIR)/evidence_doctor.json
PHASE010_REPEATED_BOARD_DIR ?= log/board/repeated_phase010_full_detecttemplates
PHASE010_REPEATED_REMOTE_DIR ?= $(REMOTE_DIR)/log/board/repeated_phase010_full_detecttemplates
PHASE010_REPEATED_RUNS ?= 01 02 03 04 05
PHASE010_EVIDENCE_SUMMARY ?= $(PHASE010_REPEATED_BOARD_DIR)/summary.md
PHASE010_EVIDENCE_MANIFEST ?= $(PHASE010_REPEATED_BOARD_DIR)/evidence_manifest.json
PHASE010_EVIDENCE_DOCTOR_MD ?= $(PHASE010_REPEATED_BOARD_DIR)/evidence_doctor.md
PHASE010_EVIDENCE_DOCTOR_JSON ?= $(PHASE010_REPEATED_BOARD_DIR)/evidence_doctor.json
PHASE020_DIR := doc/phases/020-production-integration-direct-window
PHASE020_REPEATED_BOARD_DIR ?= log/board/repeated_phase020_production_direct
PHASE020_REPEATED_REMOTE_DIR ?= $(REMOTE_DIR)/log/board/repeated_phase020_production_direct
PHASE020_REPEATED_RUNS ?= 01 02 03 04 05
PHASE020_EVIDENCE_SUMMARY ?= $(PHASE020_REPEATED_BOARD_DIR)/summary.md
PHASE020_EVIDENCE_MANIFEST ?= $(PHASE020_REPEATED_BOARD_DIR)/evidence_manifest.json
PHASE020_EVIDENCE_DOCTOR_MD ?= $(PHASE020_REPEATED_BOARD_DIR)/evidence_doctor.md
PHASE020_EVIDENCE_DOCTOR_JSON ?= $(PHASE020_REPEATED_BOARD_DIR)/evidence_doctor.json
PHASE020_TARGET_TEST_STD := test_dotmod_template_matching_production_direct_std
PHASE020_TARGET_TEST_RVV := test_dotmod_template_matching_production_direct_rvv
PHASE020_TARGET_BENCH_STD := bench_dotmod_template_matching_production_direct_std
PHASE020_TARGET_BENCH_RVV := bench_dotmod_template_matching_production_direct_rvv
PHASE020_TEST_STD_BINARY := build/riscv/$(PHASE020_TARGET_TEST_STD)
PHASE020_TEST_RVV_BINARY := build/riscv/$(PHASE020_TARGET_TEST_RVV)
PHASE020_BENCH_STD_BINARY := build/riscv/$(PHASE020_TARGET_BENCH_STD)
PHASE020_BENCH_RVV_BINARY := build/riscv/$(PHASE020_TARGET_BENCH_RVV)
PHASE020_ASM := build/asm/riscv/$(PHASE020_TARGET_BENCH_RVV).full.asm
PHASE020_PRODUCTION_SRCS = \
	$(PCL_SOURCE_ROOT)/recognition/src/dotmod.cpp \
	$(PCL_SOURCE_ROOT)/recognition/src/quantizable_modality.cpp \
	$(PCL_SOURCE_ROOT)/recognition/src/mask_map.cpp
PHASE020_TEST_SRCS = src/test_dotmod_template_matching_production_direct.cpp $(PHASE020_PRODUCTION_SRCS)
PHASE020_BENCH_SRCS = src/bench_dotmod_template_matching_production_direct.cpp $(PHASE020_PRODUCTION_SRCS)
PHASE020_LIBS_TEST := -lpcl_common -lboost_system -llz4 -lgtest -lgtest_main -lpthread -lm
PHASE020_LIBS_BENCH := -lpcl_common -lboost_system -llz4 -lm
EVIDENCE_REGISTRY ?= log/evidence_registry.json
EVIDENCE_DOC_REFS := \
	$(TOPIC_DOC_PREFIX)/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/dotmod_template_matching-evaluation.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/optimization-roadmap.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/README.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/optimization-matrix.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/000-current-state-and-submap-direct-window/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/000-current-state-and-submap-direct-window/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/010-full-detecttemplates-shaped-diagnostic/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/010-full-detecttemplates-shaped-diagnostic/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-production-integration-direct-window/plan.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/020-production-integration-direct-window/result.zh.md \
	$(TOPIC_DOC_PREFIX)/doc/phases/030-response-buffer-reuse/result.zh.md \
	doc-rvv/recognition/dotmod_template_matching-RVV.zh.md \
	doc-rvv/library-screening/recognition/recognition-function-evaluation-queue.zh.md
EVIDENCE_DOC_ARGS := $(foreach doc,$(EVIDENCE_DOC_REFS),--doc $(doc))
EVIDENCE_DOC_REF_ARGS := $(foreach doc,$(EVIDENCE_DOC_REFS),--doc-ref $(doc))

.PHONY: check_dotmod_direct_window_rvv_asm collect_submap_direct_window_repeated_board collect_full_detecttemplates_repeated_board generate_phase000_evidence_manifest run_phase000_evidence_doctor record_phase000_evidence_state generate_phase010_evidence_manifest run_phase010_evidence_doctor record_phase010_evidence_state run_production_direct_test_std run_production_direct_test_rvv run_production_direct_test_compare dump_production_direct_bench_rvv collect_production_direct_repeated_board generate_production_direct_evidence_manifest run_production_direct_evidence_doctor record_production_direct_evidence_state check_production_direct_evidence_freshness check_evidence_freshness

check_dotmod_direct_window_rvv_asm: dump_bench_rvv
	@grep -Eq 'vle8' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vand' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vmsne' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vcpop' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@echo "[PASS] DOTMOD direct-window RVV asm gate matched byte load/and/mask popcount instructions."

collect_submap_direct_window_repeated_board: check_dotmod_direct_window_rvv_asm
	rm -rf $(PHASE000_REPEATED_BOARD_DIR)
	@for run in $(PHASE000_REPEATED_RUNS); do \
		echo "[BOARD] Phase 000 submap/direct-window repeated run $$run"; \
		$(MAKE) -C $(CURDIR) board_smoke \
			OUTPUT_DIR_BOARD="$(PHASE000_REPEATED_BOARD_DIR)/run-$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(PHASE000_REPEATED_REMOTE_DIR)/run-$$run" \
			BENCH_ARGS="$(BENCH_ARGS)" \
			REMOTE_BENCH_ARGS="$(BENCH_ARGS)" \
			REMOTE_TEST_ARGS="$(REMOTE_TEST_ARGS)"; \
	done

generate_phase000_evidence_manifest:
	@$(PYTHON_RUN) script/generate_dotmod_template_matching_evidence_manifest.py \
		--input-dir "$(PHASE000_REPEATED_BOARD_DIR)" \
		--run-label dotmod_phase000_submap_direct_window_repeated \
		--manifest-case dotmod_direct_window_score \
		--summary "$(PHASE000_EVIDENCE_SUMMARY)" \
		--manifest "$(PHASE000_EVIDENCE_MANIFEST)"

run_phase000_evidence_doctor: generate_phase000_evidence_manifest
	@$(PYTHON_RUN) $(TEST_RVV_SHARED_SCRIPT_DIR)/evidence_doctor.py \
		--manifest "$(PHASE000_EVIDENCE_MANIFEST)" \
		--output "$(PHASE000_EVIDENCE_DOCTOR_MD)" \
		--json-output "$(PHASE000_EVIDENCE_DOCTOR_JSON)" \
		--fail-on error

record_phase000_evidence_state: run_phase000_evidence_doctor
	@$(PYTHON_RUN) $(TEST_RVV_SHARED_SCRIPT_DIR)/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--path "$(PHASE000_EVIDENCE_DOCTOR_MD)" \
		--summary "$(PHASE000_EVIDENCE_SUMMARY)" \
		--manifest "$(PHASE000_EVIDENCE_MANIFEST)" \
		--doctor "$(PHASE000_EVIDENCE_DOCTOR_JSON)" \
		--target collect_submap_direct_window_repeated_board \
		--backend board \
		--run-label dotmod_phase000_submap_direct_window_repeated \
		--case-filter "256 192 24 16 200 5" \
		--evidence-role production_shaped_diagnostic \
		--doc-ref test-rvv/recognition/dotmod_template_matching/doc/phases/000-current-state-and-submap-direct-window/result.zh.md \
		--doc-ref test-rvv/recognition/dotmod_template_matching/doc/dotmod_template_matching-evaluation.zh.md

collect_full_detecttemplates_repeated_board: check_dotmod_direct_window_rvv_asm
	rm -rf $(PHASE010_REPEATED_BOARD_DIR)
	@for run in $(PHASE010_REPEATED_RUNS); do \
		echo "[BOARD] Phase 010 full detectTemplates-shaped repeated run $$run"; \
		$(MAKE) -C $(CURDIR) board_smoke \
			OUTPUT_DIR_BOARD="$(PHASE010_REPEATED_BOARD_DIR)/run-$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(PHASE010_REPEATED_REMOTE_DIR)/run-$$run" \
			BENCH_ARGS="$(BENCH_ARGS)" \
			REMOTE_BENCH_ARGS="$(BENCH_ARGS)" \
			REMOTE_TEST_ARGS="$(REMOTE_TEST_ARGS)"; \
	done

generate_phase010_evidence_manifest:
	@$(PYTHON_RUN) script/generate_dotmod_template_matching_evidence_manifest.py \
		--input-dir "$(PHASE010_REPEATED_BOARD_DIR)" \
		--run-label dotmod_phase010_full_detecttemplates_repeated \
		--manifest-case dotmod_full_detecttemplates_shaped_total \
		--summary "$(PHASE010_EVIDENCE_SUMMARY)" \
		--manifest "$(PHASE010_EVIDENCE_MANIFEST)"

run_phase010_evidence_doctor: generate_phase010_evidence_manifest
	@$(PYTHON_RUN) $(TEST_RVV_SHARED_SCRIPT_DIR)/evidence_doctor.py \
		--manifest "$(PHASE010_EVIDENCE_MANIFEST)" \
		--output "$(PHASE010_EVIDENCE_DOCTOR_MD)" \
		--json-output "$(PHASE010_EVIDENCE_DOCTOR_JSON)" \
		--fail-on error

record_phase010_evidence_state: run_phase010_evidence_doctor
	@$(PYTHON_RUN) $(TEST_RVV_SHARED_SCRIPT_DIR)/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--path "$(PHASE010_EVIDENCE_DOCTOR_MD)" \
		--summary "$(PHASE010_EVIDENCE_SUMMARY)" \
		--manifest "$(PHASE010_EVIDENCE_MANIFEST)" \
		--doctor "$(PHASE010_EVIDENCE_DOCTOR_JSON)" \
		--target collect_full_detecttemplates_repeated_board \
		--backend board \
		--run-label dotmod_phase010_full_detecttemplates_repeated \
		--case-filter "256 192 24 16 100 5 8 2 0.9" \
		--evidence-role production_shaped_diagnostic \
		--doc-ref test-rvv/recognition/dotmod_template_matching/doc/phases/010-full-detecttemplates-shaped-diagnostic/result.zh.md \
		--doc-ref test-rvv/recognition/dotmod_template_matching/doc/dotmod_template_matching-evaluation.zh.md

$(PHASE020_TEST_STD_BINARY): $(PHASE020_TEST_SRCS) | $(LOG_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Phase 020 production-direct Unit Test Std ($(ARCH))..."
	$(CXX) $(filter-out -D__RVV10__,$(CXXFLAGS)) -DPCLAPI_EXPORTS $(PHASE020_TEST_SRCS) $(LDFLAGS) $(EXTRA_LDFLAGS) $(PHASE020_LIBS_TEST) -o $@

$(PHASE020_TEST_RVV_BINARY): $(PHASE020_TEST_SRCS) | $(LOG_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Phase 020 production-direct Unit Test RVV ($(ARCH))..."
	$(CXX) $(CXXFLAGS) -DPCLAPI_EXPORTS $(PHASE020_TEST_SRCS) $(LDFLAGS) $(EXTRA_LDFLAGS) $(PHASE020_LIBS_TEST) -o $@

$(PHASE020_BENCH_STD_BINARY): $(PHASE020_BENCH_SRCS) | $(LOG_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Phase 020 production-direct Benchmark Std ($(ARCH))..."
	$(CXX) $(filter-out -D__RVV10__,$(CXXFLAGS)) -DPCLAPI_EXPORTS $(PHASE020_BENCH_SRCS) $(LDFLAGS) $(EXTRA_LDFLAGS) $(PHASE020_LIBS_BENCH) -o $@

$(PHASE020_BENCH_RVV_BINARY): $(PHASE020_BENCH_SRCS) | $(LOG_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Phase 020 production-direct Benchmark RVV ($(ARCH))..."
	$(CXX) $(CXXFLAGS) -DPCLAPI_EXPORTS $(PHASE020_BENCH_SRCS) $(LDFLAGS) $(EXTRA_LDFLAGS) $(PHASE020_LIBS_BENCH) -o $@

run_production_direct_test_std: $(PHASE020_TEST_STD_BINARY) | $(OUTPUT_DIR_QEMU)
	$(RUN_CMD) ./$(PHASE020_TEST_STD_BINARY) $(TEST_ARGS) 2>&1 | tee $(OUTPUT_DIR_QEMU)/phase020-production-direct-test-std.log

run_production_direct_test_rvv: $(PHASE020_TEST_RVV_BINARY) | $(OUTPUT_DIR_QEMU)
	$(RUN_CMD) ./$(PHASE020_TEST_RVV_BINARY) $(TEST_ARGS) 2>&1 | tee $(OUTPUT_DIR_QEMU)/phase020-production-direct-test-rvv.log

run_production_direct_test_compare: run_production_direct_test_std run_production_direct_test_rvv

dump_production_direct_bench_rvv: $(PHASE020_BENCH_RVV_BINARY) | $(ASM_DIR)/$(ARCH)
	$(OBJDUMP) -d -C $(PHASE020_BENCH_RVV_BINARY) > $(PHASE020_ASM)
	grep -E $(VEC_REGEX_STR) $(PHASE020_ASM) > $(ASM_DIR)/$(ARCH)/$(PHASE020_TARGET_BENCH_RVV).asm
	grep -E 'dotmodScoreWindowDirectRVV' $(PHASE020_ASM)
	grep -E '\bvle8\.v\b' $(PHASE020_ASM)
	grep -E '\bvand\.vv\b|\bvand\.vx\b|\bvand\.vi\b' $(PHASE020_ASM)
	grep -E '\bvmsne\.vx\b|\bvmsne\.vi\b' $(PHASE020_ASM)
	grep -E '\bvcpop\.m\b' $(PHASE020_ASM)
	@echo "[DONE] Phase 020 production-direct RVV asm dump: $(PHASE020_ASM)"

collect_production_direct_repeated_board: dump_production_direct_bench_rvv
	rm -rf $(PHASE020_REPEATED_BOARD_DIR)
	@for run in $(PHASE020_REPEATED_RUNS); do \
		echo "[BOARD] Phase 020 production-direct repeated run $$run"; \
		$(MAKE) -C $(CURDIR) deploy_board \
			TARGET_BENCH_STD="$(PHASE020_TARGET_BENCH_STD)" \
			TARGET_BENCH_RVV="$(PHASE020_TARGET_BENCH_RVV)" \
			SRCS_BENCH="$(PHASE020_BENCH_SRCS)" \
			LIBS_BENCH="$(PHASE020_LIBS_BENCH)" \
			TARGET_TEST="$(PHASE020_TARGET_TEST_RVV)" \
			BOARD_TARGET_TEST="$(PHASE020_TARGET_TEST_RVV)" \
			SRCS_TEST="$(PHASE020_TEST_SRCS)" \
			LIBS_TEST="$(PHASE020_LIBS_TEST)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(PHASE020_REPEATED_REMOTE_DIR)/run-$$run' REMOTE_TEST='$(PHASE020_TARGET_TEST_RVV)' REMOTE_TEST_ARGS='--gtest_filter=DOTMODTemplateMatchingProductionDirect.*'"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$(PHASE020_REPEATED_REMOTE_DIR)/run-$$run' REMOTE_BENCH_STD='$(PHASE020_TARGET_BENCH_STD)' REMOTE_BENCH_RVV='$(PHASE020_TARGET_BENCH_RVV)' BENCH_COMPARE_SAVE='$(PHASE020_REPEATED_REMOTE_DIR)/run-$$run/analyze_bench_compare.log' BOARD_LABEL='Milkv-Jupiter Phase 020 DOTMOD production-direct' REMOTE_BENCH_ARGS='$(REMOTE_BENCH_ARGS)'"; \
		mkdir -p "$(PHASE020_REPEATED_BOARD_DIR)/run-$$run"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$(PHASE020_REPEATED_REMOTE_DIR)/run-$$run/ $(PHASE020_REPEATED_BOARD_DIR)/run-$$run/; \
	done

generate_production_direct_evidence_manifest: dump_production_direct_bench_rvv
	@$(PYTHON_RUN) script/generate_dotmod_template_matching_evidence_manifest.py \
		--input-dir "$(PHASE020_REPEATED_BOARD_DIR)" \
		--run-label dotmod_phase020_production_direct_repeated \
		--manifest-case dotmod_production_detecttemplates_total \
		--evidence-role production-public \
		--summary "$(PHASE020_EVIDENCE_SUMMARY)" \
		--manifest "$(PHASE020_EVIDENCE_MANIFEST)"

run_production_direct_evidence_doctor: generate_production_direct_evidence_manifest
	@$(PYTHON_RUN) $(TEST_RVV_SHARED_SCRIPT_DIR)/evidence_doctor.py \
		--manifest "$(PHASE020_EVIDENCE_MANIFEST)" \
		--output "$(PHASE020_EVIDENCE_DOCTOR_MD)" \
		--json-output "$(PHASE020_EVIDENCE_DOCTOR_JSON)" \
		--fail-on error

record_production_direct_evidence_state: run_production_direct_evidence_doctor
	@$(PYTHON_RUN) $(TEST_RVV_SHARED_SCRIPT_DIR)/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--path "$(PHASE020_EVIDENCE_DOCTOR_MD)" \
		--summary "$(PHASE020_EVIDENCE_SUMMARY)" \
		--manifest "$(PHASE020_EVIDENCE_MANIFEST)" \
		--doctor "$(PHASE020_EVIDENCE_DOCTOR_JSON)" \
		--target collect_production_direct_repeated_board \
		--backend board \
		--run-label dotmod_phase020_production_direct_repeated \
		--case-filter "256 192 24 16 $(BENCH_ITERATIONS) 5 8 2 0.9 1" \
		--evidence-role production-public \
		$(EVIDENCE_DOC_REF_ARGS)

check_production_direct_evidence_freshness:
	@$(PYTHON_RUN) $(TEST_RVV_SHARED_SCRIPT_DIR)/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob "test-rvv/recognition/dotmod_template_matching/log/board/repeated_phase020_production_direct/summary.md" \
		--scan-glob "test-rvv/recognition/dotmod_template_matching/log/board/repeated_phase020_production_direct/evidence_manifest.json" \
		--scan-glob "test-rvv/recognition/dotmod_template_matching/log/board/repeated_phase020_production_direct/evidence_doctor.md" \
		$(EVIDENCE_DOC_ARGS) \
		--require-doc-ref \
		--fail-on never

check_evidence_freshness:
	@$(PYTHON_RUN) $(TEST_RVV_SHARED_SCRIPT_DIR)/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob "test-rvv/recognition/dotmod_template_matching/log/board/**/summary.md" \
		--scan-glob "test-rvv/recognition/dotmod_template_matching/log/board/**/evidence_manifest.json" \
		--scan-glob "test-rvv/recognition/dotmod_template_matching/log/board/**/evidence_doctor.md" \
		--doc test-rvv/recognition/dotmod_template_matching/doc/phases/000-current-state-and-submap-direct-window/result.zh.md \
		--doc test-rvv/recognition/dotmod_template_matching/doc/dotmod_template_matching-evaluation.zh.md \
		--fail-on never
