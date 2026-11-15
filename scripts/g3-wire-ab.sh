#!/usr/bin/env bash
# G3 接线验证（libwebp）：off/on 双配置交叉构建 + qemu 跑同一探针。
# 判据：无损解码像素逐字节一致 + 有损码流字节一致（SSE 改变会改 RD 决策）。
set -euo pipefail
cd "$(dirname "$0")/.."
IMAGE=${IMAGE:-rvv-cross:2}

docker run --rm -v "$PWD":/work -w /work "${IMAGE}" bash -lc "
set -e
mkdir -p build/g3-ab/bin
for side in off on; do
  if [ \$side = on ]; then EXTRA='-DHAVE_RVV_KERNELS=1 -march=rv64gcv'; else EXTRA='-march=rv64gcv'; fi
  echo \"=== 构建 \$side（EXTRA_C_FLAGS: \$EXTRA）===\"
  HOST=riscv64-linux-gnu CC=riscv64-linux-gnu-gcc CXX=riscv64-linux-gnu-g++ EXTRA_C_FLAGS=\"\$EXTRA\" \
    ./cmake/pkg-build.sh /work/src /work/build/g3-ab/\$side/pkg > /tmp/webp-\$side.log 2>&1 \
    || { tail -40 /tmp/webp-\$side.log; exit 1; }
  riscv64-linux-gnu-gcc -march=rv64gcv -O2 -static \
    -I build/g3-ab/\$side/pkg/prefix/include test/pkg_webp.c \
    build/g3-ab/\$side/pkg/prefix/lib/libwebp.a build/g3-ab/\$side/pkg/prefix/lib/libsharpyuv.a -lm \
    -o build/g3-ab/bin/harness_\$side
  qemu-riscv64 -L /usr/riscv64-linux-gnu build/g3-ab/bin/harness_\$side | tee /tmp/whash-\$side.txt
done
echo '=== 比对 ==='
if diff -q /tmp/whash-off.txt /tmp/whash-on.txt >/dev/null; then
  echo 'PASS：开/关 RVV SSE 的像素与有损码流完全一致'
else
  echo 'FAIL：两侧输出不同'; diff /tmp/whash-off.txt /tmp/whash-on.txt; exit 1
fi
" 2>&1 | tail -14
