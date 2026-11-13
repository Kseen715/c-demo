/*
 * Matrix multiply C = A x B on the Rockchip NPU (RK3566, Orange Pi 3B)
 * against the CPU (4 Cortex-A55) on one core and on all of them.
 *
 * For each size (square, M = K = N) and each NPU type it prints:
 *   npu run   rknn_matmul_run alone, A and B already in NPU memory
 *   npu+copy  copy A and B in, hand them over, run, copy C out
 *   cpu 1/N   plain loops the compiler vectorizes, 1 and N threads
 *   check     NPU result against the CPU's: largest error relative to
 *             the largest value (float), or wrong elements (int)
 * Rates are in GOPS: 2*M*K*N operations per multiply.
 *
 * Usage: npu-matmul [SIZE...]   default 64 128 256 512 1024 2048
 * Sizes must be multiples of 32 (the NPU's alignment for K and N).
 */
#define _POSIX_C_SOURCE 199309L

#include <math.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <rknn_matmul_api.h>

/* Each timing repeats its work until this long has passed. */
#define MIN_MS 300.0

#define TIMED(ms, body)                                                    \
    do {                                                                   \
        int runs_ = 0;                                                     \
        double start_ = now_ms(), took_;                                   \
        do {                                                               \
            body;                                                          \
            runs_++;                                                       \
        } while ((took_ = now_ms() - start_) < MIN_MS);                    \
        (ms) = took_ / runs_;                                              \
    } while (0)

struct kind {
    const char *name;
    rknn_matmul_type type;
    int is_int;  /* int8 inputs, int32 output; else float16 in, float32 out */
};

/* The RK3566 rejects float16 output ("unsupported matmul dtype"); the
 * int4 and mixed types are for other chips or need quantization scales. */
static const struct kind KINDS[] = {
    {"fp16 x fp16 -> fp32", RKNN_FLOAT16_MM_FLOAT16_TO_FLOAT32, 0},
    {"int8 x int8 -> int32", RKNN_INT8_MM_INT8_TO_INT32, 1},
};

/* One size's inputs in every form, and the CPU's answers and times. */
struct data {
    int n;
    float *af, *bf, *cf;     /* float copies of the float16 values */
    __fp16 *ah, *bh;
    int8_t *ai, *bi;
    int32_t *ci;
    double cpu_fp_ms[2], cpu_int_ms[2];  /* 1 thread, all threads */
};

static double now_ms(void)
{
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static void cpu_fp(const float *A, const float *B, float *C, int n,
                   int threads)
{
#pragma omp parallel for num_threads(threads) schedule(static)
    for (int i = 0; i < n; i++) {
        float *c = C + (size_t)i * n;

        memset(c, 0, n * sizeof *c);
        for (int k = 0; k < n; k++) {
            const float a = A[(size_t)i * n + k], *b = B + (size_t)k * n;

            for (int j = 0; j < n; j++)
                c[j] += a * b[j];
        }
    }
}

static void cpu_int(const int8_t *A, const int8_t *B, int32_t *C, int n,
                    int threads)
{
#pragma omp parallel for num_threads(threads) schedule(static)
    for (int i = 0; i < n; i++) {
        int32_t *c = C + (size_t)i * n;

        memset(c, 0, n * sizeof *c);
        for (int k = 0; k < n; k++) {
            const int32_t a = A[(size_t)i * n + k];
            const int8_t *b = B + (size_t)k * n;

            for (int j = 0; j < n; j++)
                c[j] += a * b[j];
        }
    }
}

/* Random inputs; the float ones are rounded to float16 first, so the
 * CPU multiplies exactly what the NPU gets. */
static struct data *make_data(int n)
{
    size_t cells = (size_t)n * n;
    struct data *d = calloc(1, sizeof *d);

    d->n = n;
    d->af = malloc(cells * sizeof(float));
    d->bf = malloc(cells * sizeof(float));
    d->cf = malloc(cells * sizeof(float));
    d->ah = malloc(cells * sizeof(__fp16));
    d->bh = malloc(cells * sizeof(__fp16));
    d->ai = malloc(cells);
    d->bi = malloc(cells);
    d->ci = malloc(cells * sizeof(int32_t));
    for (size_t i = 0; i < cells; i++) {
        d->ah[i] = (__fp16)(rand() / (float)RAND_MAX * 2 - 1);
        d->bh[i] = (__fp16)(rand() / (float)RAND_MAX * 2 - 1);
        d->af[i] = d->ah[i];
        d->bf[i] = d->bh[i];
        d->ai[i] = (int8_t)(rand() % 256 - 128);
        d->bi[i] = (int8_t)(rand() % 256 - 128);
    }
    return d;
}

static void free_data(struct data *d)
{
    free(d->af), free(d->bf), free(d->cf), free(d->ah), free(d->bh);
    free(d->ai), free(d->bi), free(d->ci), free(d);
}

/* CPU times on 1 and on all threads; the last run leaves the answer. */
static void time_cpu(struct data *d, int threads)
{
    int counts[2] = {1, threads};

    for (int t = 0; t < 2; t++) {
        TIMED(d->cpu_fp_ms[t], cpu_fp(d->af, d->bf, d->cf, d->n, counts[t]));
        TIMED(d->cpu_int_ms[t],
              cpu_int(d->ai, d->bi, d->ci, d->n, counts[t]));
    }
}

/* The NPU's C against the CPU's: see the header. */
static double check(const struct kind *kind, const struct data *d,
                    const void *c)
{
    size_t cells = (size_t)d->n * d->n;
    double worst = 0, largest = 0;

    for (size_t i = 0; i < cells; i++) {
        if (kind->is_int) {
            worst += ((const int32_t *)c)[i] != d->ci[i];
            continue;
        }
        worst = fmax(worst, fabs(((const float *)c)[i] - d->cf[i]));
        largest = fmax(largest, fabs(d->cf[i]));
    }
    return kind->is_int ? worst : worst / largest;
}

struct npu {
    rknn_matmul_ctx ctx;
    rknn_matmul_io_attr io;
    rknn_tensor_mem *a, *b, *c;
};

static int set_io(struct npu *p)
{
    return rknn_matmul_set_io_mem(p->ctx, p->a, &p->io.A) ||
           rknn_matmul_set_io_mem(p->ctx, p->b, &p->io.B) ||
           rknn_matmul_set_io_mem(p->ctx, p->c, &p->io.C);
}

/* Copies A and B in and hands them over: normal (row-major) layouts,
 * which the runtime turns into the NPU's own. */
static int load(struct npu *p, const struct kind *kind, const struct data *d)
{
    size_t cells = (size_t)d->n * d->n;
    size_t bytes = cells * (kind->is_int ? 1 : sizeof(__fp16));

    memcpy(p->a->virt_addr, kind->is_int ? (void *)d->ai : (void *)d->ah,
           bytes);
    memcpy(p->b->virt_addr, kind->is_int ? (void *)d->bi : (void *)d->bh,
           bytes);
    return set_io(p);
}

/* Times one NPU type at one size; prints its row. */
static void bench_npu(const struct kind *kind, struct data *d, void *out)
{
    rknn_matmul_info info = {.M = d->n, .K = d->n, .N = d->n,
                             .type = kind->type};
    struct npu p = {0};
    double run_ms, copy_ms, ops = 2.0 * d->n * d->n * d->n;
    const double *cpu = kind->is_int ? d->cpu_int_ms : d->cpu_fp_ms;
    size_t c_bytes = (size_t)d->n * d->n * 4;  /* float32 or int32 */

    printf("%-21s %5d  ", kind->name, d->n);
    if (rknn_matmul_create(&p.ctx, &info, &p.io) != 0) {
        printf("not supported on this chip\n");
        return;
    }
    p.a = rknn_create_mem(p.ctx, p.io.A.size);
    p.b = rknn_create_mem(p.ctx, p.io.B.size);
    p.c = rknn_create_mem(p.ctx, p.io.C.size);
    if (!p.a || !p.b || !p.c || load(&p, kind, d) != 0 ||
        rknn_matmul_run(p.ctx) != 0) {
        printf("failed to run\n");
        goto out;
    }
    TIMED(run_ms, rknn_matmul_run(p.ctx));
    TIMED(copy_ms, {
        load(&p, kind, d);
        rknn_matmul_run(p.ctx);
        memcpy(out, p.c->virt_addr, c_bytes);
    });
    printf("%8.2f %7.1f  %8.2f %7.1f  %8.2f %6.1f  %8.2f %6.1f  ",
           run_ms, ops / run_ms / 1e6, copy_ms, ops / copy_ms / 1e6,
           cpu[0], ops / cpu[0] / 1e6, cpu[1], ops / cpu[1] / 1e6);
    if (kind->is_int)
        printf("%.0f wrong\n", check(kind, d, out));
    else
        printf("%.1e\n", check(kind, d, out));
out:
    if (p.a)
        rknn_destroy_mem(p.ctx, p.a);
    if (p.b)
        rknn_destroy_mem(p.ctx, p.b);
    if (p.c)
        rknn_destroy_mem(p.ctx, p.c);
    rknn_matmul_destroy(p.ctx);
}

int main(int argc, char **argv)
{
    static const int defaults[] = {64, 128, 256, 512, 1024, 2048};
    int count = argc > 1 ? argc - 1 : (int)(sizeof defaults / sizeof *defaults);
    int threads = omp_get_num_procs();
    char load[64] = "?";
    FILE *file = fopen("/proc/loadavg", "r");

    if (file && !fgets(load, sizeof load, file))
        strcpy(load, "?");
    if (file)
        fclose(file);
    printf("CPU threads: %d, load average: %s\n", threads, load);
    printf("%-21s %5s  %16s  %16s  %15s  %15s  %s\n", "type", "size",
           "npu run ms GOPS", "npu+copy ms GOPS", "cpu 1 ms GOPS",
           "cpu all ms GOPS", "check");
    srand(1);
    for (int s = 0; s < count; s++) {
        int n = argc > 1 ? atoi(argv[s + 1]) : defaults[s];

        if (n <= 0 || n % 32 != 0 || n > 10240) {
            fprintf(stderr, "%s: size must be a multiple of 32, at most "
                    "10240\n", argv[s + 1]);
            return 2;
        }
        struct data *d = make_data(n);
        void *out = malloc((size_t)n * n * sizeof(int32_t));

        time_cpu(d, threads);
        for (size_t k = 0; k < sizeof KINDS / sizeof *KINDS; k++)
            bench_npu(&KINDS[k], d, out);
        free(out);
        free_data(d);
    }
    return 0;
}
