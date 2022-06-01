/* Convert an image to audio.
 * Audio format: signed 8-bit, 48KHz sample rate
 */

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define DEFAULT_SAMPLE_RATE 48000
#define DEFAULT_PX_PER_MIN 240

enum {
    WAVE_SINE,
    WAVE_SAW,
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
    assert(!isnan(t));
    assert(!isnan(f));
    float x = t * f;
    return x - floorf(x) - 0.5;
}

float sine(float t, float f)
{
    assert(!isnan(t));
    assert(!isnan(f));
    return 0.5 * (1 + sin(f * t));
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

int color_to_wave(unsigned char r, unsigned char g, unsigned char b)
{
    if (r > g && r > b)
    {
        return WAVE_SINE;
    }
    return WAVE_SAW;
}

// o = output buffer
// w = waveform kind
// t = time
// f = frequency
// a = amplitude
// r = sample Rate
// s = number of samples
void generate_samples(float *o, int w, float t, float f, float a, unsigned int r, unsigned int s)
{
    assert(!isnan(t));
    assert(!isnan(f));
    assert(!isnan(a));
    float dt = 1.0 / r;
    assert(!isnan(dt));
    for (int i = 0; i < s; i++)
    {
        float rt = t + dt * i;
        float x;
        switch (w)
        {
            case WAVE_SINE:
                x = sine(rt, f);
                break;
            case WAVE_SAW:
            default:
                x = saw(rt, f);
                break;
        }
        assert(!isnan(x));
        x *= a;
        o[i] = x;
    }
}

// in_filename = name of input image file
// out_filename = name of output file to create
// r = sample rate
// tpp = time per pixel
// start_x = x to start at in the image
// returns non-zero if there is an error
int process(char *in_filename, char *out_filename, int rate, int spp, int start_x)
{
    const float tpp = (float)spp / rate;
    // load image
    // width, height, num. of channels
    int w, h, n;
    unsigned char *data = stbi_load(in_filename, &w, &h, &n, 3);
    if (!data)
    {
        fprintf(stderr, "could not load input file\n");
        return 1;
    }

    if (w <= start_x)
    {
        fprintf(stderr, "start x (%d) is larger than the image width (%d)\n", start_x, w);
        return 1;
    }

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
    const int max_y = (h < num_keys)? h : num_keys;
    for (int x = start_x; x < w; x++)
    {
        float *col_buffer = calloc(spp, sizeof(float));
        int notes = 0;
        for (int y = 0; y < max_y; y++)
        {
            if (notes > max_notes)
            {
                fprintf(stderr, "note: maximum number of notes (%d) placed at one time at x = %d\n", max_notes, x);
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
            int key = num_keys - y;
            float f = key_to_frequency(key);
            float a = color_to_amplitude(r, g, b) / max_notes;
            int w = color_to_wave(r, g, b);
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
        "    -r rate    sets the sample rate in Hertz, (integer value), default value is %d\n"
        "    -p ppm     sets the pixels per minute (integer value), default value is %d\n"
        "    -x start   ignores the left side of the image until the given x column number (starts at 1)\n",
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
    unsigned int x = 0;
    unsigned int sr = DEFAULT_SAMPLE_RATE; // default sample rate
    unsigned int ppm = DEFAULT_PX_PER_MIN;  // default pixels per minute
    // parse options
    int opt;
    char *prog = (argc && argv)? argv[0] : NULL;
    while ((opt = getopt(argc, argv, "hr:p:x:")) != -1)
    {
        switch (opt)
        {
            case 'h':
                // help
                fprintf(stdout, "Converts an image file input to an audio file output.\n");
                print_usage(stdout, prog);
                return 0;
            case 'r':
                // sample rate
                sr = atoi(optarg);
                if (sr <= 0)
                {
                    fprintf(stderr, "%s: error: -r argument %d is not greater than zero\n", prog, sr);
                    return 1;
                }
                break;
            case 'p':
                // pixels per minute
                ppm = atoi(optarg);
                if (ppm <= 0)
                {
                    fprintf(stderr, "%s: error: -p argument %d is not greater than zero\n", prog, ppm);
                    return 1;
                }
                break;
            case 'x':
                // starting x offset in image
                x = atoi(optarg);
                if (ppm < 1)
                {
                    fprintf(stderr, "%s: error: -x argument %d is less than 1\n", prog, ppm);
                    return 1;
                }
                break;
            default:
                return 1;
        }
    }
    if (argc - optind < 2)
    {
        fprintf(stderr, "%s: error: missing required arguments\n", prog);
        print_usage(stderr, prog);
        return 1;
    }
    in_filename = argv[optind];
    out_filename = argv[optind + 1];
    unsigned int spp = calc_spp(sr, ppm); // samples per pixel
    return process(in_filename, out_filename, sr, spp, x - 1);
}

