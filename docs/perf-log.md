# 性能记录（libwebp RVV 移植）

目标机：RISC-V RVV 板卡（8 核，VLEN=256），gcc 15.2.0。
输入：512×512 合成 PPM，`cwebp -q {75,90} -m 4`。B 侧 = `baseline`，A 侧 = `src`（仅本内核）。

## 逐轮记录

| 轮 | 口径 | 内核 | A(ms) | B(ms) | 收益 | 位精确 | 备注 |
|---|---|---|---|---|---|---|---|
| 1 | `-march=rv64gcv` 对 `rv64gc`（未控自动向量化） | `SSE16x16`、`SSE16x8` | q75 308 / q90 316 | 162 / 170 | **0.53x（回退）** | 通过（webp 逐字节相同） | 见下：回退来自 gcc 15 的 RVV 自动向量化，不是内核 |
| 1b | 同上，去掉内核开关（纯 `-march=rv64gcv`） | 无 | q75 309 | 162 | 0.52x | 通过 | 证明 `-march` 本身就会让编码器变慢 |
| 2 | 两侧都加 `-fno-tree-vectorize` | `SSE16x16`、`SSE16x8` | q75 **154** / q90 **162** | 162 / 171 | **1.05x / 1.06x** | 通过（逐字节相同） | 关掉自动向量化后内核实打实生效；RD 的 SSE 代价函数精确整数，码流不变 |

## 结论与口径

1. **gcc 15.2 在 `-march=rv64gcv` 下的自动向量化对 libwebp 是负收益**（0.52–0.53x）。
   凡是「A 档只加 `-march=rv64gcv`、不开内核」的构建，都不能算 RVV 收益，只能算编译器噪声；
   与 eigen 那一轮（自动向量化把耗时放大 35 倍）是同一类问题。
2. 本工程的 A/B 口径固定为：`-march=rv64gcv -O2 -fno-tree-vectorize -DHAVE_RVV_KERNELS=1`
   对 `-march=rv64gc -O2 -fno-tree-vectorize`。这样 A 侧增益只可能来自手写 intrinsics。
3. 只接 16 宽的 `SSE16x16` / `SSE16x8`；`SSE8x8` / `SSE4x4` 的车道数太少（8/4），
   `vsetvl` 与装载开销吃满收益，未启用。这与 openh264 的 4x4 结论一致。

## 复现

```bash
cmake -S baseline -B build/B -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS="-march=rv64gc -O2 -fno-tree-vectorize" \
      -DWEBP_BUILD_CWEBP=ON -DBUILD_SHARED_LIBS=OFF && make -C build/B -j8
cmake -S src -B build/A -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS="-march=rv64gcv -O2 -fno-tree-vectorize -DHAVE_RVV_KERNELS=1" \
      -DWEBP_BUILD_CWEBP=ON -DBUILD_SHARED_LIBS=OFF && make -C build/A -j8
for s in B A; do build/$s/cwebp -quiet -q 75 -m 4 -o /tmp/$s.webp test.ppm; done
cmp /tmp/B.webp /tmp/A.webp && echo bit-identical
```
