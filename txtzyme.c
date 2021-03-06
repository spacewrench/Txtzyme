/* Simple example for Teensy USB Development Board
 * http://www.pjrc.com/teensy/
 * Copyright (c) 2008 PJRC.COM, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <util/delay.h>
#include "usb_serial.h"
#include "analog.h"

#define LED_CONFIG	(DDRD |= (1<<6))
#define LED_OFF		(PORTD &= ~(1<<6))
#define LED_ON		(PORTD |= (1<<6))
#define CPU_PRESCALE(n) (CLKPR = 0x80, CLKPR = (n))
#define BIGBUF      1024

void send_str(const char *s);
uint16_t recv_str(char *buf, uint16_t size);
void parse(const char *buf);

// Basic command interpreter for controlling port pins
int main(void) {
	char buf[BIGBUF];
	uint8_t n;

	// set for 16 MHz clock, and turn on the LED
	CPU_PRESCALE(0);
	LED_CONFIG;
	LED_ON;

	// normal mode, 16 MHz, count to 2^16 and roll over
	TCCR1A = 0x00;
	TCCR1B = _BV(CS10);
	TCCR1C = 0x00;

	// initialize the USB, and then wait for the host
	// to set configuration.  If the Teensy is powered
	// without a PC connected to the USB port, this
	// will wait forever.
	usb_init();
	while (!usb_configured()) /* wait */ ;
	_delay_ms(1000);

	while (1) {
		// wait for the user to run their terminal emulator program
		// which sets DTR to indicate it is ready to receive.
		while (!(usb_serial_get_control() & USB_SERIAL_DTR)) /* wait */ ;

		// discard anything that was received prior.  Sometimes the
		// operating system or other software will send a modem
		// "AT command", which can still be buffered.
		// usb_serial_flush_input();

		// and then listen for commands and process them
		while (1) {
			// send_str(PSTR("> "));
			n = recv_str(buf, sizeof(buf));
			if (n == (uint16_t)(~0)) break;
			// send_str(PSTR("\r\n"));
			parse(buf);
		}
	}
}

// Send a string to the USB serial port.  The string must be in
// flash memory, using PSTR
//
void send_str(const char *s) {
	char c;
	while (1) {
		c = pgm_read_byte(s++);
		if (!c) break;
		usb_serial_putchar(c);
	}
}

void send_num(const uint16_t num) {
	if (num > 9) {
		send_num(num/10);
	}
	usb_serial_putchar('0' + num%10);
}		

// Receive a string from the USB serial port.  The string is stored
// in the buffer and this function will not exceed the buffer size.
// A carriage return or newline completes the string, and is not
// stored into the buffer.
// The return value is the number of characters received, or 255 if
// the virtual serial connection was closed while waiting.
//
uint16_t recv_str(char *buf, uint16_t size) {
	int16_t r;
	uint16_t count=0;

	while (count < (size-1)) {
		r = usb_serial_getchar();
		if (r != -1) {
			if (r == '\r' || r == '\n') break;
			if (r >= ' ' && r <= '~') {
				*buf++ = r;
				// usb_serial_putchar(r);
				count++;
			}
		} else {
			if (!usb_configured() ||
			  !(usb_serial_get_control() & USB_SERIAL_DTR)) {
				// user no longer connected
				*buf = 0;
				return ~0;
			}
			// just a normal timeout, keep waiting
		}
	}
	*buf = 0;
	return 0;
}

// parse a user command and execute it, or print an error message
//

uint8_t port = 'd'-'a';
uint8_t pin = 6;
uint16_t x = 0;

void parse(const char *buf) {
	uint16_t count = 0;
	const char *loop = 0;
    uint8_t sreg = SREG;
    volatile uint8_t *pinx = (volatile uint8_t *)(0x20 + port * 3);;
    volatile uint8_t *ddrx = (volatile uint8_t *)(0x21 + port * 3);;
    volatile uint8_t *portx = (volatile uint8_t *)(0x22 + port * 3);
	char ch;
	while ((ch = *buf++)) {
		switch (ch) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				x = ch - '0';
				while (*buf >= '0' && *buf <= '9') {
					x = x*10 + (*buf++ - '0');
				}
				break;
			case 'p':
				send_num(x);
				send_str(PSTR("\r\n"));
				break;
			case 'a':
			case 'b':
			case 'c':
			case 'd':
			case 'e':
			case 'f':
				port = ch - 'a';
				pin = x % 8;
                pinx  = (volatile uint8_t *)(0x20 + port * 3);
                ddrx  = (volatile uint8_t *)(0x21 + port * 3);
                portx = (volatile uint8_t *)(0x22 + port * 3);
				break;
			case 'i':
                *ddrx &= ~(1 << pin);
			case 'r':
                x = (*pinx & (1 << pin)) ? 1 : 0;
                TCNT1 = 0; TIFR1 = _BV(TOV1);
				break;
			case 'I':
                *ddrx  &= ~(1 << pin);
                *portx |=  (1 << pin);
                x = (*pinx & (1 << pin)) ? 1 : 0;
                TCNT1 = 0; TIFR1 = _BV(TOV1);
				break;
			case 'z':
                *ddrx  &= ~(1 << pin);
                *portx &= ~(1 << pin);
                x = (*pinx & (1 << pin)) ? 1 : 0;
                TCNT1 = 0; TIFR1 = _BV(TOV1);
				break;
			case '^':
                *pinx = (1 << pin);
                TCNT1 = 0; TIFR1 = _BV(TOV1);
				break;
			case 'o':
				if (x % 2) {
                    *portx |=  (1 << pin);
				} else {
                    *portx &= ~(1 << pin);
				}
                *ddrx |= (1 << pin);
                TCNT1 = 0; TIFR1 = _BV(TOV1);
				break;
			case 'm':
				for (uint16_t xx = x; xx; --xx) _delay_us( 995 );
				TCNT1 = 0; TIFR1 = _BV(TOV1);
				break;
			case 'u':
                if ((TIFR1 & _BV(TOV1)) || (x & 0xf000)) {
                    // Counter1 has overflowed, or (x >= 0x1000)
                    // Use standard delay_loop tactics -- but note:
                    // won't work for delays > 0x4000us.
                    _delay_loop_2( x * (F_CPU/4000000UL) );
                } else {
                    // Just wait for Counter1 to get to x * 16
                    while (TCNT1 < (x << 4)) ;
                }
                TCNT1 = 0; TIFR1 = _BV(TOV1);
				break;
			case '{':
				count = x;
				loop = buf;
				while ((ch = *buf++) && ch != '}') {
				}
                // FALLS THROUGH!
			case '}':
				if (count) {
					count--;
					buf = loop;
				}
				break;
            case '[':
                sreg = SREG;
                cli( );
                break;
            case ']':
                SREG = sreg;
                break;
			case 'k':
				x = count;
				break;
			case '_':
				while ((ch = *buf++) && ch != '_') {
					usb_serial_putchar(ch);
				}
				send_str(PSTR("\r\n"));
				break;
			case 's':
				x = analogRead(x);
				break;
			case 'v':
				#define QUOTEME_(x) #x
				#define QUOTEME(x) QUOTEME_(x)
				send_str(PSTR(QUOTEME(MCU)));
				send_str(PSTR("\r\n"));
				break;
			case 'h':
				send_str(PSTR("Txtzyme [+bigbuf] " __DATE__ " " __TIME__ "\r\n0-9<num>\tenter number\r\n<num>p\t\tprint number\r\n<num>a-f<pin>\tselect pin\r\n<pin>i<num>\tinput\r\n<pin>I<num>\tinput (pull-up enabled)\r\n<pin>z<num>\tinput (pull-up disabled)\r\n<pin>r<num>\tread pin (read only -- don't change input state)\r\n<pin><num>o\toutput\r\n<pin>^\t\ttoggle pin\r\n<num>m\t\tmsec delay\r\n<num>u\t\tusec delay\r\n<num>{}\t\trepeat\r\n[<code>]\texecute <code> with interrupts off\r\nk<num>\t\tloop count\r\n_<words>_\tprint words\r\n<num>s<num>\tanalog sample\r\nv\t\tprint version\r\n<pin>R<num>\tread OneWire bit\r\n<pin><num>W\twrite OneWire bit\r\nh\t\tprint help\r\n<pin>t<num>\tpulse width\r\n"));
				break;
			case 't':
				*(uint8_t *)(0x21 + port * 3) &= ~(1 << pin);		// direction = input
				{
					volatile uint8_t *tport = (uint8_t *)(0x20 + port * 3);
					uint8_t tpin = 1 << pin;
					uint8_t tstate = *tport & tpin;
					uint16_t tcount = 0;
					while (++tcount) {
						if ((*tport & tpin) != tstate) break;
					}
					x = tcount;
					if (!tcount) break;
					while (++tcount) {
						if ((*tport & tpin) == tstate) break;
					}
					x = tcount;
				}
                TCNT1 = 0; TIFR1 = _BV(TOV1);
				break;
            case 'R':	// OneWire Read Bit
                sreg = SREG;
                cli( );
                *portx &= ~(1 << pin);		// Disable port pull-up, ready to pull down
                TCNT1 = 0; TIFR1 = _BV(TOV1);
                *ddrx  |=  (1 << pin);		// set pin to output -- pull down
                while (TCNT1 < 2 * 16 + 8);	// Delay 2.5us -- start pulse
                *ddrx  &= ~(1 << pin);		// Release line (back to input)
                *portx |=  (1 << pin);      // Internal pull-up back on
                while (TCNT1 < 14 * 16) ;
                x = (*pinx & (1 << pin)) ? 1 : 0; // Sample at 14us
                SREG = sreg;
                while (TCNT1 < 120 * 16) ;	// Allow long recovery
                break;
            case 'W':	// OneWire Write Bit
                sreg = SREG;
                cli( );
                *portx &= ~(1 << pin);			    // Disable port pull-up, ready to pull down
                TCNT1 = 0; TIFR1 = _BV(TOV1);
                *ddrx  |=  (1 << pin);			    // set pin to output -- pull down
                if (x & 1) {
                    while (TCNT1 < (2 * 16 + 8)) ;	// Delay 2.5us -- start pulse
                } else {
                    while (TCNT1 < (60 * 16)) ;     // Hold low for 60us
                }
                *ddrx  &= ~(1 << pin);		        // Release line (back to input)
                *portx |=  (1 << pin);		        // Internal pull-up back on
                SREG = sreg;
                while (TCNT1 < (120 * 16)) ;        // 60us recovery (only 1us needed)
                break;
		}
	}
}


