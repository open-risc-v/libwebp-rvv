#!/usr/bin/env bash
# 真实包用例：libwebp 无损编码→解码必须零像素差，有损编码同时记录码流哈希。
set -euo pipefail
: "${STAGE:=run}" "${SIDE:=A}" "${BIN:=build/native}" "${DATA:=build/ab-data}" "${OUT:=build/ab-data/out}"
mkdir -p "$DATA" "$OUT"
case "$STAGE" in
  gen) ;;
  run)
    [ -x "$BIN/webp_harness" ] || { echo "缺少 $BIN/webp_harness" >&2; exit 1; }
    "$BIN/webp_harness" "$OUT/hash.txt" > /dev/null
    grep -q "px_diff=0" "$OUT/hash.txt" || { echo "无损回环有像素差" >&2; cat "$OUT/hash.txt"; exit 1; }
    ;;
  *) echo "未知 STAGE=$STAGE" >&2; exit 2 ;;
esac
