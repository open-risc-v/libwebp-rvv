# 交叉编译基线（riscv64）— libwebp

## 口径
- 只编上游 baseline，`MARCH=rv64gc`，`rvv-cross:1`
- 静态库 `webp` + `sharpyuv`（工具/测试全关）。探针必须同时链 `sharpyuv`，
  否则 `picture_csp_enc.c` 的 `SharpYuv*` 符号未定义。
- 探针 `test/pkg_webp.c`：133×101 确定性 RGB → 无损编码/解码（要求零像素差）→
  有损 q=75 编码/解码（码流与解码像素一起哈希）

## 结果
| 侧 | 输出 |
|---|---|
| riscv64 | `size=133x101 lossless=30026 fnv_ll=69c27e48073e9609 px_diff=0 lossy=4706 fnv_ly=065927dd4d793a7d fnv_lypx=a8689aa48db264e2` |
| native | 同左 |

**全字段一致**：无损像素零差，有损码流字节也完全相同。仍未写 RVV 内核。

## 复现
```bash
cd ~/work/libwebp-rvv
./test/run_ab.sh && ./scripts/cross-baseline.sh
```
