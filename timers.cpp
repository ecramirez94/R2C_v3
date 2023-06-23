/*
   Library to configure and control the internal timer/counter units within the ATMega328(P)

   In the ATMega328(P) datasheet, see the following sections:
   - Section 15, pg. 93 for 8-bit Timer/Counter0
   - Section 16, pg. 111 for 16-bit Timer/Counter1
   - Section 18. pg. 141 for 8-bit Timer/Counter2

   Timer/Counter uses:
   - 8-bit Timer/Counter0 is used for counting pulses coming from Hall sensor. Used in concert with 16-bit Timer/Counter1 to calculate router RPM.
   - 16-bit Timer/Counter1 is used for timing the pulse collection interval. Used in concert with 8-bit Timer/Counter0 to calculate router RPM.
   - 8-bit Timer/Counter2 is used to set the display update interval.
*/
#include "timers.h"

/*
   8-bit Timer/Counter0. "Hall Pulse Counter"
*/
void Timers::initHallPulseCounter(void)
{
  /* Since TIMCNT0 is being used as a counter:
      - Set Compare Match Output A/B to Normal Operation.
      - Waveform Generation Mode to Normal.
      - Force Output Compare A/B explicitly written to 0.
      - Clock Select set to 0x06, "External clock source on T0. Clock on falling edge"
      - Disable TIMCNT0 interrupts
      - Since Compare Match Output and interrupts are disabled,
      Output Compare Register A/B do not need to be configured.
  */
  TCCR0A = (0 << COM0A1) | (0 << COM0A0) | (0 << COM0B1) | (0 << COM0B0) | (1 << WGM01) | (0 << WGM00);
  TCCR0B = (0 << FOC0A) | (0 << FOC0B) | (0 << WGM02) | (0 << CS02) | (0 << CS01) | (0 << CS00);
  TIMSK0 = (0 << OCIE0B) | (1 << OCIE0A) | (0 << TOIE0);

  OCR0A = 16; // Number of rotations to trigger interrupt. Must be a multiple of 4.
}

void Timers::enableHallPulseCounter(void)
{
  /* Clear Counter Register */
  TCNT0 = 0;
  
  TCCR0B = (0 << FOC0A) | (0 << FOC0B) | (0 << WGM02) | (1 << CS02) | (1 << CS01) | (0 << CS00);
  startPulseCollectionTimer();
}

void Timers::disableHallPulseCounter(void)
{
  TCCR0B = (0 << FOC0A) | (0 << FOC0B) | (0 << WGM02) | (0 << CS02) | (0 << CS01) | (0 << CS00);
  stopPulseCollectionTimer();
}

void Timers::clearHallPulseCounter(void)
{
  /* Clear Counter Register */
  TCNT0 = 0;
}

/*
   16-bit Timer/Counter1. "Pulse Collection Timer"
*/
void Timers::initPulseCollectionTimer(void)
{
  /* Since TIMCNT1 is being used as a timer with no Output Compare functionality:
     - Set Compare Match Output A/B to Normal Operation.
     - Waveform Generation mode set to "Clear Timer on Compare match (CTC)" mode (0x04).
     - Force Output Compare A/B explicitly written to 0.
     - Clock Select set to 0 (no clock) until start function is called.
     - Enable OCIE1A interrupt.
  */
  TCCR1A = (0 << COM1A1) | (0 << COM1A0) | (0 << COM1B1) | (0 << COM1B0) | (0 << WGM11) | (0 << WGM10);
  TCCR1B = (0 << ICNC1) | (0 << ICES1) | (0 << WGM13) | (0 << WGM12) | (0 << CS12) | (0 << CS11) | (0 << CS10);
  TCCR1C = (0 << FOC1A) | (0 << FOC1B);

  /* Clear Timer Register */
  TCNT1 = 0;

  /* Disable all interrupts */
  TIMSK1 = (0 << ICIE1) | (0 << OCIE1B) | (0 << OCIE1A) | (0 << TOIE1);
}

void Timers::startPulseCollectionTimer(void)
{
  /* Clear timer */
  TCNT1 = 0;

  /* Next, clear and resync Timer/Counter0 and Timer/Counter1 Prescaler */
  GTCCR = (1 << TSM);  // Activate Timer/Counter Synchronization Mode. Stops Timer.

  /* Configure Timer1 to count once Sync Mode is de-activated. Prescale value: 64 */
  TCCR1B = (0 << ICNC1) | (0 << ICES1) | (0 << WGM13) | (0 << WGM12) | (0 << CS12) | (1 << CS11) | (1 << CS10);

  /* Reset Prescaler */
  GTCCR = (1 << PSRSYNC);

  /* Disable Sync Mode and begin timing/counting */
  GTCCR = (0 << TSM);
}

void Timers::stopPulseCollectionTimer(void)
{
  TCCR1B = (0 << ICNC1) | (0 << ICES1) | (0 << WGM13) | (1 << WGM12) | (0 << CS12) | (0 << CS11) | (0 << CS10);
}


/*
   8-bit Timer/Counter2. "Display Update Timer"
*/
void Timers::startDisplayUpdateTimer(void)
{
  /* Set the TCNT2 interrupt to occur every ~10ms
    When System clock is 16MHz and prescaled by 1024, a timer tick occurs every 64uS.
    Thus, 157 timer ticks ~= 10mS; OCR0A = 156.

    In the Compare Match ISR, use a secondary counter to achieve intervals greater than (and multiples of) ~10mS.
  */

  /* Normal port operation on I/O pins | WGM mode = CTC */
  TCCR2A = (0 << COM2A1) | (0 << COM2A0) | (0 << COM2B1) | (0 << COM2B0) | (1 << WGM21) | (0 << WGM20);
  /* Configure clock source */
  ASSR = (0 << EXCLK) | (0 << AS2);
  /* Clear Timer/Counter */
  TCNT2 = 0;
  /* Set OCR2A to generate interrupt every ~10mS when clock prescaler set to 1024. */
  OCR2A = 156;
  /* Enable interrupt generation on OCIE2A match */
  TIMSK2 = (0 << OCIE2B) | (1 << OCIE2A) | (0 << TOIE2);
  /* Start timer */
  TCCR2B = (0 << FOC2A) | (0 << FOC2B) | (0 << WGM22) | (1 << CS22) | (1 << CS21) | (1 << CS20);
}

void Timers::stopDisplayUpdateTimer(void)
{
  /* Stop timer (Set all Clock Select (CS) bits to 0) */
  TCCR2B = (0 << FOC2A) | (0 << FOC2B) | (0 << WGM22) | (0 << CS22) | (0 << CS21) | (0 << CS20);
}
