// ============================================================================
// CONTACT_SENSORS.CPP - Limit Switch / Contact Sensor Implementation
// ============================================================================

#include "hardware/ContactSensors.h"

// ============================================================================
// SINGLETON INSTANCE
// ============================================================================

ContactSensors& ContactSensors::getInstance() {
    static ContactSensors instance;
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void ContactSensors::init() {
    if (m_initialized) return;  // Prevent double initialization
    
    // Configure contact pins as inputs with internal pull-up
    // Contacts are normally open (HIGH), closed when engaged (LOW)
    pinMode(PIN_START_CONTACT, INPUT_PULLUP);
    pinMode(PIN_END_CONTACT, INPUT_PULLUP);
    
    m_initialized = true;
}

// ============================================================================
// DEBOUNCED READING - SPECIFIC CONTACTS
// ============================================================================

bool ContactSensors::isStartContactActive(uint8_t checks, uint16_t delayUs) {
    // Contact is active when LOW (closed to ground)
    return readDebounced(PIN_START_CONTACT, LOW, checks, delayUs);
}

bool ContactSensors::isEndContactActive(uint8_t checks, uint16_t delayUs) {
    // Contact is active when LOW (closed to ground)
    return readDebounced(PIN_END_CONTACT, LOW, checks, delayUs);
}

// ============================================================================
// RAW READING (NO DEBOUNCE)
// ============================================================================

bool ContactSensors::readStartContactRaw() {
    return digitalRead(PIN_START_CONTACT) == LOW;
}

bool ContactSensors::readEndContactRaw() {
    return digitalRead(PIN_END_CONTACT) == LOW;
}

// ============================================================================
// GENERIC DEBOUNCED READING
// ============================================================================

bool ContactSensors::readDebounced(uint8_t pin, uint8_t expectedState, uint8_t checks, uint16_t delayUs) {
    // Majority voting algorithm
    // Requires (checks/2 + 1) matching reads to confirm state
    // Example: 3 checks requires 2/3, 5 checks requires 3/5
    
    int validCount = 0;
    int requiredValid = (checks + 1) / 2;  // Ceiling division for majority
    
    for (uint8_t i = 0; i < checks; i++) {
        if (digitalRead(pin) == expectedState) {
            validCount++;
            
            // Early exit: majority already reached
            if (validCount >= requiredValid) {
                return true;
            }
        }
        
        // Delay between samples (except after last sample)
        if (i < checks - 1) {
            delayMicroseconds(delayUs);
        }
    }
    
    // Not enough valid reads to confirm expected state
    return false;
}
