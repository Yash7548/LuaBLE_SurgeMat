/**
 * @file UserButton.h
 * @brief Header file for UserButton class - handles button state and events
 */

#ifndef USER_BUTTON_H
#define USER_BUTTON_H

#include <Arduino.h>


class UserButton
{
public:
    // Button event types
    enum class ButtonEvent
    {
        NONE,
        CLICK,
        DOUBLE_CLICK,
        LONG_PRESS,
        MULTI_CLICK
    };

    // Current button state
    struct ButtonState
    {
        bool isPressed;     // Button is currently pressed
        bool isLongPress;   // Current press is a long press
        uint8_t clickCount; // Number of clicks in multi-click sequence
    };

    // Button event flags
    struct ButtonEventFlags
    {
        bool click;       // Single click detected
        bool doubleClick; // Double click detected
        bool longPress;   // Long press detected
        bool multiClick;  // Multi-click detected
    };

    /**
     * @brief Constructor
     * @param buttonPin Pin number for button input
     */
    UserButton(uint8_t buttonPin);

    /**
     * @brief Initialize button hardware
     */
    void begin(bool backgroundMode = false);

    /**
     * @brief Set long press detection time
     * @param ms Time in milliseconds
     */
    void setLongPressTime(uint32_t ms);

    /**
     * @brief Set double click detection time
     * @param ms Time in milliseconds
     */
    void setDoubleClickTime(uint32_t ms);

    /**
     * @brief Set multi-click detection time
     * @param ms Time in milliseconds
     */
    void setMultiClickTime(uint32_t ms);

    /**
     * @brief Set maximum number of clicks for multi-click detection
     * @param count Maximum click count
     */
    void setMaxMultiClicks(uint8_t count);

    /**
     * @brief Get current button state
     * @return ButtonState struct
     */
    ButtonState getButtonState() const;

    /**
     * @brief Get current button events
     * @return ButtonEventFlags struct
     */
    ButtonEventFlags getButtonEvents() const;

    /**
     * @brief Clear all event flags
     */
    void clearEvents();

    /**
     * @brief Update button state machine
     * Should be called regularly in loop()
     */
    void tick();

private:
    // Button states for state machine
    enum class State
    {
        IDLE,
        DEBOUNCE_PRESS,
        PRESSED,
        WAIT_RELEASE,
        DEBOUNCE_RELEASE,
        WAIT_DOUBLE_CLICK
    };

    static const uint32_t DEBOUNCE_DELAY = 50; // Debounce time in ms

    bool _backgroundMode;
    static void tickTask(void *param);
    uint8_t _buttonPin;        // Hardware pin for button
    uint32_t _longPressTime;   // Time threshold for long press
    uint32_t _doubleClickTime; // Time window for double click
    uint32_t _multiClickTime;  // Time window for multi-click
    uint8_t _maxMultiClicks;   // Maximum clicks to detect

    State _state;                 // Current state machine state
    ButtonState _buttonState;     // Current button state
    ButtonEventFlags _eventFlags; // Current event flags

    uint32_t _pressStartTime;  // Time when button was pressed
    uint32_t _lastReleaseTime; // Time when button was last released
    uint32_t _stateTime;       // Time entered current state

    bool readButton() const;              // Read physical button state
    void resetInternalState();            // Reset state machine
    void transitionTo(State newState);    // Change state machine state
    void setEventFlag(ButtonEvent event); // Set event flag
    void handleButtonStateMachine();      // Process state machine
};

#endif // USER_BUTTON_H