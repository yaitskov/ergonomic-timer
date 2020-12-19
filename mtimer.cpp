#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>
// #include <stdio.h>

typedef unsigned char b;
typedef int ms;
typedef int ps;

#define DIF(a,b) ((int) a - (int) b)

class HumanTime {
private:
  ps _ps;
  ms _ms;
  b _sec;
  b _min;
  b _hour;
public:
  // HumanTime() : _ps(0), _ms(0), _sec(0), _min(0), _hour(0) {}
  void tick(ms msTicks, ps psTicks) {
    _ps += psTicks;
    if (_ps > 999) {
      ++_ms;
      _ps -= 1000;
    }
    _ms += msTicks;
    while (_ms > 999) {
      _ms = _ms - 1000;
      if (++_sec > 59) {
        _sec = 0;
        if (++_min > 59) {
          _min = 0;
          _hour = (_hour+1) % 24;
        }
      };
    }
  }

  int compare(const HumanTime& t2) const {
    int d = DIF(_hour,t2._hour);
    if (!d) {
      d = DIF(_min, t2._min);
      if (!d) {
        d = DIF(_sec, t2._sec);
        if (!d) {
          d = DIF(_ms, t2._ms);
          if (!d) {
            d = DIF(_ps, t2._ps);
          }
        }
      }
    }
    return d;
  }

  bool operator>(const HumanTime& t2) const {
    return compare(t2) > 0;
  }
};


HumanTime wallClock;
HumanTime turnOnAfter;
HumanTime turnOffAfter;
// #define  turnOffAfter turnOnAfter

#define ON_SET 1
#define ON_AFTER_SET 2
#define OFF_AFTER_SET 4
b stateFlags = 0;

// pins
#define POWER_LED_PIN (1 << 0)
#define POWER_PIN (1 << 1)
#define BTN_PIN (1 << 2)

#define TIMER_PRESCALE 64
#define TIMER_OVERFLOW 256
#define TIMER_PERIOD (TIMER_OVERFLOW * 1.0/(F_CPU / (1.0 * TIMER_PRESCALE)))
#define TIMER_PERIOD_MS ((int) (TIMER_PERIOD * 1000))
#define TIMER_PERIOD_PS ((int) (1000 * (TIMER_PERIOD * 1000 - TIMER_PERIOD_MS)))

void timerInterrupt() {
  wallClock.tick(TIMER_PERIOD_MS, TIMER_PERIOD_PS);
}

void onOffBtnInterrupt() {
  if (stateFlags & ON_SET) {
    turnOffAfter = wallClock;
  } else {
    turnOnAfter = wallClock;
  }
  stateFlags ^= ON_SET;
}

int main(void) {
  // timerInterrupt();

  DDRB |= (1 << POWER_PIN) | (1 << POWER_LED_PIN);
  PORTB |= ~((1 << POWER_PIN) | (1 << POWER_LED_PIN));

  while (1) {
    if ((stateFlags & ON_SET) && wallClock > turnOffAfter) {
      PORTB &= ~(1 << POWER_PIN | 1 << POWER_LED_PIN);
    } else if (!(stateFlags & ON_SET) && wallClock > turnOnAfter) {
      PORTB |= 1 << POWER_PIN | 1 << POWER_LED_PIN;
    }
  }
}
