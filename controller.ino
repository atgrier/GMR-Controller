#include <Arduino.h>
#include <RHReliableDatagram.h>
#include <RH_RF69.h>
#include "train.h"
#include "controller.h"

RH_RF69 driver(RFM69_CS, RFM69_INT);
RHReliableDatagram manager(driver, CLIENT_ADDRESS);
uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];

int encoder_val = 0;
// int encoder_vals[] = {
//     0,
//     0,
//     0,
//     0
// };

int current_train;
int previous_train;
bool push_button;
bool encoder_button;
bool e_stopped;
uint32_t e_stop_timer;

Train trains[] = {
    Train(201),  // DB Steam
    Train(202),  // Great Norther Steam
    Train(203),  // RhB Ge 6/6 1 (Crocodile)
    Train(204)   // Stainz
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
    attachInterrupt(0, readEncoder, CHANGE);

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
    e_stopped = false;
}

void loop()
{
    // Get push button
    if (digitalRead(PUSH_BUTTON))
        push_button = false;
    else
        push_button = true;
    // Serial.print("Push BUTTON: ");
    // Serial.println(digitalRead(PUSH_BUTTON));

    // E-Stop trains, and reset speeds to zero
    if (push_button)
        eStop();

    // Get selected train
    // previous_train = current_train;
    getCurrentTrain();

    // Serial.print("Current Train: ");
    // Serial.println(current_train);

    // Get encoder button
    if (digitalRead(ENCODER_BUTTON))
        encoder_button = false;
    else
        encoder_button = true;
    // Serial.print("Encoder Button: ");
    // Serial.println(digitalRead(ENCODER_BUTTON));

    // Allow selected train's speed to be changed
    if (current_train == -1)
    {
        digitalWrite(INDICATOR_LED_0, LOW);
        digitalWrite(INDICATOR_LED_1, LOW);
    }
    else
    {
        Serial.print("Current Train: ");
        Serial.println(current_train);
        if (current_train != previous_train)
        {
            // detachInterrupt(digitalPinToInterrupt(0));
            encoder_val = (trains[current_train].speed == 0 ? 0 : trains[current_train].speed + 5)
                * trains[current_train].direction;
            // attachInterrupt(0, readEncoder, CHANGE);
            previous_train = current_train;
        }

        // Get new speed and direction
        int current_encoder = encoder_val;
        Serial.print("Current Speed: ");
        Serial.println(current_encoder);
        if (abs(current_encoder) < SPEED_DEADZONE + 1)
        {
            trains[current_train].speed = 0;
            trains[current_train].direction = 1;
            digitalWrite(INDICATOR_LED_0, LOW);
            digitalWrite(INDICATOR_LED_1, LOW);
            digitalWrite(train_LEDS[current_train], LOW);
        }
        else
        {
            trains[current_train].speed = abs(current_encoder) < SPEED_DEADZONE ? 0 : abs(current_encoder) - 5;
            if (current_encoder < 0)
            {
                trains[current_train].direction = -1;
                digitalWrite(INDICATOR_LED_0, LOW);
                digitalWrite(INDICATOR_LED_1, HIGH);
                digitalWrite(train_LEDS[current_train], HIGH);
            }
            else
            {
                digitalWrite(INDICATOR_LED_1, LOW);
                digitalWrite(INDICATOR_LED_0, HIGH);
                digitalWrite(train_LEDS[current_train], HIGH);
            }
        }
    }

    // Create and send commands
    // for (Train& train : trains)
    // {
    //     char pdata[100];
    //     sprintf(pdata, "<t 1 3 %d %d>", train.speed, train.direction);
    //     manager.sendto((uint8_t *)pdata, strlen(pdata) + 1, train.ADDRESS);
    // }

    delay(100);
}

void getCurrentTrain()
{
    Serial.println("Train Selectors:");
    Serial.println(digitalRead(TRAIN_SELECTOR_0));
    Serial.println(digitalRead(TRAIN_SELECTOR_1));
    Serial.println(digitalRead(TRAIN_SELECTOR_2));
    Serial.println(digitalRead(TRAIN_SELECTOR_3));
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
}

void eStop()
{
    Serial.println("E Stopped");
    digitalWrite(INDICATOR_LED_0, HIGH);
    digitalWrite(INDICATOR_LED_1, HIGH);
    previous_train = -1;

    // detachInterrupt(digitalPinToInterrupt(0));
    for (int i = 0; i < sizeof(trains); i++)
    {
        digitalWrite(train_LEDS[i], LOW);
        trains[i].speed = 0;
        trains[i].direction = 1;
    }

    // // Send command several times to ensure engines receive it
    // for (int i = 0; i < 5; i++)
    //     for (Train& train : trains)
    //     {
    //         char pdata[100];
    //         sprintf(pdata, "<t 1 3 -1 1>");
    //         // manager.sendto((uint8_t *)pdata, strlen(pdata) + 1, train.ADDRESS);
    //     }

    e_stop_timer = millis();
    // Serial.println(e_stop_timer);
    // Serial.println(millis());
    // Serial.println(millis() - e_stop_timer);
    while (millis() - e_stop_timer < ESTOP_DURATION)
    {
        // Serial.println("E Stop Loop");
        if (digitalRead(PUSH_BUTTON))
            e_stop_timer = millis();
        delay(100);
    }

    digitalWrite(INDICATOR_LED_0, LOW);
    digitalWrite(INDICATOR_LED_1, LOW);
    delay(1000);
    // attachInterrupt(0, readEncoder, CHANGE);
}

void readEncoder()
{
    int val1 = digitalRead(ENCODER_IN_1);
    int val2 = digitalRead(ENCODER_IN_2);
    int change = SPEED_CHANGE;

    // if (current_train != previous_train)
    //     {
    //         encoder_val = (trains[current_train].speed == 0 ? 0 : trains[current_train].speed + 5)
    //             * trains[current_train].direction;
    //         // encoder_val = trains[current_train].speed * trains[current_train].direction;
    //         previous_train = current_train;
    //     }

    if (abs(encoder_val) <= SPEED_DEADZONE)
        change = max(change * SPEED_DEADZONE_MULT, 1);

    if (val1 != val2)
    {
        encoder_val -= change;
        if (encoder_val < ENCODER_MAX * -1)
            encoder_val = ENCODER_MAX * -1;
    }
    else
    {
        encoder_val += change;
        if (encoder_val > ENCODER_MAX)
            encoder_val = ENCODER_MAX;
    }

}
