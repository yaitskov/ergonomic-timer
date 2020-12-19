#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>

typedef unsigned char b;
typedef int ms;
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
