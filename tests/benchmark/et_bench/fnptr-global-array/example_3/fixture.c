/*
 * et_bench fixture: fnptr-global-array/example_3
 *
 * Scenario: ASS (Advanced SubStation Alpha) subtitle parser field conversion
 * dispatch. A global array convert_func[] maps field types to converter
 * functions. ass_split_section() indexes into this array and calls through.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Types --- */

typedef enum {
    ASS_STR = 0,
    ASS_INT,
    ASS_FLT,
    ASS_COLOR,
    ASS_TIMESTAMP,
    ASS_ALGN,
    ASS_FIELD_TYPE_COUNT
} ASSFieldType;

typedef void (*ASSConvertFunc)(void *dst, const char *src, int len);

typedef struct {
    ASSFieldType type;
    size_t offset;
} ASSField;

typedef struct {
    const ASSField *fields;
    int num_fields;
} ASSSection;

typedef struct {
    int current_section;
    int field_number[32];
    int field_order[32][16];
} ASSSplitContext;

/* --- Target functions (6 unique callees) --- */

static void convert_str(void *dst, const char *src, int len)
{
    char *out = (char *)dst;
    memcpy(out, src, len);
    out[len] = '\0';
}

static void convert_int(void *dst, const char *src, int len)
{
    char buf[32];
    if (len >= (int)sizeof(buf))
        len = (int)sizeof(buf) - 1;
    memcpy(buf, src, len);
    buf[len] = '\0';
    *(int *)dst = atoi(buf);
}

static void convert_flt(void *dst, const char *src, int len)
{
    char buf[32];
    if (len >= (int)sizeof(buf))
        len = (int)sizeof(buf) - 1;
    memcpy(buf, src, len);
    buf[len] = '\0';
    *(float *)dst = (float)atof(buf);
}

static void convert_color(void *dst, const char *src, int len)
{
    char buf[16];
    if (len >= (int)sizeof(buf))
        len = (int)sizeof(buf) - 1;
    memcpy(buf, src, len);
    buf[len] = '\0';
    *(unsigned int *)dst = (unsigned int)strtoul(buf, NULL, 16);
}

static void convert_timestamp(void *dst, const char *src, int len)
{
    char buf[32];
    int h = 0, m = 0, s = 0, cs = 0;
    if (len >= (int)sizeof(buf))
        len = (int)sizeof(buf) - 1;
    memcpy(buf, src, len);
    buf[len] = '\0';
    sscanf(buf, "%d:%d:%d.%d", &h, &m, &s, &cs);
    *(int *)dst = h * 360000 + m * 6000 + s * 100 + cs;
}

static void convert_alignment(void *dst, const char *src, int len)
{
    char buf[8];
    if (len >= (int)sizeof(buf))
        len = (int)sizeof(buf) - 1;
    memcpy(buf, src, len);
    buf[len] = '\0';
    *(int *)dst = atoi(buf) & 0xF;
}

/* --- Global function pointer array --- */

static const ASSConvertFunc convert_func[ASS_FIELD_TYPE_COUNT] = {
    convert_str,
    convert_int,
    convert_flt,
    convert_color,
    convert_timestamp,
    convert_alignment,
};

/* --- Section definitions (static data) --- */

static const ASSField style_fields[] = {
    { ASS_STR,  0 },
    { ASS_INT,  0 },
    { ASS_FLT,  0 },
    { ASS_COLOR, 0 },
    { ASS_TIMESTAMP, 0 },
    { ASS_ALGN, 0 },
};

static const ASSSection ass_sections[] = {
    { style_fields, 6 },
};

/* --- Caller: indexes into convert_func[field_type] and calls through --- */

static int ass_split_section(ASSSplitContext *ctx, const char *buf,
                             void *struct_ptr)
{
    const ASSSection *section = &ass_sections[ctx->current_section];
    int *number = &ctx->field_number[ctx->current_section];
    int *order = ctx->field_order[ctx->current_section];
    int i, len;
    void *ptr;
    int is_last;

    while (buf && *buf) {
        len = (int)strcspn(buf, "\r\n");
        if (len == 0) {
            buf++;
            continue;
        }
        buf += len;
        if (*buf == '\r') buf++;
        if (*buf == '\n') buf++;

        for (i = 0; (*buf != '\r' && *buf != '\n' && *buf != '\0') && i < *number; i++) {
            is_last = (i == *number - 1);
            while (*buf == ' ' || *buf == '\t') buf++;
            len = (int)strcspn(buf, is_last ? "\r\n" : ",\r\n");
            if (order[i] >= 0 && order[i] < section->num_fields) {
                int field_type = section->fields[order[i]].type;
                ptr = (char *)struct_ptr + section->fields[order[i]].offset;
                convert_func[field_type](ptr, buf, len);
            }
            buf += len;
            if (!is_last && *buf) buf++;
            while (*buf == ' ' || *buf == '\t') buf++;
        }
    }
    return 0;
}
