/* 在用户态读取 RISC-V CSR vlenb（字节），打印一行供 board.mk / 脚本解析。
 * 须在带 RVV 且内核允许该 csr 的机器上运行；无向量扩展时会非法指令中止。
 *
 * 板卡就地编译示例：
 *   gcc -O2 -o read_vlen riscv_read_vlen.c
 * 交叉编译见 test-rvv/app/Makefile $(BUILD_DIR)/read_vlen
 */
#include <stdio.h>

int
main (void)
{
  unsigned long vlenb = 0;
#if defined(__riscv) && (__riscv_xlen == 64)
  __asm__ volatile("csrr %0, vlenb" : "=r"(vlenb));
  printf ("%lu bits (hardware vlenb csr, %lu bytes)\n", vlenb * 8UL, vlenb);
#else
  (void)vlenb;
  fprintf (stderr, "riscv_read_vlen: 仅应在 riscv64 目标上使用本程序。\n");
  return 2;
#endif
  return 0;
}
