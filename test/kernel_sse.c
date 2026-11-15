// libwebp SSE 内核级 A/B：进程内先用 C 复刻版逐值比对，再分别计时。
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BPS 32

typedef int (*metric_t)(const uint8_t *, const uint8_t *);
extern int (*VP8SSE16x16)(const uint8_t *, const uint8_t *);
extern int (*VP8SSE16x8)(const uint8_t *, const uint8_t *);
extern int (*VP8SSE8x8)(const uint8_t *, const uint8_t *);
extern int (*VP8SSE4x4)(const uint8_t *, const uint8_t *);

static int sse_c(const uint8_t *a, const uint8_t *b, int w, int h)
{
    int x, y, c = 0;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int d = a[x + y * BPS] - b[x + y * BPS];
            c += d * d;
        }
    return c;
}

static double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static uint32_t rng = 0x13579u;
static uint8_t rnd8(void)
{
    rng = rng * 1103515245u + 12345u;
    return (uint8_t)(rng >> 16);
}

static uint8_t a[16 * BPS], b[16 * BPS];
/* volatile sink：否则纯函数 + 丢结果会被 -O2 整段消除（实测 C 侧只剩计时器开销）。 */
static volatile int sink;

int main(int argc, char **argv)
{
    int reps = argc > 1 ? atoi(argv[1]) : 200000;
    int verbose = argc > 2 && argv[2][0] == 'v';
    int i, r, sz;
    static const int ws[4] = {16, 16, 8, 4}, hs[4] = {16, 8, 8, 4};
    static const char *names[4] = {"16x16", "16x8", "8x8", "4x4"};
    metric_t fns[4];

    /* libwebp 的 DSP init 是惰性的：必须先跑一次编码才会填函数指针表。 */
    {
        extern void VP8EncDspInit(void);
        VP8EncDspInit();
    }

    for (i = 0; i < 16 * BPS; i++) { a[i] = rnd8(); b[i] = rnd8(); }

    fns[0] = VP8SSE16x16; fns[1] = VP8SSE16x8; fns[2] = VP8SSE8x8; fns[3] = VP8SSE4x4;
    for (sz = 0; sz < 4; sz++) {
        int c = sse_c(a, b, ws[sz], hs[sz]);
        int v = fns[sz](a, b);
        if (c != v) { printf("FAIL size=%s c=%d rvv=%d\n", names[sz], c, v); return 1; }
        if (verbose) printf("  [%s] %d\n", names[sz], c);
    }
    for (sz = 0; sz < 4; sz++) {
        double tc = 0, tr = 0;
        for (r = 0; r < reps; r++) {
            double t0, t1;
            t0 = now_ns(); sink += sse_c(a, b, ws[sz], hs[sz]); t1 = now_ns(); tc += t1 - t0;
            t0 = now_ns(); sink += fns[sz](a, b); t1 = now_ns(); tr += t1 - t0;
        }
        printf("op=sse size=%s c=%.2f ns  rvv=%.2f ns  speedup=%.2fx\n",
               names[sz], tc / reps, tr / reps, tc / tr);
    }
    return 0;
}
