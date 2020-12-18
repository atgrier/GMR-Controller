#include <Arduino.h>
#include <RHReliableDatagram.h>
#include <RH_RF69.h>
#include <Locomotive.h>
#include "controller.h"

RH_RF69 driver(RFM69_CS, RFM69_INT);
RHReliableDatagram manager(driver, CLIENT_ADDRESS);
uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];

int encoder_val = 0;

int current_train;
int previous_train;
bool push_button;
bool encoder_button;
uint32_t e_stop_timer;

Locomotive trains[] = {
    Locomotive(201),  // DB Steam
    Locomotive(202),  // Great Norther Steam
    Locomotive(203),  // RhB Ge 6/6 1 (Crocodile)
    Locomotive(204)   // Stainz
};

int train_LEDS[] = {
    TRAIN_LED_0,
    TRAIN_LED_1,
    TRAIN_LED_2,
    TRAIN_LED_3};

void setup()
{
    // Radio Module
    Serial.begin(115200);
    manager.init();
    driver.setFrequency(RF69_FREQ);
    driver.setTxPower(20, true);
    uint8_t key[] = {0xa, 0xb, 0xa, 0xd, 0xc, 0xa, 0xf, 0xe,
                     0xd, 0xe, 0xa, 0xd, 0xb, 0xe, 0xe, 0xf};
    driver.setEncryptionKey(key);
    manager.setTimeout(50);
    //manager.setRetries(0);

    // Speed Control Encoder
    pinMode(ENCODER_BUTTON, INPUT_PULLUP);
    pinMode(ENCODER_IN_1, INPUT_PULLUP);
    pinMode(ENCODER_IN_2, INPUT_PULLUP);

    // Train Selector
    pinMode(TRAIN_SELECTOR_0, INPUT);
    pinMode(TRAIN_SELECTOR_1, INPUT);
    pinMode(TRAIN_SELECTOR_2, INPUT);
    pinMode(TRAIN_SELECTOR_3, INPUT);
    pinMode(TRAIN_LED_0, OUTPUT);
    pinMode(TRAIN_LED_1, OUTPUT);
    pinMode(TRAIN_LED_2, OUTPUT);
    pinMode(TRAIN_LED_3, OUTPUT);

    // Push Button / Indicator LED
    pinMode(PUSH_BUTTON, INPUT_PULLUP);
    pinMode(INDICATOR_LED_0, OUTPUT);
    pinMode(INDICATOR_LED_1, OUTPUT);

    // Initialize variables
    getCurrentTrain();
    previous_train = current_train;
    indicatorLED(STOP);

    // Enable interrupt
    attachInterrupt(1, readEncoder, CHANGE);
}

void loop()
{
    // Get push button state
    if (digitalRead(PUSH_BUTTON))
        push_button = false;
    else
        push_button = true;

    // E-Stop trains, and reset speeds to zero
    if (push_button)
        eStop();

    // Get selected train
    // previous_train = current_train;
    getCurrentTrain();
    Serial.print("Current Train: ");
    Serial.println(current_train);

    // Get encoder button state
    if (digitalRead(ENCODER_BUTTON))
        encoder_button = false;
    else
        encoder_button = true;

    // Invalid train selected (switch has 8 positions)
    if (current_train == -1)
        indicatorLED(IDLE);

    // Valid train selected
    else
    {
        // If the selected train has changed, update encoder value based on speed of newly
        // selected train.
        // If train is stopped, set encoder to zero
        // Otherwise set encoder to (speed + deadzone) * direction
        DISABLE_readEncoder;
        if (current_train != previous_train)
        {
            encoder_val = (trains[current_train].speed == 0 ? 0 : trains[current_train].speed +
                           SPEED_DEADZONE) * trains[current_train].direction;
            previous_train = current_train;
        }

        // Get new speed and direction
        int current_encoder = encoder_val;
        Serial.print("Current Speed: ");
        Serial.println(current_encoder);

        // Inside deadzone, set speed to zero
        if (abs(current_encoder) < SPEED_DEADZONE + 1)
        {
            trains[current_train].speed = 0;
            trains[current_train].direction = 1;
            indicatorLED(STOP);
        }

        // Train is moving! Set the speed as abs(encoder) - deadzone
        else
        {
            trains[current_train].speed = abs(current_encoder) < SPEED_DEADZONE ? 0 : abs(current_encoder) - 5;

            // Reverse
            if (current_encoder < 0)
            {
                trains[current_train].direction = -1;
                indicatorLED(REVERSE);
            }

            // Forwards
            else
            {
                trains[current_train].direction = 1;
                indicatorLED(FORWARDS);
            }
        }
        ENABLE_readEncoder;
    }

    // Create and send commands
    for (Locomotive& train : trains)
    {
        char pdata[100];
        sprintf(pdata, "<t 1 3 %d %d>", train.speed, train.direction);
        manager.sendto((uint8_t *)pdata, strlen(pdata) + 1, train.ADDRESS);
    }

    delay(100);
}


// Get the currently selected locomotive
void getCurrentTrain()
{
    DISABLE_readEncoder;
    if (digitalRead(TRAIN_SELECTOR_0))
        current_train = 0;
    else if (digitalRead(TRAIN_SELECTOR_1))
        current_train = 1;
    else if (digitalRead(TRAIN_SELECTOR_2))
        current_train = 2;
    else if (digitalRead(TRAIN_SELECTOR_3))
        current_train = 3;
    else
        current_train = -1;
    if (current_train != previous_train)
        indicatorLED(RUNNING);
    ENABLE_readEncoder;
}


// Set indicator LED
void indicatorLED(int state)
{
    if (state == FORWARDS)
    {
        digitalWrite(INDICATOR_LED_1, LOW);
        analogWrite(INDICATOR_LED_0, map(trains[current_train].speed, 0, SPEED_MAX, 0, 255));
        digitalWrite(train_LEDS[current_train], (trains[current_train].speed == SPEED_MAX) ? HIGH : LOW);
    }

    else if (state == REVERSE)
    {
        digitalWrite(INDICATOR_LED_0, LOW);
        analogWrite(INDICATOR_LED_1, map(trains[current_train].speed, 0, SPEED_MAX, 0, 255));
        digitalWrite(train_LEDS[current_train], (trains[current_train].speed == SPEED_MAX) ? HIGH : LOW);
    }

    else if (state == STOP)
    {
        digitalWrite(INDICATOR_LED_0, LOW);
        digitalWrite(INDICATOR_LED_1, LOW);
        digitalWrite(train_LEDS[current_train], LOW);
    }

    else if (state == IDLE)
    {
        digitalWrite(INDICATOR_LED_0, LOW);
        digitalWrite(INDICATOR_LED_1, LOW);
    }

    else if (state == WARNING)
    {
        digitalWrite(INDICATOR_LED_0, HIGH);
        digitalWrite(INDICATOR_LED_1, HIGH);
    }

    else if (state == RUNNING);
    {
        if (previous_train == -1 || trains[previous_train].speed == 0) return;
        digitalWrite(train_LEDS[previous_train], HIGH);
    }
}


// Trigger E-Stop to stop all locomotives and idle until reset command recieved
void eStop()
{
    Serial.println("E Stopped");
    indicatorLED(WARNING);
    DISABLE_readEncoder;
    previous_train = -1;

    // Set all trains' speeds to zero
    for (int i = 0; i < NUM_TRAINS; i++)
    {
        digitalWrite(train_LEDS[i], LOW);
        trains[i].speed = 0;
        trains[i].direction = 1;
    }

    // Send stop command several times to ensure engines receive it
    for (int i = 0; i < 5; i++)
        for (Locomotive& train : trains)
        {
            char pdata[100];
            sprintf(pdata, "<t 1 3 -1 1>");
            manager.sendto((uint8_t *)pdata, strlen(pdata) + 1, train.ADDRESS);
        }

    // Reset command is holding e-stop button continuously for the duration (2000 milliseconds)
    e_stop_timer = millis();
    while (millis() - e_stop_timer < ESTOP_DURATION)
    {
        if (digitalRead(PUSH_BUTTON))
            e_stop_timer = millis();
        delay(100);
    }

    indicatorLED(IDLE);
    delay(1000);
    ENABLE_readEncoder;
}


// Interrupt service routine to get updated encoder values
// Should be triggered on `CHANGE`
void readEncoder()
{
    if (current_train < 0) return;

    int val1 = digitalRead(ENCODER_IN_1);
    int val2 = digitalRead(ENCODER_IN_2);
    int change = SPEED_CHANGE;

    // Inside deadzone, traverse slower
    if (abs(encoder_val) <= SPEED_DEADZONE)
        change = max(change * SPEED_DEADZONE_MULT, 1);


    // Decrease value
    if (val1 != val2)
    {
        encoder_val -= change;
        // When less than the minimum permissible value, reset to that minimum value.
        if (encoder_val < ENCODER_MAX * -1)
            encoder_val = ENCODER_MAX * -1;
    }

    // Increase value
    else
    {
        encoder_val += change;
        // When greater than the maximum permissible value, reset to that maximum value.
        if (encoder_val > ENCODER_MAX)
            encoder_val = ENCODER_MAX;
    }
}
