#include "UI.h"

extern volatile bool stationTriggered[NUM_STATIONS];

//const int stationPins[] = {36, 39, 34, 35, 33, 16, 17, 23};
const int stationPins[] = {36, 34, 35, 33, 16, 17, 23, 39};

UI::UI() {

}

void UI::setupPinsAndSensors() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.show();

    //show leds for testing
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Red;
        FastLED.show();
        delay(100);
        leds[i] = CRGB::Black;
        FastLED.show();
    }

    leds[NUM_LEDS - 1] = CRGB(0, 0, 200);
    FastLED.show();

    for (int i = 0; i < 8; i++) {
        pinMode(stationPins[i], INPUT_PULLUP);
    }

    pinMode(SEL0_IN, OUTPUT);
    pinMode(SEL1_IN, OUTPUT);
    pinMode(SEL2_IN, OUTPUT);
    pinMode(SEL3_IN, OUTPUT);
    pinMode(IO_IN, INPUT_PULLUP);

    Serial2.begin(9600, SERIAL_8N1, 34, 12);
    delay(100);
    executeCMD(0x06, 0, 20);
    delay(100);
    Serial.println("Volume set to: " + String(20));
    
}

STATION_STATE UI::sampleStations() {
    const unsigned long debounceDelay = 30; // Debounce delay in milliseconds

    // 1) LOCAL: Station 0 (STATION_START) is still wired directly to this board
    int station0State = digitalRead(stationPins[0]); // pin for STATION_START

    if (station0State == LOW) {
        // Debounce for local station 0
        if ((millis() - stationDebounceTimes[0]) > debounceDelay) {
            if (!stationStableStates[0]) { // new stable LOW
                stationStableStates[0] = true;
                Serial.println("Station 0 (START) is LOW (local)");
                return STATION_START;
            }
        }
    } else {
        // Reset debounce for station 0 when it's HIGH
        stationStableStates[0] = false;
        stationDebounceTimes[0] = millis();
    }

    // 2) REMOTE: Stations 1..7 come via ESP-NOW (stationTriggered[])
    for (int i = 1; i < NUM_STATIONS; i++) {
        if (stationTriggered[i]) {
            stationTriggered[i] = false; // consume the event

            Serial.println("Station " + String(i) + " is LOW (remote)");
            return static_cast<STATION_STATE>(i); // STATION_1..STATION_LAST
        }
    }

    return STATION_NONE; // No station active
}

BUTTON_SENSORS_INPUTS UI::inputReceived() {
    static int currentChannel = 0;
    static unsigned long lastMillis = 0;
    static unsigned long lastButtonPressTime = 0;
    static unsigned long buttonHoldStartTime[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    #define HOLD_TIME_THRESHOLD 5
    #define HOLD_TIME_THRESHOLD_BUTTONS 20
    #define BUTTON_HOLD_DELAY 500

    
    // Select the current MUX channel
    selectMuxChannel(currentChannel);

    // Check if 1 millisecond has passed for scanning
    if (millis() - lastMillis >= 1) {
        lastMillis = millis();  // Update the lastMillis for the next cycle

        // Read the button state after the delay
        int buttonState = digitalRead(IO_IN);
        if (buttonState == LOW) {

            //Serial.println("currentChannel: " + String(currentChannel) + ", Button hold start time: " + String(millis() - buttonHoldStartTime[currentChannel]));

            BUTTON_SENSORS_INPUTS button = static_cast<BUTTON_SENSORS_INPUTS>(currentChannel);

            if (buttonHoldStartTime[currentChannel] == 0) {
                // Start tracking time if this is the first LOW detection
                buttonHoldStartTime[currentChannel] = millis();
            }

            // Check if button is one of the first six real buttons and enforce delay if so

            else if (millis() - buttonHoldStartTime[currentChannel] < HOLD_TIME_THRESHOLD) {
                currentChannel = (currentChannel + 1) % 15;
                return NO_INPUTS_RECEIVED;
            }
            if (button <= BUTTON_BACKWARDS && millis() - lastButtonPressTime < BUTTON_HOLD_DELAY) {
                // Skip further processing if 500ms delay is not met
                currentChannel = (currentChannel + 1) % 15;
                return NO_INPUTS_RECEIVED;
            }
            else if (button <= BUTTON_BACKWARDS && millis() - buttonHoldStartTime[currentChannel] < HOLD_TIME_THRESHOLD_BUTTONS) {
                currentChannel = (currentChannel + 1) % 15;
                return NO_INPUTS_RECEIVED;
            }
            // Update the last button press time for real buttons
            if (button <= BUTTON_BACKWARDS) {
                lastButtonPressTime = millis();
            }

            printButtonName(button);  // Print button name
            currentChannel = (currentChannel + 1) % 15;  // Move to the next channel for the next cycle
            return button;
        }
        else {
            // Reset the button hold start time if the button is not pressed
            buttonHoldStartTime[currentChannel] = 0;
        }

        // Move to the next channel
        currentChannel = (currentChannel + 1) % 15;
    }

    return NO_INPUTS_RECEIVED;
}

void UI::turnLoopLED(int state) {
    if (state == 1) {
        leds[0] = CRGB(0, 200, 0); // Custom green with reduced intensity
    } else {
        leds[0] = CRGB::Black;
    }
    FastLED.show();
}

void UI::printButtonName(BUTTON_SENSORS_INPUTS button) {
    static unsigned long lastPrintTime = 0;

    // Check if 500 milliseconds have passed since the last print
    if (millis() - lastPrintTime < 500) {
        return; // Exit the function if 500ms wait time has not been met
    }

    // Print the button name if enough time has passed
    switch (button) {
        case BUTTON_SOUND_ON_OFF: Serial.println("Button pressed: BUTTON_SOUND_ON_OFF"); break;
        case BUTTON_VOLUME_UP: Serial.println("Button pressed: BUTTON_VOLUME_UP"); break;
        case BUTTON_VOLUME_DOWN: Serial.println("Button pressed: BUTTON_VOLUME_DOWN"); break;
        case BUTTON_PLAY_PAUSE: Serial.println("Button pressed: BUTTON_PLAY_PAUSE"); break;
        case BUTTON_LOOP: Serial.println("Button pressed: BUTTON_LOOP"); break;
        case BUTTON_BACKWARDS: Serial.println("Button pressed: BUTTON_BACKWARDS"); break;
        //default: Serial.println("Button pressed: NO_INPUTS_RECEIVED"); break;
    }

    // Update last print time after a print is made
    lastPrintTime = millis();
}

void UI::selectMuxChannel(int channel) {
    digitalWrite(SEL0_IN, (channel & 0x01) ? HIGH : LOW);
    digitalWrite(SEL1_IN, (channel & 0x02) ? HIGH : LOW);
    digitalWrite(SEL2_IN, (channel & 0x04) ? HIGH : LOW);
    digitalWrite(SEL3_IN, (channel & 0x08) ? HIGH : LOW);
}

void UI::playSound() {
    Serial.println("Playing sound!");
    executeCMD(0x0F, 0x01, 0x01);
}

void UI::executeCMD(byte CMD, byte Par1, byte Par2) {
    #define Start_Byte 0x7E
    #define Version_Byte 0xFF
    #define Command_Length 0x06
    #define End_Byte 0xEF
    #define Acknowledge 0x00 //Returns info with command 0x41 [0x01: info, 0x00: no info]
    
    // Calculate the checksum (2 bytes)
    word checksum = -(Version_Byte + Command_Length + CMD + Acknowledge + Par1 + Par2);
    
    // Build the command line
    byte Command_line[10] = { Start_Byte, Version_Byte, Command_Length, CMD, Acknowledge,
                                Par1, Par2, highByte(checksum), lowByte(checksum), End_Byte };
    
    // Send the command line to the module
    for (byte k = 0; k < 10; k++) {
        Serial2.write(Command_line[k]);
    }
}

bool UI::isBusy() {
    #define BUSY_PIN 25
    pinMode(BUSY_PIN, INPUT);
    int busyRead = digitalRead(BUSY_PIN);
    if (busyRead == 1) {
        Serial.println("DFPlayer not busy!");
        return false;
    }
    return true;
}

void UI::setVolume(int volume) {
    Serial.println("Volume set to: " + String(volume));
    executeCMD(0x06, 0, volume);
}

void UI::changeVolume(int volume) {
    #define VOLUME_CHANGE_DEFICIT 5
    if (volume == VOLUME_UP) {
        currentVolume += VOLUME_CHANGE_DEFICIT;
        if (currentVolume > 30) {
            currentVolume = 30;
        }
    } else if (volume == VOLUME_DOWN) {
        currentVolume -= VOLUME_CHANGE_DEFICIT;
        if (currentVolume < 0) {
            currentVolume = 0;
        }
    } else if (volume == CHANGE_STATE) {
        if (currentVolume == 0) {
            currentVolume = 20;
        } else {
            currentVolume = 0;
        }
    }
    setVolume(currentVolume);
    if (currentVolume == 0) {
        leds[1] = CRGB(0, 0, 0);
        FastLED.show();
    }
    else {
        leds[1] = CRGB(0, 0, 200);
        FastLED.show();
    }
}

void UI::updateSoundLed() {
    if (currentVolume == 0) {
        leds[1] = CRGB(0, 0, 0);
    }
    else {
        leds[1] = CRGB(0, 0, 200);
    }
    FastLED.show();
}