#pragma once
#include <Arduino.h>

// ─────────────────────────────────────────────────────────────
//  Pin definitions — match your wiring
// ─────────────────────────────────────────────────────────────
#define BTN_UP     32
#define BTN_DOWN   33
#define BTN_LEFT   25
#define BTN_RIGHT  26
#define BTN_A      27
#define BTN_B      17

// ─────────────────────────────────────────────────────────────
//  Button events (kept for future use)
// ─────────────────────────────────────────────────────────────
enum ButtonEvent {
    BTN_NONE,
    BTN_EV_UP,
    BTN_EV_DOWN,
    BTN_EV_LEFT,
    BTN_EV_RIGHT,
    BTN_EV_A,
    BTN_EV_B
};

// ─────────────────────────────────────────────────────────────
//  Input manager
//
//  held(pin)         — raw synchronous read, no debounce.
//                      Use for smooth held movement.
//
//  justPressed(pin)  — returns true exactly once per physical
//                      press (falling edge + debounce).
//                      Use for one-shot actions: menu toggle,
//                      confirm, cancel, attack, etc.
//                      Must be called every loop() frame for
//                      the pin you care about.
// ─────────────────────────────────────────────────────────────
class InputManager {
public:

    void init() {
        pinMode(BTN_UP,    INPUT_PULLUP);
        pinMode(BTN_DOWN,  INPUT_PULLUP);
        pinMode(BTN_LEFT,  INPUT_PULLUP);
        pinMode(BTN_RIGHT, INPUT_PULLUP);
        pinMode(BTN_A,     INPUT_PULLUP);
        pinMode(BTN_B,     INPUT_PULLUP);

        for (int i = 0; i < BTN_COUNT; i++) {
            _prevState[i] = HIGH;
            _lastPress[i] = 0;
        }
    }

    // Raw read — no debounce. Use for held movement checks.
    bool held(int pin) {
        return digitalRead(pin) == LOW;
    }

    // Returns true once on the falling edge of the given pin,
    // with its own independent debounce. Safe to call while
    // other buttons are held — each pin is fully independent.
    bool justPressed(int pin) {
        int idx = indexFor(pin);
        if (idx < 0) return false;

        unsigned long now   = millis();
        int           state = digitalRead(pin);
        bool          result = false;

        if (_prevState[idx] == HIGH && state == LOW
                && (now - _lastPress[idx]) >= DEBOUNCE_MS) {
            _lastPress[idx] = now;
            result = true;
        }

        _prevState[idx] = state;
        return result;
    }

private:
    static const unsigned long DEBOUNCE_MS = 120;
    static const int           BTN_COUNT   = 6;

    const int _pins[BTN_COUNT] = {
        BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_A, BTN_B
    };

    int           _prevState[BTN_COUNT];
    unsigned long _lastPress[BTN_COUNT];

    int indexFor(int pin) {
        for (int i = 0; i < BTN_COUNT; i++)
            if (_pins[i] == pin) return i;
        return -1;
    }
};
