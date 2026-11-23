# libwebp G3：编码器 SSE 代价函数的 RVV 内核

- 日期：2026-11-15
- 上游：`webmproject/libwebp` @ `b4254dca32a4fc3b22bbdcc10b1eac2176a9d238`
- 构建：CMake 静态（`BUILD_SHARED_LIBS=OFF`），两侧同为 `-march=rv64gcv`，
  唯一变量 `-DHAVE_RVV_KERNELS=1`
- 目标板：SpacemiT K3 / <distro>，`Linux k3 6.18.3 riscv64`，RVV 1.0

## 1. 选型理由

`VP8SSE16x16/16x8/8x8/4x4` 是有损编码 RD 循环里的代价函数，上游
`enc_sse2.c`/`enc_neon.c` 都有同名实现，接口是 `int (const uint8_t*, const uint8_t*)`
配 `BPS=32` 固定步长 —— 属于 SAD/SSD 家族，前四个包（libvpx / openh264 / x264 /
libaom）的经验可直接复用。

## 2. 接线

- 新增 `src/dsp/enc_rvv.c`，由 `src/dsp/enc.c` 同 TU `#include`（避免改 CMake 源列表）。
- 全局函数指针 `VP8SSE16x16` 等由 `VP8EncDspInit()` 赋成 C 版，我们在其末尾
  （NEON 块之后、`assert` 之前）调 `VP8EncDspInitRVV()` 覆盖。
- **DSP init 是惰性的**：只在 `enc/webp_enc.c:242` 第一次编码时触发，所以内核级
  bench 必须先手动调 `VP8EncDspInit()`，否则读到的指针是 NULL（会直接段错误）。

## 3. 内核设计

每行 `vle8` 取 16/8/4 个字节 → `vmaxu/vminu/vsub` 得绝对差（GCC 13/14 无 `vabd`）
→ `vzext` 抬到 u16 → `vwmulu` 平方进 u32m4 累加 → 整块末尾 `vredsum` 一次归约。
vl 直接取块宽，不假设 VLEN/MVL。

## 4. 功能验证（qemu，逐字节）

`./scripts/g3-wire-ab.sh`：

```
off: lossless=30026 fnv_ll=69c27e48073e9609 px_diff=0
     lossy=4706 fnv_ly=065927dd4d793a7d fnv_lypx=a8689aa48db264e2
on : 同上一字不差
PASS
```

无损像素与有损码流都与 G2 基线一致（SSE 是 RD 决策输入，位精确才敢说"只是加速"）。

## 5. 真机性能（K3）

### 5.1 内核级（200k 次/档 ×2）

| 尺寸 | C | RVV | 比值 |
|---|---|---|---|
| 16x16 | 337.8–339.8 ns | 187.3–187.9 ns | **1.80–1.81×** |
| 16x8 | 192.1–193.9 ns | 122.1–123.7 ns | **1.57×** |
| 8x8 | 150.4–152.5 ns | 122.0–123.0 ns | **1.23–1.24×** |
| 4x4 | 99.5 ns | 93.6 ns | 1.06× |

### 5.2 整链（512×384 q=75 method=4 有损编码，5 轮交替）

| 配置 | off | on | 比值 |
|---|---|---|---|
| 四张表全接 | 126.4–127.5 ms | 130.5–131.2 ms | **0.968×（退化）** |
| 只接 16x16 / 16x8 | 126.5–127.8 ms | 125.5–126.4 ms | **≈1.009×**（均值 127.12 → 126.01，5/5 同向） |

码流两侧一致：`bytes=34188 fnv=a69caad683943683`。

## 6. 结论与归因

- **必须按宽度收窄派发**：4x4 是 RD 里调用次数最多的一档，但它只有 4 个 lane，
  `vsetvl + vle` 的固定开销把 1.06× 的微薄收益全部吃掉，全开时整链 -3.2%。
  这与 openh264 的 4x4/4x8 保留 C 是同一结论，已经是第二次验证。
- 收窄后整链 +0.9%。内核倍数只有 1.8×（对比 x264 SAD 的 6×），功率有限：
  SSE 在 libwebp 的 RD 里不是最重的一项（重的是 VP8 变换/量化与熵编码）。
- 因此本包的结论是"**弱正值，可保留但收益有限**"，与 libde265 IDCT 的 +0.4% 同类。

## 7. 测量坑（重要）

内核 bench 最初写成 `(void)sse_c(...)` 丢弃结果，`-O2` 把整个 C 循环删掉，四个尺寸
都测出恒定的 45.6 ns（只剩两次 `clock_gettime` 的开销），RVV 侧因为是函数指针调用
才没被删 —— 直接读出 0.24× 的假负优化。**纯函数计时必须用 `volatile` sink 接住返回值。**

## 8. 尚未做

- `VP8TDisto4x4/16x16`（变换域加权失真，RD 里另一个代价函数）、`VP8FTransform`、
  `VP8EncQuantizeBlock` 未做。
- 无损路径（`SubtractGreen` / `TransformColorInverse` / 预测器）完全未碰。
- 无运行时 CPU 探测。

## 9. 复现

- 功能：`./scripts/g3-wire-ab.sh`
- 内核：`/root/rvv-k/libwebp/kernel_sse <reps> [v]`
- 整链：`/root/rvv-k/libwebp/bench_off|bench_on <reps> [quality]`
