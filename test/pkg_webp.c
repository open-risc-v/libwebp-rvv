// libwebp A/B 探针：无损 RGB 编码 → 解码回环（像素必须逐字节一致），
// 再跑一遍 quality=75 的有损编码（码流字节也参与哈希，用于追回归）。
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <webp/decode.h>
#include <webp/encode.h>

#define W 133
#define H 101

static uint64_t fnv1a(const void *p, size_t n)
{
    const uint8_t *b = p;
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; }
    return h;
}

static void make_rgb(uint8_t *rgb)
{
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            uint8_t *p = rgb + ((size_t)y * W + x) * 3;
            unsigned int n = (unsigned int)(x * 2654435761u + y * 40503u + 11u);
            n ^= n >> 11;
            p[0] = (uint8_t)(((x * 2 + y) & 0xff) ^ (n & 0x3f));
            p[1] = (uint8_t)(((y * 3 + x / 2) & 0xff) ^ ((n >> 6) & 0x3f));
            p[2] = (uint8_t)(((x ^ y) & 0xff) ^ ((n >> 12) & 0x3f));
        }
}

int main(int argc, char **argv)
{
    const char *outp = argc > 1 ? argv[1] : NULL;
    size_t npx = (size_t)W * H * 3;
    uint8_t *rgb = malloc(npx);
    uint8_t *dec = malloc(npx);
    if (!rgb || !dec) return 2;
    make_rgb(rgb);

    /* 1) 无损：像素必须完全复原 */
    uint8_t *lossless = NULL;
    size_t ll_size = WebPEncodeLosslessRGB(rgb, W, H, W * 3, &lossless);
    if (ll_size == 0) { fprintf(stderr, "无损编码失败\n"); return 3; }

    int dw = 0, dh = 0;
    uint8_t *ok = WebPDecodeRGB(lossless, ll_size, &dw, &dh);
    if (!ok || dw != W || dh != H) { fprintf(stderr, "无损解码失败\n"); return 4; }
    size_t px_diff = 0;
    for (size_t i = 0; i < npx; i++) if (ok[i] != rgb[i]) px_diff++;

    /* 2) 有损：定参（quality/method/segments 全部固定），字节串参与哈希 */
    uint8_t *lossy = NULL;
    size_t ly_size = WebPEncodeRGB(rgb, W, H, W * 3, 75.0f, &lossy);
    if (ly_size == 0) { fprintf(stderr, "有损编码失败\n"); return 5; }
    uint8_t *ly_dec = WebPDecodeRGB(lossy, ly_size, &dw, &dh);
    if (!ly_dec) { fprintf(stderr, "有损解码失败\n"); return 6; }

    char line[256];
    snprintf(line, sizeof line,
             "size=%dx%d lossless=%zu fnv_ll=%016llx px_diff=%zu lossy=%zu fnv_ly=%016llx fnv_lypx=%016llx\n",
             W, H, ll_size, (unsigned long long)fnv1a(lossless, ll_size), px_diff,
             ly_size, (unsigned long long)fnv1a(lossy, ly_size),
             (unsigned long long)fnv1a(ly_dec, (size_t)dw * dh * 3));

    if (outp) {
        FILE *fp = fopen(outp, "wb");
        if (!fp) return 7;
        fwrite(line, 1, strlen(line), fp);
        fclose(fp);
    }
    fputs(line, stdout);

    WebPFree(lossless); WebPFree(lossy);
    WebPFree(ok); WebPFree(ly_dec);
    free(rgb); free(dec);
    return px_diff == 0 ? 0 : 8;
}
