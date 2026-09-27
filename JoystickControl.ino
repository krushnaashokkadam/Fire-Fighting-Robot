#define CUSTOM_SETTINGS
#define INCLUDE_GAMEPAD_MODULE
#include <DabbleESP32.h>

// Right motor (mapped from earlier code)
int enableRightMotor = 32;   // ENB (was 32 in earlier sketch)
int rightMotorPin1  = 25;   // IN3
int rightMotorPin2  = 33;   // IN4

// Left motor (mapped from earlier code)
int enableLeftMotor  = 14;   // ENA (was 14 in earlier sketch)
int leftMotorPin1    = 27;   // IN1
int leftMotorPin2    = 26;   // IN2

#define MAX_MOTOR_SPEED 255

const int PWMFreq = 1000; /* 1 KHz */
const int PWMResolution = 8;
const int rightMotorPWMSpeedChannel = 4;
const int leftMotorPWMSpeedChannel = 5;

void rotateMotor(int rightMotorSpeed, int leftMotorSpeed)
{
  // Right motor direction
  if (rightMotorSpeed < 0)
  {
    digitalWrite(rightMotorPin1, LOW);
    digitalWrite(rightMotorPin2, HIGH);
  }
  else if (rightMotorSpeed > 0)
  {
    digitalWrite(rightMotorPin1, HIGH);
    digitalWrite(rightMotorPin2, LOW);
  }
  else
  {
    digitalWrite(rightMotorPin1, LOW);
    digitalWrite(rightMotorPin2, LOW);
  }

  // Left motor direction
  if (leftMotorSpeed < 0)
  {
    digitalWrite(leftMotorPin1, LOW);
    digitalWrite(leftMotorPin2, HIGH);
  }
  else if (leftMotorSpeed > 0)
  {
    digitalWrite(leftMotorPin1, HIGH);
    digitalWrite(leftMotorPin2, LOW);
  }
  else
  {
    digitalWrite(leftMotorPin1, LOW);
    digitalWrite(leftMotorPin2, LOW);
  }

  // write PWM (speed) - using absolute values
  ledcWrite(rightMotorPWMSpeedChannel, constrain(abs(rightMotorSpeed), 0, MAX_MOTOR_SPEED));
  ledcWrite(leftMotorPWMSpeedChannel, constrain(abs(leftMotorSpeed), 0, MAX_MOTOR_SPEED));
}

void setUpPinModes()
{
  pinMode(enableRightMotor, OUTPUT);
  pinMode(rightMotorPin1, OUTPUT);
  pinMode(rightMotorPin2, OUTPUT);

  pinMode(enableLeftMotor, OUTPUT);
  pinMode(leftMotorPin1, OUTPUT);
  pinMode(leftMotorPin2, OUTPUT);

  // Set up PWM for speed
  ledcSetup(rightMotorPWMSpeedChannel, PWMFreq, PWMResolution);
  ledcSetup(leftMotorPWMSpeedChannel, PWMFreq, PWMResolution);
  ledcAttachPin(enableRightMotor, rightMotorPWMSpeedChannel);
  ledcAttachPin(enableLeftMotor, leftMotorPWMSpeedChannel);

  rotateMotor(0, 0);
}

void setup()
{
  setUpPinModes();
  Dabble.begin("MyBluetoothCar");
  // Optional Serial for debugging:
  // Serial.begin(115200);
}

void loop()
{
  int rightMotorSpeed = 0;
  int leftMotorSpeed  = 0;

  Dabble.processInput(); // update GamePad data

  // Read joystick axes (values typically -7 .. 7)
  // NOTE: joyX is negated to flip left/right joystick direction
  int joyX = -GamePad.getXaxisData(); // left (-) to right (+) => negated
  int joyY = GamePad.getYaxisData();  // down (-) to up (+)

  // If joystick is moved enough, use analog joystick control
  if (abs(joyX) > 1 || abs(joyY) > 1)
  {
    // Map joystick -7..7 to -MAX..MAX
    int forward = map(joyY, -7, 7, -MAX_MOTOR_SPEED, MAX_MOTOR_SPEED);
    int turn    = map(joyX, -7, 7, -MAX_MOTOR_SPEED, MAX_MOTOR_SPEED);

    // Mix forward and turn into left/right motor speeds (arcade drive)
    // With joyX negated above, this flips left/right control
    long r = (long)forward - (long)turn;
    long l = (long)forward + (long)turn;

    // Clamp to allowed range
    r = constrain(r, -MAX_MOTOR_SPEED, MAX_MOTOR_SPEED);
    l = constrain(l, -MAX_MOTOR_SPEED, MAX_MOTOR_SPEED);

    rightMotorSpeed = (int)r;
    leftMotorSpeed  = (int)l;
  }
  else
  {
    // Joystick near center -> use digital D-pad buttons (left/right swapped)
    if (GamePad.isUpPressed())
    {
      rightMotorSpeed = MAX_MOTOR_SPEED;
      leftMotorSpeed  = MAX_MOTOR_SPEED;
    }
    else if (GamePad.isDownPressed())
    {
      rightMotorSpeed = -MAX_MOTOR_SPEED;
      leftMotorSpeed  = -MAX_MOTOR_SPEED;
    }
    // swapped: pressing Left will now perform the previous Right action
    else if (GamePad.isLeftPressed())
    {
      rightMotorSpeed = -MAX_MOTOR_SPEED;
      leftMotorSpeed  = MAX_MOTOR_SPEED;
    }
    // swapped: pressing Right will now perform the previous Left action
    else if (GamePad.isRightPressed())
    {
      rightMotorSpeed = MAX_MOTOR_SPEED;
      leftMotorSpeed  = -MAX_MOTOR_SPEED;
    }
    else
    {
      // no input: stop
      rightMotorSpeed = 0;
      leftMotorSpeed  = 0;
    }
  }

  rotateMotor(rightMotorSpeed, leftMotorSpeed);
}
