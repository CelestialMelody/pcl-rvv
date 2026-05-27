/*
 * =============================================================================
 * term_table.hpp — 终端等宽字体下的 UTF-8 竖线表格工具
 * =============================================================================
 *
 * 位于 test-rvv/script/，与 analyze_bench_compare.py 等共用目录，供各子项目 C++ 数学
 * 基准/对比测试直接 #include。显示宽度按 Unicode 常见终端规则估算（参见 Markus Kuhn
 * 的 wcwidth 思路），不依赖 libc 的 locale / wcwidth，适合嵌入式或 LC_ALL=C 环境。
 *
 * 首列若含中文/全角标点，会按 East Asian 字宽估算列数；输出本身为 UTF-8 纯文本。
 * 从终端复制到 math.log 后：若编辑器用**比例字体**或字族与终端不同，CJK 的视觉列宽
 * 会与程序假定不一致，竖线会显得不齐。请对日志使用**等宽字体**，或
 * `program > math.log` 后在终端 `cat`/`less` 查看，与终端观感一致。
 *
 * -----------------------------------------------------------------------------
 * 能力分层（从底到顶）
 * -----------------------------------------------------------------------------
 *
 * 1) 文本与宽度 — utf8_* / unicode_term_width / utf8_display_width
 *    用于计算「终端上占几列」，给中文/全角标点垫空格，避免 printf 域宽错位。
 *
 * 2) 字符与横线 — print_pad_spaces / print_rule_chars
 *    输出空格、重复字符横线（如 '='、'-'）。
 *
 * 3) 定宽缓冲 — fmt_fixed_right_bytes / fmt_fixed_e / fmt_fixed_f
 *    把 ASCII 或已格式化的数字串右对齐塞进固定「字节宽度」字段（与多数终端列一致）。
 *    中文等宽字符请放在首列用 utf8_display_width 垫格，不要塞进这些右对齐字节域。
 *
 * 4) 通用表格（推荐扩展用）— pipe_grid_line_columns /
 *    print_pipe_grid_header_line / print_pipe_grid_row /
 *    print_pipe_grid_row_continuation
 *    - 行数：任意多次调用 print_pipe_grid_row 即多行数据。
 *    - 列数：ncols 任意（由 col_widths[] 与 cell_texts[] 长度决定）。
 *    - 列宽：每个 col_widths[i] 自定；首列 UTF-8 标签的「占位」由 w_label_disp 统一指定。
 *    - 写入内容：每个单元格为 C 字符串（const char*）。数值请先在调用侧 snprintf
 *      进局部 char buf[]，再传入（本头文件不负责无限种类型格式化）。
 *
 * 5) 便捷封装 — Table3NumCols、print_pipe_table_3num_*、Perf2Cols、print_perf_pipe_*
 *    对应常见「1 个中文标签 + 3 个数字列」以及「Kernel + 时间 + 说明」两数值列版式。
 *    列宽默认值见 k_table3_num_default_*、k_perf_default_w_*；各测试可用 {w,...} 构造函数覆盖。
 *
 * -----------------------------------------------------------------------------
 * Include 路径
 * -----------------------------------------------------------------------------
 * 自 test-rvv/common/common 等子目录：  #include "../../script/term_table.hpp"
 * 或在编译参数中加入 -I/path/to/test-rvv/script 后：  #include "term_table.hpp"
 *
 * =============================================================================
 */

#ifndef MATH_TEST_TERM_TABLE_HPP_
#define MATH_TEST_TERM_TABLE_HPP_

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace math_test {

/* -------------------------------------------------------------------------- */
/* 一、UTF-8 与显示列宽                                                      */
/* -------------------------------------------------------------------------- */

/**
 * @brief 从合法或近似合法的 UTF-8 流中取下一码位。
 * @param p 当前字节指针（必须以 \\0 结尾的串中；*p==0 时返回 p 且 *cp=0）。
 * @param cp 输出码位；非法字节则推进 1 字节并输出 U+FFFD。
 * @return 下一码位起始指针。
 */
inline const char* utf8_next_codepoint(const char* p, std::uint32_t* cp)
{
  const auto u8 = [](unsigned char c) -> unsigned char { return c; };
  unsigned char c0 = u8(p[0]);
  if (c0 == 0) {
    *cp = 0;
    return p;
  }
  if (c0 < 0x80u) {
    *cp = c0;
    return p + 1;
  }
  if ((c0 >> 5) == 0x6u) {
    if (u8(p[1]) < 0x80u || (u8(p[1]) >> 6) != 2u) {
      *cp = 0xfffd;
      return p + 1;
    }
    std::uint32_t v = (std::uint32_t)(c0 & 0x1fu) << 6 | (std::uint32_t)(u8(p[1]) & 0x3fu);
    *cp = (v < 0x80u) ? 0xfffd : v;
    return p + 2;
  }
  if ((c0 >> 4) == 0x0eu) {
    if (u8(p[1]) < 0x80u || (u8(p[1]) >> 6) != 2u || u8(p[2]) < 0x80u ||
        (u8(p[2]) >> 6) != 2u) {
      *cp = 0xfffd;
      return p + 1;
    }
    std::uint32_t v = (std::uint32_t)(c0 & 0x0fu) << 12 |
        (std::uint32_t)(u8(p[1]) & 0x3fu) << 6 | (std::uint32_t)(u8(p[2]) & 0x3fu);
    if (v < 0x800u || (v >= 0xd800u && v <= 0xdfffu))
      v = 0xfffd;
    *cp = v;
    return p + 3;
  }
  if ((c0 >> 3) == 0x1eu) {
    if (u8(p[1]) < 0x80u || (u8(p[1]) >> 6) != 2u || u8(p[2]) < 0x80u ||
        (u8(p[2]) >> 6) != 2u || u8(p[3]) < 0x80u || (u8(p[3]) >> 6) != 2u) {
      *cp = 0xfffd;
      return p + 1;
    }
    std::uint32_t v = (std::uint32_t)(c0 & 0x07u) << 18 |
        (std::uint32_t)(u8(p[1]) & 0x3fu) << 12 | (std::uint32_t)(u8(p[2]) & 0x3fu) << 6 |
        (std::uint32_t)(u8(p[3]) & 0x3fu);
    if (v < 0x10000u || v > 0x10ffffu)
      v = 0xfffd;
    *cp = v;
    return p + 4;
  }
  *cp = 0xfffd;
  return p + 1;
}

/** @brief 单码位在等宽终端中占用的列数（东亚宽字一般为 2）。 */
inline int unicode_term_width(std::uint32_t ucs)
{
  if (ucs == 0)
    return 0;
  if (ucs < 32 || (ucs >= 0x7fu && ucs < 0xa0u))
    return 0;
  if ((ucs >= 0x0300u && ucs <= 0x036fu) || (ucs >= 0x200bu && ucs <= 0x200fu) ||
      ucs == 0xfeffu || (ucs >= 0x1160u && ucs <= 0x11ffu))
    return 0;
  int wide = (ucs >= 0x1100u &&
              (ucs <= 0x115fu || ucs == 0x2329u || ucs == 0x232au ||
                  (ucs >= 0x2e80u && ucs <= 0xa4cfu && ucs != 0x303fu) ||
                  (ucs >= 0xac00u && ucs <= 0xd7a3u) || (ucs >= 0xf900u && ucs <= 0xfaffu) ||
                  (ucs >= 0xfe10u && ucs <= 0xfe19u) || (ucs >= 0xfe30u && ucs <= 0xfe6fu) ||
                  (ucs >= 0xff00u && ucs <= 0xff60u) || (ucs >= 0xffe0u && ucs <= 0xffe6u) ||
                  (ucs >= 0x20000u && ucs <= 0x2fffdu) || (ucs >= 0x30000u && ucs <= 0x3fffdu)))
             ? 1
             : 0;
  return 1 + wide;
}

/**
 * @brief 整段 UTF-8 文本在终端中的显示宽度之和（用于垫格对齐）。
 * @note 全角/合字等极端排版仍可能与个别终端有 1 列偏差；数值列请用 ASCII + 定宽缓冲。
 */
inline int utf8_display_width(const char* s)
{
  if (!s || !*s)
    return 0;
  int total = 0;
  for (const char* p = s;;) {
    std::uint32_t cp = 0;
    const char* q = utf8_next_codepoint(p, &cp);
    if (q == p || cp == 0)
      break;
    p = q;
    total += unicode_term_width(cp);
  }
  return total;
}

/* -------------------------------------------------------------------------- */
/* 二、输出与定宽字段                                                        */
/* -------------------------------------------------------------------------- */

/** @brief 输出 n 个空格（n<=0 时不输出）。 */
inline void print_pad_spaces(int n)
{
  for (int i = 0; i < n; ++i)
    std::putchar(' ');
}

/**
 * @brief 输出一行由同一字符重复 columns 次，末尾换行（用于表格上下边框）。
 * @param columns 等宽终端列数，可用 pipe_grid_line_columns 计算与数据行一致。
 */
inline void print_rule_chars(char ch, int columns)
{
  for (int i = 0; i < columns; ++i)
    std::putchar(ch);
  std::putchar('\n');
}

/**
 * @brief 将字符串 s 右对齐写入宽度 w（按字节/列计，适用 ASCII 数字与英文）。
 * @param dst 缓冲至少 w+1 字节；超长时从左侧按 UTF-8 边界截断保留右侧 w 字节列（尽量少用中文）。
 */
inline void fmt_fixed_right_bytes(char* dst, int w, const char* s)
{
  int n = (int)std::strlen(s);
  if (n == 0) {
    std::memset(dst, ' ', (size_t)w);
    dst[w] = '\0';
    return;
  }
  if (n >= w) {
    int start = n - w;
    while (start > 0 && (s[start] & 0xc0) == 0x80)
      ++start;
    std::memcpy(dst, s + start, (size_t)(n - start));
    dst[n - start] = '\0';
    return;
  }
  int pad = w - n;
  std::memset(dst, ' ', (size_t)pad);
  std::memcpy(dst + pad, s, (size_t)n + 1);
}

/** @brief 科学计数法右对齐字段：sprintf 风格 %.*e。 */
inline void fmt_fixed_e(char* dst, int w, int prec, double x)
{
  char t[64];
  std::snprintf(t, sizeof(t), "%.*e", prec, x);
  fmt_fixed_right_bytes(dst, w, t);
}

/** @brief 定点小数右对齐字段：sprintf 风格 %.*f。 */
inline void fmt_fixed_f(char* dst, int w, int prec, double x)
{
  char t[64];
  std::snprintf(t, sizeof(t), "%.*f", prec, x);
  fmt_fixed_right_bytes(dst, w, t);
}

/* -------------------------------------------------------------------------- */
/* 三、通用 N 列竖线表（首列 UTF-8 标签 + 任意列 ASCII/定宽单元）            */
/* -------------------------------------------------------------------------- */

/** 行版式："| " + 首列(定显示宽 w_label) + " | " + 单元0 + " | " + … + " |\n"。 */
inline int pipe_grid_line_columns(int w_label_disp, const int* col_widths, int ncols)
{
  if (ncols <= 0)
    return 2 + w_label_disp + 2;
  int sumw = 0;
  for (int i = 0; i < ncols; ++i)
    sumw += col_widths[i];
  return 7 + w_label_disp + sumw + 3 * (ncols - 1);
}

/**
 * @brief 打印表头行：首列左对齐 UTF-8 文本并垫到显示宽度 w_label_disp，
 *        其余列使用已格式化的 ASCII 串 cell_texts[i]，宽度 col_widths[i]（右对齐）。
 * @param left_header_text 表头第一列文字（如 "Case / kernel"）。
 * @param w_label_disp 第一列在终端中占用的总显示列数（应不小于 utf8_display_width(left_header_text)）。
 * @param cell_texts 长度 ncols；每项为一句 C 字符串，建议仅 ASCII。
 * @param col_widths 长度 ncols；每项建议 1…255（内部对每个单元使用 256 字节临时缓冲）。
 * @param ncols 数据列数（竖线分隔的右侧列数）。
 */
inline void print_pipe_grid_header_line(
    const char* left_header_text,
    int w_label_disp,
    const char* const* cell_texts,
    const int* col_widths,
    int ncols)
{
  std::printf("| ");
  std::printf("%s", left_header_text);
  {
    int ph = w_label_disp - utf8_display_width(left_header_text);
    if (ph < 1)
      ph = 1;
    print_pad_spaces(ph);
  }
  for (int i = 0; i < ncols; ++i) {
    char buf[256];
    fmt_fixed_right_bytes(buf, col_widths[i], cell_texts[i] ? cell_texts[i] : "");
    std::printf(" | %s", buf);
  }
  std::printf(" |\n");
}

/**
 * @brief 打印数据行：首列为 UTF-8 标签（如中文说明），垫到 w_label_disp；
 *        右侧与 print_pipe_grid_header_line 相同。
 * @param label 可为中文；宽度用 utf8_display_width 计算后与 w_label_disp 比较决定补空格。
 */
inline void print_pipe_grid_row(
    int w_label_disp,
    const char* label,
    const char* const* cell_texts,
    const int* col_widths,
    int ncols)
{
  std::printf("| ");
  std::printf("%s", label ? label : "");
  int ld = utf8_display_width(label ? label : "");
  int pad = w_label_disp - ld;
  if (pad < 1)
    pad = 1;
  print_pad_spaces(pad);
  for (int i = 0; i < ncols; ++i) {
    char buf[256];
    fmt_fixed_right_bytes(buf, col_widths[i], cell_texts[i] ? cell_texts[i] : "");
    std::printf(" | %s", buf);
  }
  std::printf(" |\n");
}

/**
 * @brief 续行：第一列仅留白（占位 w_label_disp），用于同一逻辑块第二行例如只填右侧列。
 * @param w_label_disp 应与主表首列占位一致。
 */
inline void print_pipe_grid_row_continuation(
    int w_label_disp, const char* const* cell_texts, const int* col_widths, int ncols)
{
  std::printf("| ");
  print_pad_spaces(w_label_disp > 0 ? w_label_disp : 1);
  for (int i = 0; i < ncols; ++i) {
    char buf[256];
    fmt_fixed_right_bytes(buf, col_widths[i], cell_texts[i] ? cell_texts[i] : "");
    std::printf(" | %s", buf);
  }
  std::printf(" |\n");
}

/* -------------------------------------------------------------------------- */
/* 四、便捷：三数值列（内部转调通用网格）                                     */
/* -------------------------------------------------------------------------- */

/** 误差表三列数字域默认宽度（科学计数/定点）；单测可 `Table3NumCols{w0,w1,w2}` 覆盖。 */
inline constexpr int k_table3_num_default_w0 = 16;
inline constexpr int k_table3_num_default_w1 = 8;
inline constexpr int k_table3_num_default_w2 = 16;

/** 默认列宽对应 atan2 误差表；亦可 `Table3NumCols{a,b,c}` 传入自定义宽度。 */
struct Table3NumCols {
  int w0;
  int w1;
  int w2;

  constexpr Table3NumCols() noexcept
      : w0(k_table3_num_default_w0),
        w1(k_table3_num_default_w1),
        w2(k_table3_num_default_w2)
  {
  }

  constexpr Table3NumCols(int width0, int width1, int width2) noexcept
      : w0(width0), w1(width1), w2(width2)
  {
  }

  int line_columns(int w_label_disp) const noexcept
  {
    const int ws[3] = { w0, w1, w2 };
    return pipe_grid_line_columns(w_label_disp, ws, 3);
  }
};

inline void print_pipe_table_3num_header_line(
    const char* left_header,
    int w_label_disp,
    const char* num_hdr0,
    const char* num_hdr1,
    const char* num_hdr2,
    const Table3NumCols& col = Table3NumCols{})
{
  const char* hdrs[3] = { num_hdr0, num_hdr1, num_hdr2 };
  const int ws[3] = { col.w0, col.w1, col.w2 };
  print_pipe_grid_header_line(left_header, w_label_disp, hdrs, ws, 3);
}

inline void print_pipe_table_3num_row(
    int w_label_disp,
    const char* label,
    double v0,
    bool e0,
    int p0,
    double v1,
    bool e1,
    int p1,
    double v2,
    bool e2,
    int p2,
    const Table3NumCols& col = Table3NumCols{})
{
  char b1[64], b2[64], b3[64];
  if (e0)
    fmt_fixed_e(b1, col.w0, p0, v0);
  else
    fmt_fixed_f(b1, col.w0, p0, v0);
  if (e1)
    fmt_fixed_e(b2, col.w1, p1, v1);
  else
    fmt_fixed_f(b2, col.w1, p1, v1);
  if (e2)
    fmt_fixed_e(b3, col.w2, p2, v2);
  else
    fmt_fixed_f(b3, col.w2, p2, v2);

  const char* cells[3] = { b1, b2, b3 };
  const int ws[3] = { col.w0, col.w1, col.w2 };
  print_pipe_grid_row(w_label_disp, label, cells, ws, 3);
}

/* -------------------------------------------------------------------------- */
/* 五、便捷 / 性能两列                                                        */
/* -------------------------------------------------------------------------- */

/** 性能表「时间」「speedup」列默认宽度；单测可 `Perf2Cols{w_time,w_note}` 或改此常量后全项目生效。 */
inline constexpr int k_perf_default_w_time = 9;
inline constexpr int k_perf_default_w_note = 15;

/**
 * @brief 性能表「Kernel + Time + 说明」两数据列的列宽配置。
 *
 * 默认宽度由 k_perf_default_w_time / k_perf_default_w_note 决定。
 * 某测试需要更宽时间列或说明列时，在对应 .cpp 中例如：
 *   constexpr math_test::Perf2Cols k_perf_tbl{11, 22};
 * 再传入 print_perf_pipe_header / print_perf_pipe_row / print_perf_pipe_close。
 */
struct Perf2Cols {
  int w_time;
  int w_note;

  constexpr Perf2Cols() noexcept
      : w_time(k_perf_default_w_time), w_note(k_perf_default_w_note)
  {
  }

  constexpr Perf2Cols(int width_time_ms, int width_note) noexcept
      : w_time(width_time_ms), w_note(width_note)
  {
  }

  int line_columns(int w_kern_disp) const noexcept
  {
    const int ws[2] = { w_time, w_note };
    return pipe_grid_line_columns(w_kern_disp, ws, 2);
  }
};

/**
 * @brief 性能表标题行 + 上下横线中的上一道 '=' 与表头下的 '-'（与旧行为一致）。
 * @note 仅负责表头段落；底下的 '=' 用 print_perf_pipe_close。
 */
inline void print_perf_pipe_header(
    std::size_t n_pts,
    int iters,
    int w_kern_disp,
    const char* left_hdr = "Kernel",
    const char* time_hdr = "Time (ms)",
    const char* note_hdr = "Throughput / ratio",
    const Perf2Cols& perf = Perf2Cols{})
{
  const int line_cols = perf.line_columns(w_kern_disp);
  std::printf("\n=== Performance (n = %zu, iters = %d) ===\n", n_pts, iters);
  print_rule_chars('=', line_cols);
  const char* hdrs[2] = { time_hdr, note_hdr };
  const int ws[2] = { perf.w_time, perf.w_note };
  print_pipe_grid_header_line(left_hdr, w_kern_disp, hdrs, ws, 2);
  print_rule_chars('-', line_cols);
}

inline void print_perf_pipe_row(
    int w_kern_disp,
    const char* label,
    double ms,
    int ms_prec,
    const char* note,
    const Perf2Cols& perf = Perf2Cols{})
{
  char tt[64], nn[64];
  fmt_fixed_f(tt, perf.w_time, ms_prec, ms);
  fmt_fixed_right_bytes(nn, perf.w_note, note ? note : "");
  const char* cells[2] = { tt, nn };
  const int ws[2] = { perf.w_time, perf.w_note };
  print_pipe_grid_row(w_kern_disp, label, cells, ws, 2);
}

inline void print_perf_pipe_close(int w_kern_disp, const Perf2Cols& perf = Perf2Cols{})
{
  print_rule_chars('=', perf.line_columns(w_kern_disp));
}

} // namespace math_test

#endif // MATH_TEST_TERM_TABLE_HPP_
