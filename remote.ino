#include <IRremote.h>
#include <IRremoteInt.h>
#include <Time.h>
#define DEBUG true

/* 
 * Alarm Clock w/ IR Remote and Multiplication Shutoff
 * Regular clock w/ alarm set via infrared remote. Display regular time
 * until toggling to set alarm, which starts the LED blinking and enables
 * alarm setting. Disable w/ remote, LED returns to normal, and the
 * regular time is shown again. AM/PM is shown via the bottom-right dot
 * turning on for PM. Piazo plays some tune when alarm time is hit. Can
 * also enable alarm sequence w/ button. Enable/disable the alarm with a
 * button which sets an LED (pin 13). Turn off sounding alarm by solving
 * a math question (multiplication or division of a single digit and
 * double digit number). Control the volume with the knob. 
 * 
 * Written by Ben Sergent V
 * Friday, May 31st, 2019
 * for ECE301 - Intro. to Circuits demo
 */

// Pin mappings
const int PIN_SEGMENT[8] = {
	2,  // A
	6,  // B
	10, // C
	8,  // D
	7,  // E
	3,  // F
	11, // G
	9   // DP
};
const int PIN_DIGIT[4] = {
	A5, // Left
	4,  // Middle-Left
	5,  // Middle-right
	12  // Right
};
const int PIN_STATUS = 13;
const int PIN_PIEZO = A0;
const int PIN_IR_RECEIVER = A3;
const int PIN_SEED = A4;

// Other constants
const int MAP_DIGIT[12] = {
	B11111100, // 0
	B01100000, // 1
	B11011010, // 2
	B11110010, // 3
	B01100110, // 4
	B10110110, // 5
	B10111110, // 6
	B11100000, // 7
	B11111110, // 8
	B11110110, // 9
	B01001010, // /
	B01101110  // *
};
const int DISPLAY_FREQ = 1024; // Hz
const int DISPLAY_SETTLE_STATES = 1;
const int DISPLAY_FREQ_INPUT = 2; // Hz

// State
time_t clock_offset = 0;
int clock_digits[5] = { 0, 0, 0, 0, 0 };
time_t alarm_offset = 0;
int alarm_digits[5] = { 0, 0, 0, 0, 0 };
int game_digits[5] { 0, 10, 0, 0, 0 };
int game_answer = 0;
bool alarm_enabled = false;
bool alarm_editing = false;
bool alarm_sounding = false;
int input_digits[5] = { 0, 0, 0, 0, 0 };
int input_digits_index = 0;
int display_digit_index = 0;
long display_last_switch = 0;
IRrecv irrecv(PIN_IR_RECEIVER);
decode_results results_ir;

// Melody
const unsigned int _B5 = 988;
const unsigned int _A5 = 880;
const unsigned int _E5 = 659;
const unsigned int _C5 = 523;

const unsigned int _B4 = 494;
const unsigned int _Bb4= 466;
const unsigned int _A4 = 440;
const unsigned int _G4 = 392;
const unsigned int _F4 = 349;
const unsigned int _E4 = 329;
const unsigned int _D4 = 293;
const unsigned int _C4 = 261;

const unsigned int _B3 = 246;
const unsigned int _A3 = 220;
const unsigned int _G3 = 196;
const unsigned int _F3 = 174;
const unsigned int _E3 = 165;
const unsigned int _D3 = 147;

unsigned int melody_freqs[92] = {
	_G4, _G4, _G4, _G4, _A4, _A4, _G4, _E4, _C4, // 9
	_C4, _C4, _C4, _D4, _C4, _D4, _E4, // 7
	_G4, _G4, _G4, _G4, _A4, _A4, _G4, _E4, _C4, // 9
	_C4, _C4, _E4, _E4, _E4, _D4, _C4, // 7

	_G4, _G4, _G4, _G4, _A4, _A4, _G4, _E4, _C4, // 9
	_C4, _C4, _D4, _C4, _D4, _E4, // 6
	_G4, _G4, _G4, _G4, _G4, _A4, _A4, _G4, _E4, _C4, // 10
	_C4, _C4, _D4, _E4, _D4, _C4, // 6

	_A4, _A4, _A4, _A4, _G4, _G4, _G4, _Bb4, _Bb4, _Bb4, _G4, _A4, // 12
	_A4, _A4, _G4, _E4, _C4, _C4, _C4, _C4, _D4, _D4, _E4, // 11
	_C4, _C4, _C4, _D4, _D4, _C4 // 6
};
unsigned int melody_durs[92]  = {
	250, 250, 250, 250, 250, 250, 125, 125, 250,
	250, 125, 125, 125, 125, 250, 999,
	250, 250, 250, 250, 250, 250, 125, 125, 250,
	250, 250, 125, 125, 125, 125, 999,

	250, 250, 250, 250, 250, 250, 125, 125, 250,
	250, 250, 125, 125, 250, 999,
	250, 125, 125, 250, 250, 250, 250, 125, 125, 250,
	250, 250, 125, 125, 250, 999,

	125, 125, 500, 250, 250, 250, 500, 250, 250, 375, 125, 999,
	500, 500, 125, 125, 750, 125, 125, 250, 250, 250, 999,
	125, 125, 250, 250, 250, 999
};
unsigned int melody_length = 92;
int melody_index = 0;
time_t melody_last_note_start = 0;

void setup() {
	// Initialize seven-segment display pins
	for (int p = 0; p < 8; p++)
		pinMode(PIN_SEGMENT[p], OUTPUT);
	for (int p = 0; p < 4; p++)
		pinMode(PIN_DIGIT[p], OUTPUT);
	
	// Initialize piezo pins
	pinMode(PIN_PIEZO, OUTPUT);

	// Initialize other pins
	pinMode(PIN_STATUS, OUTPUT);
	pinMode(PIN_IR_RECEIVER, OUTPUT);
	pinMode(PIN_SEED, INPUT);

	if (DEBUG)
		Serial.begin(9600);

	irrecv.enableIRIn();
	setTime(0);
	randomSeed(analogRead(PIN_SEED));
}

void loop() {
	// Check for infrared codes
	if (irrecv.decode(&results_ir)) {
		Serial.println(results_ir.value, HEX);
		irrecv.resume();
		processRemote(results_ir.value);
	}

	// Handle alarm sounding
	updateAlarm();
	if (alarm_sounding) {
		// Start next note if necessary
		if (melody_index == -1 || melody_durs[melody_index] <= millis() - melody_last_note_start) {
			melody_index = (melody_index + 1) % melody_length;
			melody_last_note_start = millis();
			noTone(PIN_PIEZO);
			delay(melody_index == 0 ? 1000 : 25);
			tone(PIN_PIEZO, melody_freqs[melody_index]);
		}
	} else {
		noTone(PIN_PIEZO);
		melody_index == -1;
	}

	// Pulse seven-segment
	updateDisplayDigits();
	if (DEBUG && millis() % 5000 < 1 && false)
		printDigits(clock_digits);
	int selected_digit = display_digit_index / (DISPLAY_SETTLE_STATES + 1);
	int* digits_to_display;
	if (input_digits_index > 0)
		digits_to_display = input_digits;
	else if (alarm_editing)
		digits_to_display = alarm_digits;
	else if (alarm_sounding)
		digits_to_display = game_digits;
	else
		digits_to_display = clock_digits;
	if (display_digit_index % (DISPLAY_SETTLE_STATES + 1) == 0 && !(input_digits_index > 0 && isFreqOn(DISPLAY_FREQ_INPUT))) {
		// Set the cathodes and annodes to display the current digit
		for (int d = 0; d < 4; d++)
			digitalWrite(PIN_DIGIT[d], selected_digit == d ? HIGH : LOW);
		for (int s = 0; s < 8; s++)
			digitalWrite(PIN_SEGMENT[s], bitRead(MAP_DIGIT[digits_to_display[selected_digit]], 7 - s) ? LOW : HIGH);
		
		// Tick second indicator
		if (selected_digit == 1 && getDisplaySecond() && !alarm_editing)
			digitalWrite(PIN_SEGMENT[7], LOW);
	
		// Handle AM/PM
		if (selected_digit == 3 && digits_to_display[4])
			digitalWrite(PIN_SEGMENT[7], LOW);

	} else {
		// Blanking time b/w digits to let LEDs settle
		for (int s = 0; s < 8; s++)
			digitalWrite(PIN_SEGMENT[s], HIGH);
	}

	if (millis() - display_last_switch > (1.0f / DISPLAY_FREQ) * 1000) {
		display_digit_index = (display_digit_index + 1) % ((DISPLAY_SETTLE_STATES + 1) * 4);
		display_last_switch = millis();
	}

	// Status LED
	if (alarm_sounding || alarm_editing) {
		if (isFreqOn(DISPLAY_FREQ_INPUT))
			digitalWrite(PIN_STATUS, HIGH);
		else
			digitalWrite(PIN_STATUS, LOW);
	} else if (alarm_enabled) {
		digitalWrite(PIN_STATUS, HIGH);
	} else {
		digitalWrite(PIN_STATUS, LOW);
	}
}

bool isFreqOn(int freq) {
	return millis() % (int)(1.0f / freq * 1000) >= (1.0f / freq / 2 * 1000);
}

// Decode the modulated IR codes
void processRemote(int code) {
	switch (code) {
		case 0xFF6897: // 0
			processDigitPress(0);
			break;
		case 0xFF30CF: // 1
			processDigitPress(1);
			break;
		case 0xFF18E7: // 2
			processDigitPress(2);
			break;
		case 0xFF7A85: // 3
			processDigitPress(3);
			break;
		case 0xFF10EF: // 4
			processDigitPress(4);
			break;
		case 0xFF38C7: // 5
			processDigitPress(5);
			break;
		case 0xFF5AA5: // 6
			processDigitPress(6);
			break;
		case 0xFF42BD: // 7
			processDigitPress(7);
			break;
		case 0xFF4AB5: // 8
			processDigitPress(8);
			break;
		case 0xFF52AD: // 9
			processDigitPress(9);
			break;
		case 0xFF906F: // EQ
			if (alarm_editing)
				alarm_editing = false;
			else {
				if (alarm_sounding)
					Serial.println("Cannot edit alarm while sounding.");
				else {
					alarm_editing = true;
					Serial.println("Editing alarm.");
				}
			}
			break;
		case 0xFFC23D: // Resume/Pause
			if (alarm_enabled) {
				if (alarm_sounding)
					Serial.println("Cannot disable alarm while sounding.");
				else {
					alarm_enabled = false;
					Serial.println("Disabled alarm.");
				}
			} else {
				alarm_enabled = true;
				Serial.println("Enabled alarm.");
			}
			break;
		case 0xFFB04F: // 200+
			if (input_digits_index > 0) {
				if (input_digits[4]) input_digits[4] = 0;
				else input_digits[4] = 1;
			} else if (alarm_editing) {
				if (alarm_digits[4]) alarm_digits[4] = 0;
				else alarm_digits[4] = 1;
				alarm_offset += 43200;
				alarm_enabled = true;
			} else setTime(now() + 43200);
			Serial.println("AM/PM toggled.");
			break;
		case 0xFFA25D: // CH-
			Serial.print("Millis() = ");
			Serial.println(millis());
			Serial.print("now() = ");
			Serial.println(now());
			break;
		case 0xFF629D: // CH
			Serial.println("Sounding alarm.");
			alarm_offset = now();
			alarm_enabled = true;
			break;
		case 0xFFFFFFFF:
			break;
		default:
			Serial.println("Incorrect button code received.");
	}
}

// Handle press of number button (set input and eventually set time/alarm)
void processDigitPress(int digit) {
	// Save inputted digit
	input_digits[input_digits_index++] = digit;
	printDigits(input_digits);

	// Entered a full set of four digits
	if (input_digits_index == 4) {
		// Set current time or alarm
		if (alarm_editing) {
			alarm_offset = convertDigitsToTimeOffset(input_digits);
			alarm_editing = false;
			alarm_enabled = true;
			for (int d = 0; d < 5; d++)
				alarm_digits[d] = input_digits[d];
		} else if (alarm_sounding) {
			int ans = convertDigitsToNumber(input_digits);
			if (ans == game_answer)
				alarm_enabled = false;
			else
				Serial.println("Wrong answer.");
		} else setTime(convertDigitsToTimeOffset(input_digits));
		if (DEBUG) {
			Serial.print("Clock: ");
			Serial.println(now());
			Serial.print("Alarm: ");
			Serial.println(alarm_offset);
			updateDisplayDigits();
			printDigits(clock_digits);
		}
		// Clear inputs
		for (int d = 0; d < 4; d++)
			input_digits[d] = 0;
		// Reset input index
		input_digits_index = 0;
	}
}

void printDigits(int* digits) {
	if (!DEBUG) return;
	Serial.print(digits[0]);
	Serial.print(digits[1]);
	Serial.print(":");
	Serial.print(digits[2]);
	Serial.print(digits[3]);
	Serial.println(digits[4] ? " p.m." : " a.m.");
}

// Convert inputted digits into seconds
long convertDigitsToTimeOffset(int digits[5]) {
	long offset_minutes = 0;

	// Hours
	offset_minutes += digits[0] * 10 * 60;
	offset_minutes += digits[1] * 60;
	// Minutes
	offset_minutes += digits[2] * 10;
	offset_minutes += digits[3];
	// AM/PM
	if (digits[4])
		offset_minutes += 12 * 60;
	
	if (DEBUG) {
		Serial.print("Converted ");
		Serial.print(digits[0]);
		Serial.print(digits[1]);
		Serial.print(":");
		Serial.print(digits[2]);
		Serial.print(digits[3]);
		Serial.print(digits[4] ? " p.m." : " a.m.");
		Serial.print(" to ");
		Serial.print(offset_minutes);
		Serial.print(" min or ");
		Serial.print(offset_minutes * 60 * 1000);
		Serial.println(" ms.");
	}
	return offset_minutes * 60; // Convert to seconds
}

// Convert inputted digits to decimal number
int convertDigitsToNumber(int digits[5]) {
	int num = 0;
	num += digits[0] * 1000;
	num += digits[1] * 100;
	num += digits[2] * 10;
	num += digits[3] * 1;
	return num;
}

// Returns digits for display on the seven-segment display
void updateDisplayDigits() {
	unsigned int hours = hourFormat12();
	unsigned int minutes = minute();
	clock_digits[0] = hours / 10;
	clock_digits[1] = hours % 10;
	clock_digits[2] = minutes / 10;
	clock_digits[3] = minutes % 10;
	clock_digits[4] = isPM();
}

// Returns true every other second
bool getDisplaySecond() {
	return second() % 2 == 0;
}

// Checks if current time is within one minute period of the alarm
void updateAlarm() {
	bool prev = alarm_sounding;
	alarm_sounding = alarm_enabled
		&& hour() == alarm_offset / 3600
		&& minute() == (alarm_offset % 3600) / 60;
	if (!prev && alarm_sounding) {
		melody_index = -1;

		// Set up game for solving
		int x = random(10, 100);
		int y = random(2, 10);
		int func = random(10, 12);
		while (func == 10 && x % y != 0) {
			x = random(10, 100);
			y = random(2, 10);
		}
		game_answer = func == 10 ? x / y : x * y;
		if (DEBUG) {
			Serial.print("game_answer = ");
			Serial.println(game_answer);
		}
		game_digits[0] = x / 10;
		game_digits[1] = x % 10;
		game_digits[2] = func;
		game_digits[3] = y;
	}
}