#include "pixelstick.h"

#define DEBOUNCE_TIME 20 // ms for debounce
Switch userSwitch;

void setSwitch(Switch s)
{
    userSwitch = s;
}

Switch getSwitch()
{
    return userSwitch;
}

void checkSwitch()
{
    static unsigned long bounceTimer;

    int currentState = digitalRead(USER_SWITCH);

    switch (userSwitch)
    {
    case SWITCH_OFF:
        if (currentState == SWITCH_OFF) //  Not changed or bouncing
        {
            bounceTimer = millis();
            return;
        }
        else // The switch has changed
        {
            if (millis() - bounceTimer > DEBOUNCE_TIME)
            { //Make sure the debounce time has passed
                userSwitch = SWITCH_CHANGED_ON;
            }
        }
        break;
    case SWITCH_ON:
        if (currentState == SWITCH_ON) //  Not changed or bouncing
        {
            bounceTimer = millis();
            return;
        }
        else // The switch has changed
        {
            if (millis() - bounceTimer > DEBOUNCE_TIME)
            { //Make sure the debounce time has passed
                userSwitch = SWITCH_CHANGED_OFF;
            }
        }
        break;
    case SWITCH_CHANGED_OFF:
        userSwitch = SWITCH_OFF;
        break;
    case SWITCH_CHANGED_ON:
        userSwitch = SWITCH_ON;
        break;
    }
}