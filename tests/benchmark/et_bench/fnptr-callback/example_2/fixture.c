/* ET-Bench fixture: fnptr-callback/example_2 */
/* Based on wrk's stats printing pattern */
/* fnptr: fmt, targets: format_time_us, format_metric */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>

typedef struct {
    uint64_t max;
    double mean_val;
    double variance;
    uint64_t *buckets;
    size_t bucket_count;
} stats;

static uint64_t stats_percentile(stats *s, double p) {
    size_t idx = (size_t)(p / 100.0 * (s->bucket_count - 1));
    if (idx >= s->bucket_count) idx = s->bucket_count - 1;
    return s->buckets[idx];
}

static double stats_mean(stats *s) { return s->mean_val; }
static double stats_stdev(stats *s, double mean) { return sqrt(s->variance); }
static double stats_within_stdev(stats *s, double mean, double sd, int n) {
    return 68.27; /* simplified */
}

static void print_stats_header(void) {
    printf("%-12s %10s %10s %10s %10s\n",
           "Name", "Mean", "Stdev", "Max", "Stdev%");
}

typedef struct {
    stats *latency;
    stats *requests;
} all_stats;

static all_stats statistics;

static char *format_time_us(long double n) {
    char *buf = malloc(32);
    if (n < 1000.0)
        snprintf(buf, 32, "%.2Lfus", n);
    else if (n < 1000000.0)
        snprintf(buf, 32, "%.2Lfms", n / 1000.0);
    else
        snprintf(buf, 32, "%.2Lfs", n / 1000000.0);
    return buf;
}

static char *format_metric(long double n) {
    char *buf = malloc(32);
    if (n < 1000.0)
        snprintf(buf, 32, "%.2Lf", n);
    else if (n < 1000000.0)
        snprintf(buf, 32, "%.2LfK", n / 1000.0);
    else
        snprintf(buf, 32, "%.2LfM", n / 1000000.0);
    return buf;
}

static void print_units(long double n, char *(*fmt)(long double), int width) {
    char *msg = fmt(n);
    int len = strlen(msg), pad = 2;

    if (len > 0 && isalpha((unsigned char)msg[len-1])) pad--;
    if (len > 1 && isalpha((unsigned char)msg[len-2])) pad--;
    width -= pad;

    printf("%*.*s%.*s", width, width, msg, pad, "  ");
    free(msg);
}

static void print_stats_latency(stats *stats) {
    long double percentiles[] = { 50.0, 75.0, 90.0, 99.0 };
    printf("  Latency Distribution\n");
    for (size_t i = 0; i < sizeof(percentiles) / sizeof(long double); i++) {
        long double p = percentiles[i];
        uint64_t n = stats_percentile(stats, p);
        printf("%7.0Lf%%", p);
        print_units(n, format_time_us, 10);
        printf("\n");
    }
}

static void print_stats(const char *name, stats *s, char *(*fmt)(long double)) {
    uint64_t max = s->max;
    long double mean  = stats_mean(s);
    long double stdev = stats_stdev(s, mean);

    printf("    %-10s", name);
    print_units(mean,  fmt, 8);
    print_units(stdev, fmt, 10);
    print_units(max,   fmt, 9);
    printf("%8.2Lf%%\n", stats_within_stdev(s, mean, stdev, 1));
}

int main(int argc, char **argv) {
    print_stats_header();
    print_stats("Latency", statistics.latency, format_time_us);
    print_stats("Req/Sec", statistics.requests, format_metric);
    return 0;
}
