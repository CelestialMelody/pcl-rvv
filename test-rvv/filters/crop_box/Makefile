# =============================================================================
#  PCL filters/crop_box RVV test/bench Makefile
#
# 板卡：开发机 make deploy_board 后，可继续 make run_board_test /
#       make run_board_bench_compare / make fetch_board_logs；板卡侧入口见 board.mk。
# =============================================================================

LOG_DIR           = log
TIMESTAMP        := $(shell date +%Y-%m-%d_%H-%M-%S)
LOG_FILE          = $(LOG_VEC_MISS_DIR)/vec_missed_$(TIMESTAMP).log
LATEST_VEC_LOG    = $(LOG_DIR)/latest_vec_missed.log
FOCUS_DIR         = pcl/filters/include/pcl/filters/impl/crop_box.hpp
FILTER_REPORT     = $(LOG_DIR)/filtered_crop_box.log
VEC_LOGS_DIR      = $(LOG_DIR)/vec_logs
LOG_VEC_MISS_DIR  = $(LOG_DIR)/vec_missed_log
ANALYZE_REPORT    = $(LOG_DIR)/analyze_crop_box.log

OUTPUT_DIR        = output
OUTPUT_DIR_BOARD  = $(OUTPUT_DIR)/board
OUTPUT_DIR_QEMU   = $(OUTPUT_DIR)/qemu

BUILD_DIR         = build
ASM_DIR           = $(BUILD_DIR)/asm
TEST_RVV_SHARED_SCRIPT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../../script)

BENCH_STD_OUTPUT_FILE     ?= $(OUTPUT_DIR_QEMU)/run_bench_std.log
BENCH_RVV_OUTPUT_FILE     ?= $(OUTPUT_DIR_QEMU)/run_bench_rvv.log
BENCH_COMPARE_OUTPUT_FILE ?= $(OUTPUT_DIR_QEMU)/analyze_bench_compare.log
TEST_STD_OUTPUT_FILE      ?= $(OUTPUT_DIR_QEMU)/run_test_std.log
TEST_RVV_OUTPUT_FILE      ?= $(OUTPUT_DIR_QEMU)/run_test_rvv.log
TEST_OUTPUT_FILE          ?= $(OUTPUT_DIR_QEMU)/run_test.log
UPSTREAM_TEST_OUTPUT_FILE ?= $(OUTPUT_DIR_QEMU)/run_upstream_test.log
UPSTREAM_TEST_STD_OUTPUT_FILE ?= $(OUTPUT_DIR_QEMU)/run_upstream_test_std.log
UPSTREAM_TEST_RVV_OUTPUT_FILE ?= $(OUTPUT_DIR_QEMU)/run_upstream_test_rvv.log

ANALYZE_VEC_SCRIPT    = $(TEST_RVV_SHARED_SCRIPT_DIR)/analyze_vec_log.py
BENCH_COMPARE_SCRIPT  = $(TEST_RVV_SHARED_SCRIPT_DIR)/analyze_bench_compare.py

VENV_ACTIVATE := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/.venv/bin/activate)
PYTHON ?= python3
PYTHON_RUN = bash -lc 'if [ -f "$(VENV_ACTIVATE)" ]; then . "$(VENV_ACTIVATE)"; fi; $(PYTHON) "$$@"' --

TARGET_BENCH       ?= bench_crop_box
TARGET_BENCH_STD   ?= bench_crop_box_std
TARGET_BENCH_RVV   ?= bench_crop_box_rvv
SRCS_BENCH          = bench_crop_box.cpp
TARGET_TEST        ?= test_crop_box
TARGET_TEST_STD    ?= test_crop_box_std
TARGET_TEST_RVV    ?= test_crop_box_rvv
SRCS_TEST           = test_crop_box.cpp
TARGET_UPSTREAM_TEST ?= test_crop_box_upstream
TARGET_UPSTREAM_TEST_STD ?= test_crop_box_upstream_std
TARGET_UPSTREAM_TEST_RVV ?= test_crop_box_upstream_rvv
SRCS_UPSTREAM_TEST  = $(PCL_SOURCE_ROOT)/test/filters/test_clipper.cpp
UPSTREAM_TEST_ARGS ?=

ARCH ?= riscv
TARGET_BENCH_BIN = $(BUILD_DIR)/$(ARCH)/$(TARGET_BENCH)
TARGET_TEST_BIN  = $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST)
TARGET_UPSTREAM_TEST_BIN = $(BUILD_DIR)/$(ARCH)/$(TARGET_UPSTREAM_TEST)

USE_PCL_RVV10  ?= 1

WS               = /home/zoomin/codes/RISCV/workspace
PCL_SOURCE_ROOT  = $(WS)/pcl

ifeq ($(ARCH),riscv)
CC  = riscv64-unknown-linux-gnu-gcc
CXX = riscv64-unknown-linux-gnu-g++
STRIP = riscv64-unknown-linux-gnu-strip
OBJDUMP = riscv64-unknown-linux-gnu-objdump

PCL_INSTALL_ROOT = $(WS)/riscv/pcl-rvv
RISCV_DEPS       = $(WS)/riscv
RISCV_SYSROOT   := $(shell $(CC) -print-sysroot)

BOOST_ROOT  = $(RISCV_DEPS)/boost
EIGEN_ROOT  = $(RISCV_DEPS)/eigen-rvv
GTEST_ROOT  = $(RISCV_DEPS)/gtest
FLANN_ROOT  = $(RISCV_DEPS)/flann
LZ4_ROOT    = $(RISCV_DEPS)/lz4
HDF5_ROOT   = $(RISCV_DEPS)/hdf5
ZLIB_ROOT   = $(RISCV_DEPS)/zlib
LIBPNG_ROOT = $(RISCV_DEPS)/libpng

EIGEN_RVV_FLAGS ?= -DEIGEN_RISCV64_USE_RVV10 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl
LIB_DIRS_LIST = \
	$(PCL_INSTALL_ROOT)/lib \
	$(BOOST_ROOT)/lib \
	$(GTEST_ROOT)/lib \
	$(FLANN_ROOT)/lib \
	$(LZ4_ROOT)/lib \
	$(HDF5_ROOT)/lib \
	$(ZLIB_ROOT)/lib \
	$(LIBPNG_ROOT)/lib
empty :=
space := $(empty) $(empty)
LIB_PATH_VAL := $(subst $(space),:,$(LIB_DIRS_LIST))

CXXFLAGS_ARCH = -march=rv64gcv -mabi=lp64d \
	-fopt-info-vec-missed=$(LOG_FILE) \
	-DPCL_SILENCE_MALLOC_WARNING=1
CXXFLAGS_ARCH += $(EIGEN_RVV_FLAGS)
ifeq ($(USE_PCL_RVV10),1)
CXXFLAGS_ARCH += -D__RVV10__
endif

LDFLAGS = $(foreach dir,$(LIB_DIRS_LIST),-L$(dir) -Wl,-rpath-link=$(dir))
RUN_CMD = LD_LIBRARY_PATH=$(LIB_PATH_VAL):$$LD_LIBRARY_PATH qemu-riscv64 -L $(RISCV_SYSROOT) -cpu rv64,v=true,vlen=256,elen=64
VEC_REGEX_STR := "[[:space:]]+v[a-z0-9]+(\.[a-z0-9]+)*[[:space:]]+"
endif

ifneq ($(filter $(ARCH),riscv),$(ARCH))
$(error Unsupported ARCH=$(ARCH), expected riscv)
endif

INCLUDES = \
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
LIBS_BENCH = -lpcl_common -lm
LIBS_TEST  = -lpcl_common -lgtest -lgtest_main -lpthread -lm
LIBS_UPSTREAM_TEST = \
	-lpcl_filters \
	-lpcl_io \
	-lpcl_sample_consensus \
	-lpcl_search \
	-lpcl_kdtree \
	-lpcl_octree \
	-lpcl_common \
	-lflann_cpp \
	-lboost_filesystem -lboost_iostreams -lboost_system \
	-lgtest -lgtest_main -lpthread \
	-lz -lhdf5 -llz4 -lpng -lm

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
$(LOG_DIR):
	mkdir -p $(LOG_DIR)
$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)
$(VEC_LOGS_DIR): $(LOG_DIR)
	mkdir -p $(VEC_LOGS_DIR)
$(LOG_VEC_MISS_DIR): $(LOG_DIR)
	mkdir -p $(LOG_VEC_MISS_DIR)
$(BUILD_DIR)/$(ARCH): $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/$(ARCH)
$(ASM_DIR): $(BUILD_DIR)
	mkdir -p $(ASM_DIR)
$(ASM_DIR)/$(ARCH): $(ASM_DIR)
	mkdir -p $(ASM_DIR)/$(ARCH)
$(OUTPUT_DIR_QEMU): $(OUTPUT_DIR)
	mkdir -p $(OUTPUT_DIR_QEMU)
$(OUTPUT_DIR_BOARD): $(OUTPUT_DIR)
	mkdir -p $(OUTPUT_DIR_BOARD)

$(TARGET_BENCH_BIN): $(SRCS_BENCH) | $(LOG_DIR) $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling Benchmark ($(ARCH), USE_PCL_RVV10=$(USE_PCL_RVV10))..."
	$(CXX) $(CXXFLAGS) $(SRCS_BENCH) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_BENCH) -o $(TARGET_BENCH_BIN)

$(TARGET_TEST_BIN): $(SRCS_TEST) | $(LOG_DIR) $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling Unit Test ($(ARCH), USE_PCL_RVV10=$(USE_PCL_RVV10))..."
	$(CXX) $(CXXFLAGS) $(SRCS_TEST) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_TEST) -o $(TARGET_TEST_BIN)

$(TARGET_UPSTREAM_TEST_BIN): $(SRCS_UPSTREAM_TEST) | $(LOG_DIR) $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	@echo "[BUILD] Compiling Upstream Unit Test ($(ARCH), USE_PCL_RVV10=$(USE_PCL_RVV10))..."
	$(CXX) $(CXXFLAGS) $(SRCS_UPSTREAM_TEST) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LIBS_UPSTREAM_TEST) -o $(TARGET_UPSTREAM_TEST_BIN)

run_test: clean_test $(TARGET_TEST_BIN) | $(OUTPUT_DIR_QEMU)
	@echo "[RUN] Unit Test on $(ARCH)..."
	$(RUN_CMD) ./$(TARGET_TEST_BIN) 2>&1 | tee $(TEST_OUTPUT_FILE)
run_test_std: clean_test_std | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_test USE_PCL_RVV10=0 TARGET_TEST=$(TARGET_TEST_STD) TEST_OUTPUT_FILE=$(TEST_STD_OUTPUT_FILE)
run_test_rvv: clean_test_rvv | $(OUTPUT_DIR_QEMU)
	@$(MAKE) -C $(CURDIR) run_test USE_PCL_RVV10=1 TARGET_TEST=$(TARGET_TEST_RVV) TEST_OUTPUT_FILE=$(TEST_RVV_OUTPUT_FILE)
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
run_bench_compare: run_bench_std run_bench_rvv analyze_bench_compare

generate_vec_report: $(TARGET_BENCH_BIN) | $(LOG_DIR) $(LOG_VEC_MISS_DIR)
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
clean:
	rm -rf $(BUILD_DIR) $(LOG_DIR) $(OUTPUT_DIR)

REMOTE_USER ?= root
REMOTE_IP   ?= 172.16.89.126
REMOTE_DIR  ?= /root/pcl-test/filters/crop_box
REMOTE_SCRIPT_DIR = $(REMOTE_DIR)/script
REMOTE_BOARD_OUTPUT_DIR = $(REMOTE_DIR)/output
BOARD_BENCH_COMPARE_OUTPUT_FILE ?= $(OUTPUT_DIR_BOARD)/analyze_bench_compare.log
BOARD_TEST_OUTPUT_FILE ?= $(OUTPUT_DIR_BOARD)/run_test.log
SSH_OPTS ?= -F $(HOME)/.ssh/config
SSH_CMD ?= ssh $(SSH_OPTS)
RSYNC_SSH ?= ssh $(SSH_OPTS)

deploy_files: check_board_ssh
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $(REMOTE_SCRIPT_DIR)"
	@rsync -e "$(RSYNC_SSH)" -avzP $(BENCH_COMPARE_SCRIPT) $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_SCRIPT_DIR)/
	@rsync -e "$(RSYNC_SSH)" -avzP board.mk $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/Makefile
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
deploy_test: deploy_files clean_test_rvv
	@$(MAKE) -C $(CURDIR) USE_PCL_RVV10=1 TARGET_TEST=$(TARGET_TEST_RVV) $(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_RVV)
	@$(STRIP) -s ./$(BUILD_DIR)/$(ARCH)/$(TARGET_TEST_RVV) -o ./$(TARGET_TEST_RVV)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_TEST_RVV)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_TEST_RVV)
	@rm -f ./$(TARGET_TEST_RVV)_stripped
deploy_board: deploy_bench_std deploy_bench_rvv deploy_test
run_board_test: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_test"
run_board_bench_compare: deploy_board | $(OUTPUT_DIR_BOARD)
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "cd $(REMOTE_DIR) && $(MAKE) run_bench_compare BENCH_COMPARE_SAVE=output/analyze_bench_compare.log"
fetch_board_logs: | $(OUTPUT_DIR_BOARD)
	@rsync -e "$(RSYNC_SSH)" -avzP $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_BOARD_OUTPUT_DIR)/ $(OUTPUT_DIR_BOARD)/
board_smoke: run_board_test run_board_bench_compare fetch_board_logs

.PHONY: run_test run_test_std run_test_rvv run_test_compare run_upstream_test run_upstream_test_std run_upstream_test_rvv run_upstream_test_compare run_test_all run_bench run_bench_std run_bench_rvv run_bench_compare analyze_bench_compare generate_vec_report dump_bench_rvv clean clean_test clean_test_std clean_test_rvv clean_upstream_test clean_upstream_test_std clean_upstream_test_rvv clean_bench clean_bench_std clean_bench_rvv check_board_ssh deploy_files deploy_bench_rvv deploy_bench_std deploy_test deploy_board run_board_test run_board_bench_compare fetch_board_logs board_smoke
