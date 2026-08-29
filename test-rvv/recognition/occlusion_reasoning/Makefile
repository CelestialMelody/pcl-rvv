# =============================================================================
# PCL recognition/occlusion_reasoning RVV topic Makefile
#
# 本 topic 已把 ZBuffering::filter(model, indices) 的 projection-filter
# 接入 production direct（真实生产路径）验证。QEMU（仿真器）只用于
# correctness（正确性）、构建和日志形状；性能结论必须来自 board（板卡）
# 或目标硬件 repeated benchmark（重复性能测试）。
# =============================================================================

TOPIC  := occlusion_reasoning
MODULE := recognition

FOCUS_DIR      := pcl/recognition/include/pcl/recognition/impl/hv/occlusion_reasoning.hpp
FILTER_REPORT  := log/filtered_occlusion_reasoning.log
ANALYZE_REPORT := log/analyze_occlusion_reasoning.log

TARGET_TEST     := test_occlusion_reasoning
TARGET_TEST_STD := test_occlusion_reasoning_std
TARGET_TEST_RVV := test_occlusion_reasoning_rvv
SRCS_TEST       := src/test_occlusion_reasoning.cpp

TARGET_BENCH     := bench_occlusion_reasoning
TARGET_BENCH_STD := bench_occlusion_reasoning_std
TARGET_BENCH_RVV := bench_occlusion_reasoning_rvv
SRCS_BENCH       := src/bench_occlusion_reasoning.cpp
BENCH_ARGS       ?= 65536 150 150 200 5

TARGET_INLINE_BENCH     := bench_occlusion_reasoning_public_inline
TARGET_INLINE_BENCH_STD := bench_occlusion_reasoning_public_inline_std
TARGET_INLINE_BENCH_RVV := bench_occlusion_reasoning_public_inline_rvv
SRCS_INLINE_BENCH       := src/bench_occlusion_reasoning_public_inline.cpp
INLINE_BENCH_ARGS       ?= $(BENCH_ARGS)

REMOTE_DIR ?= /root/pcl-test/recognition/occlusion_reasoning

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
EXTRA_CXXFLAGS += -DPCL_RVV_OCCLUSION_REASONING_TEST_HOOK

REMOTE_BENCH_ARGS ?= $(BENCH_ARGS)
REMOTE_TEST_ARGS ?=

REPEATED_BOARD_RUNS ?= 5
REPEATED_BOARD_TAG ?= phase010_production_direct
REPEATED_BOARD_TITLE ?= Occlusion reasoning ZBuffering production-direct repeated board summary
REPEATED_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated_$(REPEATED_BOARD_TAG)
REPEATED_BOARD_REMOTE_TAG ?= phase010_production_direct
REPEATED_BOARD_RUN_LABEL ?= occlusion_reasoning_phase010_production_direct_repeated
REPEATED_BOARD_CASE_NAME ?= production_filter_indices_projection_mask_compress
EVIDENCE_ROLE_REPEATED ?= production-public
EVIDENCE_MANIFEST_REPEATED ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_manifest.json
EVIDENCE_DOCTOR_REPEATED_MD ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.md
EVIDENCE_DOCTOR_REPEATED_JSON ?= $(REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.json
EVIDENCE_REGISTRY ?= $(LOG_DIR)/evidence_registry.json
INLINE_REPEATED_BOARD_TAG ?= phase020_inline_filter_production_direct
INLINE_REPEATED_BOARD_TITLE ?= Occlusion reasoning public inline filter production-direct repeated board summary
INLINE_REPEATED_BOARD_OUTPUT_DIR ?= $(OUTPUT_DIR_BOARD)/repeated_$(INLINE_REPEATED_BOARD_TAG)
INLINE_REPEATED_BOARD_REMOTE_TAG ?= phase020_inline_filter_production_direct
INLINE_REPEATED_BOARD_RUN_LABEL ?= occlusion_reasoning_phase020_inline_filter_production_direct_repeated
INLINE_REPEATED_BOARD_CASE_NAME ?= public_inline_filter_visible_points
INLINE_EVIDENCE_ROLE_REPEATED ?= production-public
INLINE_EVIDENCE_MANIFEST_REPEATED ?= $(INLINE_REPEATED_BOARD_OUTPUT_DIR)/evidence_manifest.json
INLINE_EVIDENCE_DOCTOR_REPEATED_MD ?= $(INLINE_REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.md
INLINE_EVIDENCE_DOCTOR_REPEATED_JSON ?= $(INLINE_REPEATED_BOARD_OUTPUT_DIR)/evidence_doctor.json
EVIDENCE_DOC_REFS := \
	test-rvv/recognition/occlusion_reasoning/README.zh.md \
	test-rvv/recognition/occlusion_reasoning/doc/occlusion_reasoning-evaluation.zh.md \
	test-rvv/recognition/occlusion_reasoning/doc/testing-overview.zh.md \
	test-rvv/recognition/occlusion_reasoning/doc/correctness-tests.zh.md \
	test-rvv/recognition/occlusion_reasoning/doc/benchmark-and-evidence.zh.md \
	test-rvv/recognition/occlusion_reasoning/doc/optimization-evidence.zh.md \
	test-rvv/recognition/occlusion_reasoning/doc/test-support-code-map.zh.md \
	test-rvv/recognition/occlusion_reasoning/doc/optimization-roadmap.zh.md \
	test-rvv/recognition/occlusion_reasoning/doc/phases/README.zh.md \
	test-rvv/recognition/occlusion_reasoning/doc/phases/optimization-matrix.zh.md \
	test-rvv/recognition/occlusion_reasoning/doc/phases/000-current-state-and-filter-diagnostic/plan.zh.md \
	test-rvv/recognition/occlusion_reasoning/doc/phases/000-current-state-and-filter-diagnostic/result.zh.md \
	test-rvv/recognition/occlusion_reasoning/doc/phases/010-production-integration/plan.zh.md \
	test-rvv/recognition/occlusion_reasoning/doc/phases/010-production-integration/result.zh.md \
	test-rvv/recognition/occlusion_reasoning/doc/phases/020-inline-filter-production-integration/plan.zh.md \
	test-rvv/recognition/occlusion_reasoning/doc/phases/020-inline-filter-production-integration/result.zh.md \
	doc-rvv/recognition/occlusion_reasoning-RVV.zh.md
EVIDENCE_DOC_ARGS := $(foreach doc,$(EVIDENCE_DOC_REFS),--doc $(doc))
EVIDENCE_DOC_REF_ARGS := $(foreach doc,$(EVIDENCE_DOC_REFS),--doc-ref $(doc))

include ../../mk/rvv-topic.mk

run_qemu_smoke: run_test_compare

check_occlusion_filter_rvv_asm: clean_bench_rvv dump_bench_rvv
	@grep -Eq 'vlse32|vlsseg3e32' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vfdiv' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@grep -Eq 'vfcvt' $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm
	@echo "[PASS] occlusion filter RVV asm gate matched expected production projection instructions."

check_inline_filter_rvv_asm: | $(ASM_DIR)/$(ARCH)
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_BENCH=$(TARGET_INLINE_BENCH_RVV) SRCS_BENCH="$(SRCS_INLINE_BENCH)" $(BUILD_DIR)/$(ARCH)/$(TARGET_INLINE_BENCH_RVV)
	$(OBJDUMP) -d -C $(BUILD_DIR)/$(ARCH)/$(TARGET_INLINE_BENCH_RVV) > $(ASM_DIR)/$(ARCH)/$(TARGET_INLINE_BENCH_RVV).full.asm
	grep -E $(VEC_REGEX_STR) $(ASM_DIR)/$(ARCH)/$(TARGET_INLINE_BENCH_RVV).full.asm > $(ASM_DIR)/$(ARCH)/$(TARGET_INLINE_BENCH_RVV).asm || true
	@grep -Eq 'vlse32|vlsseg3e32' $(ASM_DIR)/$(ARCH)/$(TARGET_INLINE_BENCH_RVV).asm
	@grep -Eq 'vfdiv' $(ASM_DIR)/$(ARCH)/$(TARGET_INLINE_BENCH_RVV).asm
	@grep -Eq 'vfcvt' $(ASM_DIR)/$(ARCH)/$(TARGET_INLINE_BENCH_RVV).asm
	@echo "[PASS] inline occlusion filter RVV asm gate matched expected production projection instructions."

deploy_inline_bench_rvv: deploy_files
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_BENCH=$(TARGET_INLINE_BENCH_RVV) SRCS_BENCH="$(SRCS_INLINE_BENCH)" $(BUILD_DIR)/$(ARCH)/$(TARGET_INLINE_BENCH_RVV)
	@$(STRIP) -s ./$(BUILD_DIR)/$(ARCH)/$(TARGET_INLINE_BENCH_RVV) -o ./$(TARGET_INLINE_BENCH_RVV)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_INLINE_BENCH_RVV)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_INLINE_BENCH_RVV)
	@rm -f ./$(TARGET_INLINE_BENCH_RVV)_stripped

deploy_inline_bench_std: deploy_files
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=0 TARGET_BENCH=$(TARGET_INLINE_BENCH_STD) SRCS_BENCH="$(SRCS_INLINE_BENCH)" $(BUILD_DIR)/$(ARCH)/$(TARGET_INLINE_BENCH_STD)
	@$(STRIP) -s ./$(BUILD_DIR)/$(ARCH)/$(TARGET_INLINE_BENCH_STD) -o ./$(TARGET_INLINE_BENCH_STD)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_INLINE_BENCH_STD)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_INLINE_BENCH_STD)
	@rm -f ./$(TARGET_INLINE_BENCH_STD)_stripped

deploy_inline_board: deploy_files deploy_inline_bench_std deploy_inline_bench_rvv

run_inline_board_bench_compare: deploy_inline_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_BENCH_STD='$(TARGET_INLINE_BENCH_STD)' REMOTE_BENCH_RVV='$(TARGET_INLINE_BENCH_RVV)' REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' BENCH_COMPARE_SAVE='$(REMOTE_BOARD_OUTPUT_DIR)/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(INLINE_BENCH_ARGS)'"

fetch_inline_board_logs: | $(OUTPUT_DIR_BOARD)
	@rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_BOARD_OUTPUT_DIR)/ $(OUTPUT_DIR_BOARD)/

board_repeated: check_occlusion_filter_rvv_asm
	@set -eu; \
	for i in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_board_bench_compare fetch_board_logs \
			BENCH_ARGS="$(BENCH_ARGS)" \
			OUTPUT_DIR_BOARD="$(REPEATED_BOARD_OUTPUT_DIR)/$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated_$(REPEATED_BOARD_REMOTE_TAG)/$$run" \
			REMOTE_BENCH_ARGS="$(REMOTE_BENCH_ARGS)" \
			REMOTE_TEST_ARGS="$(REMOTE_TEST_ARGS)"; \
	done

inline_board_repeated: check_inline_filter_rvv_asm
	@set -eu; \
	for i in $$(seq 1 $(REPEATED_BOARD_RUNS)); do \
		run=$$(printf 'run_%02d' "$$i"); \
		$(MAKE) -C $(CURDIR) run_inline_board_bench_compare fetch_inline_board_logs \
			INLINE_BENCH_ARGS="$(INLINE_BENCH_ARGS)" \
			OUTPUT_DIR_BOARD="$(INLINE_REPEATED_BOARD_OUTPUT_DIR)/$$run" \
			REMOTE_BOARD_OUTPUT_DIR="$(REMOTE_DIR)/log/board/repeated_$(INLINE_REPEATED_BOARD_REMOTE_TAG)/$$run" \
			REMOTE_TEST_ARGS="$(REMOTE_TEST_ARGS)"; \
	done

evidence_manifest_repeated:
	python3 script/generate_occlusion_reasoning_evidence_manifest.py \
		--summary-dir "$(REPEATED_BOARD_OUTPUT_DIR)" \
		--output "$(EVIDENCE_MANIFEST_REPEATED)" \
		--summary-output "$(REPEATED_BOARD_OUTPUT_DIR)/summary.md" \
		--title "$(REPEATED_BOARD_TITLE)" \
		--run-label "$(REPEATED_BOARD_RUN_LABEL)" \
		--case-name "$(REPEATED_BOARD_CASE_NAME)" \
		--evidence-role "$(EVIDENCE_ROLE_REPEATED)" \
		--expected-runs "$(REPEATED_BOARD_RUNS)" \
		--asm "$(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm"

evidence_doctor_repeated: evidence_manifest_repeated
	python3 ../../script/evidence_doctor.py \
		--manifest "$(EVIDENCE_MANIFEST_REPEATED)" \
		--output "$(EVIDENCE_DOCTOR_REPEATED_MD)" \
		--json-output "$(EVIDENCE_DOCTOR_REPEATED_JSON)" \
		--fail-on never

inline_evidence_manifest_repeated:
	python3 script/generate_occlusion_reasoning_evidence_manifest.py \
		--summary-dir "$(INLINE_REPEATED_BOARD_OUTPUT_DIR)" \
		--output "$(INLINE_EVIDENCE_MANIFEST_REPEATED)" \
		--summary-output "$(INLINE_REPEATED_BOARD_OUTPUT_DIR)/summary.md" \
		--title "$(INLINE_REPEATED_BOARD_TITLE)" \
		--run-label "$(INLINE_REPEATED_BOARD_RUN_LABEL)" \
		--case-name "$(INLINE_REPEATED_BOARD_CASE_NAME)" \
		--evidence-role "$(INLINE_EVIDENCE_ROLE_REPEATED)" \
		--expected-runs "$(REPEATED_BOARD_RUNS)" \
		--asm "$(ASM_DIR)/$(ARCH)/$(TARGET_INLINE_BENCH_RVV).asm" \
		--wrapper "pcl::occlusion_reasoning::filter(scene, model, f, threshold)" \
		--timer-boundary "production_public_inline_filter_includes_copyPointCloud_excludes_scene_model_setup" \
		--row-source "organized_scene_depth_plus_model_point_stream" \
		--group "phase020_inline_filter_production_direct" \
		--baseline-implementation "occlusionReasoningFilterStd" \
		--candidate-implementation "occlusionReasoningFilterRVV" \
		--point-type "PointXYZ / float / AoS" \
		--checksum-policy "filtered_cloud_xyz_fnv1a"

inline_evidence_doctor_repeated: inline_evidence_manifest_repeated
	python3 ../../script/evidence_doctor.py \
		--manifest "$(INLINE_EVIDENCE_MANIFEST_REPEATED)" \
		--output "$(INLINE_EVIDENCE_DOCTOR_REPEATED_MD)" \
		--json-output "$(INLINE_EVIDENCE_DOCTOR_REPEATED_JSON)" \
		--fail-on never

record_evidence_state_repeated: evidence_doctor_repeated
	python3 ../../script/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(REPEATED_BOARD_OUTPUT_DIR)/summary.md" \
		--manifest "$(EVIDENCE_MANIFEST_REPEATED)" \
		--doctor "$(EVIDENCE_DOCTOR_REPEATED_MD)" \
		--path "$(EVIDENCE_DOCTOR_REPEATED_JSON)" \
		--target board_repeated \
		--backend board \
		--run-label "$(REPEATED_BOARD_RUN_LABEL)" \
		--case-filter "$(BENCH_ARGS)" \
		--evidence-role "$(EVIDENCE_ROLE_REPEATED)" \
		$(EVIDENCE_DOC_REF_ARGS) \
		--freshness-state fresh

record_inline_evidence_state_repeated: inline_evidence_doctor_repeated
	python3 ../../script/evidence_registry.py record \
		--registry "$(EVIDENCE_REGISTRY)" \
		--summary "$(INLINE_REPEATED_BOARD_OUTPUT_DIR)/summary.md" \
		--manifest "$(INLINE_EVIDENCE_MANIFEST_REPEATED)" \
		--doctor "$(INLINE_EVIDENCE_DOCTOR_REPEATED_MD)" \
		--path "$(INLINE_EVIDENCE_DOCTOR_REPEATED_JSON)" \
		--target inline_board_repeated \
		--backend board \
		--run-label "$(INLINE_REPEATED_BOARD_RUN_LABEL)" \
		--case-filter "$(INLINE_BENCH_ARGS)" \
		--evidence-role "$(INLINE_EVIDENCE_ROLE_REPEATED)" \
		$(EVIDENCE_DOC_REF_ARGS) \
		--freshness-state fresh

check_evidence_freshness:
	python3 ../../script/evidence_registry.py check \
		--registry "$(EVIDENCE_REGISTRY)" \
		--scan-glob 'test-rvv/recognition/occlusion_reasoning/log/board/**/summary.md' \
		--scan-glob 'test-rvv/recognition/occlusion_reasoning/log/board/**/evidence_manifest.json' \
		--scan-glob 'test-rvv/recognition/occlusion_reasoning/log/board/**/evidence_doctor.md' \
		--scan-glob 'test-rvv/recognition/occlusion_reasoning/log/board/**/evidence_doctor.json' \
		$(EVIDENCE_DOC_ARGS) \
		--require-doc-ref \
		--fail-on never

.PHONY: run_qemu_smoke check_occlusion_filter_rvv_asm check_inline_filter_rvv_asm deploy_inline_bench_rvv deploy_inline_bench_std deploy_inline_board run_inline_board_bench_compare fetch_inline_board_logs inline_board_repeated evidence_manifest_repeated evidence_doctor_repeated inline_evidence_manifest_repeated inline_evidence_doctor_repeated record_evidence_state_repeated record_inline_evidence_state_repeated check_evidence_freshness
