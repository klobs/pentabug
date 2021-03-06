#include <pentabug/hal.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include <pentabug/lifecycle.h>

#include <pentabug/timer.h>

static volatile uint8_t ir_active = 0;
static volatile int16_t wait_time = 0;

static uint8_t timerdivider;

static uint8_t button_count[2];
static uint8_t button_pressed[2];

// interrupt for button handling, every 10ms
ISR(TIMER2_COMPA_vect,ISR_NOBLOCK) {
	uint8_t i = 0;

	for(i = 0; i < 2; ++i) {
		// button pressed?
		if(PINB & (1 << i)) {
			// pressed for more than 50ms is a click
			if(button_count[i] > 5 && button_count[i] < 100) {
				button_pressed[i] = 1;
			}

			// not pressed, reset
			button_count[i] = 0;
		} else {
			//.count time pressed
			++button_count[i];
		}

		// 1s pressed, request next app
		if(button_count[i] == 100) {
			next_app(i ? 1 : -1);
		}
	}
}

ISR(TIMER0_COMPA_vect) {
	// (2*38)kHz  ISR
	//generate 38kHz signal:
	if(ir_active) {
		PORTD ^= 1 << 2;
	}
	timerdivider ++;
	
	//quaterdivider for wait_ms
	if(!(timerdivider & 0x03)) {
			--wait_time;
	}

}

#if 0
pinchangeportisr(ireingangport)
{
	if pinchanged is ir input:
		if ir inactive
			irrstate = IRR_IDLE
			return
		time_since_last  = timedivider - oldtime  //2d handle overflow

		//State machine:
		select (irrstate){
			case ( IRR_IDLE):
				irrstate = IRR_AWAIT_START_COMP;
				…
				break;
			case (IRR_AWAIT_START_COMP):
				//check for transition in time
				if (time_since_last > IR_MAX_STARTBIT_TICKS)
				{
					irrstate = IRR_GET_BIT_FIRSTHALF;
					bitnum = 0;
				} else { irrstate = IRR_IDLE // errorreset}
				break;
			case (IRR_GET_BIT_FIRSTHALF)
				//check beeing in time
				// -> buffer bit[bitnum]
				// else reset
				irrstate = IRR_GET_BIT_SECHALF;
				break;
			case (IRR_GET_BIT_SECHALF)
				//check beeing in time
				// -> inc bitnum
				// -> irrstate = (maxbits)?IRR_GET_STOP_COND:IRR_GET_BIT_FIRSTHALF;
				// else reset
				…
				break;
			case (IRR_GET_STOP_COND):
				//check beeing in time <- need macro for this :-)
				//-> mark received done and set ir inactive (mainloop has to poll done or idle loop callback registration?)
				//-> else reset
		}
}



#endif 


void init_hw(void) {
	// we need to get real fast (8MHz) to handle 38kHz IR frequency ...

	CLKPR = 0b10000000;
	CLKPR = 0b00000000;

	// IR timer

	TIMSK0 |= (1 << OCIE0A);

	// calculated and works, but frequency is a little bit off?
	OCR0A = 105;

	TCCR0A = (1 << WGM01);
	TCCR0B = (1 << CS00);

	// button timer

	TIMSK2 |= (1 << OCIE2A);

	OCR2A = 70;

	TCCR2A = (1 << WGM01);
	TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);

	// activate interrupts

	sei();
}

void reset_hw(void) {
	stop_timer();

	// 0: S1
	// 1: S2
	// 6: MOTOR
	// 7: BUZZR
	PORTB = (1 << 0) | (1 << 1) | (1 << 7);
	DDRB = (1 << 6) | (1 << 7);

	// 0: BUZGND
	// 2: LED2
	// 3: LED2 (+)
	PORTC = (1 << 2) | (1 << 3);
	DDRC = (1 << 0) | (1 << 2) | (1 << 3);

	// 2: IRSEND
	// 3: IRRECV
	// 4: LED
	PORTD = (1 << 4);
	DDRD = (1 << 2) | (1 << 4);

	// do not carry button state

	button_pressed[0] = 0;
	button_pressed[1] = 0;

	// turn ir off

	ir_off();

	// disable adc

	ADCSRA &= ~(1 << ADEN);
}

uint8_t button_state(uint8_t btn) {
	return !(PINB & (1 << btn));
}

uint8_t button_clicked(uint8_t btn) {
	uint8_t clicked = button_pressed[btn];
	button_pressed[btn] = 0;
	return clicked;
}

void button_reset(uint8_t btn) {
	button_pressed[btn] = 0;
}

void led_set(uint8_t led, uint8_t state) {
	if(state) {
		led_on(led);
	} else {
		led_off(led);
	}
}

void led_on(uint8_t led) {
	if(led == RIGHT) {
		PORTC &= ~(1 << 2);
	} else {
		PORTD &= ~(1 << 4);
	}
}

void led_off(uint8_t led) {
	if(led == RIGHT) {
		PORTC |= 1 << 2;
	} else {
		PORTD |= 1 << 4;
	}
}

void led_inv(uint8_t led) {
	if(led == RIGHT) {
		PORTC ^= 1 << 2;
	} else {
		PORTD ^= 1 << 4;
	}
}

uint8_t led_state(uint8_t led) {
	if(led == RIGHT) {
		return !(PORTC & (1 << 2));
	} else {
		return !(PORTD & (1 << 4));
	}
}

void motor_set(uint8_t state) {
	if(state) {
		motor_on();
	} else {
		motor_off();
	}
}

void motor_on(void) {
	PORTB |= 1 << 6;
}

void motor_off(void) {
	PORTB &= ~(1 << 6);
}

void motor_inv(void) {
	PORTB ^= 1 << 6;
}

void buzzer_up(void) {
	PORTB |= 1 << 7;
	PORTC &= ~(1 << 0);
}

void buzzer_down(void) {
	PORTB &= ~(1 << 7);
	PORTC |= 1 << 0;
}

void buzzer_inv(void) {
	PORTB ^= 1 << 7;
	PORTC ^= 1 << 0;
}

void buzzer_off(void) {
	PORTB &= ~(1 << 7);
	PORTC &= ~(1 << 0);
}


// uses the 76k ISR dividet by 4 -> 4 * 19 == 76
void wait_ms(uint16_t ms) {
	wait_time = ms * (int16_t) 19;
	while(wait_time > 0) {
		test_stop_app();
	}
}

void wait_ticks(int16_t ticks) {
	wait_time = ticks;

	while(wait_time > 0) {
		test_stop_app();
	}
}

void ir_on(void) {
	ir_active = 1;
}

void ir_off(void) {
	ir_active = 0;
	PORTD &= ~(1 << 2);
}

void ir_inv(void) {
	if(ir_active) {
		ir_off();
	} else {
		ir_on();
	}
}

void ir_set(uint8_t state) {
	if(state) {
		ir_on();
	} else {
		ir_off();
	}
}

uint8_t ir_recv(void) {
	return !(PIND & (1 << 3));
}

