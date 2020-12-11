/*
 * sddc_test - simple test program for libsddc
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

#include "libsddc.h"


#if _WIN32
#include <windows.h>
#define sleep(x) Sleep(x*1000)
#endif

static void blink_led(sddc_t *sddc, uint8_t color);


int main(int argc, char **argv)
{
  if (argc != 2) {
    fprintf(stderr, "usage: %s <image file>\n", argv[0]);
    return -1;
  }
  char *imagefile = argv[1];

  /* count devices */
  int count = sddc_get_device_count();
  if (count < 0) {
    fprintf(stderr, "ERROR - sddc_get_device_count() failed\n");
    return -1;
  }
  printf("device count=%d\n", count);

  /* get device info */
  struct sddc_device_info *sddc_device_infos;
  count = sddc_get_device_info(&sddc_device_infos);
  if (count < 0) {
    fprintf(stderr, "ERROR - sddc_get_device_info() failed\n");
    return -1;
  }
  for (int i = 0; i < count; ++i) {
    printf("%d - manufacturer=\"%s\" product=\"%s\" serial number=\"%s\"\n",
           i, sddc_device_infos[i].manufacturer, sddc_device_infos[i].product,
           sddc_device_infos[i].serial_number);
  }
  sddc_free_device_info(sddc_device_infos);

  /* open and close device */
  sddc_t *sddc = sddc_open(0, imagefile);
  if (sddc == 0) {
    fprintf(stderr, "ERROR - sddc_open() failed\n");
    return -1;
  }

  /* blink the LEDs */
  printf("blinking the red LED\n");
  blink_led(sddc, RED_LED);
  printf("blinking the yellow LED\n");
  blink_led(sddc, YELLOW_LED);
  printf("blinking the blue LED\n");
  blink_led(sddc, BLUE_LED);

  /* done */
  sddc_close(sddc);

  return 0;
}

static void blink_led(sddc_t *sddc, uint8_t color)
{
  for (int i = 0; i < 5; ++i) {
    int ret = sddc_led_on(sddc, color);
    if (ret < 0) {
      fprintf(stderr, "ERROR - sddc_led_on(%02x) failed\n", color);
      return;
    }
    sleep(1);
    ret = sddc_led_off(sddc, color);
    if (ret < 0) {
      fprintf(stderr, "ERROR - sddc_led_off(%02x) failed\n", color);
      return;
    }
    sleep(1);
  }
  return;
}