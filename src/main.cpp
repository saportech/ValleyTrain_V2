#include <Arduino.h>
#include "UI.h"
#include "TRAIN.h"
#include "SEMAPHORE_T.h"
#include <EEPROM.h>

#include <WiFi.h>
#include <esp_now.h>

#define EEPROM_SIZE 1  // We only need to store 1 byte for loopEnabled
#define EEPROM_ADDR 0

#define MOVING_FORWARD 0
#define MOVING_BACKWARD 1
#define STOPPED 2

#define LOOP_LED_ON 1
#define LOOP_LED_OFF 0

UI ui;
Train train;
Semaphore semaphores;
int input;
STATION_STATE activeStation = STATION_NONE;

bool loopEnabled = false;
bool firstLoopEnabled = false; 

// One flag per station – set true by ESP-NOW callback when that station fires
volatile bool stationTriggered[NUM_STATIONS] = {false};

// MAC table: which sender ESP32 belongs to which station index
// NOTE: station index 0 == STATION_START, 1 == STATION_1, ..., 7 == STATION_LAST
const uint8_t stationMacs[NUM_STATIONS][6] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // index 0 -> STATION_START (your Sender #1)
    {0xCC, 0x7B, 0x5C, 0x28, 0x84, 0x4C}, // index 1 -> STATION_1
    {0xD8, 0xBC, 0x38, 0xF8, 0x90, 0x24}, // index 2 -> STATION_2
    {0x80, 0x64, 0x6F, 0xC4, 0x92, 0xD0}, // index 3 -> STATION_3
    {0xF0, 0x08, 0xD1, 0xD5, 0x3D, 0x80}, // index 4 -> STATION_4
    {0xA4, 0xCF, 0x12, 0x6A, 0x50, 0xD4}, // index 5 -> STATION_5
    {0xB4, 0xE6, 0x2D, 0xBA, 0xDB, 0x61}, // index 6 -> STATION_6
    {0x34, 0xB7, 0xDA, 0xF9, 0x4B, 0x4C}, // index 7 -> STATION_LAST
};

void handleSoundAndLoop();
void vallleyTrainStateMachine();
String getStatusText(int input, int activeStation, int state, int trainState, int station, bool initiatedToRed);

void printMacAddress();
int findStationIndexByMac(const uint8_t *mac);
void onEspNowReceive(const uint8_t *mac, const uint8_t *data, int len);
void setupEspNowReceiver();

void loopAnalysis();

void setup() {
    Serial.begin(115200);
    Serial.println("Starting Valley Train");
    //delay(1000);

    printMacAddress();
    setupEspNowReceiver();

    ui.setupPinsAndSensors();
    train.initTrain();
    semaphores.init();

    EEPROM.begin(EEPROM_SIZE);
    
    // Read stored value from EEPROM
    loopEnabled = EEPROM.read(EEPROM_ADDR);
    
    Serial.print("Loop mode loaded: ");
    Serial.println(loopEnabled ? "ENABLED" : "DISABLED");

    // Set the loop LED accordingly
    if (loopEnabled) {
        ui.turnLoopLED(LOOP_LED_ON);
        firstLoopEnabled = true;
    } else {
        ui.turnLoopLED(LOOP_LED_OFF);
    }

}

void loop() {

    input = ui.inputReceived();

    activeStation = ui.sampleStations();

    handleSoundAndLoop();

    vallleyTrainStateMachine();

}

void vallleyTrainStateMachine() {
    static int state = 0;
    static unsigned long previousMillis = 0;
    static unsigned long lastPrintTime = 0;
    static int trainState = STOPPED;
    static int station = 0;
    static bool initiatedToRed = false;

    #define WAITING_AT_SEMAPHORE_TIME 2000
    #define PRINT_TIME 2000
    #define GOING_BACKWARD_DELAY 2000

    enum TRAIN_STATE_TYPE {
        START,
        GOING_TO_STATION_X,
        TURN_GREEN_LIGHT_ON_AT_STATION_X,
        WAITING_AT_STATION_X,
        GOING_TO_LAST_STATION,
        WAIT_BEFORE_GOING_BACKWARD,
        GOING_BACKWARD,
        WAITING_BEFORE_NEXT_LOOP
    };

    if (firstLoopEnabled) {
        state = WAITING_BEFORE_NEXT_LOOP;
        firstLoopEnabled = false;
    }

    if (millis() - lastPrintTime > PRINT_TIME) {
        //Serial.println(getStatusText(input, state, trainState, station, initiatedToRed));
        lastPrintTime = millis();
    }

    if (input == BUTTON_PLAY_PAUSE && trainState != STOPPED) {
        train.stop();
        trainState = STOPPED;
        state = GOING_TO_STATION_X;
    }
    else if (input == BUTTON_PLAY_PAUSE && trainState == STOPPED) {
        initiatedToRed = false;
        state = GOING_TO_STATION_X;
        train.moveForward();
        trainState = MOVING_FORWARD;
    }
    else if (input == BUTTON_BACKWARDS) {
        train.stop();
        trainState = STOPPED;
        previousMillis = millis();
        state = WAIT_BEFORE_GOING_BACKWARD;
    }

    switch (state) {
        case START:
            if (!initiatedToRed && semaphores.initToRed()) {
                initiatedToRed = true;
            }
            else if (trainState == MOVING_FORWARD && initiatedToRed) {
                initiatedToRed = false;
                state = GOING_TO_STATION_X;
                Serial.println("Going to station 1");
            }
            break;
        case GOING_TO_STATION_X://Train is going to one of the stations
            
            switch (activeStation)
            {
                case STATION_1:
                    station = 1;
                    break;
                case STATION_2:
                    station = 2;
                    break;
                case STATION_3:
                    station = 3;
                    break;
                case STATION_4:  
                    station = 4;
                    break;
                case STATION_5:
                    station = 5;
                    break;
                case STATION_6:
                    station = 6;
                    break;
                case STATION_LAST:
                    station = 7;
                    break;
                default:
                    break;
            }
            
            if (activeStation == STATION_1 || activeStation == STATION_2 || activeStation == STATION_3 || activeStation == STATION_4 || activeStation == STATION_5 || activeStation == STATION_6 || activeStation == STATION_LAST) {
                train.stop();
                trainState = STOPPED;
                state = TURN_GREEN_LIGHT_ON_AT_STATION_X;
                Serial.println("Reached station " + String(station) + ". Waiting");
                previousMillis = millis();
            }
            break;
        case TURN_GREEN_LIGHT_ON_AT_STATION_X:
            if (station != 7) {
                if (semaphores.setSemaphore(station,GREEN)) {
                    previousMillis = millis();
                    state = WAITING_AT_STATION_X;
                    break;
                }
            }
            else {
                Serial.println("Station 7 reached, not turning green light on");
                previousMillis = millis();
                state = WAITING_AT_STATION_X;
                break;
            }
            break;
        case WAITING_AT_STATION_X:
            if (millis() - previousMillis > WAITING_AT_SEMAPHORE_TIME) {
                if (station != 7) {
                    train.moveForward();
                    trainState = MOVING_FORWARD;
                    state = GOING_TO_STATION_X;
                    Serial.println("Going to station " + String(station + 1));
                    break;
                }
                else if (station == 7) {
                    Serial.println("Youv'e reached the last station");
                    train.moveBackward();
                    trainState = MOVING_BACKWARD;
                    state = GOING_BACKWARD;
                    break;
                }
                else {
                    Serial.println("Error, station not found");
                    break;
                }
            }
            break;
        case WAIT_BEFORE_GOING_BACKWARD:
            if (millis() - previousMillis > GOING_BACKWARD_DELAY) {
                trainState = MOVING_BACKWARD;
                train.moveBackward();
                previousMillis = millis();
                state = GOING_BACKWARD;
            }
            break;
        case GOING_BACKWARD://Train is going backwards
            if (activeStation == STATION_START && trainState == MOVING_BACKWARD) {
                Serial.println("Reached Start Station");
                train.stop();
                trainState = STOPPED;
                station = 0;
                state = START;
                if (loopEnabled) {
                    state = WAITING_BEFORE_NEXT_LOOP;
                    previousMillis = millis();
                    break;
                }
            }
            break;
        case WAITING_BEFORE_NEXT_LOOP:
            if (!initiatedToRed && semaphores.initToRed()) {
                initiatedToRed = true;
            }
            if (millis() - previousMillis > WAITING_AT_SEMAPHORE_TIME && initiatedToRed) {
                initiatedToRed = false;
                train.moveForward();
                trainState = MOVING_FORWARD;
                state = GOING_TO_STATION_X;
                Serial.println("New Loop, Going to station 1");
            }
            break;
    }

}

void handleSoundAndLoop() {
static unsigned long lastSoundTime = 0;
static unsigned long lastLedsUpdateTime = 0;
static bool firstTimePlaying = true;
#define PLAYING_TIME 324000

    if (firstTimePlaying) {
        ui.playSound();
        firstTimePlaying = false;
    }

    if (millis() - lastSoundTime > PLAYING_TIME) {
        ui.playSound();
        lastSoundTime = millis();
    }
    
    if (input == BUTTON_VOLUME_UP) {
        ui.changeVolume(VOLUME_UP);
    }
    if (input == BUTTON_VOLUME_DOWN) {
        ui.changeVolume(VOLUME_DOWN);
    }
    if (input == BUTTON_SOUND_ON_OFF) {
        ui.changeVolume(CHANGE_STATE);
    }

    if (input == BUTTON_LOOP) {
        loopEnabled = !loopEnabled; // Toggle the state of loopEnabled

        EEPROM.write(EEPROM_ADDR, loopEnabled);
        EEPROM.commit(); 

        if (loopEnabled) {
            ui.turnLoopLED(LOOP_LED_ON);
            Serial.println("Loop mode enabled!");
        } else {
            ui.turnLoopLED(LOOP_LED_OFF);
            Serial.println("Loop mode disabled!");
        }
    }

    if (millis() - lastLedsUpdateTime > 2000 ) {
        if (loopEnabled) {
            ui.turnLoopLED(LOOP_LED_ON);
            //Serial.println("Loop mode enabled!");
        } else {
            ui.turnLoopLED(LOOP_LED_OFF);
            //Serial.println("Loop mode disabled!");
        }

        ui.updateSoundLed();//Just to update the LED

        lastLedsUpdateTime = millis();
    }
}

String getStatusText(int input, int activeStation, int state, int trainState, int station, bool initiatedToRed) {
    String stateText;
    String trainStateText;
    String stationText;
    String redInitText = initiatedToRed ? "Yes" : "No";
    String inputText;

    switch (input) {
        case BUTTON_SOUND_ON_OFF: inputText = "BUTTON_SOUND_ON_OFF"; break;
        case BUTTON_VOLUME_UP: inputText = "BUTTON_VOLUME_UP"; break;
        case BUTTON_VOLUME_DOWN: inputText = "BUTTON_VOLUME_DOWN"; break;
        case BUTTON_PLAY_PAUSE: inputText = "BUTTON_PLAY_PAUSE"; break;
        case BUTTON_LOOP: inputText = "BUTTON_LOOP"; break;
        case BUTTON_BACKWARDS: inputText = "BUTTON_BACKWARDS"; break;
        default: inputText = "NO_INPUTS_RECEIVED"; break;
    }

    if (activeStation != STATION_NONE) {
        // Handle the active station
        switch (activeStation) {
            case STATION_START: Serial.println("Active: STATION_START"); break;
            case STATION_1: Serial.println("Active: STATION_1"); break;
            case STATION_2: Serial.println("Active: STATION_2"); break;
            case STATION_3: Serial.println("Active: STATION_3"); break;
            case STATION_4: Serial.println("Active: STATION_4"); break;
            case STATION_5: Serial.println("Active: STATION_5"); break;
            case STATION_6: Serial.println("Active: STATION_6"); break;
            case STATION_LAST: Serial.println("Active: STATION_LAST"); break;
            default: break;
        }
    }

    // Convert state to human-readable text
    switch (state) {
        case 0: stateText = "START"; break;
        case 1: stateText = "GOING_TO_STATION_X"; break;
        case 2: stateText = "TURN_GREEN_LIGHT_ON_AT_STATION_X"; break;
        case 3: stateText = "WAITING_AT_STATION_X"; break;
        case 4: stateText = "GOING_TO_LAST_STATION"; break;
        case 5: stateText = "WAIT_BEFORE_GOING_BACKWARD"; break;
        case 6: stateText = "GOING_BACKWARD"; break;
        case 7: stateText = "WAITING_BEFORE_NEXT_LOOP"; break;
        default: stateText = "UNKNOWN"; break;
    }

    // Convert trainState to human-readable text
    switch (trainState) {
        case MOVING_FORWARD: trainStateText = "MOVING_FORWARD"; break;
        case MOVING_BACKWARD: trainStateText = "MOVING_BACKWARD"; break;
        case STOPPED: trainStateText = "STOPPED"; break;
        default: trainStateText = "UNKNOWN"; break;
    }

    // Convert station number to human-readable text
    if (station == 0) {
        stationText = "Start Station";
    } else if (station >= 1 && station <= 6) {
        stationText = "Station " + String(station);
    } else if (station == 7) {
        stationText = "Last Station";
    } else {
        stationText = "No Station";
    }

    // Return a formatted status string
    return "Input: " + inputText + ", State: " + stateText + ", Train State: " + trainStateText + ", Station: " + stationText + ", Initiated to Red: " + redInitText;
}

void printMacAddress() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    Serial.print("Receiver MAC Address: ");
    for (int i = 0; i < 6; i++) {
        if (mac[i] < 16) Serial.print("0");
        Serial.print(mac[i], HEX);
        if (i < 5) Serial.print(":");
    }
    Serial.println();
}

int findStationIndexByMac(const uint8_t *mac) {
    for (int i = 0; i < NUM_STATIONS; i++) {
        bool match = true;
        for (int j = 0; j < 6; j++) {
            if (mac[j] != stationMacs[i][j]) {
                match = false;
                break;
            }
        }
        if (match) return i;
    }
    return -1; // unknown sender
}

void onEspNowReceive(const uint8_t *mac, const uint8_t *data, int len) {
    if (len < 1) return;

    int stationIndex = findStationIndexByMac(mac);
    if (stationIndex < 0) {
        Serial.println("ESP-NOW: message from unknown MAC");
        return;
    }

    uint8_t value = data[0];  // sender sends 1 when station becomes stably LOW

    if (value == 1) {
        stationTriggered[stationIndex] = true;
        Serial.print("Station ");
        Serial.print(stationIndex);
        Serial.println(" triggered (via ESP-NOW)");
    }
}

void setupEspNowReceiver() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    esp_now_register_recv_cb(onEspNowReceive);
}

void loopAnalysis() {
    static unsigned long loopStartTime = 0;
    static unsigned long loopEndTime = 0;
    static unsigned long minLoopTime = 0xFFFFFFFF;
    static unsigned long maxLoopTime = 0;
    static unsigned long loopCount = 0;
    static unsigned long totalLoopTime = 0;
    static unsigned long lastPrintTime = 0;

    // Measure the start time of the loop
    if (loopStartTime == 0) {
        loopStartTime = micros();
        return;
    }

    // Measure the end time of the loop
    loopEndTime = micros();
    unsigned long loopDuration = loopEndTime - loopStartTime;

    // Update statistics
    minLoopTime = min(minLoopTime, loopDuration);
    maxLoopTime = max(maxLoopTime, loopDuration);
    totalLoopTime += loopDuration;
    loopCount++;

    // Print analysis every second (1000 milliseconds)
    if (millis() - lastPrintTime >= 1000) {
        Serial.println("---- Loop Analysis (Microseconds) ----");
        Serial.print("Min Loop Time: ");
        Serial.print(minLoopTime);
        Serial.println(" us");

        Serial.print("Max Loop Time: ");
        Serial.print(maxLoopTime);
        Serial.println(" us");

        Serial.print("Avg Loop Time: ");
        Serial.print(totalLoopTime / loopCount);
        Serial.println(" us");

        Serial.print("Loop Count: ");
        Serial.println(loopCount);

        // Reset statistics for the next analysis period
        minLoopTime = 0xFFFFFFFF;
        maxLoopTime = 0;
        totalLoopTime = 0;
        loopCount = 0;
        lastPrintTime = millis(); // Update the last print time
    }

    // Reset loop start time for the next measurement
    loopStartTime = loopEndTime;
}
