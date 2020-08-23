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
    attachInterrupt(0, readEncoder, 1);

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
    e_stopped = false;
}

void loop()
{
    // Get push button
    if (digitalRead(A5))
        push_button = false;
    else
        push_button = true;
    // Serial.print("Push BUTTON: ");
    // Serial.println(digitalRead(PUSH_BUTTON));

    // E-Stop trains, and reset speeds to zero
    if (push_button)
    {
        eStop();
    }

    // Get selected train
    // previous_train = current_train;
    getCurrentTrain();

    // Serial.print("Current Train: ");
    // Serial.println(current_train);

    // Get encoder button
    if (digitalRead(A4))
        encoder_button = false;
    else
        encoder_button = true;
    // Serial.print("Encoder Button: ");
    // Serial.println(digitalRead(ENCODER_BUTTON));

    // Allow selected train's speed to be changed
    if (current_train != -1)
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
        if (abs(current_encoder) < 5)
        {
            trains[current_train].speed = 0;
            trains[current_train].direction = 1;
            digitalWrite(6, 0x0);
            digitalWrite(9, 0x0);
            digitalWrite(train_LEDS[current_train], 0x0);
        }
        else
        {
            trains[current_train].speed = abs(current_encoder) < 5 ? 0 : abs(current_encoder) - 5;
            if (current_encoder < 0)
            {
                trains[current_train].direction = -1;
                digitalWrite(9, 0x0);
                digitalWrite(6, 0x1);
                digitalWrite(train_LEDS[current_train], 0x1);
            }
            else
            {
                digitalWrite(6, 0x0);
                digitalWrite(9, 0x1);
                digitalWrite(train_LEDS[current_train], 0x1);
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
    Serial.println(digitalRead(A1));
    Serial.println(digitalRead(A0));
    Serial.println(digitalRead(A3));
    Serial.println(digitalRead(A2));
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
}

void eStop()
{
    Serial.println("E Stopped");
    digitalWrite(9, 0x1);
    digitalWrite(6, 0x1);

    detachInterrupt(((0) == 0 ? 2 : ((0) == 1 ? 3 : ((0) == 2 ? 1 : ((0) == 3 ? 0 : ((0) == 7 ? 4 : -1))))));
    for (int i = 0; i < sizeof(trains); i++)
    {
        digitalWrite(train_LEDS[i], 0x0);
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
    while (millis() - e_stop_timer < 2000)
    {
        // Serial.println("E Stop Loop");
        if (digitalRead(A5))
            e_stop_timer = millis();
        delay(100);
    }

    digitalWrite(6, 0x0);
    digitalWrite(9, 0x0);
    delay(1000);
    attachInterrupt(0, readEncoder, 1);
}

void readEncoder()
{
    int val1 = digitalRead(2);
    int val2 = digitalRead(3);
    int change = 2;

    if (current_train != previous_train)
        {
            encoder_val = trains[current_train].speed * trains[current_train].direction;
            previous_train = current_train;
        }


    if (abs(encoder_val) <= 5)
        change = ((change * 0.5)>(1)?(change * 0.5):(1));

    if (val1 != val2)
    {
        encoder_val -= change;
        if (encoder_val < (126 + 5) * -1)
            encoder_val = (126 + 5) * -1;
    }
    else
    {
        encoder_val += change;
        if (encoder_val > (126 + 5))
            encoder_val = (126 + 5);
    }

}
