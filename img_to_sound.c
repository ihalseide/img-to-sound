#include <assert.h>
#include <stdio.h>
#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

enum {
    WAVE_SINE,
    WAVE_SAW,
};

/* Convert an image to audio.
 * Audio format: signed 8-bit, 48KHz */

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
// returns non-zero if there is an error
int process(char *in_filename, char *out_filename, int rate, int spp)
{
    const float tpp = (float)spp / rate;
    printf(
            "converting %s to %s with sample rate of %dHz where each pixel is %fs long\n",
            in_filename, out_filename, rate, tpp);
    // load image
    // width, height, num. of channels
    int w, h, n;
    unsigned char *data = stbi_load(in_filename, &w, &h, &n, 3);
    if (!data)
    {
        fprintf(stderr, "could not load file\n");
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
    for (int x = 0; x < w; x++)
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
            int i = y * w * n + x;
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

int main(int argc, char **argv)
{
    if (argc > 3)
    {
        fprintf(stderr, "too many arguments\n");
        return 1;
    }
    if (argc < 3)
    {
        fprintf(stderr, "missing file name argument\n");
        return 1;
    }
    char *in_filename = argv[1];
    char *out_filename = argv[2];
    int sample_rate = 48000;
    int per_pixel = sample_rate / 32;
    return process(in_filename, out_filename, sample_rate, per_pixel);
}

