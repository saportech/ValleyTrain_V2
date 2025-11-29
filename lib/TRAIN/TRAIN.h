#ifndef TRAIN_H
#define TRAIN_H

#include <Arduino.h>

// Define the pins for controlling the train
const uint8_t FORWARD_PIN = 21;
const uint8_t BACKWARD_PIN = 22;

class Train {
public:
    Train();               // Constructor
    void initTrain();      // Initialize the train
    void moveForward();    // Move the train forward
    void moveBackward();   // Move the train backward
    void stop();           // Stop the train
};

#endif // TRAIN_H
