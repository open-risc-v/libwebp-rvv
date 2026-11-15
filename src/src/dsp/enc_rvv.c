/* libwebp 编码器 SSE 内核的 RVV 实现（4x4 / 8x8 / 16x8 / 16x16）。
 *
 * 被 src/dsp/enc.c 以同一 TU include（`-DHAVE_RVV_KERNELS=1` 时），因此
 * 直接沿用 enc.c 已有的 BPS（=32，编码器块步长）与 dsp.h 类型。
 *
 * SSE 是 RD 循环里最热的代价函数：每个 4x4/16x16 候选都要和参考块比一次，
 * 上游 SSE2/NEON 都有同名实现（enc_sse2.c / enc_neon.c 里的 SSE*_SSE2/NEON）。
 * 输入是 u8，输出 int（块内平方差之和）。
 */
#include <riscv_vector.h>

static int SSE_RVV(const uint8_t* WEBP_RESTRICT a,
                   const uint8_t* WEBP_RESTRICT b,
                   int w, int h) {
  size_t vl = (size_t)w;   /* 宽度就是 vl：4/8/16，不假设 VLEN/MVL */
  vuint32m4_t acc = __riscv_vmv_v_x_u32m4(0, vl);
  int y;

  for (y = 0; y < h; ++y) {
    vuint8m1_t va = __riscv_vle8_v_u8m1(a + y * BPS, vl);
    vuint8m1_t vb = __riscv_vle8_v_u8m1(b + y * BPS, vl);
    /* |d|^2 == d^2：先取绝对差（GCC 13/14 无 vabd intrinsic），再平方 */
    vuint8m1_t hi = __riscv_vmaxu_vv_u8m1(va, vb, vl);
    vuint8m1_t lo = __riscv_vminu_vv_u8m1(va, vb, vl);
    vuint8m1_t vd = __riscv_vsub_vv_u8m1(hi, lo, vl);
    vuint16m2_t v16 = __riscv_vzext_vf2_u16m2(vd, vl);
    vuint32m4_t sq = __riscv_vwmulu_vv_u32m4(v16, v16, vl);
    acc = __riscv_vadd_vv_u32m4(acc, sq, vl);
  }
  {
    /* 归约命名 "源在前"：vredsum_vs_<源 LMUL>_<目标> */
    vuint32m1_t zero = __riscv_vmv_v_x_u32m1(0, vl);
    vuint32m1_t s = __riscv_vredsum_vs_u32m4_u32m1(acc, zero, vl);
    return (int)__riscv_vmv_x_s_u32m1_u32(s);
  }
}

static int SSE16x16_RVV(const uint8_t* WEBP_RESTRICT a,
                        const uint8_t* WEBP_RESTRICT b) {
  return SSE_RVV(a, b, 16, 16);
}
static int SSE16x8_RVV(const uint8_t* WEBP_RESTRICT a,
                       const uint8_t* WEBP_RESTRICT b) {
  return SSE_RVV(a, b, 16, 8);
}
static int SSE8x8_RVV(const uint8_t* WEBP_RESTRICT a,
                      const uint8_t* WEBP_RESTRICT b) {
  return SSE_RVV(a, b, 8, 8);
}
static int SSE4x4_RVV(const uint8_t* WEBP_RESTRICT a,
                      const uint8_t* WEBP_RESTRICT b) {
  return SSE_RVV(a, b, 4, 4);
}

/* 只接 16 宽的块。实测（K3）：
     16x16 1.81×  16x8 1.57×  8x8 1.24×  4x4 1.06×
   4x4 是 RD 里调用次数最多的一档，但它只有 4 个 lane、vsetvl+load 开销吃满收益，
   全开时整链反而从 126.5 ms 退化到 130.7 ms（0.968×）。所以按宽度收窄，
   VP8SSE8x8 / VP8SSE4x4 保留 C —— 与 openh264 的 4x4/4x8 处理同一结论。 */
static void VP8EncDspInitRVV(void) {
  VP8SSE16x16 = SSE16x16_RVV;
  VP8SSE16x8 = SSE16x8_RVV;
}
