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
  _test_(A);
  _test_(B);
  _test_(C);
  _test_(D);
  _test_(E);
  _test_(F);
  _test_(G);
  _test_(H);
  _test_(I);
  _test_(J);
  _test_(K);
  _test_(L);
  _test_(M);
  _test_(N);
  _test_(O);
  _test_(P);
  _test_(Q);
  _test_(R);
  _test_(S);
  _test_(T);
  _test_(U);
  _test_(V);
  _test_(W);
  _test_(X);
  _test_(Y);
  _test_(Z);
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

#include <linux/input.h>
#define _map_(x) [x] = CTI__##x
static int linux_event_to_keycode[] = {
  _map_(KEY_TV),
  _map_(KEY_VIDEO),
  _map_(KEY_MP3),
  _map_(KEY_MEDIA),
  _map_(KEY_RADIO),
  _map_(KEY_OK),
  _map_(KEY_VOLUMEDOWN),
  _map_(KEY_VOLUMEUP),
  _map_(KEY_CHANNELUP),
  _map_(KEY_CHANNELDOWN),
  _map_(KEY_PREVIOUS),
  _map_(KEY_MUTE),
  _map_(KEY_PLAY),
  _map_(KEY_PAUSE),
  _map_(KEY_RED),
  _map_(KEY_GREEN),
  _map_(KEY_YELLOW),
  _map_(KEY_BLUE),
  _map_(KEY_RECORD),
  _map_(KEY_STOP),
  _map_(KEY_MENU),
  _map_(KEY_EXIT),
  _map_(KEY_UP),
  _map_(KEY_DOWN),
  _map_(KEY_LEFT),
  _map_(KEY_RIGHT),
  _map_(KEY_NUMERIC_POUND),
  _map_(KEY_NUMERIC_STAR),
  _map_(KEY_FASTFORWARD),
  _map_(KEY_REWIND),
  _map_(KEY_BACK),
  _map_(KEY_FORWARD),
  _map_(KEY_HOME),
  _map_(KEY_POWER),
  _map_(KEY_0),
  _map_(KEY_1),
  _map_(KEY_2),
  _map_(KEY_3),
  _map_(KEY_4),
  _map_(KEY_5),
  _map_(KEY_6),
  _map_(KEY_7),
  _map_(KEY_8),
  _map_(KEY_9),
  _map_(KEY_A),
  _map_(KEY_B),
  _map_(KEY_C),
  _map_(KEY_D),
  _map_(KEY_E),
  _map_(KEY_F),
  _map_(KEY_G),
  _map_(KEY_H),
  _map_(KEY_I),
  _map_(KEY_J),
  _map_(KEY_K),
  _map_(KEY_L),
  _map_(KEY_M),
  _map_(KEY_N),
  _map_(KEY_O),
  _map_(KEY_P),
  _map_(KEY_Q),
  _map_(KEY_R),
  _map_(KEY_S),
  _map_(KEY_T),
  _map_(KEY_U),
  _map_(KEY_V),
  _map_(KEY_W),
  _map_(KEY_X),
  _map_(KEY_Y),
  _map_(KEY_Z),
  _map_(KEY_MINUS),
  //_map_(KEY_PLUS),
  //_map_(KEY_EQUALS),
  //_map_(KEY_CARET),
  
};


#define _ksmap_(x) [x] = #x
static const char * keycode_to_string_map[] = {
  _ksmap_(CTI__KEY_UNKNOWN),
  _ksmap_(CTI__KEY_TV),
  _ksmap_(CTI__KEY_VIDEO),
  _ksmap_(CTI__KEY_MP3),
  _ksmap_(CTI__KEY_MEDIA),
  _ksmap_(CTI__KEY_RADIO),
  _ksmap_(CTI__KEY_OK),
  _ksmap_(CTI__KEY_VOLUMEDOWN),
  _ksmap_(CTI__KEY_VOLUMEUP),
  _ksmap_(CTI__KEY_CHANNELUP),
  _ksmap_(CTI__KEY_CHANNELDOWN),
  _ksmap_(CTI__KEY_PREVIOUS),
  _ksmap_(CTI__KEY_MUTE),
  _ksmap_(CTI__KEY_PLAY),
  _ksmap_(CTI__KEY_PAUSE),
  _ksmap_(CTI__KEY_RED),
  _ksmap_(CTI__KEY_GREEN),
  _ksmap_(CTI__KEY_YELLOW),
  _ksmap_(CTI__KEY_BLUE),
  _ksmap_(CTI__KEY_RECORD),
  _ksmap_(CTI__KEY_STOP),
  _ksmap_(CTI__KEY_MENU),
  _ksmap_(CTI__KEY_EXIT),
  _ksmap_(CTI__KEY_UP),
  _ksmap_(CTI__KEY_DOWN),
  _ksmap_(CTI__KEY_LEFT),
  _ksmap_(CTI__KEY_RIGHT),
  _ksmap_(CTI__KEY_NUMERIC_POUND),
  _ksmap_(CTI__KEY_NUMERIC_STAR),
  _ksmap_(CTI__KEY_FASTFORWARD),
  _ksmap_(CTI__KEY_REWIND),
  _ksmap_(CTI__KEY_BACK),
  _ksmap_(CTI__KEY_FORWARD),
  _ksmap_(CTI__KEY_HOME),
  _ksmap_(CTI__KEY_POWER),
  _ksmap_(CTI__KEY_0),
  _ksmap_(CTI__KEY_1),
  _ksmap_(CTI__KEY_2),
  _ksmap_(CTI__KEY_3),
  _ksmap_(CTI__KEY_4),
  _ksmap_(CTI__KEY_5),
  _ksmap_(CTI__KEY_6),
  _ksmap_(CTI__KEY_7),
  _ksmap_(CTI__KEY_8),
  _ksmap_(CTI__KEY_9),
  _ksmap_(CTI__KEY_A),
  _ksmap_(CTI__KEY_B),
  _ksmap_(CTI__KEY_C),
  _ksmap_(CTI__KEY_D),
  _ksmap_(CTI__KEY_E),
  _ksmap_(CTI__KEY_F),
  _ksmap_(CTI__KEY_G),
  _ksmap_(CTI__KEY_H),
  _ksmap_(CTI__KEY_I),
  _ksmap_(CTI__KEY_J),
  _ksmap_(CTI__KEY_K),
  _ksmap_(CTI__KEY_L),
  _ksmap_(CTI__KEY_M),
  _ksmap_(CTI__KEY_N),
  _ksmap_(CTI__KEY_O),
  _ksmap_(CTI__KEY_P),
  _ksmap_(CTI__KEY_Q),
  _ksmap_(CTI__KEY_R),
  _ksmap_(CTI__KEY_S),
  _ksmap_(CTI__KEY_T),
  _ksmap_(CTI__KEY_U),
  _ksmap_(CTI__KEY_V),
  _ksmap_(CTI__KEY_W),
  _ksmap_(CTI__KEY_X),
  _ksmap_(CTI__KEY_Y),
  _ksmap_(CTI__KEY_Z),
  _ksmap_(CTI__KEY_PLUS),
  _ksmap_(CTI__KEY_MINUS),
  _ksmap_(CTI__KEY_EQUALS),
  _ksmap_(CTI__KEY_CARET),
};


const char * Keycode_to_string(int keycode)
{
  if (keycode > cti_table_size(keycode_to_string_map)) {
    return keycode_to_string_map[CTI__KEY_UNKNOWN];
  }
  return keycode_to_string_map[keycode];
}


int Keycode_from_linux_event(uint16_t code)
{
  if (code > cti_table_size(linux_event_to_keycode)) {
    return CTI__KEY_UNKNOWN;
  }
  return linux_event_to_keycode[code];
}
