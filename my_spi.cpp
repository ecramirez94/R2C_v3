#include "my_spi.h"

MY_SPI::MY_SPI(uint8_t cs_pin, uint8_t mosi_pin, uint8_t miso_pin, uint8_t sck_pin, uint8_t spi_mode, bool interrupt, uint8_t data_order, bool invert_cs)
{
    /* Initialize pins */
  _pins[CS_pos] = cs_pin;
  _pins[MOSI_pos] = mosi_pin;
  _pins[MISO_pos] = miso_pin;
  _pins[SCK_pos] = sck_pin;

  for (uint8_t i = 0; i < sizeof(_pins); i++)
  {
    if (i == MISO_pos)
      pinMode(_pins[i], INPUT);
    else
      pinMode(_pins[i], OUTPUT);
  }

  /* Configure CS polarity */
  _cs = invert_cs;
  digitalWrite(_pins[CS_pos], !_cs);

  /* Set SPI Mode - See Section 19.4 "Data Modes" and 19.5 "Register Description" in ATMega328 datasheet for more information on SPI Modes */
  uint8_t spi_settings = 0;

  /* SPI Interrupt */
  interrupt ? spi_settings |= (1<<SPIE) : spi_settings |= (0<<SPE);

  /* Enable SPI interface */
  spi_settings |= (1<<SPE);

  /* Set Data Order (LSB or MSB) */
  data_order ? spi_settings |= (1<<DORD) : spi_settings |= (0<<DORD);

  /* Set SPI interface to Master */
  spi_settings |= (1<<MSTR);

  /* Set SPI Mode */
  switch (spi_mode)
  {
    case 0:
      spi_settings |= (0<<CPOL) | (0<<CPHA);
      break;
    case 1:
      spi_settings |= (0<<CPOL) | (1<<CPHA);
      break;
    case 2:
      spi_settings |= (1<<CPOL) | (0<<CPHA);
      break;
    case 3:
      spi_settings |= (1<<CPOL) | (1<<CPHA);
      break;
  }
  
  /* Set SPI speed to 250KHz */
  spi_settings |= (1<<SPR1) | (0<<SPR0);

  /* After settings have been configured, write config to SPI Control Register (SPCR) */
  SPCR = spi_settings;

//  utility->binaryPrint(F("SPI Settings:"), spi_settings);
}

void MY_SPI::write(uint8_t data)
{
  digitalWrite(_pins[CS_pos], _cs);
    SPDR = data;
    while (!(SPSR & (1<<SPIF)));
  digitalWrite(_pins[CS_pos], !_cs);
}

void MY_SPI::write(uint16_t data)
{
  digitalWrite(_pins[CS_pos], _cs);
    SPDR = highByte(data);
    while (!(SPSR & (1<<SPIF)));
    SPDR = lowByte(data);
    while (!(SPSR & (1<<SPIF)));
  digitalWrite(_pins[CS_pos], !_cs);
}
