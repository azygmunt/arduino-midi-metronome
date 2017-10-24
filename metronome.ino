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

#define GREENBUT 0
#define YELLOWBUT 1
#define REDBUT 2

#define LIGHTSW 3
#define SWSUBDIV buttonState[4]
#define SWMIDI buttonState[5]

#define SWLIGHTFLAG buttonFlag[3]
#define SWSUBDIVFLAG buttonFlag[4]
#define SWMIDIFLAG buttonFlag[5]

//RGB values for events
byte downbeat[3] = { 255, 255, 255 };
byte beat[3] = { 40, 40, 40 };
byte beat_and[3] = { 0, 0, 32 };

//set up the LED 7-segment display
//int dataPin, int clkPin, int csPin, int numDevices=1
LedControl disp = LedControl(5, 7, 6, 1);

// the current reading from the input pin
boolean buttonState[6] = { HIGH, HIGH, HIGH, HIGH, HIGH, HIGH };
boolean buttonFlag[6] = { false, false, false, false, false, false };

// the previous reading from the input pin
//boolean lastButtonState[6] = { HIGH, HIGH, HIGH, HIGH, HIGH, HIGH };

// the last time the output pin was toggled
//unsigned long lastDebounceTime[6] = { 0, 0, 0, 0, 0, 0 };

//only need to check time on green and yellow buttons, which are 0 and 1
unsigned long buttonTime[2] = { 0, 0 };

//counters
unsigned int cc = 0; //clock counter
unsigned int bc = 0; //beat counter
//unsigned int nc = 0; //note counter
unsigned int mc = 0; //measure counter

//free mode tempo
unsigned int tempo = 120;
const unsigned long tempo_blink = 10; //length of light blink in ms
const unsigned long tempo_hold_start_length = 250; //starting delay for tempo hold inc/dec
const unsigned long tempo_hold_end_length = 25; //shortest delay for tempo hold inc/dec
const float tempo_hold_inc = 0.9; //rate for hold increase speed
unsigned long tempo_hold_time = tempo_hold_start_length; //duration of hold inc/dec. Decreases to tempo_hold_end_length as the button is held
unsigned long tempo_msp; //holds the millisecond of the previous beat
//unsigned long tempo_msp = millis(); //holds the millisecond of the previous beat
unsigned long tempo_ms = 60000 / tempo;

//flag for keeping track of tempo light
boolean tempolitFlag = false;

//midi states
int action = 2; //0 =note off ; 1=note on ; 2= nothing
boolean clockFlag = false; // whether or not clock play has been received

//sonar's midi clock note
//const int tick_note = 42;

//default time signature
unsigned int timesig = 4;

//time signature subdivision
byte sd = 2;
byte sdcc = 12;

unsigned long sd_msp;
//unsigned long sd_msp = millis();
unsigned long sd_ms = 60000 / (tempo * sd);

//flag for keeping track of subdivided tempo light
boolean sd_lit = false;

//flag for keeping track of the song position
boolean spFlag = false;

boolean noteFlag = false;

byte _ledTable[256] = { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3,
		3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9,
		9, 9, 10, 10, 10, 11, 11, 12, 12, 12, 13, 13, 14, 14, 15, 15, 15, 16,
		16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 22, 22, 23, 23, 24, 25, 25, 26,
		26, 27, 28, 28, 29, 30, 30, 31, 32, 33, 33, 34, 35, 36, 36, 37, 38, 39,
		40, 40, 41, 42, 43, 44, 45, 46, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,
		56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 67, 68, 69, 70, 71, 72, 73, 75,
		76, 77, 78, 80, 81, 82, 83, 85, 86, 87, 89, 90, 91, 93, 94, 95, 97, 98,
		99, 101, 102, 104, 105, 107, 108, 110, 111, 113, 114, 116, 117, 119,
		121, 122, 124, 125, 127, 129, 130, 132, 134, 135, 137, 139, 141, 142,
		144, 146, 148, 150, 151, 153, 155, 157, 159, 161, 163, 165, 166, 168,
		170, 172, 174, 176, 178, 180, 182, 184, 186, 189, 191, 193, 195, 197,
		199, 201, 204, 206, 208, 210, 212, 215, 217, 219, 221, 224, 226, 228,
		231, 233, 235, 238, 240, 243, 245, 248, 250, 253, 255 };

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
	MIDI.setHandleControlChange(handleControlChange);

	pinMode(pin_R, OUTPUT);
	pinMode(pin_G, OUTPUT);
	pinMode(pin_B, OUTPUT);

	pinMode(b0, INPUT);
	pinMode(b1, INPUT);
	pinMode(b2, INPUT);
	pinMode(sw0, INPUT);
	pinMode(sw1, INPUT);
	pinMode(sw2, INPUT);

	digitalWrite(b0, HIGH);
	digitalWrite(b1, HIGH);
	digitalWrite(b2, HIGH);
	digitalWrite(sw0, HIGH);
	digitalWrite(sw1, HIGH);
	digitalWrite(sw2, HIGH);

	startupAnimation();
	lightOff();
}

void incrementClock() {
	if (!noteFlag) {
		lightOff();
		if (clockFlag) { //make sure a start or continue event was received
			disp.setLed(0, 1, 0, true); //turn on dot after measure to show incoming clock
			if (cc == 0) { //first of the 1/24th beat clock signals - flash a light
				//increment the beat counter
				++bc;
				showBeat(2);
				if (SWSUBDIV== LOW) {
					disp.setLed(0, 3, 0, true);
				}

				//reset the beat counter and increment the measure counter based on the time signature
				if (bc >= timesig) {
					bc = 0;
				} else if (bc == 1) {
					//only if there is no sonar note counter
					++mc;
					printDigits(mc, 0, 1);
				}
			} else if (cc % sdcc == 0) {
				if (SWSUBDIV== LOW) {
					lightRGB(beat_and);
					disp.setLed(0, 3, 0, true);
				}
			} else if (cc % sdcc == 1) {
				disp.setLed(0, 3, 0, false);
			}

			//increment clock counter and reset it on the beat
			++cc;
			if (cc == 24) {
				cc = 0;
			}
		}
	}
}

void startClock() {
	clockFlag = true;
	if (!spFlag) {
		cc = 1;
		bc = 1;
		mc = 1;
	}
	spFlag = false;
	//set the display to " 1 1"
	disp.setChar(0, 0, ' ', false);
	disp.setDigit(0, 1, 1, false);
	showBeat(2);
}

void stopClock() {
	//disable all lights
	lightOff();

	//turn off clock indicator dot
	disp.setLed(0, 1, 0, false);

	clockFlag = false;
}

void continueClock() {
	clockFlag = true;
}

void handleSongPosition(unsigned int beats) {
	mc = beats / (timesig * 4) + 1;
	bc = beats % (timesig * 4) + 1;
	cc = 1;
	printDigits(mc, 0, 1);
	printDigits(bc, 2, 3);
	spFlag = true;
}

void handleControlChange(byte inChannel, byte inNumber, byte inValue) {
	switch (inNumber) {
	case 12:
		downbeat[0] = inValue >> 1;
		break;
	case 13:
		downbeat[1] = inValue >> 1;
		break;
	case 14:
		downbeat[2] = inValue >> 1;
		break;
	case 15:
		beat[0] = inValue >> 1;
		break;
	case 16:
		beat[1] = inValue >> 1;
		break;
	case 17:
		beat[2] = inValue >> 1;
		break;
	}
}

void handleNoteOn(byte inChannel, byte inNote, byte inVelocity) {
//use sonar's default metronome note
	switch (inNote) {
	case 42:
		//if the velocity is 127 it is a downbeat. otherwise it is a regular beat. color accordingly
		if (inVelocity == 127) {
			bc = 1;
			lightRGB(downbeat);
			printDigits(mc, 0, 1);
			++mc;
		} else {
			++bc;
			lightRGB(beat);
		}
		printDigits(bc, 2, 3);
		noteFlag = true;
		break;
	case 0:
		downbeat[0] = inVelocity >> 1;
		break;
	case 1:
		downbeat[1] = inVelocity >> 1;
		break;
	case 2:
		downbeat[2] = inVelocity >> 1;
		break;
	case 3:
		beat[0] = inVelocity >> 1;
		break;
	case 4:
		beat[1] = inVelocity >> 1;
		break;
	case 5:
		beat[2] = inVelocity >> 1;
		break;
	}
}

void handleNoteOff(byte inChannel, byte inNote, byte inVelocity) {
	if (inNote == 42)
		lightOff();
}

void loop() {
// read the buttons and switches
// layout:
// sw0 sw1 sw2 b0(g) b1(y) b2(r)
// [3] [4] [5] [0]   [1]   [2]

	readButtons();

// midi mode on switch 3
	if (SWMIDI== LOW) {
		modeTempo();
		SWMIDIFLAG=true;
	}

	else {
		if(SWMIDIFLAG) {
			disp.setChar(0, 0, '-', false);
			disp.setChar(0, 1, '-', false);
			disp.setChar(0, 2, '-', false);
			disp.setChar(0, 3, '-', false);
			SWMIDIFLAG=false;
		}
		modeMidi();
	}
}

//routine that deals with keeping track of beats based on midi notes
void modeMidi() {
	//subdivision buttons
	if (SWSUBDIV== LOW) {
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
	noteFlag = false;
	// reset display when red button is pressed
	if (buttonState[REDBUT] == LOW) {
		if (buttonFlag[REDBUT] == false) {
			disp.setChar(0, 0, '-', false);
			disp.setChar(0, 1, '-', false);
			disp.setChar(0, 2, '-', false);
			disp.setChar(0, 3, '-', false);
			lightOff();
			buttonFlag[REDBUT] = true;
		}

		// increment/decrement time signature with green/yellow buttons while the red button is held
		buttonsTimesig();
	}

	else {
		//reset the counters when the red button is released
		if (buttonFlag[REDBUT] == true) {
			mc = 0;
			bc = 0;
			buttonFlag[REDBUT] = false;
			tempo_msc = tempo_msp + tempo_ms; //set current time to just before the beat
		}

		//beat subdivision buttons if sw1 is flipped
		if (SWSUBDIV== LOW) {
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

			sd_msp = tempo_msc;

			//if neither button is pressed, show the measure and two-digit beats
			if (buttonFlag[GREENBUT] == false && buttonFlag[YELLOWBUT] == false) {
				tempolitFlag = true;
				printDigits(mc + 1, 0, 1);
				showBeat(2);
				disp.setLed(0, 1, 0, true); //turn on dot after measure
				if (SWSUBDIV== LOW) {
					disp.setLed(0, 3, 0, true); //turn on dot after beat
				}
			} else { //otherwise, the tempo is showing on the first 3 digits.
				//only use 1 digit for the beat and hide the measure
				showBeat(1);
				tempolitFlag = true;
				if (SWSUBDIV == LOW) {
					disp.setLed(0, 3, 0, true); //turn on dot after beat
				}
			}
		}

		//turn off the light after the specified blink time
		else if (tempo_msc >= tempo_msp + tempo_blink && tempolitFlag) {
			lightOff();
			tempolitFlag = false;
			disp.setLed(0, 3, 0, false); //turn off dot after beat
		}

		//flash subdivided beat
		else if (SWSUBDIV == LOW) {
			if (tempo_msc - sd_msp > sd_ms) {
				sd_msp = tempo_msc;
				lightRGB(beat_and);
				sd_lit = true;
				disp.setLed(0, 3, 0, true); //turn on dot after beat
			}
			else if (tempo_msc >= sd_msp + tempo_blink && sd_lit) {
				lightOff();
				sd_lit = false;
				disp.setLed(0, 3, 0, false); //turn off dot after beat
			}
		}
	}
}

void readButtons() {
	const byte inputs[6] = { b0, b1, b2, sw0, sw1, sw2 };
	const unsigned long debounceDelay = 50; // the debounce time; increase if the output flickers
	static unsigned long lastDebounceTime[6] = { 0, 0, 0, 0, 0, 0 }; // the last time the output pin was toggled
	static boolean lastButtonState[6] = { HIGH, HIGH, HIGH, HIGH, HIGH, HIGH }; // the previous reading from the input pin
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
	if (buttonState[GREENBUT] == LOW) {
		if (buttonFlag[GREENBUT] == false) {
			buttonFlag[GREENBUT] = true;
			timesig--;
			if (timesig <= 1) {
				timesig = 1;
			}
			printDigits(timesig, 2, 3);
		}
	} else {
		buttonFlag[GREENBUT] = false;
	}

	// increment the time signature with the yellow button. upper limit 99
	if (buttonState[YELLOWBUT] == LOW) {
		if (buttonFlag[YELLOWBUT] == false) {
			buttonFlag[YELLOWBUT] = true;
			timesig++;
			if (timesig >= 99) {
				timesig = 99;
			}
			printDigits(timesig, 2, 3);
		}
	} else {
		buttonFlag[YELLOWBUT] = false;
	}
}

void buttonsSubdiv() {
	// decrement the beat subdivider with the green button. lower limit 2
	if (buttonState[GREENBUT] == LOW) {
		if (buttonFlag[GREENBUT] == false) {
			buttonFlag[GREENBUT] = true;
			sd--;
			if (sd <= 2) {
				sd = 2;
			}
			sd_ms = 60000 / (tempo * sd);
			sdcc = 24 / sd;
			printDigits(sd, 3, 3);
		}
	} else {
		buttonFlag[GREENBUT] = false;
	}

	// increment the beat subdivider with the yellow button. upper limit 4
	if (buttonState[YELLOWBUT] == LOW) {
		if (buttonFlag[YELLOWBUT] == false) {
			buttonFlag[YELLOWBUT] = true;
			sd++;
			if (sd >= 4) {
				sd = 4;
			}
			sd_ms = 60000 / (tempo * sd);
			sdcc = 24 / sd;
			printDigits(sd, 3, 3);
		}
	} else {
		buttonFlag[YELLOWBUT] = false;
	}
}

void buttonsTempo() {
	// increment the tempo with the yellow button. no upper limit
	if (buttonState[YELLOWBUT] == LOW) {  //yellow button is pressed
		if (buttonFlag[YELLOWBUT] == false) { //button was up. turn on the flag to mark it as held and store when
			buttonFlag[YELLOWBUT] = true;
			buttonTime[YELLOWBUT] = millis();
			tempo++;
			tempo_ms = 60000 / tempo;
			tempo_hold_time = tempo_hold_start_length;
		} else { //button is held. increment the tempo
			if (millis() > buttonTime[YELLOWBUT] + tempo_hold_time) {
				buttonTime[YELLOWBUT] = millis();
				tempo_hold_time = tempo_hold_time * tempo_hold_inc;
				if (tempo_hold_time < tempo_hold_end_length) {
					tempo_hold_time = tempo_hold_end_length;
				}
				tempo++;
				tempo_ms = 60000 / tempo;
			}
		}
		printDigits(tempo, 0, 2);
		disp.setLed(0, 2, 0, true); //turn on dot after tempo
		sd_ms = 60000 / (tempo * sd);
	} else { //button was released. flag it
		buttonFlag[YELLOWBUT] = false;
	}
	if (buttonState[GREENBUT] == LOW) {  //green button is pressed
		if (buttonFlag[GREENBUT] == false) { //button was up. turn on the flag to mark it as held and store when
			buttonFlag[GREENBUT] = true;
			buttonTime[GREENBUT] = millis();
			tempo--;
			if (tempo <= 1) {
				tempo = 1;
			}
			tempo_ms = 60000 / tempo;
			tempo_hold_time = tempo_hold_start_length;
		} else {
			if (millis() > buttonTime[GREENBUT] + tempo_hold_time) {
				buttonTime[GREENBUT] = millis();
				tempo_hold_time = tempo_hold_time * tempo_hold_inc;
				if (tempo_hold_time < tempo_hold_end_length) {
					tempo_hold_time = tempo_hold_end_length;
				}
				tempo--;
				if (tempo <= 1) {
					tempo = 1;
				}
				tempo_ms = 60000 / tempo;
			}
		}
		printDigits(tempo, 0, 2);
		disp.setLed(0, 2, 0, true); //turn on dot after tempo
		sd_ms = 60000 / (tempo * sd);
	} else { //button was released. flag it
		buttonFlag[GREENBUT] = false;
	}
}

void startupAnimation() {
	lightOff();
	disp.shutdown(0, false);
	disp.clearDisplay(0);
	int j = 0;
	for (int i = 0; i < 24; ++i) {
		disp.setIntensity(0, i * 2 / 3);
		delay(20);
		if (i % 6 == 0) {
			disp.setChar(0, j, '8', true);
			++j;
		}
	}
	j = 0;
	for (int i = 0; i < 24; ++i) {
		disp.setIntensity(0, i * 2 / 3);
		delay(20);
		if (i % 6 == 0) {
			disp.setChar(0, j, '-', false);
			++j;
		}
	}
}

void showBeat(unsigned int d) { //d is the number of digits to display
	if (bc == 1)  //first beat - flash downbeat
		lightRGB(downbeat);
	else
		lightRGB(beat);

	printDigits(bc, 4 - d, 3);
}

void lightRGB(byte rgb[3]) {
	if (buttonState[LIGHTSW] == HIGH) {
		analogWrite(pin_R, _ledTable[rgb[0]]);
		analogWrite(pin_G, _ledTable[rgb[1]]);
		analogWrite(pin_B, _ledTable[rgb[2]]);
	} else
		lightOff();
}

void lightOff() {
	analogWrite(pin_R, 0);
	analogWrite(pin_G, 0);
	analogWrite(pin_B, 0);
}

void printDigits(unsigned int v, byte p1, byte p2) {
	char* view = int2str(v);
	byte range = p2 - p1 + 1;
	int offset = range - strlen(view);

	for (int i = 0; i < range; ++i) {
		if (i < offset)
			disp.setChar(0, i + p1, ' ', false);
		else
			disp.setChar(0, i + p1, view[i - offset], false);
	}

}

char _int2str[7];
char* int2str(register int i) {
	register unsigned char L = 1;
	register char c;
	register boolean m = false;
	register char b;  // lower-byte of i
	// negative
	if (i < 0) {
		_int2str[0] = '-';
		i = -i;
	} else
		L = 0;
	// ten-thousands
	if (i > 9999) {
		c = i < 20000 ? 1 : i < 30000 ? 2 : 3;
		_int2str[L++] = c + 48;
		i -= c * 10000;
		m = true;
	}
	// thousands
	if (i > 999) {
		c = i < 5000 ? (i < 3000 ? (i < 2000 ? 1 : 2) : i < 4000 ? 3 : 4) :
			i < 8000 ? (i < 6000 ? 5 : i < 7000 ? 6 : 7) : i < 9000 ? 8 : 9;
		_int2str[L++] = c + 48;
		i -= c * 1000;
		m = true;
	} else if (m)
		_int2str[L++] = '0';
	// hundreds
	if (i > 99) {
		c = i < 500 ? (i < 300 ? (i < 200 ? 1 : 2) : i < 400 ? 3 : 4) :
			i < 800 ? (i < 600 ? 5 : i < 700 ? 6 : 7) : i < 900 ? 8 : 9;
		_int2str[L++] = c + 48;
		i -= c * 100;
		m = true;
	} else if (m)
		_int2str[L++] = '0';
	// decades (check on lower byte to optimize code)
	b = char(i);
	if (b > 9) {
		c = b < 50 ? (b < 30 ? (b < 20 ? 1 : 2) : b < 40 ? 3 : 4) :
			b < 80 ? (i < 60 ? 5 : i < 70 ? 6 : 7) : i < 90 ? 8 : 9;
		_int2str[L++] = c + 48;
		b -= c * 10;
		m = true;
	} else if (m)
		_int2str[L++] = '0';
	// last digit
	_int2str[L++] = b + 48;
	// null terminator
	_int2str[L] = 0;
	return _int2str;
}
