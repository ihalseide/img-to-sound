/* IMG_TO_SOUND : convert an image to music.
 *
 * Copyright (C) 2022 Izak Nathanael Halseide
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// Audio format: signed 8-bit PWM.

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define DEFAULT_SAMPLE_RATE 48000
#define DEFAULT_PX_PER_MIN 240

enum {
    WAVE_SINE,
    WAVE_SAW,
    WAVE_TRIANGLE,
    WAVE_SQUARE,
};

// convert piano key number to frequency
float key_to_frequency(int n)
{
    float p = (n - 49.0) / 12.0;
    return 440.0 * powf(2, p);
}

// sample a sawtooth wave
// t = time
// f = frequency
float saw(float t, float f)
{
    float x = t * f;
    return x - floorf(x) - 0.5;
}

// sample sine wave
float sine(float t, float f)
{
    return sin(2*f*t);
}

// sample triangle wave
float triangle(float t, float f)
{
    return 2 * fabs(saw(t, f)) - 0.5;
}

// sample square wave
float square(float t, float f)
{
    float x = f*t;
    return 4*floorf(x) - 2*floorf(2*x) + 1;
}

// convert an RGB color value to an amplitude between 0 and 1.0
// by dividing the biggest channel's value by 255
float color_to_amplitude(unsigned char r, unsigned char g, unsigned char b)
{
    unsigned char x;
    x = (r > g)? r : g;
    x = (x > b)? x : b;
    return (float)x / 255.0;
}

// choose a waveform based on the color
// <dominant color → waveform>
//     red → sine
//     blue → triangle
//     green → square
//     otherwise → sawtooth
int color_to_wave(unsigned char r, unsigned char g, unsigned char b)
{
    if (r > g && r > b) return WAVE_SINE;
    if (g > r && g > b) return WAVE_SQUARE;
    if (b > r && b > g) return WAVE_TRIANGLE;
    return WAVE_SAW;
}

// o = output buffer
// w = waveform kind
// t = time
// f = frequency
// a = amplitude
// r = sample Rate
// s = number of samples
void generate_samples(
        float *o, int w, float t0, float f, float a,
        unsigned int r, unsigned int s)
{
    float dt = 1.0 / r;
    for (int i = 0; i < s; i++)
    {
        float t = t0 + dt * i;
        float x;
        switch (w)
        {
            case WAVE_SINE:
                x = sine(t, f);
                break;
            case WAVE_TRIANGLE:
                x = triangle(t, f);
                break;
            case WAVE_SQUARE:
                x = square(t, f);
                break;
            case WAVE_SAW:
                x = saw(t, f);
                break;
            default:
                assert(0 && "invalid wave kind");
                break;
        }
        x *= a;
        o[i] = x;
    }
}

int process_check(
        char *in_filename, char *out_filename, unsigned int rate,
        unsigned int spp, char v)
{
    if (strcmp(in_filename, out_filename) == 0)
    {
        if (v) fprintf(stderr, "input filename and output filename must be different\n");
        return 1;
    }
    if (!in_filename) 
    {
        if (v) fprintf(stderr, "invalid input filename\n");
        return 1;
    }
    if (!out_filename)
    {
        if (v) fprintf(stderr, "invalid output filename\n");
        return 1;
    }
    if (!rate)
    {
        if (v) fprintf(stderr, "invalid rate\n");
        return 1;
    }
    if (!spp)
    {
        if (v) fprintf(stderr, "invalid samples per pixel\n");
        return 1;
    }
    return 0;
}

// in_filename = name of input image file
// out_filename = name of output file to create
// r = sample rate
// tpp = time per pixel
// ox = offset x
// oy = offset y
// v = verbose flag
// returns non-zero if there is an error
int process(
        char *in_filename, char *out_filename, unsigned int rate,
        unsigned int spp, unsigned int ox, unsigned int oy, char v)
{
    if (process_check(in_filename, out_filename, rate, spp, v))
        return 1;

    const float tpp = (float)spp / rate; // time per pixel
    if (v)
        fprintf(stderr, "time per pixel: %fs\n", tpp);

    // load image
    // width, height, num. of channels
    int w, h, n;
    unsigned char *data = stbi_load(in_filename, &w, &h, &n, 3);
    if (!data)
    {
        fprintf(stderr, "could not load input file\n");
        return 1;
    }

    if (v)
        fprintf(stderr, "input image size is %dx%d\n", w, h);

    // check starting offsets
    if (w <= ox)
    {
        fprintf(stderr, "start x (%d) is larger than the image width (%d)\n", ox, w);
        return 1;
    }
    if (h <= oy)
    {
        fprintf(stderr, "start y (%d) is larger than the image height (%d)\n", oy, h);
        return 1;
    }

    if (v) fprintf(stderr, "output length will be %fs long\n", w * tpp);

    // audio output file
    FILE *out = fopen(out_filename, "w+");
    if (!out)
    {
        perror("fopen");
        stbi_image_free(data);
        return 1;
    }

    float t = 0; // time
    // process each pixel
    const int num_keys = 88;
    const int max_notes = 12; // maximum notes to play at once (inclusive)
    const int end_y = (h - oy < num_keys)? h : (oy + num_keys);
    for (int x = ox; x < w; x++)
    {
        float *col_buffer = calloc(spp, sizeof(float));
        int notes = 0;
        for (int y = oy; y < end_y; y++)
        {
            if (notes > max_notes)
            {
                fprintf(
                        stderr,
                        "note: maximum number of notes (%d) placed at one time at x = %d\n",
                        max_notes, x);
                break;
            }
            int i = (y * w + x) * n;
            char r = data[i + 0];
            char g = data[i + 1];
            char b = data[i + 2];
            if (!r && !g && !b)
            {
                // silence
                continue;
            }
            // add a note
            notes++;
            int key = num_keys - (y - oy);
            float f = key_to_frequency(key);
            //printf("f: %f\n", f);
            float a = color_to_amplitude(r, g, b) / max_notes;
            int w = color_to_wave(r, g, b);
            //printf("w:%d\n",w);
            float *place_buffer = malloc(spp * sizeof(float));
            generate_samples(place_buffer, w, t, f, a, rate, spp);
            for (int i = 0; i < spp; i++)
            {
                col_buffer[i] += place_buffer[i];
            }
            free(place_buffer);
        }
        // no notes were added for this x/time
        if (!notes)
        {
            for (int i = 0; i < spp; i++)
            {
                col_buffer[i] = 0;
            }
        }
        // write current range to file
        int8_t *int_buffer = malloc(spp * sizeof(*int_buffer));
        for (int i = 0; i < spp; i++)
        {
            int_buffer[i] = (int8_t)(col_buffer[i] * INT8_MAX);
        }
        fwrite(int_buffer, sizeof(*int_buffer), spp, out);
        free(int_buffer);
        free(col_buffer);
        t += tpp;
    }

    // cleanup
    fclose(out);
    stbi_image_free(data);
    return 0;
}

void print_usage(FILE *fp, char *program)
{
    fprintf(fp,
        "usage:\n"
        "    %s [OPTIONS] file-in file-out\n"
        "Options:\n"
        "    -h         show the help mesage\n"
        "    -v         print out information (verbose)\n"
        "    -o file    output audio to file\n"
        "    -r rate    set the sample rate in Hertz (default is %d)\n"
        "    -p ppm     set the pixels per minute, also know as tempo, (default is %d)\n"
        "    -x offset  ignore the first <offset> X columns of the image (default is 0)\n"
        "    -y offset  ignore the first <offset> Y rows of the image (default is 0)\n"
        "NOTE: All options that take arguments take integer arguments.\n",
        program, DEFAULT_SAMPLE_RATE, DEFAULT_PX_PER_MIN);
}

// Calculate samples per pixel
// sr = sample rate
// ppm = pixels/beats per minute
int calc_spp(unsigned int sr, unsigned int ppm)
{
    assert(sr != 0);
    assert(ppm != 0);
    float pps = (float)ppm / 60.0; // pixel per second
    return sr / pps;
}

int main(int argc, char **argv)
{
    char *in_filename = NULL;
    char *out_filename = NULL;
    char v = 0; // verbose flag
    unsigned int x = 0;
    unsigned int y = 0;
    unsigned int sr = DEFAULT_SAMPLE_RATE; // default sample rate
    unsigned int ppm = DEFAULT_PX_PER_MIN;  // default pixels per minute
    // parse options
    int opt;
    char *prog = (argc && argv)? argv[0] : NULL;
    while ((opt = getopt(argc, argv, "hvr:p:x:y:o:")) != -1)
    {
        switch (opt)
        {
            case 'h':
                // help
                fprintf(stdout, "Converts an image file input to an audio file output.\n");
                print_usage(stdout, prog);
                return 0;
            case 'v':
                // verbose
                v = 1; 
                fprintf(stderr, "verbose\n");
                break;
            case 'r':
                // sample rate
                sr = atoi(optarg);
                if (sr <= 0)
                {
                    fprintf(stderr, "%s: error: -r argument %d must be greater than zero\n", prog, sr);
                    return 1;
                }
                break;
            case 'p':
                // pixels per minute
                ppm = atoi(optarg);
                if (ppm <= 0)
                {
                    fprintf(stderr, "%s: error: -p argument %d must be greater than zero\n", prog, ppm);
                    return 1;
                }
                break;
            case 'x':
                // starting x offset in image
                x = atoi(optarg);
                if (x < 0)
                {
                    fprintf(stderr, "%s: error: -x argument %d cannot be negative\n", prog, x);
                    return 1;
                }
                break;
            case 'y':
                // starting y offset in image
                y = atoi(optarg);
                if (y < 0)
                {
                    fprintf(stderr, "%s: error: -y argument %d cannot be negative\n", prog, y);
                    return 1;
                }
                break;
            case 'o':
                // output filename
                out_filename = optarg;
                break;
            default:
                return 1;
        }
    }
    // get required file input
    if (argc - optind < 1)
    {
        fprintf(stderr, "%s: error: missing required arguments\n", prog);
        print_usage(stderr, prog);
        return 1;
    }
    in_filename = argv[optind];
    // run!
    unsigned int spp = calc_spp(sr, ppm); // samples per pixel
    if (!out_filename)
    {
        printf("audio samples per pixel: %d\n", spp);
    }
    return process(in_filename, out_filename, sr, spp, x, y, v);
}

