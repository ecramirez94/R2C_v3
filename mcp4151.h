/*
 * Since the MCP4151 is pin limited (see data sheet) and opto-isolated, it was not practical to implement
 * any read (MISO) functionality. As a consequence, there really is no way of confirming what the current 
 * setting of the wiper is, only a software variable tracking the value of the count as sent to the MCP4151.
 * 
 * Written by: Carlos Ramirez 5/4/23
 */

#ifndef MCP4151_h
#define MCP4151_h
#include <Arduino.h>
#include "my_spi.h"
#include "pins.h"

/* MCP4151 only operates in SPI mode (0,0) = 0 and (1,1) = 3 
* However, since the opto-isolator is effectively a logical NOT function, the clock signal needs to be inverted
* by the SPI hardware by setting the clock mode to 1 so it arrives at the MCP4151 in SPI clock mode 0. This gives
* SPI mode 2. SPI Mode 3 could work too, though wasn't tested. Further, the ~CS signal also needs to be inverted
* for the same reason, and is handled in the MY_SPI library. Unfortunately the MOSI signal could not be inverted
* by any software changes so an external NOT gate is placed between the MCU MOSI pin and the opto-isolator to 
* achieve the same effect.
*/
#define MCP4151_SPI_MODE 2
/* MCP4151 has 255 wiper positions along resistor string plus ability to connect to Vcc and GND (255 + 2 = 257) */
#define MCP4151_STEPS 257

/* See Section 7.0 of MCP4151 datasheet for more details on registers */
/* Memory Addresses */
#define WIPER_0_ADDR 0x00
#define TCON_ADDR 0x04
#define STATUS_REG_ADDR 0x05

/* Command Bits */
#define WRITE_DATA_COMMAND_BITS 0x00
#define INCREMENT_COMMAND_BITS 0x01 << 2
#define DECREMENT_COMMAND_BITS 0x02 << 2
#define READ_DATA_COMMAND_BITS 0x03 << 2

/* Commands */
#define WRITE_DATA WIPER_0_ADDR | WRITE_DATA_COMMAND_BITS
#define READ_DATA WIPER_0_ADDR | READ_DATA_COMMAND_BITS
#define INCREMENT WIPER_0_ADDR | INCREMENT_COMMAND_BITS
#define DECREMENT WIPER_0_ADDR | DECREMENT_COMMAND_BITS

class MCP4151
{
  protected:
  MY_SPI *spi;
  
  public:
  bool begin(void);
  int16_t increment(void);
  int16_t decrement(void);
  bool setCount(char *data, uint8_t data_len);
  int16_t getCount(void);

  private:
  void _setCount(int16_t count);
  /* Upon power-up, the MCP4151 starts at Vcc/2 */
  int16_t _count = floor(MCP4151_STEPS / 2);
};
#endif
