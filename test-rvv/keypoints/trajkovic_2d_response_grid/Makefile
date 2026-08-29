# =============================================================================
# PCL keypoints/trajkovic_2d_response_grid RVV topic Makefile
#
# QEMU 只用于 correctness（正确性）和 RVV 指令路径检查；性能结论必须来自
# board（板卡）或目标硬件 repeated benchmark（重复性能测试）。
# =============================================================================

TOPIC  := trajkovic_2d_response_grid
MODULE := keypoints

FOCUS_DIR      := pcl/keypoints/impl/trajkovic_2d.hpp
FILTER_REPORT  := log/filtered_trajkovic_2d_response_grid.log
ANALYZE_REPORT := log/analyze_trajkovic_2d_response_grid.log

TARGET_TEST     := test_trajkovic_2d_response_grid
TARGET_TEST_STD := test_trajkovic_2d_response_grid_std
TARGET_TEST_RVV := test_trajkovic_2d_response_grid_rvv
BOARD_TARGET_TEST := test_trajkovic_2d_response_grid_rvv
SRCS_TEST       := src/test_trajkovic_2d_response_grid.cpp

TARGET_BENCH     := bench_trajkovic_2d_response_grid
TARGET_BENCH_STD := bench_trajkovic_2d_response_grid_std
TARGET_BENCH_RVV := bench_trajkovic_2d_response_grid_rvv
SRCS_BENCH       := src/bench_trajkovic_2d_response_grid.cpp

T2D_BOARD_MANIFEST_JSON := log/board/evidence_manifest.json
T2D_BOARD_DOCTOR_MD := log/board/evidence_doctor.md
T2D_EVIDENCE_REGISTRY := log/evidence_registry.json
T2D_BOARD_REPEATED_SUMMARY := log/board/repeated-summary.md
T2D_BOARD_REPEATED_RUNS ?= 5
T2D_BOARD_REPEATED_LABEL ?= trajkovic_2d_phase000_response_grid_production_probe
T2D_BOARD_REPEATED_RUN_DIRS := \
	log/board/repeated_001 \
	log/board/repeated_002 \
	log/board/repeated_003 \
	log/board/repeated_004 \
	log/board/repeated_005

INCLUDES = \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/keypoints/include \
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

LIBS_TEST  := -lpcl_common -llz4 -lgtest -lgtest_main -lpthread -lm
LIBS_BENCH := -lpcl_common -llz4 -lm

include ../../mk/rvv-topic.mk

dump_test_rvv: | $(ASM_DIR)/$(ARCH)
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_TEST=$(TARGET_TEST_RVV) $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_RVV)
	$(OBJDUMP) -d -C $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_RVV) > $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).full.asm
	grep -E $(VEC_REGEX_STR) $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).full.asm > $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm || true
	@echo "[DONE] RVV test asm dump: $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm"

check_production_rvv_asm: dump_test_rvv
	@grep -Eq 'TrajkovicKeypoint2D<.*PointXYZI.*>::detectKeypoints' $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).full.asm
	@grep -Eq 'vlse32[.]v' $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm
	@grep -Eq 'vfmul[.]vv' $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm
	@grep -Eq 'vfmin[.]vv' $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm
	@grep -Eq 'vfdiv[.]vv' $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm
	@grep -Eq 'vmfge[.]vf' $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm
	@grep -Eq 'vse32[.]v' $(ASM_DIR)/$(ARCH)/$(TARGET_TEST_RVV).asm
	@echo "[PASS] Trajkovic 2D production RVV asm gate matched inlined EIGHT_CORNERS response-grid instructions."

collect_board_trajkovic_2d_repeated: deploy_board
	@rm -rf $(T2D_BOARD_REPEATED_RUN_DIRS)
	@set -e; for run in $$(seq 1 $(T2D_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%03d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(T2D_BOARD_REPEATED_LABEL)/repeated_$$run_id"; \
		local_dir="log/board/repeated_$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] trajkovic_2d repeated run $$run_id/$(T2D_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(BENCH_ARGS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest:
	@$(PYTHON_RUN) script/generate_trajkovic_2d_evidence_manifest.py \
		$(foreach run_dir,$(T2D_BOARD_REPEATED_RUN_DIRS),--run-dir $(run_dir)) \
		--summary $(T2D_BOARD_REPEATED_SUMMARY) \
		--output $(T2D_BOARD_MANIFEST_JSON)

run_board_evidence_doctor: generate_board_evidence_manifest
	@$(PYTHON_RUN) ../../script/evidence_doctor.py \
		--manifest $(T2D_BOARD_MANIFEST_JSON) \
		--output $(T2D_BOARD_DOCTOR_MD) \
		--fail-on never

record_board_evidence_state: run_board_evidence_doctor
	@$(PYTHON_RUN) ../../script/evidence_registry.py record \
		--registry $(T2D_EVIDENCE_REGISTRY) \
		--summary $(T2D_BOARD_REPEATED_SUMMARY) \
		--manifest $(T2D_BOARD_MANIFEST_JSON) \
		--doctor $(T2D_BOARD_DOCTOR_MD) \
		--target collect_board_trajkovic_2d_repeated \
		--backend board \
		--run-label board-trajkovic-2d-response-grid-phase000 \
		--case-filter four_corners_320x240,eight_corners_320x240,four_corners_641x481_tail,eight_corners_641x481_tail \
		--evidence-role production-public \
		--doc-ref doc/phases/000-current-state-and-response-grid-production-probe/result.zh.md \
		--doc-ref doc/trajkovic_2d_response_grid-evaluation.zh.md \
		--doc-ref ../../../doc-rvv/keypoints/trajkovic_2d_response_grid-RVV.zh.md \
		--freshness-state fresh

run_board_trajkovic_2d_repeated: collect_board_trajkovic_2d_repeated record_board_evidence_state

evidence_status:
	@$(PYTHON_RUN) ../../script/evidence_registry.py check \
		--registry $(T2D_EVIDENCE_REGISTRY) \
		--scan-glob 'test-rvv/keypoints/trajkovic_2d_response_grid/log/board/repeated-summary.md' \
		--scan-glob 'test-rvv/keypoints/trajkovic_2d_response_grid/log/board/evidence_manifest.json' \
		--scan-glob 'test-rvv/keypoints/trajkovic_2d_response_grid/log/board/evidence_doctor.md' \
		--doc test-rvv/keypoints/trajkovic_2d_response_grid/doc/phases/000-current-state-and-response-grid-production-probe/result.zh.md \
		--doc test-rvv/keypoints/trajkovic_2d_response_grid/doc/trajkovic_2d_response_grid-evaluation.zh.md \
		--doc doc-rvv/keypoints/trajkovic_2d_response_grid-RVV.zh.md \
		--fail-on never

.PHONY: dump_test_rvv check_production_rvv_asm collect_board_trajkovic_2d_repeated generate_board_evidence_manifest run_board_evidence_doctor record_board_evidence_state run_board_trajkovic_2d_repeated evidence_status
