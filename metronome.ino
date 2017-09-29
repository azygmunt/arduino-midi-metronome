//midi metronome
//by Aaron Zygmunt
//works in a general clock mode with 24 ticks per beat
//or a sonar-specific note based metronome, checks for incoming note

#include <LedControl.h>
#include <MIDI.h>

//define output pins
#define pin_R 9
#define pin_G 10
#define pin_B 11

//define input pins
#define b0 A3
#define b1 A4
#define b2 A5
#define sw0 A0
#define sw1 A1
#define sw2 A2

//RGB values for events
const unsigned int downbeat[3] = { 255, 255, 255 };
const unsigned int beat[3] = { 8, 8, 10 };
const unsigned int beat_and[3] = { 0, 0, 63 };
const unsigned int clock_event[3] = { 0, 1, 0 };

//set up the LED 7-segment display
LedControl disp = LedControl(5, 7, 6, 1);

const int inputs[6] = {
b0, b1, b2, sw0, sw1, sw2 };

//for debouncing input
// the current reading from the input pin
boolean buttonState[6] = { HIGH, HIGH, HIGH, HIGH, HIGH, HIGH };

// the previous reading from the input pin
boolean lastButtonState[6] = { HIGH, HIGH, HIGH, HIGH, HIGH, HIGH };

// the last time the output pin was toggled
unsigned long lastDebounceTime[6] = { 0, 0, 0, 0, 0, 0 };

// the debounce time; increase if the output flickers
unsigned long debounceDelay = 50;

// for storing button states
boolean buttonFlag[6] = { false, false, false, false, false, false };
unsigned long buttonTime[6] = { 0, 0, 0, 0, 0, 0 };

//incoming midi info
byte incomingByte;
byte note;
byte velocity;

//const byte midi_noteon = 0x99; //note on for channel 10
//const byte midi_noteoff = 0x89; //note off for channel 10
const byte midi_noteon = 0x9F; //note on for channel 16
const byte midi_noteoff = 0x8F; //note off for channel 16

//counters
unsigned int cc = 0; //clock counter
unsigned int bc = 0; //beat counter
unsigned int nc = 0; //note counter
unsigned int mc = 0; //measure counter

//free mode tempo
unsigned int tempo = 120;
const unsigned long tempo_blink = 10; //length of light blink in ms
const unsigned long tempo_hold_start_length = 250; //starting delay for tempo hold inc/dec
const unsigned long tempo_hold_end_length = 25; //shortest delay for tempo hold inc/dec
const float tempo_hold_inc = 0.9; //rate for hold increase speed
unsigned long tempo_hold_time = tempo_hold_start_length; //duration of hold inc/dec. Decreases to tempo_hold_end_length as the button is held
unsigned long tempo_msp = millis(); //holds the millisecond of the previous beat
unsigned long tempo_ms = 60000 / tempo;

//flag for keeping track of tempo light
boolean t_lit = false;

//midi states
int action = 2; //0 =note off ; 1=note on ; 2= nothing
boolean clock_flag = false; // whether or not clock play has been received

//sonar's midi clock note
const int tick_note = 42;

//default time signature
unsigned int timesig = 4;

//time signature subdivision
int sd = 2;
int sdcc = 24 / sd;
unsigned long sd_msp = millis();
unsigned long sd_ms = 60000 / (tempo * sd);

//flag for keeping track of subdivided tempo light
boolean sd_lit = false;

//flag for keeping track of the song position
boolean spFlag = false;

MIDI_CREATE_DEFAULT_INSTANCE();

void setup() {
	//start serial with midi baudrate 31250 or 38400 for debugging or 115200 for usb serial
	Serial.begin(115200);
	MIDI.begin(16);
	MIDI.setHandleStart(startClock);
	MIDI.setHandleStop(stopClock);
	MIDI.setHandleContinue(continueClock);
	MIDI.setHandleClock(incrementClock);
	MIDI.setHandleSongPosition(handleSongPosition);
	MIDI.setHandleNoteOn(handleNoteOn);
	MIDI.setHandleNoteOff(handleNoteOff);

	startupAnimation();

	pinMode(pin_R, OUTPUT);
	pinMode(pin_G, OUTPUT);
	pinMode(pin_B, OUTPUT);

	pinMode(b0, INPUT);
	pinMode(b1, INPUT);
	pinMode(b2, INPUT);
	pinMode(sw0, INPUT);
	pinMode(sw1, INPUT);
	pinMode(sw2, INPUT);

	digitalWrite(pin_R, LOW);
	digitalWrite(pin_G, LOW);
	digitalWrite(pin_B, LOW);

	digitalWrite(b0, HIGH);
	digitalWrite(b1, HIGH);
	digitalWrite(b2, HIGH);
	digitalWrite(sw0, HIGH);
	digitalWrite(sw1, HIGH);
	digitalWrite(sw2, HIGH);
}

void incrementClock() {
	lightOff();
	if (clock_flag) {
		disp.setLed(0, 1, 0, true); //turn on dot after measure to show incoming clock
		if (cc == 0) { //first of the 1/24th beat clock signals - flash a light
			if (nc == 0) { // if the note counter is 0, we are using a raw timing clock and not sonar's midi notes
				//increment the beat counter
				++bc;
				flashBeat(2);
				if (buttonState[4] == LOW) {
					disp.setLed(0, 3, 0, true);
				}

				//reset the beat counter and increment the measure counter based on the time signature
				if (bc >= timesig) {
					bc = 0;
				} else if (bc == 1) {
					//only if there is no sonar note counter
					if (nc == 0) {
						++mc;
						printDigits(mc, 0, 1);
					}
				}
			}
		} else if (cc % sdcc == 0) {
			if (buttonState[4] == LOW) {
				lightRGB(beat_and);
				disp.setLed(0, 3, 0, true);
			}
		} else if (cc % sdcc == 1) {
			lightOff();
			disp.setLed(0, 3, 0, false);
		}

		//increment clock counter and reset it on the beat
		++cc;
		if (cc == 24) {
			cc = 0;
		}
	}
}

void startClock() {
	clock_flag = true;
	if (!spFlag) {
		cc = 1;
		bc = 1;
		mc = 1;
	}
	nc = 0;
	spFlag = false;
	//set the display to " 1 1"
	disp.setChar(0, 0, ' ', false);
	disp.setDigit(0, 1, 1, false);
	disp.setChar(0, 2, ' ', false);
	disp.setDigit(0, 3, 1, false);
}

void stopClock() {
	//disable all lights
	lightOff();

	//turn off clock indicator dot
	disp.setLed(0, 1, 0, false);

	clock_flag = false;
}

void continueClock() {
	clock_flag = true;
}

void handleSongPosition(unsigned int beats) {
	mc = beats / (timesig * 4) + 1;
	bc = beats % (timesig * 4) + 1;
	cc = 1;
	printDigits(mc, 0, 1);
	printDigits(bc, 2, 3);
	spFlag = true;
}

void handleNoteOn(byte inChannel, byte inNote, byte inVelocity) {
	//use sonar's default metronome note
	if (inNote == 42) {
		//if the velocity is 127 it is a downbeat. otherwise it is a regular beat. color accordingly
		if (inVelocity == 127) {
			lightRGB(downbeat);
			nc = 1;
			printDigits(mc, 0, 1);
			++mc;
		} else {
			lightRGB(beat);
			++nc;
		}
		printDigits(nc, 2, 3);
	}
}

void handleNoteOff(byte inChannel, byte inNote, byte inVelocity) {
	lightOff();
}

void loop() {
// read the buttons and switches
// layout:
// sw0 sw1 sw2 b0(g) b1(y) b2(r)
// [3] [4] [5] [0]   [1]   [2]
	readButtons();

// free mode on switch 3
	if (buttonState[5] == LOW) {
		modeTempo();
	}

	else {
		modeMidi();
	}
}

//routine that deals with keeping track of beats based on midi notes
void modeMidi() {
//subdivision buttons
	if (buttonState[4] == LOW) {
		buttonsSubdiv();
	}
//increment/decrement time signature with green/yellow buttons
	else {
		buttonsTimesig();
	}

	MIDI.read();

}

//routine that deals with keeping track of beats based on milliseconds
void modeTempo() {
	unsigned long tempo_msc = millis(); //current tempo time counter

// reset display while red button is held
	if (buttonState[2] == LOW) {
		if (buttonFlag[2] == false) {
			disp.setChar(0, 0, '-', false);
			disp.setChar(0, 1, '-', false);
			disp.setChar(0, 2, '-', false);
			disp.setChar(0, 3, '-', false);
			lightOff();
			buttonFlag[2] = true;
		}

		// increment/decrement time signature with green/yellow buttons while the red button is held
		buttonsTimesig();
	} else {
		//reset the counters when the red button is released
		if (buttonFlag[2] == true) {
			mc = 0;
			bc = 0;
			buttonFlag[2] = false;
			tempo_msc = tempo_msp + tempo_ms; //set current time to just before the beat
		}

		//beat subdivision buttons if sw1 is flipped
		if (buttonState[4] == LOW) {
			buttonsSubdiv();
		}

		//check the yellow/green buttons and increment/decrement the tempo
		else {
			buttonsTempo();
		}

		//check to see if the time passed is greater than the length of a beat
		if (tempo_msc - tempo_msp > tempo_ms) {
			// store current time to measure against
			tempo_msp = tempo_msc;

			// new beat - increment counters
			// reset beat counter based on time signature. count new measure
			if (bc >= timesig) {
				bc = 0;
				++mc;
			}
			++bc;

			//reset the beat subdivision counter
			sdcc = 1;
			sd_msp = tempo_msc;

			//if neither button is pressed, show the measure and two-digit beats
			if (buttonFlag[0] == false && buttonFlag[1] == false) {
				flashBeat(2);
				t_lit = true;
				printDigits(mc + 1, 0, 1);
				disp.setLed(0, 1, 0, true); //turn on dot after measure
				if (buttonState[4] == LOW) {
					disp.setLed(0, 3, 0, true); //turn on dot after beat
				}
			} else { //otherwise, the tempo is showing on the first 3 digits.
				//only use 1 digit for the beat and hide the measure
				flashBeat(1);
				t_lit = true;
				if (buttonState[4] == LOW) {
					disp.setLed(0, 3, 0, true); //turn on dot after beat
				}
			}
		}

		//turn off the light after the specified blink time
		if (tempo_msc >= tempo_msp + tempo_blink && t_lit) {
			lightOff();
			t_lit = false;
			disp.setLed(0, 3, 0, false); //turn off dot after beat
		}

		//flash subdivided beat
		if (buttonState[4] == LOW) {
			if (tempo_msc - sd_msp > sd_ms) {
				sd_msp = tempo_msc;
				lightRGB(beat_and);
				sd_lit = true;
				disp.setLed(0, 3, 0, true); //turn on dot after beat
			}
		}
		if (tempo_msc >= sd_msp + tempo_blink && sd_lit) {
			lightOff();
			sd_lit = false;
			disp.setLed(0, 3, 0, false); //turn off dot after beat
		}
	}
}

void readButtons() {
	int reading[6];
	for (int i = 0; i < 6; ++i) {
		reading[i] = digitalRead(inputs[i]);
		if (reading[i] != lastButtonState[i]) {
			lastDebounceTime[i] = millis();
		}
		if ((millis() - lastDebounceTime[i]) > debounceDelay) {
			// if the button state has changed:
			if (reading[i] != buttonState[i]) {
				buttonState[i] = reading[i];
			}
		}
		lastButtonState[i] = reading[i];
	}
}

void buttonsTimesig() {
// decrement the time signature with the green button. lower limit 1
	if (buttonState[0] == LOW) {
		if (buttonFlag[0] == false) {
			buttonFlag[0] = true;
			timesig--;
			if (timesig <= 1) {
				timesig = 1;
			}
			printDigits(timesig, 2, 3);
		}
	} else {
		buttonFlag[0] = false;
	}

// increment the time signature with the yellow button. upper limit 99
	if (buttonState[1] == LOW) {
		if (buttonFlag[1] == false) {
			buttonFlag[1] = true;
			timesig++;
			if (timesig >= 99) {
				timesig = 99;
			}
			printDigits(timesig, 2, 3);
		}
	} else {
		buttonFlag[1] = false;
	}
}

void buttonsSubdiv() {
// decrement the beat subdivider with the green button. lower limit 2
	if (buttonState[0] == LOW) {
		if (buttonFlag[0] == false) {
			buttonFlag[0] = true;
			sd--;
			if (sd <= 2) {
				sd = 2;
			}
			sd_ms = 60000 / (tempo * sd);
			sdcc = 24 / sd;
			printDigits(sd, 3, 3);
		}
	} else {
		buttonFlag[0] = false;
	}

// increment the beat subdivider with the yellow button. upper limit 4
	if (buttonState[1] == LOW) {
		if (buttonFlag[1] == false) {
			buttonFlag[1] = true;
			sd++;
			if (sd >= 4) {
				sd = 4;
			}
			sd_ms = 60000 / (tempo * sd);
			sdcc = 24 / sd;
			printDigits(sd, 3, 3);
		}
	} else {
		buttonFlag[1] = false;
	}
}

void buttonsTempo() {
// increment the tempo with the yellow button. no upper limit
	if (buttonState[1] == LOW) {  //yellow button is pressed
		if (buttonFlag[1] == false) { //button was up. turn on the flag to mark it as held and store when
			buttonFlag[1] = true;
			buttonTime[1] = millis();
			tempo++;
			tempo_ms = 60000 / tempo;
			printDigits(tempo, 0, 2);
			disp.setLed(0, 2, 0, true); //turn on dot after tempo
			tempo_hold_time = tempo_hold_start_length;
		} else { //button is held. increment the tempo
			if (millis() > buttonTime[1] + tempo_hold_time) {
				buttonTime[1] = millis();
				tempo_hold_time = tempo_hold_time * tempo_hold_inc;
				if (tempo_hold_time < tempo_hold_end_length) {
					tempo_hold_time = tempo_hold_end_length;
				}
				tempo++;
				tempo_ms = 60000 / tempo;
				printDigits(tempo, 0, 2);
				disp.setLed(0, 2, 0, true); //turn on dot after tempo
			}
		}
	} else { //button was released. flag it
		buttonFlag[1] = false;

		// decrement the tempo with the green button. lower limit 1
		if (buttonState[0] == LOW) {  //green button is pressed
			if (buttonFlag[0] == false) { //button was up. turn on the flag to mark it as held and store when
				buttonFlag[0] = true;
				buttonTime[0] = millis();
				tempo--;
				if (tempo <= 1) {
					tempo = 1;
				}
				tempo_ms = 60000 / tempo;
				printDigits(tempo, 0, 2);
				disp.setLed(0, 2, 0, true); //turn on dot after tempo
				tempo_hold_time = tempo_hold_start_length;
			} else {
				if (millis() > buttonTime[0] + tempo_hold_time) {
					buttonTime[0] = millis();
					tempo_hold_time = tempo_hold_time * tempo_hold_inc;
					if (tempo_hold_time < tempo_hold_end_length) {
						tempo_hold_time = tempo_hold_end_length;
					}
					tempo--;
					if (tempo <= 1) {
						tempo = 1;
					}
					tempo_ms = 60000 / tempo;
					printDigits(tempo, 0, 2);
					disp.setLed(0, 2, 0, true); //turn on dot after tempo
				}
			}
		} else { //button was released. flag it
			buttonFlag[0] = false;
			disp.setLed(0, 2, 0, false);
		}
	}
}

void startupAnimation() {
	lightOff();
	disp.shutdown(0, false);
	disp.clearDisplay(0);
	int j = 0;
	for (int i = 0; i < 24; ++i) {
		disp.setIntensity(0, i * 2 / 3);
		delay(50);
		if (i % 6 == 0) {
			disp.setChar(0, j, '8', true);
			++j;
		}
	}
	j = 0;
	for (int i = 0; i < 24; ++i) {
		disp.setIntensity(0, i * 2 / 3);
		delay(50);
		if (i % 6 == 0) {
			disp.setChar(0, j, '-', false);
			++j;
		}
	}
}

void flashBeat(unsigned int d) { //d is the number of digits to display
	if (bc == 1) { //first beat - flash downbeat
		lightRGB(downbeat);
	} else { //regular beat - flash beat color
		lightRGB(beat);
	}
//show the beat on the 7-seg display
	printDigits(bc, 4 - d, 3);
}

void lightRGB(const unsigned int rgb[3]) {
	if (buttonState[3] == HIGH) {
		analogWrite(pin_R, rgb[0]);
		analogWrite(pin_G, rgb[1]);
		analogWrite(pin_B, rgb[2]);
	} else {
		lightOff();
	}
}

void lightOff() {
	digitalWrite(pin_R, LOW);
	digitalWrite(pin_G, LOW);
	digitalWrite(pin_B, LOW);
}

//print a number on the display. args are the number to display and to and from column places.
//higher powers are discarded if there are insufficient display digits. ex 123->23
void printDigits(unsigned int v, unsigned int p1, unsigned int p2) {
//p1 and p2 are led digit positions (from p1 to p2) valid range 0..3
	unsigned int d[4]; //digits
	unsigned int ex[4] = { 10, 100, 1000, 10000 };
	int range = p2 - p1 + 1;
	int c = v;

//ditch negatives
//if (v < 0) {
//	return;
//}

//chop off preceding digits
	for (int i = 0; i < range; ++i) {
		d[i] = c % 10;
		c = c / 10;
		if (d[i] == 0 && ex[i] > v) {
			disp.setChar(0, p2 - i, ' ', false);
		} else {
			disp.setDigit(0, p2 - i, (byte) d[i], false);
		}
	}
}
