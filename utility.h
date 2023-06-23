/*
   A Class for various utility functions
*/
#ifndef UTILITY_h
#define UTILITY_h

#include "Arduino.h"

class Utility
{
  public:
    Utility(void);
    uint8_t charArrToByte(char *ascii_num);
    uint8_t charArrToByte(uint8_t *ascii_num);
    void binaryPrint(String s, size_t number);
    uint16_t freeMemory(void);

  private:

};
#endif
