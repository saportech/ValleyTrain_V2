#ifndef SEMAPHORE_T_H
#define SEMAPHORE_T_H

#include <Arduino.h>

// Define pins for multiplexer control
const uint8_t MUX_OUTPUT_PIN = 19;
const uint8_t SEL0 = 15;
const uint8_t SEL1 = 4;
const uint8_t SEL2 = 5;
const uint8_t SEL3 = 18;

// Semaphore states
enum SemaphoreState { RED, GREEN };

class Semaphore {
public:
    void init();                               // Initialize semaphore pins
    bool initToRed();                          // Initialize all semaphores to RED state, returns true when complete
    bool setSemaphore(uint8_t id, SemaphoreState state); // Set semaphore color, returns true when pulse is complete

private:
    void selectMuxChannel(uint8_t channel);    // Select MUX channel for semaphore and color
};

#endif // SEMAPHORE_T_H
