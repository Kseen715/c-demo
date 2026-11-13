/*
 * Image classification on the Rockchip NPU (RK3566 on the Orange Pi 3B).
 *
 * Loads an .rknn model, feeds it one RGB image, prints the five most
 * likely labels, then times a number of runs.
 *
 * Usage: npu-classify MODEL IMAGE LABELS [RUNS]
 *   MODEL   .rknn file built for this chip (rk3566/rk3568)
 *   IMAGE   JPEG or PNG, the size of the model's input (224x224)
 *   LABELS  one "index:name" per line, as in Rockchip's labels.txt
 *   RUNS    timed runs after the first, default 100
 */
#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <rknn_api.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include <stb_image.h>

#define TOP 5
#define LABEL_LEN 128

/* Reads a whole file; the caller frees it. */
static void *read_file(const char *path, long *size)
{
    FILE *file = fopen(path, "rb");
    void *data = NULL;

    if (!file) {
        perror(path);
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) == 0 && (*size = ftell(file)) > 0 &&
        fseek(file, 0, SEEK_SET) == 0 && (data = malloc(*size)) &&
        fread(data, 1, *size, file) != (size_t)*size) {
        free(data);
        data = NULL;
    }
    if (!data)
        fprintf(stderr, "%s: cannot read\n", path);
    fclose(file);
    return data;
}

/* The label for a class index, from lines like "208:Labrador retriever". */
static void label_of(const char *path, unsigned index, char *label)
{
    FILE *file = fopen(path, "r");
    char line[LABEL_LEN + 16];

    snprintf(label, LABEL_LEN, "class %u", index);
    if (!file)
        return;
    while (fgets(line, sizeof line, file)) {
        char *name = strchr(line, ':');

        if (name && (unsigned)strtoul(line, NULL, 10) == index) {
            name[strcspn(name, "\r\n")] = '\0';
            snprintf(label, LABEL_LEN, "%s", name + 1);
            break;
        }
    }
    fclose(file);
}

/* Prints the TOP highest scores with their labels. */
static void print_top(const float *scores, unsigned count, const char *labels)
{
    unsigned best[TOP] = {0};
    char label[LABEL_LEN];

    for (unsigned rank = 0; rank < TOP && rank < count; rank++) {
        int pick = -1;

        for (unsigned i = 0; i < count; i++) {
            int taken = 0;

            for (unsigned r = 0; r < rank; r++)
                taken |= best[r] == i;
            if (!taken && (pick < 0 || scores[i] > scores[pick]))
                pick = i;
        }
        best[rank] = pick;
        label_of(labels, best[rank], label);
        printf("  %.3f  %s\n", scores[best[rank]], label);
    }
}

static double now_ms(void)
{
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

/* Height, width, channels of the model's first input. */
static int input_shape(rknn_context ctx, int *h, int *w, int *c)
{
    rknn_tensor_attr attr = {.index = 0};

    if (rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &attr, sizeof attr) != 0)
        return -1;
    if (attr.fmt == RKNN_TENSOR_NCHW) {
        *c = attr.dims[1], *h = attr.dims[2], *w = attr.dims[3];
    } else {
        *h = attr.dims[1], *w = attr.dims[2], *c = attr.dims[3];
    }
    printf("input %s: %dx%dx%d\n", attr.name, *w, *h, *c);
    return 0;
}

/* Sets the image as input and runs the model once. */
static int infer(rknn_context ctx, unsigned char *pixels, int size)
{
    rknn_input input = {
        .index = 0,
        .buf = pixels,
        .size = size,
        .type = RKNN_TENSOR_UINT8,
        .fmt = RKNN_TENSOR_NHWC,
    };

    if (rknn_inputs_set(ctx, 1, &input) != 0)
        return -1;
    return rknn_run(ctx, NULL);
}

/* Runs the image once and prints the top classes. */
static int classify(rknn_context ctx, unsigned char *pixels, int size,
                    const char *labels)
{
    rknn_output output = {.index = 0, .want_float = 1};

    if (infer(ctx, pixels, size) != 0 ||
        rknn_outputs_get(ctx, 1, &output, NULL) != 0) {
        fprintf(stderr, "inference failed\n");
        return -1;
    }
    print_top(output.buf, output.size / sizeof(float), labels);
    rknn_outputs_release(ctx, 1, &output);
    return 0;
}

/* Times RUNS inferences of the same image. */
static int benchmark(rknn_context ctx, unsigned char *pixels, int size,
                     int runs)
{
    double start = now_ms();

    for (int i = 0; i < runs; i++) {
        if (infer(ctx, pixels, size) != 0) {
            fprintf(stderr, "run %d failed\n", i);
            return -1;
        }
    }
    double each = (now_ms() - start) / runs;
    printf("%d runs: %.2f ms each, %.1f per second\n", runs, each,
           1e3 / each);
    return 0;
}

int main(int argc, char **argv)
{
    rknn_context ctx = 0;
    int h, w, c, img_w, img_h, status = 1;
    unsigned char *pixels = NULL;
    long model_size = 0;
    void *model;

    if (argc < 4) {
        fprintf(stderr, "usage: %s MODEL IMAGE LABELS [RUNS]\n", argv[0]);
        return 2;
    }
    int runs = argc > 4 ? atoi(argv[4]) : 100;
    if (!(model = read_file(argv[1], &model_size)))
        return 1;
    if (rknn_init(&ctx, model, model_size, 0, NULL) != 0) {
        fprintf(stderr, "%s: rknn_init failed\n", argv[1]);
        goto out;
    }
    if (input_shape(ctx, &h, &w, &c) != 0)
        goto out;
    pixels = stbi_load(argv[2], &img_w, &img_h, NULL, c);
    if (!pixels || img_w != w || img_h != h) {
        fprintf(stderr, "%s: need a %dx%d image\n", argv[2], w, h);
        goto out;
    }
    if (classify(ctx, pixels, w * h * c, argv[3]) == 0 &&
        (runs <= 0 || benchmark(ctx, pixels, w * h * c, runs) == 0))
        status = 0;
out:
    stbi_image_free(pixels);
    if (ctx)
        rknn_destroy(ctx);
    free(model);
    return status;
}
