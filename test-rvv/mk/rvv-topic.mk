# =============================================================================
# Common PCL RVV test/bench Makefile fragment.
#
# Topic Makefiles define TOPIC, MODULE, source files, optional upstream test args
# and topic-specific libraries before including this file.
# =============================================================================

TOPIC ?= unknown
MODULE ?= unknown

LOG_DIR           ?= log
TIMESTAMP        := $(shell date +%Y-%m-%d_%H-%M-%S)
VEC_LOGS_DIR      ?= $(LOG_DIR)/vec_logs
LOG_VEC_MISS_DIR  ?= $(LOG_DIR)/vec_missed_log
LOG_FILE          ?= $(LOG_VEC_MISS_DIR)/vec_missed_$(TIMESTAMP).log
LATEST_VEC_LOG    ?= $(LOG_DIR)/latest_vec_missed.log
FOCUS_DIR         ?=
FILTER_REPORT     ?= $(LOG_DIR)/filtered_$(TOPIC).log
ANALYZE_REPORT    ?= $(LOG_DIR)/analyze_$(TOPIC).log
ENABLE_VEC_MISSED ?= 0
VEC_MISSED_CXXFLAGS = $(if $(filter 1 yes true,$(ENABLE_VEC_MISSED)),-fopt-info-vec-missed=$(LOG_FILE),)
VEC_MISSED_PREREQ = $(if $(filter 1 yes true,$(ENABLE_VEC_MISSED)),$(LOG_VEC_MISS_DIR),)

OUTPUT_DIR        ?= $(LOG_DIR)
OUTPUT_DIR_BOARD  ?= $(OUTPUT_DIR)/board
OUTPUT_DIR_QEMU   ?= $(OUTPUT_DIR)/qemu

BUILD_DIR         ?= build
ASM_DIR           ?= $(BUILD_DIR)/asm

include $(abspath $(dir $(lastword $(MAKEFILE_LIST))))/rvv-env.mk

BENCH_STD_OUTPUT_FILE     ?= $(OUTPUT_DIR_QEMU)/run_bench_std.log
BENCH_RVV_OUTPUT_FILE     ?= $(OUTPUT_DIR_QEMU)/run_bench_rvv.log
BENCH_COMPARE_OUTPUT_FILE ?= $(OUTPUT_DIR_QEMU)/analyze_bench_compare.log
TEST_STD_OUTPUT_FILE      ?= $(OUTPUT_DIR_QEMU)/run_test_std.log
TEST_RVV_OUTPUT_FILE      ?= $(OUTPUT_DIR_QEMU)/run_test_rvv.log
TEST_OUTPUT_FILE          ?= $(OUTPUT_DIR_QEMU)/run_test.log
UPSTREAM_TEST_OUTPUT_FILE ?= $(OUTPUT_DIR_QEMU)/run_upstream_test.log
UPSTREAM_TEST_STD_OUTPUT_FILE ?= $(OUTPUT_DIR_QEMU)/run_upstream_test_std.log
UPSTREAM_TEST_RVV_OUTPUT_FILE ?= $(OUTPUT_DIR_QEMU)/run_upstream_test_rvv.log

ANALYZE_VEC_SCRIPT    ?= $(TEST_RVV_SHARED_SCRIPT_DIR)/analyze_vec_log.py
BENCH_COMPARE_SCRIPT  ?= $(TEST_RVV_SHARED_SCRIPT_DIR)/analyze_bench_compare.py
SANITIZE_LOGS_SCRIPT  ?= $(TEST_RVV_SHARED_SCRIPT_DIR)/sanitize_evidence_logs.py
ALLOW_QEMU_BENCH_COMPARE ?= 0

# Optional data files to deploy beside the board binaries. Keep the list in the
# topic Makefile so inputs such as PCD fixtures remain topic-owned.
DEPLOY_EXTRA_FILES ?=
DEPLOY_EXTRA_REMOTE_DIR ?= $(REMOTE_DIR)

VENV_ACTIVATE ?= $(abspath $(CURDIR)/.venv/bin/activate)
PYTHON ?= python3
PYTHON_RUN = bash -lc 'if [ -f "$(VENV_ACTIVATE)" ]; then . "$(VENV_ACTIVATE)"; fi; $(PYTHON) "$$@"' --

TARGET_BENCH       ?= bench_$(TOPIC)
TARGET_BENCH_STD   ?= $(TARGET_BENCH)_std
TARGET_BENCH_RVV   ?= $(TARGET_BENCH)_rvv
# Test-only topics leave SRCS_BENCH empty; deploy_board then skips bench bins.
HAS_BENCH          ?= $(if $(strip $(SRCS_BENCH)),1,0)
TARGET_TEST        ?= test_$(TOPIC)
TARGET_TEST_STD    ?= $(TARGET_TEST)_std
TARGET_TEST_RVV    ?= $(TARGET_TEST)_rvv
# Bench-only topics leave SRCS_TEST empty; deploy_board then skips test bins.
HAS_TEST           ?= $(if $(strip $(SRCS_TEST)),1,0)
TARGET_UPSTREAM_TEST ?= test_$(TOPIC)_upstream
TARGET_UPSTREAM_TEST_STD ?= $(TARGET_UPSTREAM_TEST)_std
TARGET_UPSTREAM_TEST_RVV ?= $(TARGET_UPSTREAM_TEST)_rvv

# Board smoke tests usually deploy the RVV test binary, but a few legacy topics
# kept an un-suffixed remote name. Override BOARD_TARGET_TEST in the topic
# Makefile instead of changing TARGET_TEST_RVV or duplicating deploy rules.
BOARD_TARGET_TEST ?= $(TARGET_TEST_RVV)
BOARD_TEST_USE_PCL_RVV10 ?= 1

TARGET_BENCH_BIN = $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH)
TARGET_TEST_BIN  = $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST)
TARGET_UPSTREAM_TEST_BIN = $(BUILD_DIR)/$(ARCH)/$(TARGET_UPSTREAM_TEST)

USE_PCL_RVV10  ?= 1

CXXFLAGS_ARCH = -march=rv64gcv -mabi=lp64d \
	$(VEC_MISSED_CXXFLAGS) \
	-DPCL_SILENCE_MALLOC_WARNING=1
CXXFLAGS_ARCH += $(EIGEN_RVV_FLAGS)
ifeq ($(USE_PCL_RVV10),1)
CXXFLAGS_ARCH += -D__RVV10__
endif

VEC_REGEX_STR := "[[:space:]]+v[a-z0-9]+(\.[a-z0-9]+)*[[:space:]]+"

INCLUDES ?= \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT)/filters/include \
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

EXTRA_CXXFLAGS ?=
EXTRA_LDFLAGS  ?=
CXXFLAGS = -std=c++17 -O3 -g $(CXXFLAGS_ARCH) -DPCL_NO_PRECOMPILE $(INCLUDES) $(EXTRA_CXXFLAGS)
LIBS_BENCH ?= -lpcl_common -lm
LIBS_TEST  ?= -lpcl_common -lgtest -lgtest_main -lpthread -lm
LIBS_UPSTREAM_TEST ?= $(LIBS_TEST)

# Extra argv passed to local test binaries. Use this for topic-owned fixtures
# such as a PCD path; quote values in the topic Makefile when spaces are possible.
TEST_ARGS ?=

$(LOG_DIR):
	mkdir -p $(LOG_DIR)
$(VEC_LOGS_DIR): $(LOG_DIR)
	mkdir -p $(VEC_LOGS_DIR)
$(LOG_VEC_MISS_DIR): $(LOG_DIR)
	mkdir -p $(LOG_VEC_MISS_DIR)
$(BUILD_DIR)/$(ARCH):
	mkdir -p $(BUILD_DIR)/$(ARCH)
$(ASM_DIR):
	mkdir -p $(ASM_DIR)
$(ASM_DIR)/$(ARCH): $(ASM_DIR)
	mkdir -p $(ASM_DIR)/$(ARCH)
$(OUTPUT_DIR_QEMU):
	mkdir -p $(OUTPUT_DIR_QEMU)
$(OUTPUT_DIR_BOARD):
	mkdir -p $(OUTPUT_DIR_BOARD)

$(TARGET_BENCH_BIN): $(SRCS_BENCH) | $(LOG_DIR) $(VEC_MISSED_PREREQ) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling Benchmark ($(ARCH), USE_PCL_RVV10=$(USE_PCL_RVV10))..."
	$(CXX) $(CXXFLAGS) $(SRCS_BENCH) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_BENCH) -o $(TARGET_BENCH_BIN)

$(TARGET_TEST_BIN): $(SRCS_TEST) | $(LOG_DIR) $(VEC_MISSED_PREREQ) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling Unit Test ($(ARCH), USE_PCL_RVV10=$(USE_PCL_RVV10))..."
	$(CXX) $(CXXFLAGS) $(SRCS_TEST) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_TEST) -o $(TARGET_TEST_BIN)

$(TARGET_UPSTREAM_TEST_BIN): $(SRCS_UPSTREAM_TEST) | $(LOG_DIR) $(VEC_MISSED_PREREQ) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling Upstream Unit Test ($(ARCH), USE_PCL_RVV10=$(USE_PCL_RVV10))..."
	$(CXX) $(CXXFLAGS) $(SRCS_UPSTREAM_TEST) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_UPSTREAM_TEST) -o $(TARGET_UPSTREAM_TEST_BIN)

run_test: clean_test $(TARGET_TEST_BIN) | $(OUTPUT_DIR_QEMU)
	@echo "[RUN] Unit Test on $(ARCH)..."
	$(RUN_CMD) ./$(TARGET_TEST_BIN) $(TEST_ARGS) 2>&1 | tee $(TEST_OUTPUT_FILE)
run_test_std: clean_test_std | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_test USE_PCL_RVV10=0 TARGET_TEST=$(TARGET_TEST_STD) TEST_OUTPUT_FILE=$(TEST_STD_OUTPUT_FILE) TEST_ARGS="$(TEST_ARGS)"
run_test_rvv: clean_test_rvv | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_test USE_PCL_RVV10=1 TARGET_TEST=$(TARGET_TEST_RVV) TEST_OUTPUT_FILE=$(TEST_RVV_OUTPUT_FILE) TEST_ARGS="$(TEST_ARGS)"
run_test_compare: run_test_std run_test_rvv

run_upstream_test: clean_upstream_test $(TARGET_UPSTREAM_TEST_BIN) | $(OUTPUT_DIR_QEMU)
	@echo "[RUN] Upstream Unit Test on $(ARCH)..."
	$(RUN_CMD) ./$(TARGET_UPSTREAM_TEST_BIN) $(UPSTREAM_TEST_ARGS) 2>&1 | tee $(UPSTREAM_TEST_OUTPUT_FILE)
run_upstream_test_std: clean_upstream_test_std | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_upstream_test USE_PCL_RVV10=0 TARGET_UPSTREAM_TEST=$(TARGET_UPSTREAM_TEST_STD) UPSTREAM_TEST_OUTPUT_FILE=$(UPSTREAM_TEST_STD_OUTPUT_FILE) UPSTREAM_TEST_ARGS="$(UPSTREAM_TEST_ARGS)"
run_upstream_test_rvv: clean_upstream_test_rvv | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_upstream_test USE_PCL_RVV10=1 TARGET_UPSTREAM_TEST=$(TARGET_UPSTREAM_TEST_RVV) UPSTREAM_TEST_OUTPUT_FILE=$(UPSTREAM_TEST_RVV_OUTPUT_FILE) UPSTREAM_TEST_ARGS="$(UPSTREAM_TEST_ARGS)"
run_upstream_test_compare: run_upstream_test_std run_upstream_test_rvv
run_test_all: run_test_compare run_upstream_test_compare

run_bench: clean_bench $(TARGET_BENCH_BIN)
	@echo "[RUN] Benchmark on $(ARCH) (USE_PCL_RVV10=$(USE_PCL_RVV10))..."
	$(RUN_CMD) ./$(TARGET_BENCH_BIN) $(BENCH_ARGS)
run_bench_rvv: clean_bench_rvv | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench USE_PCL_RVV10=1 TARGET_BENCH=$(TARGET_BENCH_RVV) BENCH_ARGS="$(BENCH_ARGS)" 2>&1 | tee $(BENCH_RVV_OUTPUT_FILE)
run_bench_std: clean_bench_std | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_bench USE_PCL_RVV10=0 TARGET_BENCH=$(TARGET_BENCH_STD) BENCH_ARGS="$(BENCH_ARGS)" 2>&1 | tee $(BENCH_STD_OUTPUT_FILE)
analyze_bench_compare: $(BENCH_STD_OUTPUT_FILE) $(BENCH_RVV_OUTPUT_FILE) | $(OUTPUT_DIR_QEMU)
	@$(PYTHON_RUN) $(BENCH_COMPARE_SCRIPT) --std-log $(BENCH_STD_OUTPUT_FILE) --rvv-log $(BENCH_RVV_OUTPUT_FILE) 2>&1 | tee $(BENCH_COMPARE_OUTPUT_FILE)
guard_qemu_bench_compare:
	@if [ "$(ALLOW_QEMU_BENCH_COMPARE)" != "1" ]; then \
		echo "[guard] QEMU run_bench_compare is disabled by default; use board/target hardware for bench conclusions." >&2; \
		echo "[guard] For a deliberate narrow QEMU log-shape smoke only, rerun with ALLOW_QEMU_BENCH_COMPARE=1 and document qemu_smoke_only." >&2; \
		exit 2; \
	fi
run_bench_compare: guard_qemu_bench_compare run_bench_std run_bench_rvv analyze_bench_compare

generate_vec_report: | $(LOG_DIR) $(LOG_VEC_MISS_DIR)
	@$(MAKE) -C $(CURDIR) clean_bench
	@$(MAKE) -C $(CURDIR) ENABLE_VEC_MISSED=1 $(TARGET_BENCH_BIN)
	@latest_file=""; \
	if ls "$(LOG_VEC_MISS_DIR)"/vec_missed_*.log >/dev/null 2>&1; then \
		latest_file=$$(realpath "$$(ls -1t "$(LOG_VEC_MISS_DIR)"/vec_missed_*.log | head -n 1)"); \
	else \
		: > "$(LATEST_VEC_LOG)"; \
	fi; \
	if [ -n "$$latest_file" ]; then ln -sf "$$latest_file" "$(LATEST_VEC_LOG)"; fi; \
	grep '$(FOCUS_DIR)' '$(LATEST_VEC_LOG)' -A 1 > '$(FILTER_REPORT)' || true; \
	if [ ! -s "$(FILTER_REPORT)" ]; then grep 'missed:' '$(LATEST_VEC_LOG)' | head -n 80 > '$(FILTER_REPORT)' || true; fi; \
	$(PYTHON_RUN) "$(ANALYZE_VEC_SCRIPT)" "$(FILTER_REPORT)" -o "$(ANALYZE_REPORT)" -s "$(VEC_LOGS_DIR)"

dump_bench_rvv: | $(ASM_DIR)/$(ARCH)
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_BENCH=$(TARGET_BENCH_RVV) $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV)
	$(OBJDUMP) -d -C $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) > $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm
	grep -E $(VEC_REGEX_STR) $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).full.asm > $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm || true
	@echo "[DONE] RVV asm dump: $(ASM_DIR)/$(ARCH)/$(TARGET_BENCH_RVV).asm"

clean_test:
	rm -f $(TARGET_TEST_BIN)
clean_test_std:
	rm -f $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_STD)
clean_test_rvv:
	rm -f $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_RVV)
clean_upstream_test:
	rm -f $(TARGET_UPSTREAM_TEST_BIN)
clean_upstream_test_std:
	rm -f $(BUILD_DIR)/$(ARCH)/$(TARGET_UPSTREAM_TEST_STD)
clean_upstream_test_rvv:
	rm -f $(BUILD_DIR)/$(ARCH)/$(TARGET_UPSTREAM_TEST_RVV)
clean_bench:
	rm -f $(TARGET_BENCH_BIN)
clean_bench_std:
	rm -f $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD)
clean_bench_rvv:
	rm -f $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV)
CLEAN_TARGETS ?= $(BUILD_DIR) $(LOG_DIR) $(if $(filter-out $(LOG_DIR),$(OUTPUT_DIR)),$(OUTPUT_DIR),)
clean:
	rm -rf $(CLEAN_TARGETS)

check_board_ssh:
	@test -n "$(REMOTE_USER)" || (echo "[config] REMOTE_USER is not set. Set it in $(TEST_RVV_ROOT)/config.mk or pass REMOTE_USER=<user>." >&2; exit 1)
	@test -n "$(REMOTE_IP)" || (echo "[config] REMOTE_IP is not set. Set it in $(TEST_RVV_ROOT)/config.mk or pass REMOTE_IP=<ip>." >&2; exit 1)
	@$(SSH_CMD) -o BatchMode=yes -o ConnectTimeout=5 $(REMOTE_USER)@$(REMOTE_IP) true

deploy_files: check_board_ssh
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $(REMOTE_SCRIPT_DIR)"
	@rsync -e "$(RSYNC_SSH)" -avzP $(BENCH_COMPARE_SCRIPT) $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_SCRIPT_DIR)/
	@rsync -e "$(RSYNC_SSH)" -avzP $(BOARD_RUN_MK) $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_SCRIPT_DIR)/
	@rsync -e "$(RSYNC_SSH)" -avzP board.mk $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/Makefile
	@if [ -n "$(DEPLOY_EXTRA_FILES)" ]; then \
		$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $(DEPLOY_EXTRA_REMOTE_DIR)"; \
		rsync -e "$(RSYNC_SSH)" -avzP $(DEPLOY_EXTRA_FILES) $(REMOTE_USER)@$(REMOTE_IP):$(DEPLOY_EXTRA_REMOTE_DIR)/; \
	fi
deploy_bench_rvv: deploy_files clean_bench_rvv
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_BENCH=$(TARGET_BENCH_RVV) $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV)
	@$(STRIP) -s ./$(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_RVV) -o ./$(TARGET_BENCH_RVV)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_BENCH_RVV)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_BENCH_RVV)
	@rm -f ./$(TARGET_BENCH_RVV)_stripped
deploy_bench_std: deploy_files clean_bench_std
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=0 TARGET_BENCH=$(TARGET_BENCH_STD) $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD)
	@$(STRIP) -s ./$(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH_STD) -o ./$(TARGET_BENCH_STD)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_BENCH_STD)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_BENCH_STD)
	@rm -f ./$(TARGET_BENCH_STD)_stripped
deploy_test: deploy_files
	@rm -f ./$(BUILD_DIR)/$(ARCH)/$(BOARD_TARGET_TEST)
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=$(BOARD_TEST_USE_PCL_RVV10) TARGET_TEST=$(BOARD_TARGET_TEST) $(BUILD_DIR)/$(ARCH)/$(BOARD_TARGET_TEST)
	@$(STRIP) -s ./$(BUILD_DIR)/$(ARCH)/$(BOARD_TARGET_TEST) -o ./$(BOARD_TARGET_TEST)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(BOARD_TARGET_TEST)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(BOARD_TARGET_TEST)
	@rm -f ./$(BOARD_TARGET_TEST)_stripped
DEPLOY_BOARD_TARGETS :=
ifeq ($(HAS_BENCH),1)
DEPLOY_BOARD_TARGETS += deploy_bench_std deploy_bench_rvv
endif
ifeq ($(HAS_TEST),1)
DEPLOY_BOARD_TARGETS += deploy_test
endif
deploy_board: $(DEPLOY_BOARD_TARGETS)
run_board_test: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' REMOTE_TEST_ARGS='$(REMOTE_TEST_ARGS)'"
run_board_bench_compare: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare REMOTE_OUTPUT_DIR='$(REMOTE_BOARD_OUTPUT_DIR)' BENCH_COMPARE_SAVE='$(REMOTE_BOARD_OUTPUT_DIR)/analyze_bench_compare.log' BOARD_LABEL='$(BOARD_LABEL)' REMOTE_BENCH_ARGS='$(BENCH_ARGS)'"
fetch_board_logs: | $(OUTPUT_DIR_BOARD)
	@rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_BOARD_OUTPUT_DIR)/ $(OUTPUT_DIR_BOARD)/
board_smoke: run_board_test run_board_bench_compare fetch_board_logs

sanitize_output_logs:
	@files=$$(find "$(OUTPUT_DIR_QEMU)" "$(OUTPUT_DIR_BOARD)" -type f -name '*.log' 2>/dev/null | sort); \
	if [ -z "$$files" ]; then \
		echo "[sanitize] No output logs under $(OUTPUT_DIR_QEMU) or $(OUTPUT_DIR_BOARD)."; \
	else \
		$(PYTHON_RUN) "$(SANITIZE_LOGS_SCRIPT)" --in-place $$files; \
	fi

check_output_logs_sanitized:
	@files=$$(find "$(OUTPUT_DIR_QEMU)" "$(OUTPUT_DIR_BOARD)" -type f -name '*.log' 2>/dev/null | sort); \
	if [ -z "$$files" ]; then \
		echo "[sanitize] No output logs under $(OUTPUT_DIR_QEMU) or $(OUTPUT_DIR_BOARD)."; \
	else \
		$(PYTHON_RUN) "$(SANITIZE_LOGS_SCRIPT)" --check $$files; \
	fi

.PHONY: run_test run_test_std run_test_rvv run_test_compare run_upstream_test run_upstream_test_std run_upstream_test_rvv run_upstream_test_compare run_test_all run_bench run_bench_std run_bench_rvv guard_qemu_bench_compare run_bench_compare analyze_bench_compare generate_vec_report dump_bench_rvv clean clean_test clean_test_std clean_test_rvv clean_upstream_test clean_upstream_test_std clean_upstream_test_rvv clean_bench clean_bench_std clean_bench_rvv check_board_ssh deploy_files deploy_bench_rvv deploy_bench_std deploy_test deploy_board run_board_test run_board_bench_compare fetch_board_logs board_smoke sanitize_output_logs check_output_logs_sanitized
