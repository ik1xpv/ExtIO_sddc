/*
 * sddc_vhf_stream_test - simple VHF/UHF stream test program for libsddc
 *
 * Copyright (C) 2020 by Franco Venturi
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libsddc.h"
#include "wavewrite.h"

#if _WIN32
#include <Windows.h>
#define CLOCK_REALTIME 0
LARGE_INTEGER
getFILETIMEoffset()
{
  SYSTEMTIME s;
  FILETIME f;
  LARGE_INTEGER t;

  s.wYear = 1970;
  s.wMonth = 1;
  s.wDay = 1;
  s.wHour = 0;
  s.wMinute = 0;
  s.wSecond = 0;
  s.wMilliseconds = 0;
  SystemTimeToFileTime(&s, &f);
  t.QuadPart = f.dwHighDateTime;
  t.QuadPart <<= 32;
  t.QuadPart |= f.dwLowDateTime;
  return (t);
}

int clock_gettime(int X, struct timeval *tv)
{
  LARGE_INTEGER t;
  FILETIME f;
  double microseconds;
  static LARGE_INTEGER offset;
  static double frequencyToMicroseconds;
  static int initialized = 0;
  static BOOL usePerformanceCounter = 0;

  if (!initialized)
  {
    LARGE_INTEGER performanceFrequency;
    initialized = 1;
    usePerformanceCounter = QueryPerformanceFrequency(&performanceFrequency);
    if (usePerformanceCounter)
    {
      QueryPerformanceCounter(&offset);
      frequencyToMicroseconds = (double)performanceFrequency.QuadPart / 1000000.;
    }
    else
    {
      offset = getFILETIMEoffset();
      frequencyToMicroseconds = 10.;
    }
  }
  if (usePerformanceCounter)
    QueryPerformanceCounter(&t);
  else
  {
    GetSystemTimeAsFileTime(&f);
    t.QuadPart = f.dwHighDateTime;
    t.QuadPart <<= 32;
    t.QuadPart |= f.dwLowDateTime;
  }

  t.QuadPart -= offset.QuadPart;
  microseconds = (double)t.QuadPart / frequencyToMicroseconds;
  t.QuadPart = microseconds;
  tv->tv_sec = t.QuadPart / 1000000;
  tv->tv_usec = t.QuadPart % 1000000;
  return (0);
}
#endif

static void count_bytes_callback(uint32_t data_size, const float *data,
                                 void *context);

static unsigned long long received_samples = 0;
static unsigned long long total_samples = 0;
static int num_callbacks;
static int16_t *sampleData = 0;
static int runtime = 3000;
static struct timespec clk_start, clk_end;
static int stop_reception = 0;

static double clk_diff()
{
  return ((double)clk_end.tv_sec + 1.0e-9 * clk_end.tv_nsec) -
         ((double)clk_start.tv_sec + 1.0e-9 * clk_start.tv_nsec);
}

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    fprintf(stderr, "usage: %s <sample rate> [<runtime_in_ms> [<output_filename>]\n", argv[0]);
    return -1;
  }
  const char *outfilename = 0;
  uint64_t sample_rate = 0;

  double vhf_frequency = 100e6;
  double vhf_attenuation = 20; /* 20dB attenuation */

  sscanf(argv[1], "%ld", &sample_rate);
  if (3 < argc)
    runtime = atoi(argv[2]);
  if (4 < argc)
    outfilename = argv[3];

  if (sample_rate <= 0)
  {
    fprintf(stderr, "ERROR - given samplerate '%d' should be > 0\n", sample_rate);
    return -1;
  }

  int ret_val = -1;

  sddc_t *sddc = sddc_open(0);
  if (sddc == 0)
  {
    fprintf(stderr, "ERROR - sddc_open() failed\n");
    return -1;
  }

  if (sddc_set_rf_mode(sddc, VHF_MODE) < 0)
  {
    fprintf(stderr, "ERROR - sddc_set_rf_mode() failed\n");
    goto DONE;
  }

  if (sddc_set_sample_rate(sddc, sample_rate) < 0)
  {
    fprintf(stderr, "ERROR - sddc_set_sample_rate() failed\n");
    goto DONE;
  }

  if (sddc_set_async_params(sddc, 0, 0, count_bytes_callback, sddc) < 0)
  {
    fprintf(stderr, "ERROR - sddc_set_async_params() failed\n");
    goto DONE;
  }

  if (sddc_set_tuner_frequency(sddc, vhf_frequency) < 0)
  {
    fprintf(stderr, "ERROR - sddc_set_vhf_frequency() failed\n");
    goto DONE;
  }

  if (sddc_set_tuner_rf_attenuation(sddc, vhf_attenuation) < 0)
  {
    fprintf(stderr, "ERROR - sddc_set_tuner_rf_attenuation() failed\n");
    goto DONE;
  }

  received_samples = 0;
  num_callbacks = 0;
  if (sddc_start_streaming(sddc) < 0)
  {
    fprintf(stderr, "ERROR - sddc_start_streaming() failed\n");
    return -1;
  }

  fprintf(stderr, "started streaming .. for %d ms ..\n", runtime);
  total_samples = (unsigned long long)(runtime * sample_rate / 1000.0);

  if (outfilename)
    sampleData = (int16_t *)malloc(total_samples * sizeof(int16_t));

  /* todo: move this into a thread */
  stop_reception = 0;
  clock_gettime(CLOCK_REALTIME, &clk_start);
  while (!stop_reception)
    sddc_handle_events(sddc);

  fprintf(stderr, "finished. now stop streaming ..\n");
  if (sddc_stop_streaming(sddc) < 0)
  {
    fprintf(stderr, "ERROR - sddc_stop_streaming() failed\n");
    return -1;
  }

  double dur = clk_diff();
  fprintf(stderr, "received=%llu 16-Bit samples in %d callbacks\n", received_samples, num_callbacks);
  fprintf(stderr, "run for %f sec\n", dur);
  fprintf(stderr, "approx. samplerate is %f kSamples/sec\n", received_samples / (1000.0 * dur));

  if (outfilename && sampleData && received_samples)
  {
    FILE *f = fopen(outfilename, "wb");
    if (f)
    {
      fprintf(stderr, "saving received real samples to file ..\n");
      waveWriteHeader((unsigned)(0.5 + sample_rate), 0U /*frequency*/, 16 /*bitsPerSample*/, 1 /*numChannels*/, f);
      for (unsigned long long off = 0; off + 65536 < received_samples; off += 65536)
        waveWriteSamples(f, sampleData + off, 65536, 0 /*needCleanData*/);
      waveFinalizeHeader(f);
      fclose(f);
    }
  }

  /* done - all good */
  ret_val = 0;

DONE:
  sddc_close(sddc);

  return ret_val;
}

static void count_bytes_callback(uint32_t data_size,
                                 const float *data,
                                 void *context)
{
  if (stop_reception)
    return;
  ++num_callbacks;
  unsigned N = data_size * 2;
  if (received_samples + N < total_samples)
  {
    if (sampleData)
    {
      for (int i = 0; i < data_size * 2; i++)
      {
        sampleData[received_samples + i] = (int16_t)(data[i] * INT16_MAX);
      }
    }

    received_samples += N;
  }
  else
  {
    clock_gettime(CLOCK_REALTIME, &clk_end);
    stop_reception = 1;
  }
}
