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
const int PIN_PIEZO_REF = A1;
const int PIN_IR_RECEIVER = A3;

// Other constants
const int MAP_DIGIT[10] = {
	B11111100, // 0
	B01100000, // 1
	B11011010, // 2
	B11110010, // 3
	B01100110, // 4
	B10110110, // 5
	B10111110, // 6
	B11100000, // 7
	B11111110, // 8
	B11100110  // 9
};
const int VOL_MIN = 1.2;
const int DISPLAY_FREQ = 10; // Hz
const int DISPLAY_SETTLE_STATES = 1;

// State
time_t clock_offset = 0;
time_t alarm_offset = 0;
bool alarm_enabled = false;
bool alarm_editing = false;
long alarm_start = -1;
int input_digits[4] = { 0, 0, 0, 0 };
int input_digits_index = 0;
bool input_pm = false;
IRrecv irrecv(PIN_IR_RECEIVER);
decode_results results_ir;
int display_digit_index = 0;
int display_digits[4] = { 0, 0, 0, 0 };
long display_last_switch = 0;

void setup() {
	// Initialize seven-segment display pins
	for (int p = 0; p < 8; p++)
		pinMode(PIN_SEGMENT[p], OUTPUT);
	for (int p = 0; p < 4; p++)
		pinMode(PIN_DIGIT[p], OUTPUT);
	
	// Initialize piezo pins
	pinMode(PIN_PIEZO, OUTPUT);
	pinMode(PIN_PIEZO_REF, OUTPUT);

	// Initialize other pins
	pinMode(PIN_STATUS, OUTPUT);
	pinMode(PIN_IR_RECEIVER, OUTPUT);

	if (DEBUG)
		Serial.begin(9600);

	irrecv.enableIRIn();
	setTime(0);
}

void loop() {
	// Check for infrared codes
	if (irrecv.decode(&results_ir)) {
		Serial.println(results_ir.value, HEX);
		irrecv.resume();
		processRemote(results_ir.value);
	}

	// Handle alarm sounding
	// TODO Handle blinking and solid status LED
	checkAlarm();
	if (alarm_enabled && alarm_start >= 0 && now() / 60 == alarm_start / 60) {
		// TODO Actually control melody here
		digitalWrite(PIN_STATUS, HIGH);
	} else digitalWrite(PIN_STATUS, LOW);

	// Show math problems if alarm sounding

	// Pulse seven-segment
	updateDisplayDigits();
	if (DEBUG && millis() % 5000 < 1)
		printTime(display_digits);
	int selected_digit = display_digit_index / (DISPLAY_SETTLE_STATES + 1);
	int* digits_to_display = input_digits_index > 0 ? input_digits : display_digits;
	if (display_digit_index % (DISPLAY_SETTLE_STATES + 1) == 0 && !(input_digits_index > 0 && millis() % 500 >= 250)) {
		// Set the cathodes and annodes to display the current digit
		for (int d = 0; d < 4; d++)
			digitalWrite(PIN_DIGIT[d], selected_digit == d ? HIGH : LOW);
		for (int s = 0; s < 8; s++)
			digitalWrite(PIN_SEGMENT[s], bitRead(MAP_DIGIT[digits_to_display[selected_digit]], 7 - s) ? LOW : HIGH);
		
		// Tick second indicator
		if (selected_digit == 1 && getDisplaySecond())
			digitalWrite(PIN_SEGMENT[7], LOW);
	
		// TODO Handle AM/PM
		if (selected_digit == 3 && getDisplayPM())
			digitalWrite(PIN_SEGMENT[7], )
	} else {
		// Blanking time b/w digits to let LEDs settle
		for (int s = 0; s < 8; s++)
			digitalWrite(PIN_SEGMENT[s], LOW);
	}

	if (millis() - display_last_switch > (1 / DISPLAY_FREQ) * 1000) {
		display_digit_index = (display_digit_index + 1) % ((DISPLAY_SETTLE_STATES + 1) * 4);
		display_last_switch = millis();
	}
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
			alarm_editing = !alarm_editing;
			if (alarm_editing)
				Serial.println("Editing alarm.");
			else
				Serial.println("Stopped editing alarm.");
			break;
		case 0xFFC23D: // Resume/Pause
			alarm_enabled = !alarm_enabled;
			if (alarm_enabled)
				Serial.println("Enabled alarm.");
			else
				Serial.println("Disabled alarm.");
			break;
		case 0xFFB04F: // 200+
			input_pm = !input_pm;
			Serial.println("AM/PM toggled.");
			break;
		case 0xFFA25D: // CH-
			Serial.print("Millis() = ");
			Serial.println(millis());
			Serial.print("now() = ");
			Serial.println(now());
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
	printTime(input_digits);

	// Entered a full set of four digits
	if (input_digits_index == 4) {
		// Set current time or alarm
		if (!alarm_editing)
			setTime(convertDigitsToTimeOffset(input_digits));
		else {
			alarm_offset = convertDigitsToTimeOffset(input_digits);
			alarm_editing = false;
			alarm_enabled = true;
		}
		if (DEBUG) {
			Serial.print("Clock: ");
			Serial.println(now());
			Serial.print("Alarm: ");
			Serial.println(alarm_offset);
			updateDisplayDigits();
			printTime(display_digits);
		}
		// Clear inputs
		for (int d = 0; d < 4; d++)
			input_digits[d] = 0;
		// Reset input index
		input_digits_index = 0;
	}
}

void printTime(int* digits) {
	if (!DEBUG) return;
	Serial.print(digits[0]);
	Serial.print(digits[1]);
	Serial.print(":");
	Serial.print(digits[2]);
	Serial.print(digits[3]);
	Serial.println(input_pm ? " p.m." : " a.m.");
}

// Convert inputted digits into seconds
long convertDigitsToTimeOffset(int digits[4]) {
	long offset_minutes = 0;

	// Hours
	offset_minutes += digits[0] * 10 * 60;
	offset_minutes += digits[1] * 60;
	// Minutes
	offset_minutes += digits[2] * 10;
	offset_minutes += digits[3];
	// AM/PM
	if (input_pm)
		offset_minutes += 12 * 60;
	
	if (DEBUG) {
		Serial.print("Converted ");
		Serial.print(digits[0]);
		Serial.print(digits[1]);
		Serial.print(":");
		Serial.print(digits[2]);
		Serial.print(digits[3]);
		Serial.print(input_pm ? " p.m." : " a.m.");
		Serial.print(" to ");
		Serial.print(offset_minutes);
		Serial.print(" min or ");
		Serial.print(offset_minutes * 60 * 1000);
		Serial.println(" ms.");
	}
	return offset_minutes * 60; // Convert to seconds
}

// Returns digits for display on the seven-segment display
void updateDisplayDigits() {
	unsigned int hours = hour();
	unsigned int minutes = minute();
	if (hours >= 12) hours -= 12;
	if (hours == 0) hours = 12;
	display_digits[0] = hours / 10;
	display_digits[1] = hours % 10;
	display_digits[2] = minutes / 10;
	display_digits[3] = minutes % 10;
}

// Returns true if PM (at or after noon)
bool getDisplayPM() {
	return hour() >= 12;
}

// Returns true every other second
bool getDisplaySecond() {
	return second() % 2 == 0;
}

// Checks if current time is within one minute period of the alarm
void checkAlarm() {
	int time_of_day = (millis() + clock_offset) % (24 * 60 * 60 * 1000);
	if (time_of_day / 1000 / 60 == alarm_offset)
		alarm_start = millis();
	else
		alarm_start = -1;
}