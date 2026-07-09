# =============================================================================
#  RVV load/store micro-benchmarks (isolated)
# =============================================================================

MODULE := rvv
TOPIC  := load_store

EIGEN_RVV_FLAGS := -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=2 -march=rv64gcv_zvl256b -mrvv-vector-bits=zvl

INCLUDES = \
	-I$(PCL_SOURCE_ROOT)/common/include \
	-I$(PCL_SOURCE_ROOT) \
	-I$(PCL_INSTALL_ROOT)/include/pcl-1.15 \
	-I$(EIGEN_ROOT)/include/eigen3 \
	-I$(BOOST_ROOT)/include

TARGET_LOAD  := bench_rvv_load_compare
TARGET_STORE := bench_rvv_store_compare

TARGET_BENCH := $(TARGET_LOAD)
SRCS_BENCH   := $(TARGET_LOAD).cpp
LIBS_BENCH   := -lpcl_common -lm
HAS_BENCH    := 0
HAS_TEST     := 0

# Historical clean only removed build artifacts; output/qemu contains tracked
# reference logs in this topic.
CLEAN_TARGETS = $(BUILD_DIR)

REMOTE_DIR ?= ~/pcl-test/rvv/load_store

include ../../mk/rvv-topic.mk

.DEFAULT_GOAL := all

TARGET_LOAD_BIN  := $(TARGET_BENCH_BIN)
TARGET_STORE_BIN := $(BUILD_DIR)/$(ARCH)/$(TARGET_STORE)

# Default workload (tuned for board interactivity)
N_POINTS ?= 262144
ITERS    ?= 50
WARMUP   ?= 5

all: $(TARGET_LOAD_BIN) $(TARGET_STORE_BIN)

$(TARGET_STORE_BIN): $(TARGET_STORE).cpp | $(LOG_DIR) $(LOG_VEC_MISS_DIR) $(BUILD_DIR)/$(ARCH)
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) $(LIBS_BENCH) -o $@

run_load: $(TARGET_LOAD_BIN) | $(OUTPUT_DIR_QEMU)
	$(RUN_CMD) ./$(TARGET_LOAD_BIN) $(N_POINTS) $(ITERS) $(WARMUP) 2>&1 | tee $(OUTPUT_DIR_QEMU)/run_load.log

run_store: $(TARGET_STORE_BIN) | $(OUTPUT_DIR_QEMU)
	$(RUN_CMD) ./$(TARGET_STORE_BIN) $(N_POINTS) $(ITERS) $(WARMUP) 2>&1 | tee $(OUTPUT_DIR_QEMU)/run_store.log

deploy_load: check_board_ssh $(TARGET_LOAD_BIN)
	@echo "[DEPLOY] $(TARGET_LOAD) -> $(REMOTE_DIR)"
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $(REMOTE_DIR)"
	@$(STRIP) -s $(TARGET_LOAD_BIN) -o ./$(TARGET_LOAD)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_LOAD)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_LOAD)
	@rm -f ./$(TARGET_LOAD)_stripped

deploy_store: check_board_ssh $(TARGET_STORE_BIN)
	@echo "[DEPLOY] $(TARGET_STORE) -> $(REMOTE_DIR)"
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $(REMOTE_DIR)"
	@$(STRIP) -s $(TARGET_STORE_BIN) -o ./$(TARGET_STORE)_stripped
	@rsync -e "$(RSYNC_SSH)" -avzP ./$(TARGET_STORE)_stripped $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/$(TARGET_STORE)
	@rm -f ./$(TARGET_STORE)_stripped

deploy_board_file: check_board_ssh
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $(REMOTE_DIR)"
	@rsync -e "$(RSYNC_SSH)" -avzP board.mk $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_DIR)/board.mk

deploy_board: deploy_load deploy_store deploy_board_file

.PHONY: all run_load run_store deploy_load deploy_store deploy_board_file
