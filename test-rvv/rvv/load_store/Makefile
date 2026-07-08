ARCH ?= riscv

# =============================================================================
#  RVV load/store micro-benchmarks (isolated)
# =============================================================================
ifeq ($(ARCH), riscv)
EIGEN_RVV_FLAGS ?= -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=2 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl
MODULE ?= rvv
TOPIC ?= load_store
include ../../mk/rvv-env.mk
endif

INCLUDES = \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include

CXXFLAGS = -std=c++17 -O3 -g \
	$(CXXFLAGS_ARCH) \
	-DPCL_NO_PRECOMPILE \
	$(INCLUDES)

LIBS = -lpcl_common -lm

BUILD_DIR = build/$(ARCH)
OUTPUT_DIR = output
OUTPUT_DIR_QEMU = $(OUTPUT_DIR)/qemu

TARGET_LOAD = bench_rvv_load_compare
TARGET_STORE = bench_rvv_store_compare

TARGET_LOAD_BIN  = $(BUILD_DIR)/$(TARGET_LOAD)
TARGET_STORE_BIN = $(BUILD_DIR)/$(TARGET_STORE)

# Default workload (tuned for board interactivity)
N_POINTS ?= 262144
ITERS    ?= 50
WARMUP   ?= 5

all: $(TARGET_LOAD_BIN) $(TARGET_STORE_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OUTPUT_DIR_QEMU):
	mkdir -p $(OUTPUT_DIR_QEMU)

$(TARGET_LOAD_BIN): bench_rvv_load_compare.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) $(LIBS) -o $@

$(TARGET_STORE_BIN): bench_rvv_store_compare.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) $(LIBS) -o $@

run_load: $(TARGET_LOAD_BIN) | $(OUTPUT_DIR_QEMU)
	$(RUN_CMD) ./$(TARGET_LOAD_BIN) $(N_POINTS) $(ITERS) $(WARMUP) 2>&1 | tee $(OUTPUT_DIR_QEMU)/run_load.log

run_store: $(TARGET_STORE_BIN) | $(OUTPUT_DIR_QEMU)
	$(RUN_CMD) ./$(TARGET_STORE_BIN) $(N_POINTS) $(ITERS) $(WARMUP) 2>&1 | tee $(OUTPUT_DIR_QEMU)/run_store.log

clean:
	rm -rf build

# =============================================================================
# Remote Deployment (board)
# =============================================================================
REMOTE_USER ?=
REMOTE_IP   ?=
REMOTE_DIR  = ~/pcl-test/rvv/load_store
SSH_OPTS ?= -F $(HOME)/.ssh/config
SSH_CMD ?= ssh $(SSH_OPTS)
RSYNC_SSH ?= ssh $(SSH_OPTS)

check_board_ssh:
	@test -n "$(REMOTE_USER)" || (echo "[config] REMOTE_USER is not set. Set it in $(TEST_RVV_ROOT)/config.mk or pass REMOTE_USER=<user>." >&2; exit 1)
	@test -n "$(REMOTE_IP)" || (echo "[config] REMOTE_IP is not set. Set it in $(TEST_RVV_ROOT)/config.mk or pass REMOTE_IP=<ip>." >&2; exit 1)
	@$(SSH_CMD) -o BatchMode=yes -o ConnectTimeout=5 $(REMOTE_USER)@$(REMOTE_IP) true


deploy_load: check_board_ssh $(TARGET_LOAD_BIN)
	@echo "[DEPLOY] $(TARGET_LOAD) -> $(REMOTE_DIR)"
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $(REMOTE_DIR)"
	@$(STRIP) -s ./$(TARGET_LOAD_BIN) -o ./$(TARGET_LOAD)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_LOAD)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_LOAD)
	@rm -f ./$(TARGET_LOAD)_stripped

deploy_store: check_board_ssh $(TARGET_STORE_BIN)
	@echo "[DEPLOY] $(TARGET_STORE) -> $(REMOTE_DIR)"
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $(REMOTE_DIR)"
	@$(STRIP) -s ./$(TARGET_STORE_BIN) -o ./$(TARGET_STORE)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_STORE)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_STORE)
	@rm -f ./$(TARGET_STORE)_stripped

.PHONY: all run_load run_store clean deploy_load deploy_store
