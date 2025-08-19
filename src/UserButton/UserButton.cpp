/**
 * @file UserButton.cpp
 * @brief Implementation of UserButton class
 */

#include "UserButton.h"

UserButton::UserButton(uint8_t buttonPin) : 
    _buttonPin(buttonPin),
    _longPressTime(1000),        // Default 1 second for long press
    _doubleClickTime(250),       // Default 250ms for double click
    _multiClickTime(500),        // Default 500ms for multi-click
    _maxMultiClicks(3),          // Default max 3 clicks
    _state(State::IDLE),
    _buttonState{0},
    _eventFlags{0},
    _pressStartTime(0),
    _lastReleaseTime(0),
    _stateTime(0)
{}

void UserButton::begin(bool backgroundMode) {
    pinMode(_buttonPin, INPUT_PULLUP);
    resetInternalState();
    _backgroundMode = backgroundMode;
    if (_backgroundMode) {
        xTaskCreate(tickTask, "UserButtonTickTask", 2048, this, 1, NULL);
    }

}

void UserButton::setLongPressTime(uint32_t ms) {
    _longPressTime = ms;
}

void UserButton::setDoubleClickTime(uint32_t ms) {
    _doubleClickTime = ms;
}

void UserButton::setMultiClickTime(uint32_t ms) {
    _multiClickTime = ms;
}

void UserButton::setMaxMultiClicks(uint8_t count) {
    _maxMultiClicks = count;
}

UserButton::ButtonState UserButton::getButtonState() const {
    return _buttonState;
}

UserButton::ButtonEventFlags UserButton::getButtonEvents() const {
    return _eventFlags;
}

void UserButton::clearEvents() {
    _eventFlags = ButtonEventFlags{0};
}

bool UserButton::readButton() const {
    return !digitalRead(_buttonPin); // Inverted due to INPUT_PULLUP
}

void UserButton::resetInternalState() {
    _state = State::IDLE;
    _buttonState = ButtonState{0};
    _eventFlags = ButtonEventFlags{0};
    _pressStartTime = 0;
    _lastReleaseTime = 0;
    _stateTime = 0;
}

void UserButton::transitionTo(State newState) {
    _state = newState;
    _stateTime = millis();
}

void UserButton::setEventFlag(ButtonEvent event) {
    switch (event) {
        case ButtonEvent::CLICK:
            _eventFlags.click = true;
            break;

        case ButtonEvent::DOUBLE_CLICK:
            _eventFlags.doubleClick = true;
            break;

        case ButtonEvent::LONG_PRESS:
            _eventFlags.longPress = true;
            break;

        case ButtonEvent::MULTI_CLICK:
            _eventFlags.multiClick = true;
            break;

        default:
            break;
    }
}

void UserButton::handleButtonStateMachine() {
    const uint32_t now = millis();
    const bool buttonPressed = readButton();

    switch (_state) {
        case State::IDLE:
            if (buttonPressed) {
                transitionTo(State::DEBOUNCE_PRESS);
            }
            break;

        case State::DEBOUNCE_PRESS:
            if (now - _stateTime >= DEBOUNCE_DELAY) {
                if (buttonPressed) {
                    _pressStartTime = now;
                    _buttonState.isPressed = true;
                    transitionTo(State::PRESSED);
                } else {
                    transitionTo(State::IDLE);
                }
            }
            break;

        case State::PRESSED:
            if (!buttonPressed) {
                transitionTo(State::DEBOUNCE_RELEASE);
            } else if (now - _pressStartTime >= _longPressTime) {
                _buttonState.isLongPress = true;
                setEventFlag(ButtonEvent::LONG_PRESS);
                transitionTo(State::WAIT_RELEASE);
            }
            break;

        case State::WAIT_RELEASE:
            if (!buttonPressed) {
                transitionTo(State::DEBOUNCE_RELEASE);
            }
            break;

        case State::DEBOUNCE_RELEASE:
            if (now - _stateTime >= DEBOUNCE_DELAY) {
                _buttonState.isPressed = false;
                _buttonState.isLongPress = false;
                _lastReleaseTime = now;
                _buttonState.clickCount++; // Increment count on release

                if (_buttonState.clickCount > _maxMultiClicks) {
                    setEventFlag(ButtonEvent::MULTI_CLICK);
                    _buttonState.clickCount = 0;
                    transitionTo(State::IDLE);
                } else {
                    transitionTo(State::WAIT_DOUBLE_CLICK);
                }
            }
            break;

        case State::WAIT_DOUBLE_CLICK:
            if (buttonPressed) {
                transitionTo(State::DEBOUNCE_PRESS);
            } else if (now - _lastReleaseTime >= _doubleClickTime) {
                // Process events based on actual click count
                switch (_buttonState.clickCount) {
                    case 1:
                        setEventFlag(ButtonEvent::CLICK);
                        break;
                    case 2:
                        setEventFlag(ButtonEvent::DOUBLE_CLICK);
                        break;
                    case 3:
                        setEventFlag(ButtonEvent::MULTI_CLICK);
                        break;
                }
                _buttonState.clickCount = 0;
                transitionTo(State::IDLE);
            }
            break;
    }
}

void UserButton::tick() {
    handleButtonStateMachine();
}

void UserButton::tickTask(void *param)
{
    UserButton *userbutton = static_cast<UserButton *>(param);
    for (;;)
    {
        userbutton->tick();
        vTaskDelay(10);
    }
    vTaskDelete(NULL);
}