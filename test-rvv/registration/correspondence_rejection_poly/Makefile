# =============================================================================
#  PCL registration/correspondence_rejection_poly RVV diagnostic Makefile
# =============================================================================

TOPIC  := correspondence_rejection_poly
MODULE := registration

FOCUS_DIR      := registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp
FILTER_REPORT  := log/filtered_correspondence_rejection_poly.log
ANALYZE_REPORT := log/analyze_correspondence_rejection_poly.log

TARGET_BENCH     := bench_correspondence_rejection_poly
TARGET_BENCH_STD := bench_correspondence_rejection_poly_std
TARGET_BENCH_RVV := bench_correspondence_rejection_poly_rvv
SRCS_BENCH       := src/bench_correspondence_rejection_poly.cpp

TARGET_TEST     := test_correspondence_rejection_poly
TARGET_TEST_STD := test_correspondence_rejection_poly_std
TARGET_TEST_RVV := test_correspondence_rejection_poly_rvv
SRCS_TEST       := src/test_correspondence_rejection_poly.cpp

INCLUDES ?= \
	-I$(CURDIR)/include \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/registration/include \
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

LIBS_CORE := \
	-lpcl_registration \
	-lpcl_common \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lm
LIBS_BENCH := $(LIBS_CORE)
LIBS_TEST  := $(LIBS_CORE) -lgtest -lgtest_main -lpthread

CRPOLY_BENCH_WARMUP_ITERATIONS ?= 3
CRPOLY_BENCH_ITERATIONS ?= 20
CRPOLY_REJECTION_ITERATIONS ?= 512
CRPOLY_BOARD_REPEATED_RUNS ?= 5
CRPOLY_BOARD_BENCH_WARMUP_ITERATIONS ?= 5
CRPOLY_BOARD_BENCH_ITERATIONS ?= 50
CRPOLY_BOARD_DEVICE_LABEL ?= RISC-V board / target hardware
CRPOLY_BOARD_VLEN_DESC ?= see SoC / ELF (board)
CRPOLY_ENABLE_PRODUCTION_DIRECT_PROBE ?= 0

QEMU_EDGE_EVIDENCE_DIR = $(OUTPUT_DIR_QEMU)/edge_batch
QEMU_EDGE_GATHER_EVIDENCE_DIR = $(OUTPUT_DIR_QEMU)/edge_gather_staging
QEMU_ACCEPTANCE_EVIDENCE_DIR = $(OUTPUT_DIR_QEMU)/acceptance_filter
QEMU_PRODUCTION_DIRECT_EVIDENCE_DIR = $(OUTPUT_DIR_QEMU)/production_direct
BOARD_TEST_SMOKE_DIR = $(OUTPUT_DIR_BOARD)/test_smoke
BOARD_EDGE_REPEATED_DIR = $(OUTPUT_DIR_BOARD)/edge_batch_repeated
BOARD_EDGE_GATHER_REPEATED_DIR = $(OUTPUT_DIR_BOARD)/edge_gather_staging_repeated
BOARD_ACCEPTANCE_REPEATED_DIR = $(OUTPUT_DIR_BOARD)/acceptance_filter_repeated
BOARD_PRODUCTION_DIRECT_REPEATED_DIR = $(OUTPUT_DIR_BOARD)/production_direct_repeated
BOARD_EDGE_RUN_LABEL ?= edge_batch_repeated
BOARD_EDGE_GATHER_RUN_LABEL ?= edge_gather_staging_repeated
BOARD_ACCEPTANCE_RUN_LABEL ?= acceptance_filter_repeated
BOARD_PRODUCTION_DIRECT_RUN_LABEL ?= production_direct_repeated

include ../../mk/rvv-topic.mk

run_bench_edge_batch_smoke: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_compare BENCH_ARGS="--case-filter edge-batch --iterations $(CRPOLY_BENCH_ITERATIONS) --warmup-iterations $(CRPOLY_BENCH_WARMUP_ITERATIONS)" BENCH_STD_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_edge_batch_std.log BENCH_RVV_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_edge_batch_rvv.log BENCH_COMPARE_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/analyze_bench_compare_edge_batch.log

run_bench_edge_gather_staging_smoke: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_compare BENCH_ARGS="--case-filter edge-gather-staging --iterations $(CRPOLY_BENCH_ITERATIONS) --warmup-iterations $(CRPOLY_BENCH_WARMUP_ITERATIONS)" BENCH_STD_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_edge_gather_staging_std.log BENCH_RVV_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_edge_gather_staging_rvv.log BENCH_COMPARE_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/analyze_bench_compare_edge_gather_staging.log

run_bench_acceptance_smoke: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_compare BENCH_ARGS="--case-filter acceptance-filter --iterations $(CRPOLY_BENCH_ITERATIONS) --warmup-iterations $(CRPOLY_BENCH_WARMUP_ITERATIONS)" BENCH_STD_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_acceptance_std.log BENCH_RVV_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_acceptance_rvv.log BENCH_COMPARE_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/analyze_bench_compare_acceptance.log

run_bench_full_rejection_smoke: | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_compare BENCH_ARGS="--case-filter full-entry --iterations $(CRPOLY_BENCH_ITERATIONS) --warmup-iterations $(CRPOLY_BENCH_WARMUP_ITERATIONS) --rejection-iterations $(CRPOLY_REJECTION_ITERATIONS)" BENCH_STD_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_full_entry_std.log BENCH_RVV_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_full_entry_rvv.log BENCH_COMPARE_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/analyze_bench_compare_full_entry.log

require_production_direct_probe:
	@if [ "$(CRPOLY_ENABLE_PRODUCTION_DIRECT_PROBE)" != "1" ]; then \
		echo "[SKIP] production-direct is a historical probe target after rollback."; \
		echo "[SKIP] CRPOLY_ENABLE_PRODUCTION_DIRECT_PROBE only unlocks this Makefile target; it does not apply or enable a production RVV patch."; \
		echo "[SKIP] Set it only while a temporary production dispatch candidate is present in the production headers."; \
		exit 2; \
	fi

run_bench_production_direct_smoke: require_production_direct_probe | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench_compare BENCH_ARGS="--case-filter production-direct --iterations $(CRPOLY_BENCH_ITERATIONS) --warmup-iterations $(CRPOLY_BENCH_WARMUP_ITERATIONS) --rejection-iterations $(CRPOLY_REJECTION_ITERATIONS)" BENCH_STD_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_production_direct_std.log BENCH_RVV_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/run_bench_production_direct_rvv.log BENCH_COMPARE_OUTPUT_FILE=$(OUTPUT_DIR_QEMU)/analyze_bench_compare_production_direct.log

run_bench_qemu_smoke: run_bench_edge_batch_smoke run_bench_edge_gather_staging_smoke run_bench_acceptance_smoke

$(QEMU_EDGE_EVIDENCE_DIR) $(QEMU_EDGE_GATHER_EVIDENCE_DIR) $(QEMU_ACCEPTANCE_EVIDENCE_DIR) $(QEMU_PRODUCTION_DIRECT_EVIDENCE_DIR):
	mkdir -p $@

generate_qemu_evidence_manifest_edge_batch: | $(QEMU_EDGE_EVIDENCE_DIR)
	$(PYTHON) script/generate_crpoly_evidence_manifest.py --std-log $(OUTPUT_DIR_QEMU)/run_bench_edge_batch_std.log --rvv-log $(OUTPUT_DIR_QEMU)/run_bench_edge_batch_rvv.log --summary-md $(OUTPUT_DIR_QEMU)/analyze_bench_compare_edge_batch.log --asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm --output $(QEMU_EDGE_EVIDENCE_DIR)/evidence_manifest.json

run_evidence_doctor_edge_batch_qemu: generate_qemu_evidence_manifest_edge_batch
	$(PYTHON) ../../script/evidence_doctor.py --manifest $(QEMU_EDGE_EVIDENCE_DIR)/evidence_manifest.json --output $(QEMU_EDGE_EVIDENCE_DIR)/evidence_doctor.md --fail-on never

generate_qemu_evidence_manifest_edge_gather_staging: | $(QEMU_EDGE_GATHER_EVIDENCE_DIR)
	$(PYTHON) script/generate_crpoly_evidence_manifest.py --std-log $(OUTPUT_DIR_QEMU)/run_bench_edge_gather_staging_std.log --rvv-log $(OUTPUT_DIR_QEMU)/run_bench_edge_gather_staging_rvv.log --summary-md $(OUTPUT_DIR_QEMU)/analyze_bench_compare_edge_gather_staging.log --asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm --output $(QEMU_EDGE_GATHER_EVIDENCE_DIR)/evidence_manifest.json

run_evidence_doctor_edge_gather_staging_qemu: generate_qemu_evidence_manifest_edge_gather_staging
	$(PYTHON) ../../script/evidence_doctor.py --manifest $(QEMU_EDGE_GATHER_EVIDENCE_DIR)/evidence_manifest.json --output $(QEMU_EDGE_GATHER_EVIDENCE_DIR)/evidence_doctor.md --fail-on never

generate_qemu_evidence_manifest_acceptance: | $(QEMU_ACCEPTANCE_EVIDENCE_DIR)
	$(PYTHON) script/generate_crpoly_evidence_manifest.py --std-log $(OUTPUT_DIR_QEMU)/run_bench_acceptance_std.log --rvv-log $(OUTPUT_DIR_QEMU)/run_bench_acceptance_rvv.log --summary-md $(OUTPUT_DIR_QEMU)/analyze_bench_compare_acceptance.log --asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm --output $(QEMU_ACCEPTANCE_EVIDENCE_DIR)/evidence_manifest.json

run_evidence_doctor_acceptance_qemu: generate_qemu_evidence_manifest_acceptance
	$(PYTHON) ../../script/evidence_doctor.py --manifest $(QEMU_ACCEPTANCE_EVIDENCE_DIR)/evidence_manifest.json --output $(QEMU_ACCEPTANCE_EVIDENCE_DIR)/evidence_doctor.md --fail-on never

generate_qemu_evidence_manifest_production_direct: | $(QEMU_PRODUCTION_DIRECT_EVIDENCE_DIR)
	$(PYTHON) script/generate_crpoly_evidence_manifest.py --std-log $(OUTPUT_DIR_QEMU)/run_bench_production_direct_std.log --rvv-log $(OUTPUT_DIR_QEMU)/run_bench_production_direct_rvv.log --summary-md $(OUTPUT_DIR_QEMU)/analyze_bench_compare_production_direct.log --asm $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm --output $(QEMU_PRODUCTION_DIRECT_EVIDENCE_DIR)/evidence_manifest.json

run_evidence_doctor_production_direct_qemu: generate_qemu_evidence_manifest_production_direct
	$(PYTHON) ../../script/evidence_doctor.py --manifest $(QEMU_PRODUCTION_DIRECT_EVIDENCE_DIR)/evidence_manifest.json --output $(QEMU_PRODUCTION_DIRECT_EVIDENCE_DIR)/evidence_doctor.md --fail-on never

generate_qemu_evidence_manifest: generate_qemu_evidence_manifest_edge_batch generate_qemu_evidence_manifest_edge_gather_staging generate_qemu_evidence_manifest_acceptance generate_qemu_evidence_manifest_production_direct
run_evidence_doctor_qemu: run_evidence_doctor_edge_batch_qemu run_evidence_doctor_edge_gather_staging_qemu run_evidence_doctor_acceptance_qemu run_evidence_doctor_production_direct_qemu

$(BOARD_TEST_SMOKE_DIR) $(BOARD_EDGE_REPEATED_DIR) $(BOARD_EDGE_GATHER_REPEATED_DIR) $(BOARD_ACCEPTANCE_REPEATED_DIR) $(BOARD_PRODUCTION_DIRECT_REPEATED_DIR):
	mkdir -p $@

run_board_test_smoke: deploy_board | $(BOARD_TEST_SMOKE_DIR)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && make run_test REMOTE_OUTPUT_DIR='$(REMOTE_DIR)/log/board/test_smoke' REMOTE_TEST_ARGS='$(REMOTE_TEST_ARGS)'"
	@$(MAKE) -C $(CURDIR) fetch_board_logs OUTPUT_DIR_BOARD=$(BOARD_TEST_SMOKE_DIR) REMOTE_BOARD_OUTPUT_DIR='$(REMOTE_DIR)/log/board/test_smoke'

collect_board_bench_edge_batch_repeated: deploy_board | $(BOARD_EDGE_REPEATED_DIR)
	@for run in $$(seq 1 $(CRPOLY_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(BOARD_EDGE_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(BOARD_EDGE_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] edge_batch repeated run $$run_id/$(CRPOLY_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && make run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter edge-batch --iterations $(CRPOLY_BOARD_BENCH_ITERATIONS) --warmup-iterations $(CRPOLY_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_edge_batch:
	$(PYTHON) script/summarize_crpoly_board_repeated.py --input-dir $(BOARD_EDGE_REPEATED_DIR) --output-dir $(BOARD_EDGE_REPEATED_DIR) --run-label $(BOARD_EDGE_RUN_LABEL) --case-filter edge-batch --iterations $(CRPOLY_BOARD_BENCH_ITERATIONS) --warmup-iterations $(CRPOLY_BOARD_BENCH_WARMUP_ITERATIONS) --expected-runs $(CRPOLY_BOARD_REPEATED_RUNS) --std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) --rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) --device-label "$(CRPOLY_BOARD_DEVICE_LABEL)" --vlen-desc "$(CRPOLY_BOARD_VLEN_DESC)"

run_board_evidence_doctor_edge_batch: generate_board_evidence_manifest_edge_batch
	$(PYTHON) ../../script/evidence_doctor.py --manifest $(BOARD_EDGE_REPEATED_DIR)/evidence_manifest.json --output $(BOARD_EDGE_REPEATED_DIR)/evidence_doctor.md --fail-on never

run_board_bench_edge_batch_repeated: collect_board_bench_edge_batch_repeated run_board_evidence_doctor_edge_batch

collect_board_bench_edge_gather_staging_repeated: deploy_board | $(BOARD_EDGE_GATHER_REPEATED_DIR)
	@for run in $$(seq 1 $(CRPOLY_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(BOARD_EDGE_GATHER_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(BOARD_EDGE_GATHER_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] edge_gather_staging repeated run $$run_id/$(CRPOLY_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && make run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter edge-gather-staging --iterations $(CRPOLY_BOARD_BENCH_ITERATIONS) --warmup-iterations $(CRPOLY_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_edge_gather_staging:
	$(PYTHON) script/summarize_crpoly_board_repeated.py --input-dir $(BOARD_EDGE_GATHER_REPEATED_DIR) --output-dir $(BOARD_EDGE_GATHER_REPEATED_DIR) --run-label $(BOARD_EDGE_GATHER_RUN_LABEL) --case-filter edge-gather-staging --iterations $(CRPOLY_BOARD_BENCH_ITERATIONS) --warmup-iterations $(CRPOLY_BOARD_BENCH_WARMUP_ITERATIONS) --expected-runs $(CRPOLY_BOARD_REPEATED_RUNS) --std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) --rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) --device-label "$(CRPOLY_BOARD_DEVICE_LABEL)" --vlen-desc "$(CRPOLY_BOARD_VLEN_DESC)"

run_board_evidence_doctor_edge_gather_staging: generate_board_evidence_manifest_edge_gather_staging
	$(PYTHON) ../../script/evidence_doctor.py --manifest $(BOARD_EDGE_GATHER_REPEATED_DIR)/evidence_manifest.json --output $(BOARD_EDGE_GATHER_REPEATED_DIR)/evidence_doctor.md --fail-on never

run_board_bench_edge_gather_staging_repeated: collect_board_bench_edge_gather_staging_repeated run_board_evidence_doctor_edge_gather_staging

collect_board_bench_acceptance_repeated: deploy_board | $(BOARD_ACCEPTANCE_REPEATED_DIR)
	@for run in $$(seq 1 $(CRPOLY_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(BOARD_ACCEPTANCE_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(BOARD_ACCEPTANCE_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] acceptance_filter repeated run $$run_id/$(CRPOLY_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && make run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter acceptance-filter --iterations $(CRPOLY_BOARD_BENCH_ITERATIONS) --warmup-iterations $(CRPOLY_BOARD_BENCH_WARMUP_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_acceptance:
	$(PYTHON) script/summarize_crpoly_board_repeated.py --input-dir $(BOARD_ACCEPTANCE_REPEATED_DIR) --output-dir $(BOARD_ACCEPTANCE_REPEATED_DIR) --run-label $(BOARD_ACCEPTANCE_RUN_LABEL) --case-filter acceptance-filter --iterations $(CRPOLY_BOARD_BENCH_ITERATIONS) --warmup-iterations $(CRPOLY_BOARD_BENCH_WARMUP_ITERATIONS) --expected-runs $(CRPOLY_BOARD_REPEATED_RUNS) --std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) --rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) --device-label "$(CRPOLY_BOARD_DEVICE_LABEL)" --vlen-desc "$(CRPOLY_BOARD_VLEN_DESC)"

run_board_evidence_doctor_acceptance: generate_board_evidence_manifest_acceptance
	$(PYTHON) ../../script/evidence_doctor.py --manifest $(BOARD_ACCEPTANCE_REPEATED_DIR)/evidence_manifest.json --output $(BOARD_ACCEPTANCE_REPEATED_DIR)/evidence_doctor.md --fail-on never

run_board_bench_acceptance_repeated: collect_board_bench_acceptance_repeated run_board_evidence_doctor_acceptance

collect_board_bench_production_direct_repeated: require_production_direct_probe deploy_board | $(BOARD_PRODUCTION_DIRECT_REPEATED_DIR)
	@for run in $$(seq 1 $(CRPOLY_BOARD_REPEATED_RUNS)); do \
		run_id=$$(printf "%02d" "$$run"); \
		remote_dir="$(REMOTE_DIR)/log/board/$(BOARD_PRODUCTION_DIRECT_RUN_LABEL)/run-$$run_id"; \
		local_dir="$(BOARD_PRODUCTION_DIRECT_REPEATED_DIR)/run-$$run_id"; \
		mkdir -p "$$local_dir"; \
		echo "[BOARD] production_direct repeated run $$run_id/$(CRPOLY_BOARD_REPEATED_RUNS)"; \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && make run_bench_compare REMOTE_OUTPUT_DIR='$$remote_dir' BENCH_COMPARE_SAVE='$$remote_dir/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='--case-filter production-direct --iterations $(CRPOLY_BOARD_BENCH_ITERATIONS) --warmup-iterations $(CRPOLY_BOARD_BENCH_WARMUP_ITERATIONS) --rejection-iterations $(CRPOLY_REJECTION_ITERATIONS)'"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$$remote_dir/ "$$local_dir/"; \
	done

generate_board_evidence_manifest_production_direct:
	$(PYTHON) script/summarize_crpoly_board_repeated.py --input-dir $(BOARD_PRODUCTION_DIRECT_REPEATED_DIR) --output-dir $(BOARD_PRODUCTION_DIRECT_REPEATED_DIR) --run-label $(BOARD_PRODUCTION_DIRECT_RUN_LABEL) --case-filter production-direct --iterations $(CRPOLY_BOARD_BENCH_ITERATIONS) --warmup-iterations $(CRPOLY_BOARD_BENCH_WARMUP_ITERATIONS) --expected-runs $(CRPOLY_BOARD_REPEATED_RUNS) --std-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) --rvv-bin $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) --device-label "$(CRPOLY_BOARD_DEVICE_LABEL)" --vlen-desc "$(CRPOLY_BOARD_VLEN_DESC)"

run_board_evidence_doctor_production_direct: generate_board_evidence_manifest_production_direct
	$(PYTHON) ../../script/evidence_doctor.py --manifest $(BOARD_PRODUCTION_DIRECT_REPEATED_DIR)/evidence_manifest.json --output $(BOARD_PRODUCTION_DIRECT_REPEATED_DIR)/evidence_doctor.md --fail-on never

run_board_bench_production_direct_repeated: collect_board_bench_production_direct_repeated run_board_evidence_doctor_production_direct

run_board_bench_repeated: run_board_bench_edge_batch_repeated run_board_bench_edge_gather_staging_repeated run_board_bench_acceptance_repeated

.PHONY: run_bench_edge_batch_smoke run_bench_edge_gather_staging_smoke run_bench_acceptance_smoke run_bench_full_rejection_smoke require_production_direct_probe run_bench_production_direct_smoke run_bench_qemu_smoke generate_qemu_evidence_manifest_edge_batch run_evidence_doctor_edge_batch_qemu generate_qemu_evidence_manifest_edge_gather_staging run_evidence_doctor_edge_gather_staging_qemu generate_qemu_evidence_manifest_acceptance run_evidence_doctor_acceptance_qemu generate_qemu_evidence_manifest_production_direct run_evidence_doctor_production_direct_qemu generate_qemu_evidence_manifest run_evidence_doctor_qemu run_board_test_smoke collect_board_bench_edge_batch_repeated generate_board_evidence_manifest_edge_batch run_board_evidence_doctor_edge_batch run_board_bench_edge_batch_repeated collect_board_bench_edge_gather_staging_repeated generate_board_evidence_manifest_edge_gather_staging run_board_evidence_doctor_edge_gather_staging run_board_bench_edge_gather_staging_repeated collect_board_bench_acceptance_repeated generate_board_evidence_manifest_acceptance run_board_evidence_doctor_acceptance run_board_bench_acceptance_repeated collect_board_bench_production_direct_repeated generate_board_evidence_manifest_production_direct run_board_evidence_doctor_production_direct run_board_bench_production_direct_repeated run_board_bench_repeated
