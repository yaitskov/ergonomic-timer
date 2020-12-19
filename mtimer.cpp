#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

typedef unsigned char b;
typedef unsigned int ms;
typedef unsigned int mcs;
typedef unsigned int minute;

#define DIF(a,b) ((int) a - (int) b)

#define LAST_MIN_IN_DAY (24U * 60U - 1U)
#define LAST_MS_IN_MIN (60U * 1000U - 1U)

class HumanTime {
private:
  minute _minInDay; // 24 * 60
  ms _msInMin; // 1000 * 60
  mcs _microSecs;
public:
  HumanTime(int minInDay, ms msInMin, mcs microSecs)
    : _minInDay(minInDay), _msInMin(msInMin), _microSecs(microSecs) {}
  void tick(ms msTicks, mcs psTicks) {
    _microSecs += psTicks;
    if (_microSecs > 999) {
      ++_msInMin;
      _microSecs -= 1000;
    }
    _msInMin += msTicks;
    while (_msInMin > LAST_MS_IN_MIN) {
      _msInMin = _msInMin - 60000U;
      if (++_minInDay > LAST_MIN_IN_DAY) {
        _minInDay = 0;
      };
    }
  }

  int compare(const HumanTime& t2) const {
    int d = DIF(_minInDay,t2._minInDay);
    if (!d) {
      d = DIF(_msInMin, t2._msInMin);
      if (!d) {
        d = DIF(_microSecs, t2._microSecs);
      }
    }
    return d;
  }

  bool operator>(const HumanTime& t2) const {
    return compare(t2) > 0;
  }
};


HumanTime wallClock(0, 0, 0);
HumanTime turnOnAfter(0, 0, 0);
HumanTime turnOffAfter(LAST_MIN_IN_DAY, LAST_MS_IN_MIN, 0);

#define PAUSE_SET 1
b stateFlags = 0;

// pins
#define POWER_LED_PIN (1 << 0) //PB0
#define POWER_PIN (1 << 1) // PB1
#define TOGGLE_PIN (1 << 2)  // PB2
#define PAUSE_BTN_PIN (1 << 3) // PB3
#define PAUSE_LED_PIN (1 << 4) // PB4

#define PRESCALE_BITS 0b101
#define TIMER_PRESCALE 1024
#define TIMER_OVERFLOW 256
#define TIMER_PERIOD (TIMER_OVERFLOW * 1.0/(F_CPU / (1.0 * TIMER_PRESCALE)))
#define TIMER_PERIOD_MS ((int) (TIMER_PERIOD * 1000))
#define TIMER_PERIOD_PS ((int) (1000 * (TIMER_PERIOD * 1000 - TIMER_PERIOD_MS)))


ISR(TIMER0_OVF_vect) {
  wallClock.tick(TIMER_PERIOD_MS, TIMER_PERIOD_PS);
}

volatile uint8_t historyPortB = 0xFF;

ISR(PCINT0_vect) { // btn int
  // xor hack to avoid double exec on up/down
  b changeBits = PINB ^ historyPortB;
  historyPortB = PINB;

  if (changeBits & TOGGLE_PIN) {
    if (wallClock > turnOnAfter && turnOffAfter > wallClock) {
      // turn off because light is on
      turnOffAfter = wallClock;
    } else {
      // consequent on -> reset
      turnOnAfter = wallClock = HumanTime(0,0,0);
      turnOffAfter = HumanTime(LAST_MIN_IN_DAY, LAST_MS_IN_MIN, 0);
    }
  } else if (changeBits & PAUSE_BTN_PIN) {
    stateFlags ^= PAUSE_SET;
  }
}

int main(void) {
  TCCR0B |= PRESCALE_BITS;
  TCNT0 = 0; // reset counter 0
  TIMSK |= 1; // enable timer overflows bit TOIE0
  GIMSK |= (1 << PCIE);     // set PCIE0 to enable PCMSK0 scan
  // PCICR |= (1 << PCIE);     // set PCIE0 to enable PCMSK0 scan
  PCMSK |= (1 << PCINT0);   // set PCINT0 to trigger an interrupt on state change

  DDRB |= POWER_PIN | POWER_LED_PIN | PAUSE_LED_PIN;
  PORTB |= ~(POWER_PIN | POWER_LED_PIN | PAUSE_LED_PIN);

  sei();

  while (1) {
    if (stateFlags & PAUSE_SET) {
      PORTB |= PAUSE_LED_PIN;
      _delay_ms(300);
      PORTB &= ~(POWER_PIN | POWER_LED_PIN | PAUSE_LED_PIN);
      _delay_ms(700);
    } else {
      PORTB &= ~PAUSE_LED_PIN;
      if (wallClock > turnOnAfter && turnOffAfter > wallClock) {
        PORTB &= ~POWER_LED_PIN;
        _delay_ms(300);
        PORTB |= POWER_PIN | POWER_LED_PIN;
      } else {
        PORTB |= POWER_LED_PIN;
        _delay_ms(300);
        PORTB &= ~(POWER_PIN | POWER_LED_PIN);
      }
      _delay_ms(700);
    }
  }
}
