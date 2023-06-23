#ifndef MY_SPI_h
#define MY_SPI_h
#include <Arduino.h>
#include "utility.h"

/* Position of each pin number in _pin[n] array. */
#define CS_pos 0
#define MOSI_pos 1
#define MISO_pos 2
#define SCK_pos 3

/* Settings Constants */
#define MSB 0
#define LSB 1

class MY_SPI
{
  protected:
  Utility *utility;
  
  public:
  MY_SPI(uint8_t cs_pin, uint8_t mosi_pin, uint8_t miso_pin, uint8_t sck_pin, uint8_t spi_mode, bool interrupt = false, uint8_t data_order = MSB, bool invert_cs = false);
  void write(uint8_t data);
  void write(uint16_t data);

  private:
  uint8_t _pins[4];
  /* By default _cs is active LOW. Thus _cs = false */
  bool _cs;
  
};
#endif
