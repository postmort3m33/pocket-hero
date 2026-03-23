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
#define BTN_B      13

// ─────────────────────────────────────────────────────────────
//  Button events
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
//  poll()  — per-button edge detection with independent debounce.
//            Returns one event per call (highest-priority pending
//            press). Call once per loop() frame.
//            Directional held-state never blocks A or B.
//
//  held()  — raw synchronous read, no debounce. Use for smooth
//            movement (called independently of poll()).
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

        // Initialise previous-state to released (HIGH = not pressed)
        for (int i = 0; i < BTN_COUNT; i++) {
            _prevState[i]  = HIGH;
            _lastPress[i]  = 0;
        }
    }

    // Returns a single event — fires once on the falling edge of each
    // button press, with its own independent debounce timer.
    // Returns BTN_NONE if no new press this frame.
    ButtonEvent poll() {
        unsigned long now = millis();

        // Scan all buttons; return the first newly-pressed one found.
        for (int i = 0; i < BTN_COUNT; i++) {
            int  pin      = _pins[i];
            int  current  = digitalRead(pin);
            bool debounced = (now - _lastPress[i]) >= DEBOUNCE_MS;

            // Falling edge (HIGH → LOW) + debounce window clear
            if (_prevState[i] == HIGH && current == LOW && debounced) {
                _prevState[i] = LOW;
                _lastPress[i] = now;
                return _events[i];
            }

            // Track rising edge so next press can be detected
            if (current == HIGH) {
                _prevState[i] = HIGH;
            }
        }

        return BTN_NONE;
    }

    // Raw read — no debounce, use for held movement checks
    bool held(int pin) {
        return digitalRead(pin) == LOW;
    }

private:
    static const unsigned long DEBOUNCE_MS = 120;
    static const int           BTN_COUNT   = 6;

    // Button order doesn't affect priority anymore — all are independent
    const int         _pins[BTN_COUNT]   = { BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_A, BTN_B };
    const ButtonEvent _events[BTN_COUNT] = { BTN_EV_UP, BTN_EV_DOWN, BTN_EV_LEFT, BTN_EV_RIGHT, BTN_EV_A, BTN_EV_B };

    int           _prevState[BTN_COUNT];
    unsigned long _lastPress[BTN_COUNT];
};
