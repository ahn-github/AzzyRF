
#ifndef MEGATINYCORE
const byte commands[][32] PROGMEM = {
#else
const byte commands[][32] = {
#endif
  {0x1E,0x40,0xFF,0x00}, //Pin 0, PA6
  {0x1E,0x40,0xFF,0x10}, //pin 1, PA7
  {0x1E,0x40,0xFF,0x20}, //pin 2, PA1
  {0x1E,0x41,0xFF,0x10}, //Same as tower light, but on distinct signal
  {0x1E,0x40,0x00,0x00}, //Pin 0, PA6, off.
  {0x1E,0x40,0x00,0x10}, //Pin 1, PA7, off.
  {0x1E,0x40,0x00,0x20}, //Pin 2, PA1, off.
  {0x1F,0x1E,0x02,0x00} //legacy tower light signal
};
