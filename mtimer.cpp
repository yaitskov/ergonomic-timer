#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>
// #include <stdio.h>

typedef unsigned char b;
typedef unsigned int ms;
typedef int ps;

#define DIF(a,b) ((int) a - (int) b)

class HumanTime {
private:
  ps _ps;
  ms _msInMin; // 1000 * 60
  int _minInDay; // 24 * 60
public:
  // HumanTime() : _ps(0), _ms(0), _sec(0), _min(0), _hour(0) {}
  void tick(ms msTicks, ps psTicks) {
    _ps += psTicks;
    if (_ps > 999) {
      ++_msInMin;
      _ps -= 1000;
    }
    _msInMin += msTicks;
    while (_msInMin >= 60000U) {
      _msInMin = _msInMin - 60000U;
      if (++_minInDay >= 24 * 60) {
        _minInDay = 0;
      };
    }
  }

  int compare(const HumanTime& t2) const {
    int d = DIF(_minInDay,t2._minInDay);
    if (!d) {
      d = DIF(_msInMin, t2._msInMin);
      if (!d) {
        d = DIF(_ps, t2._ps);
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
