# =============================================================================
#  PCL features/integral_image_normal RVV diagnostic Makefile
#
#  本 topic 先评估 IntegralImageNormalEstimation::computeFeature() 里的
#  depth-change map 和 distance-map initialization。QEMU 只用于
#  correctness（正确性）和日志形状；性能结论必须来自板卡或目标硬件。
# =============================================================================

TOPIC  := integral_image_normal
MODULE := features

FOCUS_DIR      := pcl/features/include/pcl/features/impl/integral_image_normal.hpp
FILTER_REPORT  := log/filtered_integral_image_normal.log
ANALYZE_REPORT := log/analyze_integral_image_normal.log

TARGET_TEST     := test_integral_image_normal
TARGET_TEST_STD := test_integral_image_normal_std
TARGET_TEST_RVV := test_integral_image_normal_rvv
SRCS_TEST       := src/test_integral_image_normal.cpp

TARGET_BENCH     := bench_integral_image_normal
TARGET_BENCH_STD := bench_integral_image_normal_std
TARGET_BENCH_RVV := bench_integral_image_normal_rvv
SRCS_BENCH       := src/bench_integral_image_normal.cpp

IIN_BOARD_MANIFEST_JSON := log/board/evidence_manifest.json
IIN_BOARD_DOCTOR_MD := log/board/evidence_doctor.md
IIN_EVIDENCE_REGISTRY := log/evidence_registry.json
IIN_BOARD_REPEATED_SUMMARY := log/board/repeated-summary.md
IIN_BOARD_REPEATED_RUNS ?= 5
IIN_BOARD_REPEATED_LABEL ?= integral_image_normal_phase050_map_prep_production_probe
IIN_BOARD_REPEATED_RUN_DIRS := \
	log/board/repeated_001 \
	log/board/repeated_002 \
	log/board/repeated_003 \
	log/board/repeated_004 \
	log/board/repeated_005

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
LIBS_BENCH := -lpcl_features -lpcl_search -lpcl_kdtree -lpcl_common -lflann_cpp -llz4 -lm

include ../../mk/rvv-topic.mk

collect_board_integral_image_normal_repeated: deploy_board
	@rm -rf $(IIN_BOARD_REPEATED_RUN_DIRS)
	@set -e; for run in $$(seq 1 $(IIN_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%03d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(IIN_BOARD_REPEATED_LABEL)/repeated_$$run_id"; \
		local_dir="log/board/repeated_$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] integral_image_normal diagnostic repeated run $$run_id/$(IIN_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(BENCH_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest:
	@$(PYTHON_RUN) script/generate_integral_image_normal_evidence_manifest.py \
		$(foreach run_dir,$(IIN_BOARD_REPEATED_RUN_DIRS),--run-dir $(run_dir)) \
		--summary $(IIN_BOARD_REPEATED_SUMMARY) \
		--output $(IIN_BOARD_MANIFEST_JSON)

run_board_evidence_doctor: generate_board_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(IIN_BOARD_MANIFEST_JSON) \
		--output $(IIN_BOARD_DOCTOR_MD) \
		--fail-on never

record_board_evidence_state: run_board_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(IIN_EVIDENCE_REGISTRY) \
		--summary $(IIN_BOARD_REPEATED_SUMMARY) \
		--manifest $(IIN_BOARD_MANIFEST_JSON) \
		--doctor $(IIN_BOARD_DOCTOR_MD) \
		--target run_board_bench_compare \
		--backend board \
		--run-label board-integral-image-normal-production-repeated-phase050 \
		--case-filter map_prep,avg3d_diff,avg3d_profile,profile_component,pcl_iin,pcl_avg3d_profile,prod_compute \
		--evidence-role mixed_boundary_cross_check \
		--doc-ref doc/phases/000-current-state-and-map-prep-diagnostic/result.zh.md \
		--doc-ref doc/phases/020-average-3d-gradient-diff-buffer-diagnostic/result.zh.md \
		--doc-ref doc/phases/030-average-3d-gradient-production-shaped-profile/result.zh.md \
		--doc-ref doc/phases/040-pcl-integral-image2d-boundary-profile/result.zh.md \
		--doc-ref doc/phases/050-map-prep-production-probe/result.zh.md \
		--doc-ref doc/integral_image_normal-evaluation.zh.md \
		--doc-ref doc/benchmark-and-evidence.zh.md \
		--freshness-state fresh

run_board_integral_image_normal_repeated: collect_board_integral_image_normal_repeated record_board_evidence_state

evidence_status:
	@$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry $(IIN_EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/features/integral_image_normal/log/board/repeated-summary.md' \
		--scan-glob 'test-rvv/features/integral_image_normal/log/board/evidence_manifest.json' \
		--scan-glob 'test-rvv/features/integral_image_normal/log/board/evidence_doctor.md' \
		--doc test-rvv/features/integral_image_normal/doc/phases/000-current-state-and-map-prep-diagnostic/result.zh.md \
		--doc test-rvv/features/integral_image_normal/doc/phases/020-average-3d-gradient-diff-buffer-diagnostic/result.zh.md \
		--doc test-rvv/features/integral_image_normal/doc/phases/030-average-3d-gradient-production-shaped-profile/result.zh.md \
		--doc test-rvv/features/integral_image_normal/doc/phases/040-pcl-integral-image2d-boundary-profile/result.zh.md \
		--doc test-rvv/features/integral_image_normal/doc/phases/050-map-prep-production-probe/result.zh.md \
		--doc test-rvv/features/integral_image_normal/doc/integral_image_normal-evaluation.zh.md \
		--doc test-rvv/features/integral_image_normal/doc/benchmark-and-evidence.zh.md \
		--fail-on never

dump_test_rvv: | $(ASM_DIR)/$(ARCH)
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_TEST=$(TARGET_TEST_RVV) $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_RVV)
	$(OBJDUMP) -d -C $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_RVV) > $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).full.asm
	grep -E $(VEC_REGEX_STR) $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).full.asm > $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm || true
	@echo "[DONE] RVV test asm dump: $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm"

.PHONY: collect_board_integral_image_normal_repeated generate_board_evidence_manifest run_board_evidence_doctor record_board_evidence_state run_board_integral_image_normal_repeated evidence_status dump_test_rvv
