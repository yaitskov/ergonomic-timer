#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>

typedef unsigned char b;
typedef int ms;

#define DIF(a,b) ((int)a - (int)(b))

class HumanTime {
private:
  ms _ms;
  b _sec;
  b _min;
  b _hour;
public:
  HumanTime() : _ms(0), _sec(0), _min(0), _hour(0) {}
  void tick(ms ticks) {
    _ms += ticks;
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

  int compare(const HumanTime& t2) {
    int d = DIF(_hour,t2._hour);
    if (!d) {
      d = DIF(_min, t2._min);
      if (!d) {
        d = DIF(_sec, t2._sec);
        if (!d) {
          d = DIF(_ms, t2._ms);
        }
      }
    }
    return d;
  }

  //bool operator>
};


int main(void) {
  DDRB |= 0b1000;
  while (1) {
    PORTB |= 1 << 3;
    _delay_ms(1000);
    PORTB &= ~(1 << 3);
    _delay_ms(600);
  }
}
