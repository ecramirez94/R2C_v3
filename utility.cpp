/*
   Utility Class
*/

#include "utility.h"

/* Needed for freeMemory() */
extern char *__brkval;

/* ===== Constructor ===== */
Utility::Utility(void) {};

uint8_t Utility::charArrToByte(char *ascii_num)
{
  uint8_t len = strlen(ascii_num);

  if (len == 3)
    return ((ascii_num[0] - '0') * 100) + ((ascii_num[1] - '0') * 10) + (ascii_num[2] - '0');
  else if (len == 2)
    return ((ascii_num[0] - '0') * 10) + (ascii_num[1] - '0');
  else /* len == 1 */
    return (ascii_num[0] - '0');
}

uint8_t Utility::charArrToByte(uint8_t *ascii_num)
{
  uint8_t len = 0;

  while (ascii_num[len] != 0x00 /* NULL char */)
    len++;

  if (len == 3)
    return ((ascii_num[0] - '0') * 100) + ((ascii_num[1] - '0') * 10) + (ascii_num[2] - '0');
  else if (len == 2)
    return ((ascii_num[0] - '0') * 10) + (ascii_num[1] - '0');
  else /* len == 1 */
    return (ascii_num[0] - '0');
}

void Utility::binaryPrint(String s, size_t number)
{
  /* Explicitly print a binary number from MSb -> LSb.
   * e.g. "Binary Value: 00010101"
  */

  /* First print label String */
  Serial.print(s + F(": "));

  /* Then print binary value */
  for (int8_t i = (sizeof(number) * 8) - 1; i >= 0; i--)
  {
    if (bitRead(number, i))
      Serial.print("1");
    else
      Serial.print("0");
  }
  Serial.print("\n");
}

/*
   freeMemory() credit: mpflaga
   https://github.com/mpflaga/Arduino-MemoryFree.git

   This function is a reduced version since mpflaga
   has support for ARM and other Arduino flavors.
*/
uint16_t Utility::freeMemory()
{
  char top;
  return &top - __brkval;
}
