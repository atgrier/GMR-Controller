// Radio parameters
#define CLIENT_ADDRESS 101
#define RF69_FREQ 868.0
#define RFM69_CS 8
#define RFM69_INT 7

// Rotary encoder inputs
#define ENCODER_BUTTON A4
#define ENCODER_IN_1 2
#define ENCODER_IN_2 3

// Outputs for selector switch red LEDs
#define TRAIN_LED_0 10
#define TRAIN_LED_1 11
#define TRAIN_LED_2 12
#define TRAIN_LED_3 13

// Inputs for selector switch
#define TRAIN_SELECTOR_0 A1
#define TRAIN_SELECTOR_1 A0
#define TRAIN_SELECTOR_2 A3
#define TRAIN_SELECTOR_3 A2

// Push button input
#define PUSH_BUTTON A5

// Red/Blue indicator LED outputs
#define INDICATOR_LED_0 6
#define INDICATOR_LED_1 9

// Indicator LED states
#define FORWARDS 0
#define REVERSE 1
#define STOP 2
#define IDLE 3
#define WARNING 4
#define RUNNING 5

// Encoder and speed parameters
#define SPEED_CHANGE 2  // Amount to change encoder for a single step
#define SPEED_DEADZONE 5  // Size of encoder deadzone when calculating speed, +/- from zero
#define SPEED_DEADZONE_MULT 0.5  // Modify how much each step changes encoder value in deadzone
#define SPEED_MAX 126  // Maximum speed (parameter of motor controller or DCC decoder)
#define ENCODER_MAX (SPEED_MAX + SPEED_DEADZONE)  // Maximum encoder value

// Button push time required (milliseconds) to leave e-stop mode
#define ESTOP_DURATION 2000

// Interrupt control
#define ENABLE_readEncoder  (EIMSK |= bit (INT1))
#define DISABLE_readEncoder (EIMSK &= ~(bit (INT1)))

#define NUM_TRAINS ((int) (sizeof(trains) / sizeof(Locomotive)))
