#include "TRAIN.h"

Train::Train() {

}

void Train::initTrain() {
    Serial.println("Initializing train");
    pinMode(FORWARD_PIN, OUTPUT);
    pinMode(BACKWARD_PIN, OUTPUT);
    stop(); // Ensure the train is stopped at startup
}

void Train::moveForward() {
    digitalWrite(FORWARD_PIN, LOW);
    digitalWrite(BACKWARD_PIN, HIGH);
    Serial.println("Train moving forward");
}

void Train::moveBackward() {
    digitalWrite(FORWARD_PIN, HIGH);
    digitalWrite(BACKWARD_PIN, LOW);
    Serial.println("Train moving backward");
}

void Train::stop() {
    digitalWrite(FORWARD_PIN, HIGH);
    digitalWrite(BACKWARD_PIN, HIGH);
    Serial.println("Train stopped");
}
