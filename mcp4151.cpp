#include "mcp4151.h"

bool MCP4151::begin(void)
{
  /* Instantiate a new SPI Class object */
  spi = new MY_SPI(MCP4151_CS_PIN, MOSI_PIN, MISO_PIN, SCK_PIN, MCP4151_SPI_MODE, false, MSB, true);
  
  /* Initialize the device and count at 0 */
  setCount(30);

  return true;
}

int16_t MCP4151::increment(void)
{
  _count++;

  if (_count >= MCP4151_STEPS)
    _count = MCP4151_STEPS;
    
  setCount(_count);
  return _count;
}

int16_t MCP4151::decrement(void)
{
  _count--;

  if (_count <= 0)
    _count = 0;
    
  setCount(_count);
  return _count;
}

bool MCP4151::setCount(char *data, uint8_t data_len)
{
  /* Check payload lenth, if greater than three, don't even bother with
    sorting. Just return and send error message to host.
  */
  if (data_len == 0 || data_len > 3)
    return false;

  /* Next, make sure the payload is within the correct range */
  uint16_t count = uint16_t(atoi(data));
  if (count > MCP4151_STEPS)
    return false;

  setCount(count);
  return true;
}

int16_t MCP4151::getCount(void)
{
  return _count;
}

void MCP4151::setCount(int16_t count)
{
  _count = count;
  
  /* Mask the user provided count to make sure it stays within range */
  count &= 0x03FF;
  
  spi->write(uint16_t(WRITE_DATA | count));
}
