#define CUSTOM_SETTINGS
#define INCLUDE_GAMEPAD_MODULE
#include <DabbleESP32.h>
#include <ESP32Servo.h>

// ---------------- Motor Pins ----------------
int enableRightMotor = 32;
int rightMotorPin1   = 25;
int rightMotorPin2   = 33;

int enableLeftMotor  = 14;
int leftMotorPin1    = 27;
int leftMotorPin2    = 26;

#define MAX_MOTOR_SPEED 255

const int PWMFreq = 1000;
const int PWMResolution = 8;
const int rightMotorPWMSpeedChannel = 4;
const int leftMotorPWMSpeedChannel  = 5;

// ---------------- Servo Settings ----------------
Servo myServo;
const int servoPin = 13;

bool servoRunning = false;    
bool returningHome = false;   
bool aligningToStart = false; // align to 45° before sweeping

int currentAngle = 45;
unsigned long lastStep = 0;
const unsigned long delayStep = 150;  

int sweepStage = 0;   

// ---------------- Pump Settings ----------------
const int pumpPin = 23;      // GPIO 23
bool pumpRunning = false;    // pump state (active HIGH assumed)
// for edge-detect toggle:
bool prevStartPressed = false;

// -------------------------------------------------

void rotateMotor(int rightMotorSpeed, int leftMotorSpeed)
{
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

  ledcWrite(rightMotorPWMSpeedChannel, abs(rightMotorSpeed));
  ledcWrite(leftMotorPWMSpeedChannel, abs(leftMotorSpeed));
}

void setUpPinModes()
{
  pinMode(enableRightMotor, OUTPUT);
  pinMode(rightMotorPin1, OUTPUT);
  pinMode(rightMotorPin2, OUTPUT);

  pinMode(enableLeftMotor, OUTPUT);
  pinMode(leftMotorPin1, OUTPUT);
  pinMode(leftMotorPin2, OUTPUT);

  ledcSetup(rightMotorPWMSpeedChannel, PWMFreq, PWMResolution);
  ledcSetup(leftMotorPWMSpeedChannel, PWMFreq, PWMResolution);
  ledcAttachPin(enableRightMotor, rightMotorPWMSpeedChannel);
  ledcAttachPin(enableLeftMotor, leftMotorPWMSpeedChannel);

  rotateMotor(0, 0);
}

void setup()
{
  setUpPinModes();
  Dabble.begin("Krish CAR");

  myServo.attach(servoPin);
  // DO NOT MOVE servo at boot
  lastStep = millis();

  // Pump init (active HIGH)
  pinMode(pumpPin, OUTPUT);
  digitalWrite(pumpPin, LOW);
  pumpRunning = false;
  prevStartPressed = false;
}

// Safe single-step move and update currentAngle
void safeMoveTo(int angle)
{
  myServo.write(angle);
  currentAngle = angle;
}

void updateServo()
{
  unsigned long now = millis();
  if (now - lastStep < delayStep) return;
  lastStep = now;

  // FIRST: align to 45° smoothly (fixes the big jump)
  if (aligningToStart)
  {
    if (currentAngle > 45) safeMoveTo(currentAngle - 5);
    else if (currentAngle < 45) safeMoveTo(currentAngle + 5);

    if (currentAngle == 45)
    {
      aligningToStart = false;
      servoRunning = true;  // Now start sweeping
      sweepStage = 0;
    }
    return;
  }

  // Returning to home (Cross pressed)
  if (returningHome)
  {
    if (currentAngle > 45) safeMoveTo(currentAngle - 5);
    else if (currentAngle < 45) safeMoveTo(currentAngle + 5);

    if (currentAngle == 45)
    {
      returningHome = false;
      servoRunning = false;
    }
    return;
  }

  // Sweeping
  if (!servoRunning) return;

  if (sweepStage == 0)
  {
    safeMoveTo(currentAngle + 5);
    if (currentAngle >= 135) sweepStage = 1;
  }
  else if (sweepStage == 1)
  {
    safeMoveTo(currentAngle - 5);
    if (currentAngle <= 45) sweepStage = 2;
  }
  else if (sweepStage == 2)
  {
    safeMoveTo(currentAngle + 5);
    if (currentAngle >= 90) sweepStage = 0;
  }
}

void loop()
{
  int rightMotorSpeed = 0;
  int leftMotorSpeed  = 0;

  Dabble.processInput();

  // ---------------- Motor controls (unchanged) ----------------
  if (GamePad.isUpPressed())
  {
    rightMotorSpeed = MAX_MOTOR_SPEED;
    leftMotorSpeed  = MAX_MOTOR_SPEED;
  }

  if (GamePad.isDownPressed())
  {
    rightMotorSpeed = -MAX_MOTOR_SPEED;
    leftMotorSpeed  = -MAX_MOTOR_SPEED;
  }

  if (GamePad.isLeftPressed())
  {
    rightMotorSpeed = -MAX_MOTOR_SPEED;
    leftMotorSpeed  = MAX_MOTOR_SPEED;
  }

  if (GamePad.isRightPressed())
  {
    rightMotorSpeed = MAX_MOTOR_SPEED;
    leftMotorSpeed  = -MAX_MOTOR_SPEED;
  }
  // --------------------------------------------------------------

  // ---------------- SERVO BUTTON CONTROL ----------------
  // TRIANGLE → align to 45° then start continuous sweep
  if (GamePad.isTrianglePressed())
  {
    aligningToStart = true;
    returningHome = false;
    servoRunning = false;
    delay(150);
  }

  // CROSS → stop sweeping and return to 45°
  if (GamePad.isCrossPressed())
  {
    returningHome = true;
    aligningToStart = false;
    servoRunning = false;
    delay(150);
  }

  // ---------------- PUMP TOGGLE (Start button) ----------------
  bool startPressedNow = GamePad.isStartPressed();
  if (startPressedNow && !prevStartPressed)
  {
    // rising edge -> toggle pump
    pumpRunning = !pumpRunning;
    digitalWrite(pumpPin, pumpRunning ? HIGH : LOW);
    delay(150); // debounce
  }
  prevStartPressed = startPressedNow;
  // -----------------------------------------------------

  updateServo();
  rotateMotor(rightMotorSpeed, leftMotorSpeed);
}
