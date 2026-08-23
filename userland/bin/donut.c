/*
 * SzpontOS - 3D Rotating Donut (donut.c)
 * Dynamically linked with libc.so and libm.so
 *
 * Highly optimized algorithm based on Andy Sloane's Donut math &
 * akhileshthite/3d-donut. (C) Copyright by Szpont Industries. All rights
 * reserved.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int max_frames = -1;
  unsigned int delay_us = 16000; /* Default ~60 FPS */

  for (int a = 1; a < argc; a++) {
    if (strcmp(argv[a], "-h") == 0 || strcmp(argv[a], "--help") == 0) {
      printf("Usage: donut [-f frames] [-d delay_ms]\n");
      printf("  -f <num>   Render only <num> frames and exit\n");
      printf(
          "  -d <num>   Delay in milliseconds between frames (default: 16)\n");
      return 0;
    } else if (strcmp(argv[a], "-f") == 0 && a + 1 < argc) {
      max_frames = atoi(argv[++a]);
    } else if (strcmp(argv[a], "-d") == 0 && a + 1 < argc) {
      int d = atoi(argv[++a]);
      if (d >= 0)
        delay_us = (unsigned int)(d * 1000);
    }
  }

  float A = 0.0f, B = 0.0f;
  float i, j;
  int k;
  float z[1760];
  char b[1760];

  /* Clear screen and hide cursor */
  printf("\033[2J\033[?25l");

  int frame_count = 0;
  while (max_frames < 0 || frame_count < max_frames) {
    memset(b, 32, sizeof(b));
    memset(z, 0, sizeof(z));

    /* Precompute frame-constant trigonometric values */
    float cos_A = (float)cos(A), sin_A = (float)sin(A);
    float cos_B = (float)cos(B), sin_B = (float)sin(B);

    for (j = 0.0f; j < 6.28f; j += 0.07f) {
      float cos_j = (float)cos(j), sin_j = (float)sin(j);
      float h = cos_j + 2.0f;

      for (i = 0.0f; i < 6.28f; i += 0.02f) {
        float sin_i = (float)sin(i), cos_i = (float)cos(i);
        float D = 1.0f / (sin_i * h * sin_A + sin_j * cos_A + 5.0f);
        float t = sin_i * h * cos_A - sin_j * sin_A;

        int x = (int)(40 + 30 * D * (cos_i * h * cos_B - t * sin_B));
        int y = (int)(12 + 15 * D * (cos_i * h * sin_B + t * cos_B));
        int o = x + 80 * y;
        int N = (int)(8 * ((sin_j * sin_A - sin_i * cos_j * cos_A) * cos_B -
                           sin_i * cos_j * sin_A - sin_j * cos_A -
                           cos_i * cos_j * sin_B));

        if (y >= 0 && y < 22 && x >= 0 && x < 80 && D > z[o]) {
          z[o] = D;
          b[o] = ".,-~:;=!*#$@"[N > 0 ? (N < 12 ? N : 11) : 0];
        }
      }
    }

    /* Render frame into a single contiguous output buffer */
    char out_buf[2048];
    int out_len = 0;
    out_buf[out_len++] = '\033';
    out_buf[out_len++] = '[';
    out_buf[out_len++] = 'H';

    for (k = 0; k < 1760; k++) {
      out_buf[out_len++] = (k % 80) ? b[k] : '\n';
    }
    write(STDOUT_FILENO, out_buf, out_len);

    A += 0.04f;
    B += 0.02f;

    frame_count++;
    if (delay_us > 0) {
      usleep(delay_us);
    }
  }

  /* Restore cursor */
  printf("\033[?25h\n");
  return 0;
}
