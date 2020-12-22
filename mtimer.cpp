#define F_CPU 1000000UL

#include <avr/io.h>
#include <avr/interrupt.h>

#define PAUSE_SET 1

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
	b tick(ms msTicks, mcs psTicks) {
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
					return 1;
				};					
			}	
		}
		return 0;
	}
	
	void reset() {
		_secs = _minInDay = _microSecs = _ms = 0; 		
	}
	
	HumanTime operator-(const HumanTime& t2) const {
		mcs mcsD = _microSecs - t2._microSecs;
		ms msD = _ms - t2._ms;
		if (mcsD < 0) {
			mcsD += 1000;
			--msD;
		}
		b secsD = _secs - t2._secs;
		if (msD < 0) {
			msD += 1000;
			--secsD;
		}
		minute minD = _minInDay - t2._minInDay;
		if (secsD < 0) {
			--minD;
			secsD += 60;
		}
		return HumanTime(minD, secsD, msD, mcsD);
	}
	
	HumanTime operator+(const HumanTime& t2) const {
		mcs mcsD = _microSecs + t2._microSecs;
		ms msD = _ms + t2._ms;
		if (mcsD > 999) {
			mcsD -= 1000;
			++msD;
		}
		b secsD = _secs + t2._secs;
		if (msD > 999) {
			msD -= 1000;
			++secsD;
		}
		minute minD = _minInDay + t2._minInDay;
		if (secsD > 59) {
			++minD;
			secsD -= 60;
		}
		return HumanTime(minD, secsD, msD, mcsD);
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
	
	bool operator>=(const HumanTime& t2) const {
		return compare(t2) >= 0;
	}
};


class BlinkPortB {
public:
	b _maxOffTicks;
	b _bitMask;
	b _maxOnTicks;
	b _offTicks;
	b _onTicks;	
	BlinkPortB (b maxOff, b bitMask, b maxOn) 
		: _maxOffTicks(maxOff), 
			_bitMask(bitMask),
			_maxOnTicks(maxOn) {}
		
	void tick() {
		if (!_onTicks && _maxOnTicks) { // zero start - turn on
			PORTB |= _bitMask;
		} else if (_onTicks >= _maxOnTicks) {
			if (!_offTicks) { // zero start - turn off
				PORTB &= ~_bitMask;
			} else if (_offTicks >= _maxOffTicks) {
				_onTicks = _offTicks = 0;				
				return;
			}
			++_offTicks;
			return;
		}
		++_onTicks;	
		return;
	}
};


HumanTime wallClock(0, 0, 0, 0);
HumanTime turnOnAfter(0, 0, 0, 0);
HumanTime lastBtnPress(0, 0, 0, 0);
HumanTime turnOffAfter(24 * 60 - 1, 59, 0, 0);
HumanTime pausedAt(0, 0, 0, 0);
HumanTime pauseInDay(0, 0, 0, 0);
///HumanTime effectiveTurnOff(0, 0, 0, 0);
BlinkPortB powerLedBlinker(3, POWER_LED_PIN, 7);
BlinkPortB pauseLedBlinker(1, PAUSE_LED_PIN, 0);


volatile b stateFlags = 0;

// wall clock {wallClock._secs} {wallClock._ms} {wallClock._microSecs}	
ISR(TIMER0_OVF_vect) {
	if (wallClock.tick(TIMER_PERIOD_MS, TIMER_PERIOD_PS)) {
		pauseInDay.reset();
	}
	powerLedBlinker.tick();
	pauseLedBlinker.tick();
}

void buttonPressed() {
	b changeBits = PINB ^ 0xff;
	if (lastBtnPress.isNoise600ms(wallClock)) {
		return;
	}		
	if (changeBits & TOGGLE_PIN 
		&& stateFlags ^ PAUSE_SET /* ignore toggle button on pause */) {
		if (wallClock > turnOnAfter && turnOffAfter > wallClock) {
			// off at {wallClock._secs} {wallClock._ms} {wallClock._microSecs}
			turnOffAfter = wallClock;		
		} else {
			// consequent on -> reset; reset at {wallClock._secs} {wallClock._ms} {wallClock._microSecs}
			turnOnAfter.reset();
			wallClock.reset();
			turnOffAfter = HumanTime(24 * 60 - 1, 59, 0, 0);
		}
		lastBtnPress = wallClock;
	} else if (changeBits & PAUSE_BTN_PIN) {
		// set pause turn at {wallClock._secs} {wallClock._ms} {wallClock._microSecs}
		if (stateFlags ^= PAUSE_SET) {
			pausedAt = wallClock;
		} else {
			pausedAt = wallClock - pausedAt;
			pauseInDay = pauseInDay + pausedAt;
		}	
		lastBtnPress = wallClock;
	}
}

ISR(PCINT0_vect) { 
	buttonPressed();	
}

int main(void) {
	TCCR0B |= PRESCALE_BITS;
	TCNT0 = 0; // reset counter 0
	TIMSK |= 0b10; // enable timer overflows bit TOIE0
	GIMSK |= (1 << PCIE);     // set PCIE0 to enable PCMSK0 scan	
	PCMSK |=  TOGGLE_PIN |  PAUSE_BTN_PIN; // PCINT0 PCINT1

	DDRB |= POWER_PIN | POWER_LED_PIN | PAUSE_LED_PIN;	

	sei();

	while (1) {
		if (stateFlags & PAUSE_SET) {
			powerLedBlinker._maxOffTicks = 1;
			powerLedBlinker._maxOnTicks = 0;
			pauseLedBlinker._maxOffTicks = pauseLedBlinker._maxOnTicks = 2;						
			powerLedBlinker._onTicks = 0;
			if (PORTB & POWER_PIN) {				
				pauseLedBlinker._onTicks = 0;
			}
			// auto reset
			if (wallClock._minInDay - pausedAt._minInDay >= 50) {
				//cli();
				stateFlags &= ~PAUSE_SET;
				pauseInDay = wallClock - pausedAt;
				//sei();
			}
			PORTB &= ~POWER_PIN;
		} else {
			//effectiveTurnOff = turnOffAfter + pauseInDay;
			if (wallClock >= turnOnAfter &&  turnOffAfter + pauseInDay > wallClock) {			
				// turn on power at {wallClock._secs} {wallClock._ms} {wallClock._microSecs}
				powerLedBlinker._maxOffTicks = 3;
				powerLedBlinker._maxOnTicks = 7;
				if (!(PORTB & POWER_PIN)) {
					powerLedBlinker._onTicks = 0;
				}
				PORTB |= POWER_PIN;				
			} else {
				// turn off power at {wallClock._secs} {wallClock._ms} {wallClock._microSecs}
				powerLedBlinker._maxOffTicks = 7;
				powerLedBlinker._maxOnTicks = 3;
				if (PORTB & POWER_PIN) {
					powerLedBlinker._onTicks = 0;
				}
				PORTB &= ~POWER_PIN;
			}			
			pauseLedBlinker._maxOffTicks = 1;
			pauseLedBlinker._maxOnTicks = 0;
			pauseLedBlinker._onTicks = 0;
		}		
	}
}
