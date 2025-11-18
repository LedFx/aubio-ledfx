/* 
 * DP Tracker Benchmark Test
 * 
 * Comprehensive comparison of tempo tracking methods:
 * - Autocorrelation (baseline)
 * - Tempogram (multi-scale FFT)
 * - DP Tracker (dynamic programming)
 * 
 * Measures on real audio (test_bpm_changes.wav):
 * 1. BPM detection accuracy
 * 2. Response time to tempo changes
 * 3. Detection rate across sections
 */

#include <aubio.h>
#include <aubio_priv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define AUBIO_UNSTABLE 1
#include "tempo/tempo.h"

/* Test file path resolution */
#ifdef AUBIO_TEMPO_TEST_DIR
#define TEMPO_TEST_FILE(filename) AUBIO_TEMPO_TEST_DIR "/" filename
#else
#define TEMPO_TEST_FILE(filename) "tests/" filename
#endif

#define MAX_SECTIONS 10
#define TOLERANCE_BPM 10.0  /* Acceptable BPM error */

typedef struct {
    smpl_t start_time;
    smpl_t end_time;
    smpl_t bpm;
    int detected;
    smpl_t detected_time;
    smpl_t detected_bpm;
    smpl_t error;
    smpl_t response_time;
} tempo_section_t;

typedef struct {
    const char *method_name;
    int num_sections;
    tempo_section_t sections[MAX_SECTIONS];
    int sections_detected;
    smpl_t avg_error;
    smpl_t max_error;
    smpl_t avg_response_time;
} benchmark_results_t;

/* Initialize ground truth */
void init_ground_truth(benchmark_results_t *results) {
    results->num_sections = 6;
    
    results->sections[0] = (tempo_section_t){0.0, 10.0, 120.0, 0, 0, 0, 0, 0};
    results->sections[1] = (tempo_section_t){10.0, 20.0, 140.0, 0, 0, 0, 0, 0};
    results->sections[2] = (tempo_section_t){20.0, 30.0, 100.0, 0, 0, 0, 0, 0};
    results->sections[3] = (tempo_section_t){30.0, 40.0, 160.0, 0, 0, 0, 0, 0};
    results->sections[4] = (tempo_section_t){40.0, 50.0, 80.0, 0, 0, 0, 0, 0};
    results->sections[5] = (tempo_section_t){50.0, 60.0, 120.0, 0, 0, 0, 0, 0};
}

/* Calculate final metrics */
void calculate_metrics(benchmark_results_t *results) {
    smpl_t total_error = 0.0;
    smpl_t total_response = 0.0;
    int num_with_response = 0;
    
    results->max_error = 0.0;
    results->sections_detected = 0;
    
    for (int i = 0; i < results->num_sections; i++) {
        if (results->sections[i].detected) {
            results->sections_detected++;
            total_error += results->sections[i].error;
            
            if (results->sections[i].error > results->max_error) {
                results->max_error = results->sections[i].error;
            }
            
            if (results->sections[i].response_time > 0) {
                total_response += results->sections[i].response_time;
                num_with_response++;
            }
        }
    }
    
    results->avg_error = results->sections_detected > 0 ? 
        total_error / results->sections_detected : 0.0;
    results->avg_response_time = num_with_response > 0 ? 
        total_response / num_with_response : 0.0;
}

/* Run benchmark on audio file with specific method */
void run_benchmark(const char *audio_file, benchmark_results_t *results, 
                   int use_tempogram, int use_multiscale, int use_dp) {
    uint_t win_s = 1024;
    uint_t hop_s = 256;
    uint_t samplerate = 44100;
    
    /* Create tempo object */
    aubio_tempo_t *tempo = new_aubio_tempo("default", win_s, hop_s, samplerate);
    if (!tempo) {
        fprintf(stderr, "Error: Could not create tempo object\n");
        return;
    }
    
    /* Configure method */
    if (use_dp) {
        aubio_tempo_set_use_dp(tempo, 1);
    } else if (use_tempogram) {
        aubio_tempo_set_use_tempogram(tempo, 1);
        if (use_multiscale) {
            aubio_tempo_set_multiscale_tempogram(tempo, 1);
        }
    }
    
    /* Open audio file */
    aubio_source_t *source = new_aubio_source(audio_file, samplerate, hop_s);
    if (!source) {
        fprintf(stderr, "Error: Could not open %s\n", audio_file);
        del_aubio_tempo(tempo);
        return;
    }
    
    /* Process audio */
    fvec_t *input = new_fvec(hop_s);
    fvec_t *output = new_fvec(2);
    uint_t read = 0;
    uint_t total_frames = 0;
    
    do {
        aubio_source_do(source, input, &read);
        aubio_tempo_do(tempo, input, output);
        
        smpl_t current_time = (smpl_t)total_frames * hop_s / samplerate;
        smpl_t current_bpm = aubio_tempo_get_bpm(tempo);
        smpl_t confidence = aubio_tempo_get_confidence(tempo);
        
        /* Check each section */
        for (int i = 0; i < results->num_sections; i++) {
            if (current_time >= results->sections[i].start_time && 
                current_time < results->sections[i].end_time) {
                
                /* Check if this BPM matches the section */
                smpl_t error = fabs(current_bpm - results->sections[i].bpm);
                
                if (error <= TOLERANCE_BPM && confidence > 0.5) {
                    if (!results->sections[i].detected) {
                        /* First detection for this section */
                        results->sections[i].detected = 1;
                        results->sections[i].detected_time = current_time;
                        results->sections[i].detected_bpm = current_bpm;
                        results->sections[i].error = error;
                        results->sections[i].response_time = 
                            current_time - results->sections[i].start_time;
                    } else {
                        /* Update if this detection is more accurate */
                        if (error < results->sections[i].error) {
                            results->sections[i].detected_bpm = current_bpm;
                            results->sections[i].error = error;
                        }
                    }
                }
            }
        }
        
        total_frames++;
    } while (read == hop_s);
    
    /* Cleanup */
    del_fvec(input);
    del_fvec(output);
    del_aubio_source(source);
    del_aubio_tempo(tempo);
    
    /* Calculate final metrics */
    calculate_metrics(results);
}

/* Print benchmark results */
void print_results(benchmark_results_t *results) {
    fprintf(stderr, "\n=== %s ===\n", results->method_name);
    fprintf(stderr, "Detection Rate: %d/%d sections (%.1f%%)\n",
            results->sections_detected, results->num_sections,
            (float)results->sections_detected / results->num_sections * 100.0);
    fprintf(stderr, "Avg BPM Error: %.2f BPM\n", results->avg_error);
    fprintf(stderr, "Max BPM Error: %.2f BPM\n", results->max_error);
    fprintf(stderr, "Avg Response Time: %.2f seconds\n\n", results->avg_response_time);
    
    fprintf(stderr, "Section Details:\n");
    for (int i = 0; i < results->num_sections; i++) {
        fprintf(stderr, "  Section %d (%3.0f BPM): ", i+1, results->sections[i].bpm);
        if (results->sections[i].detected) {
            fprintf(stderr, "✓ Detected %.2f BPM (error: %.2f BPM, response: %.2fs)\n",
                    results->sections[i].detected_bpm, 
                    results->sections[i].error,
                    results->sections[i].response_time);
        } else {
            fprintf(stderr, "✗ Not detected\n");
        }
    }
}

int main(void) {
    fprintf(stderr, "╔══════════════════════════════════════════════════════════════╗\n");
    fprintf(stderr, "║        DP TRACKER BENCHMARK                                  ║\n");
    fprintf(stderr, "║  Comparing Autocorrelation vs Tempogram vs DP Tracker       ║\n");
    fprintf(stderr, "╚══════════════════════════════════════════════════════════════╝\n");
    
    const char *test_file = TEMPO_TEST_FILE("test_bpm_changes.wav");
    fprintf(stderr, "\nTest file: %s\n", test_file);
    
    /* Test 1: Autocorrelation (baseline) */
    fprintf(stderr, "\n>>> Running Autocorrelation benchmark...\n");
    benchmark_results_t autocorr_results = {0};
    autocorr_results.method_name = "Autocorrelation (Baseline)";
    init_ground_truth(&autocorr_results);
    run_benchmark(test_file, &autocorr_results, 0, 0, 0);
    print_results(&autocorr_results);
    
    /* Test 2: Multi-scale tempogram */
    fprintf(stderr, "\n>>> Running Multi-Scale Tempogram benchmark...\n");
    benchmark_results_t tempogram_results = {0};
    tempogram_results.method_name = "Multi-Scale Tempogram";
    init_ground_truth(&tempogram_results);
    run_benchmark(test_file, &tempogram_results, 1, 1, 0);
    print_results(&tempogram_results);
    
    /* Test 3: DP Tracker */
    fprintf(stderr, "\n>>> Running DP Tracker benchmark...\n");
    benchmark_results_t dp_results = {0};
    dp_results.method_name = "DP Tracker";
    init_ground_truth(&dp_results);
    run_benchmark(test_file, &dp_results, 0, 0, 1);
    print_results(&dp_results);
    
    /* Test 4: DP Tracker with Tempogram observation model */
    fprintf(stderr, "\n>>> Running DP + Tempogram benchmark...\n");
    benchmark_results_t dp_tempogram_results = {0};
    dp_tempogram_results.method_name = "DP Tracker + Tempogram";
    init_ground_truth(&dp_tempogram_results);
    run_benchmark(test_file, &dp_tempogram_results, 1, 1, 1);
    print_results(&dp_tempogram_results);
    
    /* Summary comparison */
    fprintf(stderr, "\n╔══════════════════════════════════════════════════════════════╗\n");
    fprintf(stderr, "║        PERFORMANCE COMPARISON                                ║\n");
    fprintf(stderr, "╚══════════════════════════════════════════════════════════════╝\n\n");
    
    fprintf(stderr, "Method                    Detection   Avg Error   Max Error   Avg Response\n");
    fprintf(stderr, "──────────────────────────────────────────────────────────────────────────\n");
    fprintf(stderr, "%-25s %d/6 (%.0f%%)   %.2f BPM    %.2f BPM    %.2f s\n",
            "Autocorrelation",
            autocorr_results.sections_detected,
            (float)autocorr_results.sections_detected / 6 * 100,
            autocorr_results.avg_error,
            autocorr_results.max_error,
            autocorr_results.avg_response_time);
    fprintf(stderr, "%-25s %d/6 (%.0f%%)   %.2f BPM    %.2f BPM    %.2f s\n",
            "Multi-Scale Tempogram",
            tempogram_results.sections_detected,
            (float)tempogram_results.sections_detected / 6 * 100,
            tempogram_results.avg_error,
            tempogram_results.max_error,
            tempogram_results.avg_response_time);
    fprintf(stderr, "%-25s %d/6 (%.0f%%)   %.2f BPM    %.2f BPM    %.2f s\n",
            "DP Tracker",
            dp_results.sections_detected,
            (float)dp_results.sections_detected / 6 * 100,
            dp_results.avg_error,
            dp_results.max_error,
            dp_results.avg_response_time);
    fprintf(stderr, "%-25s %d/6 (%.0f%%)   %.2f BPM    %.2f BPM    %.2f s\n",
            "DP + Tempogram",
            dp_tempogram_results.sections_detected,
            (float)dp_tempogram_results.sections_detected / 6 * 100,
            dp_tempogram_results.avg_error,
            dp_tempogram_results.max_error,
            dp_tempogram_results.avg_response_time);
    
    fprintf(stderr, "\n╔══════════════════════════════════════════════════════════════╗\n");
    fprintf(stderr, "║        DP TRACKER BENCHMARK COMPLETE                         ║\n");
    fprintf(stderr, "╚══════════════════════════════════════════════════════════════╝\n");
    
    return 0;
}
