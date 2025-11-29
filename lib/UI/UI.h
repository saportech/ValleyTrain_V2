#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <FastLED.h>

static const int NUM_STATIONS = 8;

extern volatile bool stationTriggered[NUM_STATIONS];

// Pin definitions
#define NUM_LEDS 2  // Increased to handle the 6 additional LEDs
#define DATA_PIN 26

#define SEL0_IN 2
#define SEL1_IN 13
#define SEL2_IN 14
#define SEL3_IN 27
#define IO_IN 32

#define VOLUME_UP 0
#define VOLUME_DOWN 1
#define CHANGE_STATE 2

enum BUTTON_SENSORS_INPUTS {
    BUTTON_SOUND_ON_OFF = 0,
    BUTTON_VOLUME_UP = 1,
    BUTTON_VOLUME_DOWN = 2,
    BUTTON_PLAY_PAUSE = 3,
    BUTTON_LOOP = 4,
    BUTTON_BACKWARDS = 5, 
    NO_INPUTS_RECEIVED = -1,
};

enum STATION_STATE {
    STATION_NONE = -1,       // No station is active
    STATION_START = 0,       // Starting station
    STATION_1 = 1,
    STATION_2 = 2,
    STATION_3 = 3,
    STATION_4 = 4,
    STATION_5 = 5,
    STATION_6 = 6,
    STATION_LAST = 7         // Last station
};

class UI {
public:
    UI();
    BUTTON_SENSORS_INPUTS inputReceived();
    void playSound();
    void setupPinsAndSensors();
    void changeVolume(int volume);
    void turnLoopLED(int state);
    void updateSoundLed();
    STATION_STATE sampleStations();  // Updated to return the station state

private:
    BUTTON_SENSORS_INPUTS buttonState;
    void printButtonName(BUTTON_SENSORS_INPUTS button);
    void selectMuxChannel(int channel);  
    bool isBusy();
    void setVolume(int volume);
    void executeCMD(byte CMD, byte Par1, byte Par2);
    CRGB leds[NUM_LEDS];
    int currentVolume = 20;

    // Debouncing for stations
    unsigned long stationDebounceTimes[8] = {0}; // For debouncing signals
    bool stationStableStates[8] = {false};      // Track stable states of station signals
};

#endif // UI_H
