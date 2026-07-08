REMOTE_LIB_DIR = ~/pcl-test/lib
TEMP_LIB_DIR = ./slim_lib

MODULE ?= test-rvv
TOPIC ?= deploy_lib
include mk/rvv-env.mk

# 交叉编译工具链的 Sysroot 路径
SYSROOT := $(RISCV_SYSROOT)

check_board_ssh:
	@test -n "$(REMOTE_USER)" || (echo "[config] REMOTE_USER is not set. Set it in $(TEST_RVV_ROOT)/config.mk or pass REMOTE_USER=<user>." >&2; exit 1)
	@test -n "$(REMOTE_IP)" || (echo "[config] REMOTE_IP is not set. Set it in $(TEST_RVV_ROOT)/config.mk or pass REMOTE_IP=<ip>." >&2; exit 1)
	@$(SSH_CMD) -o BatchMode=yes -o ConnectTimeout=5 $(REMOTE_USER)@$(REMOTE_IP) true

deploy_lib: check_board_ssh
	@echo "[PREPARE] Creating local slim_lib directory..."
	@mkdir -p $(TEMP_LIB_DIR)

	@echo "[COPY] Collecting libraries..."
# 1. 收集 PCL 核心库
	@cp $(PCL_INSTALL_ROOT)/lib/lib*.so* $(TEMP_LIB_DIR)/ 2>/dev/null || true
# 2. 收集 关键依赖库 (boost, lz4, hdf5, flann, png, zlib)
	@cp $(RISCV_DEPS)/boost/lib/lib*.so* $(TEMP_LIB_DIR)/ 2>/dev/null || true
	@cp $(RISCV_DEPS)/lz4/lib/lib*.so* $(TEMP_LIB_DIR)/ 2>/dev/null || true
	@cp $(RISCV_DEPS)/hdf5/lib/lib*.so* $(TEMP_LIB_DIR)/ 2>/dev/null || true
	@cp $(RISCV_DEPS)/flann/lib/lib*.so* $(TEMP_LIB_DIR)/ 2>/dev/null || true
	@cp $(RISCV_DEPS)/libpng/lib/lib*.so* $(TEMP_LIB_DIR)/ 2>/dev/null || true
	@cp $(RISCV_DEPS)/zlib/lib/lib*.so* $(TEMP_LIB_DIR)/ 2>/dev/null || true
# 3. 收集 GTest
	@cp $(RISCV_DEPS)/gtest/lib/lib*.so* $(TEMP_LIB_DIR)/ 2>/dev/null || true
# 4. 收集 系统基础库
	@cp $(SYSROOT)/lib/libstdc++.so* $(SYSROOT)/lib/libc.so* $(SYSROOT)/lib/libm.so* $(SYSROOT)/lib/libgcc_s.so* $(TEMP_LIB_DIR)/ 2>/dev/null || true

	@echo "[STRIP] Slimming down libraries..."
	@$(STRIP) -s $(TEMP_LIB_DIR)/*.so* 2>/dev/null || true

	@echo "[DEPLOY] Syncing slim lib to board..."
	@$(SSH_CMD) $(REMOTE_USER)@$(REMOTE_IP) "mkdir -p $(REMOTE_LIB_DIR)"
	@rsync -e "$(RSYNC_SSH)" -avzP $(TEMP_LIB_DIR)/ $(REMOTE_USER)@$(REMOTE_IP):$(REMOTE_LIB_DIR)/

	@echo "[CLEANUP] Removing local temporary slim_lib..."
	@rm -rf $(TEMP_LIB_DIR)
	@echo "[DONE] All essential lib are deployed."
