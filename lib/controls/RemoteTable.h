#include <stdint.h>
#include "Actions.h"
#include <cstddef>
#include <Arduino.h>


#define KOUT3 1
#define KOUT2 2
#define KOUT1 42
#define KIN3 40
#define KIN2 39
#define KIN1 38
#define KIND 21

struct Button {
  uint32_t sig;
  UIAction action;
};

Button buttons[] = {
  {0xFFFFFFFF, NONE},
  {0xDDDDDDDD, UP},
  {0xEEEEEEEE, DOWN},
  {0xFFFFEEEE, SELECT},
  {0xFF77FF77, MENU},
  {0xF7F7F7F7, OPTIONS},
  {0xFFFFDDDD, REWIND},
  {0xFFFFBBBB, FAST_FORWARD}
};

void setupRemote() {
  pinMode(KOUT3, OUTPUT);
  pinMode(KOUT2, OUTPUT);
  pinMode(KOUT1, OUTPUT);
  pinMode(KIN3, INPUT_PULLUP);
  pinMode(KIN2, INPUT_PULLUP);
  pinMode(KIN1, INPUT_PULLUP);
  pinMode(KIND, INPUT_PULLUP);
}

uint32_t scanSignature() {
  uint32_t sig = 0;

  for (int i = 0; i < 8; i++) {

    // Set outputs
    digitalWrite(KOUT3, (i >> 2) & 1);
    digitalWrite(KOUT2, (i >> 1) & 1);
    digitalWrite(KOUT1, (i >> 0) & 1);

    delay(2); // settle

    // Read inputs (4 bits)
    byte inputState = 0;
    inputState |= digitalRead(KIN3) << 0;
    inputState |= digitalRead(KIN2) << 1;
    inputState |= digitalRead(KIN1) << 2;
    inputState |= digitalRead(KIND) << 3;

    // Pack into 32-bit int (4 bits per step)
    sig |= ((uint32_t)inputState << (i * 4));
  }

  return sig;
}

UIAction getActionForSignal(uint32_t signal) {
    UIAction wrapNum = NONE;
    for (size_t i = 0; i < sizeof(buttons) / sizeof(Button); i++) {
        if (buttons[i].sig == signal) {
            wrapNum = buttons[i].action;
            break;
        }
    }
    return wrapNum;
  }