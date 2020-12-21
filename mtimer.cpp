#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

typedef char b;
typedef int ms;
typedef int mcs;
typedef int minute;


class HumanTime {
public:
	minute _minInDay; // 24 * 60
	b _secs; // 60
	ms _ms; // 1000 
	mcs _microSecs; // 1000
public:
	HumanTime(minute minInDay, b seconds, ms miliseconds, mcs microSecs)
		: _minInDay(minInDay), _secs(seconds), _ms(miliseconds), _microSecs(microSecs) {}
	void tick(ms msTicks, mcs psTicks) {
		_microSecs += psTicks;
		if (_microSecs > 999) {
			++_ms;
			_microSecs -= 1000;
		}
		_ms += msTicks;
		while (_ms > 999) {
			_ms = _ms - 1000;
			if (++_secs > 59) {
				_secs -= 60;
				if (++_minInDay >= 24 * 60) {
					_minInDay = 24 * 60;
				};	
			}	
		}
	}

	int compare(const HumanTime& t2) const {
		int d = _minInDay - t2._minInDay;
		if (!d) {
			d = _secs - t2._secs;
			if (!d) {
				d = _ms - t2._ms;
				if (!d) {
					d = _microSecs - t2._microSecs;
				}
			}
		}
		return d;
	}
	
	bool isNoise600ms(const HumanTime& t2) const {
		int d = _ms - t2._ms;
		if (d < 0) {
			d *= -1;
		}
		return _minInDay == t2._minInDay 
			&& _secs == t2._secs
			&& d < 600;
	}

	bool operator>(const HumanTime& t2) const {
		return compare(t2) > 0;
	}
};


HumanTime wallClock(0, 0, 0, 0);
HumanTime turnOnAfter(0, 0, 0, 0);
HumanTime lastBtnPress(0, 0, 0, 0);
HumanTime turnOffAfter(24 * 60 - 1, 59, 0, 0);

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

// wall clock {wallClock._minInDay} {wallClock._msInMin} {wallClock._microSecs}	
ISR(TIMER0_OVF_vect) {
	wallClock.tick(TIMER_PERIOD_MS, TIMER_PERIOD_PS);
}

void buttonPressed() {
	b changeBits = PINB ^ 0xff;
	if (lastBtnPress.isNoise600ms(wallClock)) {
		return;
	}		
	if (changeBits & TOGGLE_PIN 
		&& stateFlags ^ PAUSE_SET /* ignore toggle button on pause */) {
		if (wallClock > turnOnAfter && turnOffAfter > wallClock) {
			// turn off because light is on; off at {turnOffAfter._secs} {turnOffAfter._ms} {turnOffAfter._microSecs}
			turnOffAfter = wallClock;
			PORTB &= ~POWER_PIN; // immediately turn off power because main loop delay destruction
		} else {
			// consequent on -> reset
			turnOnAfter = wallClock = HumanTime(0, 0, 0, 0);
			turnOffAfter = HumanTime(24 * 60 - 1, 59, 0, 0);
			PORTB |= POWER_PIN;
		}
		lastBtnPress = wallClock;
	} else if (changeBits & PAUSE_BTN_PIN) {
		if (stateFlags ^= PAUSE_SET) {
			PORTB &= ~POWER_PIN;
		} else if (wallClock > turnOnAfter && turnOffAfter > wallClock) {
			PORTB |= POWER_PIN;	
		}
		PORTB ^= PAUSE_LED_PIN;
		lastBtnPress = wallClock;
	}
}

ISR(PCINT0_vect) { 
	buttonPressed();	
}

#define MAIN_DELAY_MS 1400
#define SLAVE_DELAY_MS 350

int main(void) {
	TCCR0B |= PRESCALE_BITS;
	TCNT0 = 0; // reset counter 0
	TIMSK |= 0b10; // enable timer overflows bit TOIE0
	GIMSK |= (1 << PCIE);     // set PCIE0 to enable PCMSK0 scan	
	PCMSK |=  TOGGLE_PIN |  PAUSE_BTN_PIN; // PCINT0 PCINT1

	DDRB |= POWER_PIN | POWER_LED_PIN | PAUSE_LED_PIN;
	PORTB = ~(/*POWER_PIN is on*/ POWER_LED_PIN | PAUSE_LED_PIN);

	sei();

	while (1) {
		if (stateFlags & PAUSE_SET) {
			PORTB |= PAUSE_LED_PIN;
			_delay_ms(SLAVE_DELAY_MS);
			PORTB &= ~(POWER_LED_PIN | PAUSE_LED_PIN);
		} else {
			PORTB &= ~PAUSE_LED_PIN;
			if (wallClock > turnOnAfter && turnOffAfter > wallClock) {
				PORTB &= ~POWER_LED_PIN;
				_delay_ms(SLAVE_DELAY_MS);
				PORTB |= POWER_LED_PIN;
			} else {
				PORTB |= POWER_LED_PIN;
				_delay_ms(SLAVE_DELAY_MS);
				PORTB &= ~POWER_LED_PIN;
			}			
		}
		_delay_ms(MAIN_DELAY_MS);			
	}
}
