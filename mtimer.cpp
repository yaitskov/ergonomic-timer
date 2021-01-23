#define F_CPU 32768UL 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define PAUSE_SET 1

// pins
#define PAUSE_BTN_PIN (1 << 0) // PB0
#define POWER_PIN (1 << 1) // PB1
#define TOGGLE_PIN (1 << 2)  // PB2
// PB4 and PB3 are for quartz
// PB5 is reset

// 64 => period 500ms at 32khz
// 64 * 256 => 16k operations between interruptions
#define PRESCALE_BITS 0b011 

/*
262 144mks : 1min 8sec faster on 50 min interval
256 202mks : 30 sec slower on 1h interval
258 183mks : - on 1h interval 
258 520mks :  1.7 sec faster on 1h interval
258 504mks :  1min 4 sec faster on 22h interval
258 497mks :  48 sec faster on 23h interval
258 495mks 100ns:  ? on 23h interval
258 493mks :  23 sec slower on 23h interval
258 485mks :  1min 15 sec slower on 22h interval
258 450mks :  2sec slower on 2h interval

calibration for quartz oscillator 32khz 22mkf
498ms        : 1h : 15s slower
499ms 550    : 1h : 3s  slower
499ms 996    : 1h : 1s  slower = -3/3h   => -12/12h => -24/24h
499ms 998    : 3h : 1s  slower =             -4/12h =>  -8/24h
499ms 999  80: 12h  3s  slower =             -3/12h =>  -6/24h
499ms 999 998: 20h  5s slower?                      =>  -5.6/24h
500   003 000: 24h: 5s slower?
500   015 000: 24h: 3s slower
500   036 000: 24h: 0.5s faster
500   033 000: 48h:?
502          : 24h  6min faster
*/
#define TIMER_PERIOD_MS 500
#define TIMER_PERIOD_MKS 33
#define TIMER_PERIOD_NS  0
typedef char b;
typedef int ns;
typedef int ms;
typedef int mcs;
typedef int minute;


class HumanTime {
public:
	volatile minute _minInDay; // 24 * 60
	volatile b _secs; // 60
	volatile ms _ms; // 1000 
	volatile mcs _microSecs; // 1000
	volatile ns _ns; // 1000
public:
	HumanTime(minute minInDay, b seconds, ms miliseconds, mcs microSecs, ns nanoSecs)
		: _minInDay(minInDay), _secs(seconds), _ms(miliseconds), _microSecs(microSecs), _ns(nanoSecs) {}
	b tick(ms msTicks, mcs psTicks, ns nanoTicks) {
		_ns += nanoTicks;
		if (_ns > 999) {
			++_microSecs;
			_ns -= 1000;
		}
		_microSecs += psTicks;
		if (_microSecs > 999) {
			++_ms;
			_microSecs -= 1000;
		}
		_ms += msTicks;
		if (_ms > 999) {
			_ms = _ms - 1000;			
			if (++_secs > 59) {
				_secs -= 60;
				if (++_minInDay >= 24 * 60) {
					_minInDay -= 24 * 60;
					return 1;
				};					
			}			
		}
		return 0;
	}
	
	void reset() {
		_secs = _minInDay = _microSecs = _ms = _ns = 0; 		
	}
	
	HumanTime operator-(const HumanTime& t2) const {
		ns nsD = _ns - t2._ns;
		mcs mcsD = _microSecs - t2._microSecs;
		if (nsD < 0) {						
			nsD += 1000;
			--mcsD;
		}
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
		return HumanTime(minD, secsD, msD, mcsD, nsD);
	}
	
	HumanTime operator+(const HumanTime& t2) const {
		ns nsD = _ns + t2._ns;
		mcs mcsD = _microSecs + t2._microSecs;
		if (nsD > 999) {			
			nsD -= 1000;
			++mcsD;
		}
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
		return HumanTime(minD, secsD, msD, mcsD, nsD);
	}

	int compare(const HumanTime& t2) const {
		int d = _minInDay - t2._minInDay;
		if (!d) {
			d = _secs - t2._secs;
			if (!d) {
				d = _ms - t2._ms;
				if (!d) {
					d = _microSecs - t2._microSecs;
					if (!d) {
						d = _ns - t2._ns;
					}
				}
			}
		}
		return d;
	}
	
	bool isNoise(const HumanTime& t2) const {        
		HumanTime dif = t2 - *this;
		return !dif._minInDay && dif._secs < 2;
	}

	bool operator>(const HumanTime& t2) const {
		return compare(t2) > 0;
	}
	
	bool operator>=(const HumanTime& t2) const {
		return compare(t2) >= 0;
	}
};


HumanTime wallClock(0, 0, 0, 0, 0);
HumanTime turnOnAfter(0, 0, 0, 0, 0);
HumanTime lastBtnPress(0, 0, 0, 0, 0);
// assume turns on at 6 am. = turn on 0
// assume turns off at 9 pm
HumanTime turnOffAfter(15 * 60, 00, 0, 0, 0);
HumanTime pausedAt(0, 0, 0, 0, 0);
HumanTime pauseInDay(0, 0, 0, 0, 0);

volatile b stateFlags = 0;

// wall clock {wallClock._secs} {wallClock._ms} {wallClock._microSecs}	
ISR(TIMER0_OVF_vect) {
	if (wallClock.tick(TIMER_PERIOD_MS, TIMER_PERIOD_MKS, TIMER_PERIOD_NS)) {
		pauseInDay.reset();
	}
	//if (wallClock._minInDay >= 2*24*60) { // 23 * 60) {
		//PORTB &= ~POWER_PIN;
	//} 
}

void buttonPressed() {
	b changeBits = PINB ^ 0xff;
	if (lastBtnPress.isNoise(wallClock)) {
		return;
	}		
	if ((changeBits & TOGGLE_PIN) && (stateFlags ^ PAUSE_SET)) {
		if (wallClock >= turnOnAfter && turnOffAfter + pauseInDay > wallClock) {
			// off at {wallClock._secs} {wallClock._ms} {wallClock._microSecs}
			turnOffAfter = wallClock;					
            pauseInDay.reset(); // it should be in else section but it makes impression that device doesn't react
			// PORTB &= ~POWER_PIN;
		} else {
			// consequent on -> reset; reset at {wallClock._secs} {wallClock._ms} {wallClock._microSecs}
			TCNT0 = 0;
			turnOnAfter.reset();
			wallClock.reset();            
			//PORTB |= POWER_PIN;
			turnOffAfter = HumanTime(15 * 60, 0, 0, 0, 0);
		}
		lastBtnPress = wallClock;
	} else if (changeBits & PAUSE_BTN_PIN) {
		// set pause turn at {wallClock._secs} {wallClock._ms} {wallClock._microSecs}
		if (stateFlags ^= PAUSE_SET) {
			pausedAt = wallClock;
		} else {			
			if (pausedAt >= turnOnAfter && turnOffAfter + pauseInDay > pausedAt) {
                pausedAt = wallClock - pausedAt;
				pauseInDay = pauseInDay + pausedAt;
			}
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

	DDRB |= POWER_PIN;
	PORTB |= ~(POWER_PIN); // pull up resistors
	sei();

	while (1) {
		if (stateFlags & PAUSE_SET) {
			// auto reset
			if (wallClock._minInDay - pausedAt._minInDay >= 50) {
				stateFlags &= ~PAUSE_SET;
				if (pausedAt >= turnOnAfter && turnOffAfter + pauseInDay > pausedAt) {			
					pauseInDay = pauseInDay + (wallClock - pausedAt);
				}
			}
			PORTB &= ~POWER_PIN;
		} else {
			//effectiveTurnOff = turnOffAfter + pauseInDay;
			if (wallClock >= turnOnAfter && turnOffAfter + pauseInDay > wallClock) {			
				// turn on power at {wallClock._secs} {wallClock._ms} {wallClock._microSecs}
				PORTB |= POWER_PIN;
			} else {
				// turn off power at {wallClock._secs} {wallClock._ms} {wallClock._microSecs}
				PORTB &= ~POWER_PIN;
			}			
		}
	}
}
