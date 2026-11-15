// 有损编码整链基准：同一张确定性 RGB 图，WebPEncodeRGB(quality=75, method=4)。
// SSE 只在有损 RD 里参与，所以这里只测有损，避免无损路径稀释信号。
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <webp/encode.h>

#define W 512
#define H 384

static uint64_t fnv1a(const void *p, size_t n)
{
    const uint8_t *b = p;
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; }
    return h;
}

static double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

int main(int argc, char **argv)
{
    int reps = argc > 1 ? atoi(argv[1]) : 3;
    float quality = argc > 2 ? (float)atof(argv[2]) : 75.0f;
    uint8_t *rgb = malloc((size_t)W * H * 3);
    uint8_t *out = NULL;
    int i, r;
    double best = 1e18;

    for (i = 0; i < W * H; i++) {
        /* 块状纹理 + 渐变，制造足够多的 RD 决策 */
        int x = i % W, y = i / W;
        rgb[i * 3 + 0] = (uint8_t)((x * 3 + (y / 4) * 7) & 0xff);
        rgb[i * 3 + 1] = (uint8_t)((y * 2 + (x / 8) * 5) & 0xff);
        rgb[i * 3 + 2] = (uint8_t)(((x + y) * 3 + ((i >> 6) & 0x3f) * 11) & 0xff);
    }

    size_t n = WebPEncodeRGB(rgb, W, H, W * 3, quality, &out);
    if (n == 0) { printf("FAIL encode\n"); return 1; }
    printf("size=%dx%d q=%.0f bytes=%zu fnv=%016llx",
           W, H, quality, n, (unsigned long long)fnv1a(out, n));

    for (r = 0; r < reps; r++) {
        double t0, t1;
        size_t m;
        WebPFree(out);
        out = NULL;
        t0 = now_ns();
        m = WebPEncodeRGB(rgb, W, H, W * 3, quality, &out);
        t1 = now_ns();
        if (m != n) { printf(" FAIL unstable\n"); return 1; }
        if (t1 - t0 < best) best = t1 - t0;
    }
    printf(" best_us=%.0f\n", best / 1000.0);
    if (out) WebPFree(out);
    free(rgb);
    return 0;
}

/* WebPEncodeRGB 声明在 encode.h 里；WebPFree 在 decode.h，这里显式声明避免多 include */
void WebPFree(void *ptr);
