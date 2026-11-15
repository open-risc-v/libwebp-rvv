# 进度 —— `media-libs/libwebp`

倒序追加，每次一个条目。门禁变更必须同时更新 `project.conf` 的 `GATE`，
否则主仓库看板会与事实脱节。

## 2026-11-13 · G0 planned

- 生成骨架（`rvv-port-project` Skill，模板来自 `libspeex-rvv`）
- 范围与通过标准写入 `docs/plan.md`
- 上游 URL：https://github.com/webmproject/libwebp；固定点：``
- 下一步（G1）：
  1. `./scripts/fetch-upstream.sh --dry-run` 确认 URL/SHA 后再拉取
  2. `src/` 初始化为 upstream 的副本（一次性提交，便于 diff）
  3. `./scripts/build.sh riscv64` + `./test/run_ab.sh` 跑通

## 2026-11-15 G3：编码器 SSE 代价函数 RVV 内核（弱正值）

- `src/dsp/enc_rvv.c`（由 enc.c 同 TU include）；`VP8EncDspInit()` 末尾覆盖指针。
- 内核（K3，200k 次）：16x16 1.81×、16x8 1.57×、8x8 1.24×、4x4 1.06×。
- **必须按宽度收窄**：四张表全接时整链 126.4→130.7 ms（0.968× 退化，4x4 拖累）；
  只接 16x16/16x8 时 127.12→126.01 ms = **1.009×**（5/5 同向）。
- 功能：qemu 无损 `fnv_ll=69c27e48073e9609`、有损 `fnv_ly=065927dd4d793a7d` 两侧一致。
- 坑：libwebp 的 DSP init 惰性（bench 要先调 VP8EncDspInit）；丢结果的纯函数计时会被
  -O2 整段删除（要用 volatile sink）；静态链 webp 需同时链接 libsharpyuv。
