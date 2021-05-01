/*
  controller.ino
  Created by Alan T. Grier, 23 September 2019.
*/

#include "controller.h"

void setup()
{
  // Radio Module
  Serial.begin(115200);
  driver.setFrequency(RF69_FREQ);
  driver.setTxPower(20, true);
  uint8_t key[] = {0xa, 0xb, 0xa, 0xd, 0xc, 0xa, 0xf, 0xe,
                   0xd, 0xe, 0xa, 0xd, 0xb, 0xe, 0xe, 0xf};
  driver.setEncryptionKey(key);

  // Speed Control Encoder
  pinMode(BUTTON_ENCODER, INPUT_PULLUP);
  pinMode(ENCODER_IN_1, INPUT_PULLUP);
  pinMode(ENCODER_IN_2, INPUT_PULLUP);

  // Train Selector
  pinMode(TRAIN_SELECTOR_0, INPUT);
  pinMode(TRAIN_SELECTOR_1, INPUT);
  pinMode(TRAIN_SELECTOR_2, INPUT);
  pinMode(TRAIN_SELECTOR_3, INPUT);

  // Push Button / Indicator LED
  pinMode(BUTTON_PUSH, INPUT_PULLUP);

  // Initialize variables
  getCurrentTrain();
  previous_train = trains.current_train();
  trains.indicatorLED(STOP);

  // Enable interrupt
  attachInterrupt(1, readEncoder, CHANGE);
}

void loop()
{
  // Get push button state
  push_button = !digitalRead(BUTTON_PUSH);

  // E-Stop trains, and reset speeds to zero
  if (push_button)
    eStop();

  // Get selected train
  // previous_train = current_train;
  getCurrentTrain();
  Serial.print("Current Train: ");
  Serial.println(trains.current_train());

  // Get encoder button state
  encoder_button = !digitalRead(BUTTON_ENCODER);

  // Change locomotive's speed based on updated encoder value
  update_locomotive_speed();

  // Create and send commands
  trains.sendThrottles();

  // Wait 100 ms, since loop doesn't need to run as fast as possible
  delay(100);
}

void update_locomotive_speed()
{
  if (trains.current_train() == -1) // Invalid locomotive selected
    return;

  DISABLE_readEncoder;
  // If the selected train has changed, update encoder value based on speed of newly selected train.
  // If train is stopped, set encoder to zero
  // Otherwise set encoder to (speed + deadzone) * direction
  if (trains.current_train() != previous_train)
  {
    int current_speed = trains.current_speed();
    encoder_val = (current_speed == 0 ? 0 : current_speed + SPEED_DEADZONE) * trains.current_direction();
    previous_train = trains.current_train();
  }

  // Get new speed and direction
  int current_encoder = encoder_val;
  Serial.print("Current Speed: ");
  Serial.println(current_encoder);

  // Inside deadzone, set speed to zero
  if (abs(current_encoder) < SPEED_DEADZONE + 1)
  {
    trains.setSpeed(0, FORWARDS);
    trains.indicatorLED(STOP);
  }

  // Train is moving! Set the speed as abs(encoder) - deadzone
  else
  {
    int state = current_encoder < 0 ? REVERSE : FORWARDS;
    trains.setSpeed(GET_SPEED, state);
    trains.indicatorLED(THROTTLE);
  }
  ENABLE_readEncoder;
}

// Get the currently selected locomotive
void getCurrentTrain()
{
  DISABLE_readEncoder;
  trains.setCurrent(
      digitalRead(TRAIN_SELECTOR_0) ? 0 : digitalRead(TRAIN_SELECTOR_1) ? 1
                                      : digitalRead(TRAIN_SELECTOR_2)   ? 2
                                      : digitalRead(TRAIN_SELECTOR_3)   ? 3
                                                                        : -1);

  if (trains.current_train() != previous_train)
    trains.indicatorLED(RUNNING, previous_train);
  ENABLE_readEncoder;

  // Invalid train selected (switch has 4 positions)
  if (trains.current_train() == -1)
    trains.indicatorLED(IDLE);
}

// Trigger E-Stop to stop all locomotives and idle until reset command recieved
void eStop()
{
  Serial.println("E Stopped");
  trains.indicatorLED(WARNING);
  DISABLE_readEncoder;
  previous_train = -1;

  // Send stop command several times to ensure engines receive it
  trains.eStopAll();

  // Reset command is holding e-stop button continuously for the duration (2000 milliseconds)
  e_stop_timer = millis();
  while (millis() - e_stop_timer < ESTOP_DURATION)
  {
    if (digitalRead(BUTTON_PUSH))
      e_stop_timer = millis();
    delay(100);
  }

  trains.indicatorLED(IDLE);
  delay(1000);
  ENABLE_readEncoder;
}

// Interrupt service routine to get updated encoder values
// Should be triggered on `CHANGE`
void readEncoder()
{
  if (trains.current_train() < 0)
    return;

  int val1 = digitalRead(ENCODER_IN_1);
  int val2 = digitalRead(ENCODER_IN_2);
  int change = SPEED_CHANGE;

  // Inside deadzone, traverse slower
  if (abs(encoder_val) <= SPEED_DEADZONE)
    change = (int)fmax(change * SPEED_DEADZONE_MULT, 1);

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
