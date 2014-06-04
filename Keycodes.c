#include "CTI.h"
#include "Keycodes.h"
#include <string.h>

Keycode_message * Keycode_message_new(int keycode)
{
  Keycode_message * msg = Mem_malloc(sizeof(*msg));
  msg->keycode = keycode;
  return msg;
}


void Keycode_message_cleanup(Keycode_message **msg)
{
  Mem_free(*msg);
  *msg = 0L;
}


int Keycode_from_string(const char *string)
{
#define _test_(x) if (streq(string, #x)) return CTI__KEY_##x
  _test_(0);
  _test_(1);
  _test_(2);
  _test_(3);
  _test_(4);
  _test_(5);
  _test_(6);
  _test_(7);
  _test_(8);
  _test_(9);
  _test_(TV);
  _test_(VIDEO);
  _test_(MP3);
  _test_(MEDIA);
  _test_(RADIO);
  _test_(OK);
  _test_(VOLUMEDOWN);
  _test_(VOLUMEUP);
  _test_(CHANNELUP);
  _test_(CHANNELDOWN);
  _test_(PREVIOUS);
  _test_(MUTE);
  _test_(PLAY);
  _test_(PAUSE);
  _test_(RED);
  _test_(GREEN);
  _test_(YELLOW);
  _test_(BLUE);
  _test_(RECORD);
  _test_(STOP);
  _test_(MENU);
  _test_(EXIT);
  _test_(UP);
  _test_(DOWN);
  _test_(LEFT);
  _test_(RIGHT);
  _test_(NUMERIC_POUND);
  _test_(NUMERIC_STAR);
  _test_(FASTFORWARD);
  _test_(REWIND);
  _test_(BACK);
  _test_(FORWARD);
  _test_(HOME);
  _test_(POWER);
  return -1;
}
