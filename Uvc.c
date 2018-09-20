/* UVC specific code. */

#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "uvcvideo.h"

/* begin lucview/v4l2uvc.c sample code */
#define GUID_SIZE               16
#include <math.h>
#include <float.h>

#if 0
static int float_to_fraction_recursive(double f, double p, int *num, int *den)
{
        int whole = (int)f;
        f = fabs(f - whole);

        if(f > p) {
                int n, d;
                int a = float_to_fraction_recursive(1 / f, p + p / f, &n, &d);
                *num = d;
                *den = d * a + n;
        }
        else {
                *num = 0;
                *den = 1;
        }
        return whole;
}

static void float_to_fraction(float f, int *num, int *den)
{
        int whole = float_to_fraction_recursive(f, FLT_EPSILON, num, den);
        *num += whole * *den;
}
#endif

/* end lucview/v4l2uvc.c sample code */

void add_focus_control(int camfd)
{
  int rc;
  // 0682-5070-49ab-b8cc-b3855e8d2256
  unsigned char guid[GUID_SIZE] = {
    [3] = 0x63,
    [2] = 0x61,
    [1] = 0x06,
    [0] = 0x82,

    [5] = 0x50,
    [4] = 0x70,

    [7] = 0x49,
    [6] = 0xab,

    [8] = 0xb8,
    [9] = 0xcc,

    [10] = 0xb3,
    [11] = 0x85,
    [12] = 0x5e,
    [13] = 0x8d,
    [14] = 0x22,
    [15] = 0x56,
  };

  struct uvc_xu_control_info info = {};
  memcpy(&info.entity, guid, GUID_SIZE);
  info.index = 2;
  info.selector = 3;
  info.size = 6;
  info.flags = 175;

  rc = ioctl(camfd, UVCIOC_CTRL_ADD, &info);
  if (rc == -1) {
    perror("UVCIOC_CTRL_ADD");
    return;
  }

  struct uvc_xu_control_mapping mapping_info = { 0 };

  mapping_info.id = 0x0a046d04;
  strcpy((char *)mapping_info.name, "Focus");
  memcpy(mapping_info.entity, guid, GUID_SIZE);
  mapping_info.selector = 3;
  mapping_info.size = 8;
  mapping_info.offset = 0;
  mapping_info.v4l2_type = 1;
  mapping_info.data_type = 2;

  rc = ioctl(camfd, UVCIOC_CTRL_MAP, &mapping_info);
  if (rc == -1) {
    perror("UVCIOC_CTRL_MAP");
    return;
  }

  printf("focus control added successfully!\n");
}


void add_led1_control(int camfd)
{
  int rc;
  // 63610682-5070-49ab-b8cc-b3855e8d221f
#define GUID_SIZE               16
  unsigned char guid[GUID_SIZE] = {
    [3] = 0x63,
    [2] = 0x61,
    [1] = 0x06,
    [0] = 0x82,

    [5] = 0x50,
    [4] = 0x70,

    [7] = 0x49,
    [6] = 0xab,

    [8] = 0xb8,
    [9] = 0xcc,

    [10] = 0xb3,
    [11] = 0x85,
    [12] = 0x5e,
    [13] = 0x8d,
    [14] = 0x22,
    [15] = 0x1f,
  };

  struct uvc_xu_control_info info = {};
  memcpy(&info.entity, guid, GUID_SIZE);
  info.index = 0;
  info.selector = 1;
  info.size = 3;
  info.flags = 191;

  rc = ioctl(camfd, UVCIOC_CTRL_ADD, &info);
  if (rc == -1) {
    perror("UVCIOC_CTRL_ADD");
    return;
  }

  struct uvc_xu_control_mapping mapping_info = { 0 };

  mapping_info.id = 0x0a046d05;
  strcpy((char *)mapping_info.name, "LED1 Mode");
  memcpy(mapping_info.entity, guid, GUID_SIZE);
  mapping_info.selector = 1;
  mapping_info.size = 8;
  mapping_info.offset = 0;
  mapping_info.v4l2_type = 1;
  mapping_info.data_type = 2;

  rc = ioctl(camfd, UVCIOC_CTRL_MAP, &mapping_info);
  if (rc == -1) {
    perror("UVCIOC_CTRL_MAP");
    return;
  }

  printf("led1 mode control added successfully!\n");

}

