# 1 "c:\\Users\\Alan\\Google Drive\\Documents\\Arduino\\wireless_loco\\controller\\controller.ino"
# 2 "c:\\Users\\Alan\\Google Drive\\Documents\\Arduino\\wireless_loco\\controller\\controller.ino" 2
# 3 "c:\\Users\\Alan\\Google Drive\\Documents\\Arduino\\wireless_loco\\controller\\controller.ino" 2
# 4 "c:\\Users\\Alan\\Google Drive\\Documents\\Arduino\\wireless_loco\\controller\\controller.ino" 2
# 5 "c:\\Users\\Alan\\Google Drive\\Documents\\Arduino\\wireless_loco\\controller\\controller.ino" 2
# 6 "c:\\Users\\Alan\\Google Drive\\Documents\\Arduino\\wireless_loco\\controller\\controller.ino" 2

RH_RF69 driver(8, 7);
RHReliableDatagram manager(driver, 101);
uint8_t buf[(64 - 4)];

int encoder_val = 0;
// bool encoder_val_locked = false;

int current_train;
int previous_train;
bool push_button;
bool encoder_button;
uint32_t e_stop_timer;

Train trains[] = {
    Train(201), // DB Steam
    Train(202), // Great Norther Steam
    Train(203), // RhB Ge 6/6 1 (Crocodile)
    Train(204) // Stainz
};

int train_LEDS[] = {
    10,
    11,
    12,
    13};

void setup()
{
    // Radio Module
    Serial.begin(115200);
    manager.init();
    driver.setFrequency(868.0);
    driver.setTxPower(20, true);
    uint8_t key[] = {0xa, 0xb, 0xa, 0xd, 0xc, 0xa, 0xf, 0xe,
                     0xd, 0xe, 0xa, 0xd, 0xb, 0xe, 0xe, 0xf};
    driver.setEncryptionKey(key);
    manager.setTimeout(50);
    //manager.setRetries(0);

    // Speed Control Encoder
    pinMode(A4, 0x2);
    pinMode(2, 0x2);
    pinMode(3, 0x2);

    // Train Selector
    pinMode(A1, 0x0);
    pinMode(A0, 0x0);
    pinMode(A3, 0x0);
    pinMode(A2, 0x0);
    pinMode(10, 0x1);
    pinMode(11, 0x1);
    pinMode(12, 0x1);
    pinMode(13, 0x1);

    // Push Button / Indicator LED
    pinMode(A5, 0x2);
    pinMode(6, 0x1);
    pinMode(9, 0x1);

    // Initialize variables
    getCurrentTrain();
    previous_train = current_train;

    // Enable interrupt
    attachInterrupt(0, readEncoder, 1);
}

void loop()
{
    // Get push button state
    if (digitalRead(A5))
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
    if (digitalRead(A4))
        encoder_button = false;
    else
        encoder_button = true;

    // Invalid train selected (switch has 8 positions)
    if (current_train == -1)
        indicatorLED(3, false);

    // Valid train selected
    else
    {
        // If the selected train has changed, update encoder value based on speed of newly
        // selected train.
        // If train is stopped, set encoder to zero
        // Otherwise set encoder to (speed + deadzone) * direction
        if (current_train != previous_train)
        {
            // encoder_val_locked = true;
            encoder_val = (trains[current_train].speed == 0 ? 0 : trains[current_train].speed +
                           5 /* Size of encoder deadzone when calculating speed, +/- from zero*/) * trains[current_train].direction;
            previous_train = current_train;
            // encoder_val_locked = false;
        }

        // Get new speed and direction
        int current_encoder = encoder_val;
        Serial.print("Current Speed: ");
        Serial.println(current_encoder);

        // Inside deadzone, set speed to zero
        if (abs(current_encoder) < 5 /* Size of encoder deadzone when calculating speed, +/- from zero*/ + 1)
        {
            trains[current_train].speed = 0;
            trains[current_train].direction = 1;
            indicatorLED(3, true);
        }

        // Train is moving! Set the speed as abs(encoder) - deadzone
        else
        {
            trains[current_train].speed = abs(current_encoder) < 5 /* Size of encoder deadzone when calculating speed, +/- from zero*/ ? 0 : abs(current_encoder) - 5;

            // Reverse
            if (current_encoder < 0)
            {
                trains[current_train].direction = -1;
                indicatorLED(1, true);
            }

            // Forwards
            else
            {
                trains[current_train].direction = 1;
                indicatorLED(0, true);
            }
        }
    }

    // Create and send commands
    for (Train& train : trains)
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
    if (digitalRead(A1))
        current_train = 0;
    else if (digitalRead(A0))
        current_train = 1;
    else if (digitalRead(A3))
        current_train = 2;
    else if (digitalRead(A2))
        current_train = 3;
    else
        current_train = -1;
    delay(1); // Needed in order to exit e-stop condition properly. Don't ask why
}


// Set indicator LED
void indicatorLED(int state, bool writeTrainLED)
{
    if (state == 0)
    {
        digitalWrite(9, 0x0);
        digitalWrite(6, 0x1);
        if (writeTrainLED)
            digitalWrite(train_LEDS[current_train], 0x1);
    }

    else if (state == 1)
    {
        digitalWrite(6, 0x0);
        digitalWrite(9, 0x1);
        if (writeTrainLED)
            digitalWrite(train_LEDS[current_train], 0x1);
    }

    else if (state == 3)
    {
        digitalWrite(6, 0x0);
        digitalWrite(9, 0x0);
        if (writeTrainLED)
            digitalWrite(train_LEDS[current_train], 0x0);
    }

    else if (state == 4)
    {
        digitalWrite(6, 0x1);
        digitalWrite(9, 0x1);
    }
}


// Trigger E-Stop to stop all locomotives and idle until reset command recieved
void eStop()
{
    Serial.println("E Stopped");
    indicatorLED(4, false);
    previous_train = -1;

    // Set all trains' speeds to zero
    for (int i = 0; i < sizeof(trains); i++)
    {
        digitalWrite(train_LEDS[i], 0x0);
        trains[i].speed = 0;
        trains[i].direction = 1;
    }

    // Send stop command several times to ensure engines receive it
    for (int i = 0; i < 5; i++)
        for (Train& train : trains)
        {
            char pdata[100];
            sprintf(pdata, "<t 1 3 -1 1>");
            manager.sendto((uint8_t *)pdata, strlen(pdata) + 1, train.ADDRESS);
        }

    // Reset command is holding e-stop button continuously for the duration (2000 milliseconds)
    e_stop_timer = millis();
    while (millis() - e_stop_timer < 2000)
    {
        if (digitalRead(A5))
            e_stop_timer = millis();
        delay(100);
    }

    indicatorLED(3, false);
    delay(1000);
}


// Interrupt service routine to get updated encoder values
// Should be triggered on `CHANGE`
void readEncoder()
{
    // if (encoder_val_locked)
    //     return;
    // {
    int val1 = digitalRead(2);
    int val2 = digitalRead(3);
    int change = 2 /* Amount to change encoder for a single step*/;

    // Inside deadzone, traverse slower
    if (abs(encoder_val) <= 5 /* Size of encoder deadzone when calculating speed, +/- from zero*/)
        change = ((change * 0.5 /* Modify how much each step changes encoder value in deadzone*/)>(1)?(change * 0.5 /* Modify how much each step changes encoder value in deadzone*/):(1));


    // Decrease value
    if (val1 != val2)
    {
        encoder_val -= change;
        // When less than the minimum permissible value, reset to that minimum value.
        if (encoder_val < (126 /* Maximum speed (parameter of motor controller or DCC decoder)*/ + 5 /* Size of encoder deadzone when calculating speed, +/- from zero*/) /* Maximum encoder value*/ * -1)
            encoder_val = (126 /* Maximum speed (parameter of motor controller or DCC decoder)*/ + 5 /* Size of encoder deadzone when calculating speed, +/- from zero*/) /* Maximum encoder value*/ * -1;
    }

    // Increase value
    else
    {
        encoder_val += change;
        // When greater than the maximum permissible value, reset to that maximum value.
        if (encoder_val > (126 /* Maximum speed (parameter of motor controller or DCC decoder)*/ + 5 /* Size of encoder deadzone when calculating speed, +/- from zero*/) /* Maximum encoder value*/)
            encoder_val = (126 /* Maximum speed (parameter of motor controller or DCC decoder)*/ + 5 /* Size of encoder deadzone when calculating speed, +/- from zero*/) /* Maximum encoder value*/;
    }
    // Serial.println(encoder_val);
    // }
}
