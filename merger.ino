// ================= HIGH SPEED FIRE FIGHTING ROBOT =================
// MANUAL + AUTOFIRE
// Improved version:
// -> Robot comes NEAR fire first
// -> Then starts spraying
// -> Better flame approach logic

#define CUSTOM_SETTINGS
#define INCLUDE_GAMEPAD_MODULE

#include <DabbleESP32.h>
#include <ESP32Servo.h>

// ---------------- Pin Definitions ----------------
#define IN1 27
#define IN2 26
#define IN3 25
#define IN4 33

#define ENA 14
#define ENB 32

#define pumpPin 23
#define ServoPin 13

// Flame Sensors
#define FlameLeft 4
#define FlameMiddle 34
#define FlameRight 5

#define inbuilt_led 2

// ---------------- PWM Settings ----------------
const int PWMFreq = 1000;
const int PWMResolution = 8;
const int channelA = 4;
const int channelB = 5;

const int MAX_PWM = 255;

// ---------------- Speed Settings ----------------
const int MANUAL_SPEED = 255;
int motorSpeed = 255;

// ---------------- Timing ----------------
#define turning_time 250
#define move_backward 250

const unsigned long sprayDurationMs = 5000;
const unsigned long sprayServoStepMS = 120;

// ---------------- Flame Detection ----------------
const uint8_t DO_SAMPLE_COUNT = 6;
const uint8_t DO_SAMPLE_DELAY_MS = 6;

const int AO_SAMPLE_COUNT = 6;

const int THRESH_ON = 1800;
const int HYSTERESIS_DELTA = 400;
const int THRESH_OFF = THRESH_ON + HYSTERESIS_DELTA;

bool middleLatched = false;

// ---------------- Servo ----------------
Servo myServo;

int currentAngle = 90;

unsigned long lastServoStep = 0;
const unsigned long delayStep = 150;

bool servoRunning = false;
bool aligningToStart = false;
bool returningHome = false;

int sweepStage = 0;

// ---------------- Robot Modes ----------------
enum RobotMode {
  MODE_MANUAL,
  MODE_AUTOFIRE
};

RobotMode currentMode = MODE_MANUAL;

// ---------------- Button States ----------------
bool prevSelect = false;
bool prevStartPressed = false;
bool crossPrev = false;

bool pumpRunning = false;

// ---------------- AUTOFIRE STATES ----------------
enum AutoState {
  A_IDLE,
  A_ORIENT_MOVE,
  A_APPROACH_FIRE,
  A_SPRAY
};

AutoState autoState = A_IDLE;

// ---------------- Spray Variables ----------------
unsigned long sprayStartTime = 0;
unsigned long lastSprayServoStep = 0;

int spraySweepAngle = 45;
int spraySweepDir = 1;

// ---------------- Function Prototypes ----------------
void setUpPinModes();

void rotateMotor(int rightMotorSpeed, int leftMotorSpeed);

void stopMoving();

void safeStopAll();

void runManualLoop();

void manualServoUpdate();

void startAutoFireMode();

void stopAutoFireMode();

void autoFireStateMachine();

bool isFirePresent();

bool readDOWithSensitivity(uint8_t pin);

int readMiddleAverage();

void moveForward();

void moveBackward();

void slightLeft();

void slightRight();

// ======================================================
// SETUP PIN MODES
// ======================================================
void setUpPinModes() {

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  ledcSetup(channelA, PWMFreq, PWMResolution);
  ledcSetup(channelB, PWMFreq, PWMResolution);

  ledcAttachPin(ENA, channelA);
  ledcAttachPin(ENB, channelB);

  pinMode(pumpPin, OUTPUT);
  pinMode(inbuilt_led, OUTPUT);

  pinMode(FlameLeft, INPUT_PULLUP);
  pinMode(FlameRight, INPUT_PULLUP);

  analogReadResolution(12);

  rotateMotor(0, 0);

  digitalWrite(pumpPin, LOW);
  digitalWrite(inbuilt_led, LOW);
}

// ======================================================
// SETUP
// ======================================================
void setup() {

  Serial.begin(115200);

  setUpPinModes();

  Dabble.begin("Krish CAR");

  myServo.attach(ServoPin);

  myServo.write(90);

  Serial.println("HIGH SPEED FIRE FIGHTING ROBOT READY");
}

// ======================================================
// MAIN LOOP
// ======================================================
void loop() {

  Dabble.processInput();

  // ---------------- EMERGENCY STOP ----------------
  bool crossNow = GamePad.isCrossPressed();

  if (crossNow && !crossPrev) {

    safeStopAll();

    currentMode = MODE_MANUAL;

    autoState = A_IDLE;

    Serial.println("EMERGENCY STOP");
  }

  crossPrev = crossNow;

  // ---------------- MODE TOGGLE ----------------
  bool selectNow = GamePad.isSelectPressed();

  if (selectNow && !prevSelect) {

    if (currentMode == MODE_MANUAL) {

      startAutoFireMode();

      currentMode = MODE_AUTOFIRE;

      Serial.println("AUTOFIRE ON");

    } else {

      stopAutoFireMode();

      currentMode = MODE_MANUAL;

      Serial.println("MANUAL MODE");
    }
  }

  prevSelect = selectNow;

  // ---------------- RUN CURRENT MODE ----------------
  if (currentMode == MODE_AUTOFIRE) {

    autoFireStateMachine();

  } else {

    runManualLoop();
  }

  delay(10);
}

// ======================================================
// MANUAL MODE
// ======================================================
void runManualLoop() {

  bool startNow = GamePad.isStartPressed();

  // Pump Toggle
  if (startNow && !prevStartPressed) {

    pumpRunning = !pumpRunning;

    digitalWrite(pumpPin, pumpRunning ? HIGH : LOW);

    delay(120);
  }

  prevStartPressed = startNow;

  int leftPWM = 0;
  int rightPWM = 0;

  // ---------------- MOVEMENT ----------------
  if (GamePad.isUpPressed()) {

    leftPWM = MANUAL_SPEED;
    rightPWM = MANUAL_SPEED;

  } else if (GamePad.isDownPressed()) {

    leftPWM = -MANUAL_SPEED;
    rightPWM = -MANUAL_SPEED;

  } else if (GamePad.isLeftPressed()) {

    leftPWM = MANUAL_SPEED;
    rightPWM = -MANUAL_SPEED;

  } else if (GamePad.isRightPressed()) {

    leftPWM = -MANUAL_SPEED;
    rightPWM = MANUAL_SPEED;
  }

  rotateMotor(rightPWM, leftPWM);

  // ---------------- SERVO CONTROL ----------------
  if (GamePad.isTrianglePressed()) {

    aligningToStart = true;

    returningHome = false;

    servoRunning = false;

    delay(150);
  }

  if (GamePad.isCirclePressed()) {

    returningHome = true;

    aligningToStart = false;

    servoRunning = false;

    delay(150);
  }

  manualServoUpdate();
}

// ======================================================
// MANUAL SERVO UPDATE
// ======================================================
void manualServoUpdate() {

  unsigned long now = millis();

  if (now - lastServoStep < delayStep) return;

  lastServoStep = now;

  // Move to 45 degree
  if (aligningToStart) {

    if (currentAngle > 45) currentAngle -= 5;
    else if (currentAngle < 45) currentAngle += 5;

    myServo.write(currentAngle);

    if (currentAngle == 45) {

      aligningToStart = false;

      servoRunning = true;

      sweepStage = 0;
    }

    return;
  }

  // Return to center
  if (returningHome) {

    if (currentAngle > 90) currentAngle -= 5;
    else if (currentAngle < 90) currentAngle += 5;

    myServo.write(currentAngle);

    if (currentAngle == 90) {

      returningHome = false;

      servoRunning = false;
    }

    return;
  }

  // Servo Sweep
  if (!servoRunning) return;

  if (sweepStage == 0) {

    currentAngle += 5;

    if (currentAngle >= 135) sweepStage = 1;

  } else if (sweepStage == 1) {

    currentAngle -= 5;

    if (currentAngle <= 45) sweepStage = 2;

  } else {

    currentAngle += 5;

    if (currentAngle >= 90) sweepStage = 0;
  }

  myServo.write(currentAngle);
}

// ======================================================
// AUTOFIRE START
// ======================================================
void startAutoFireMode() {

  rotateMotor(0, 0);

  digitalWrite(pumpPin, LOW);

  autoState = A_IDLE;

  digitalWrite(inbuilt_led, HIGH);
}

// ======================================================
// AUTOFIRE STOP
// ======================================================
void stopAutoFireMode() {

  digitalWrite(pumpPin, LOW);

  digitalWrite(inbuilt_led, LOW);

  rotateMotor(0, 0);

  myServo.write(90);

  autoState = A_IDLE;
}

// ======================================================
// SENSOR FUNCTIONS
// ======================================================
bool readDOWithSensitivity(uint8_t pin) {

  uint8_t lowCount = 0;

  for (uint8_t i = 0; i < DO_SAMPLE_COUNT; i++) {

    if (digitalRead(pin) == LOW) lowCount++;

    delay(DO_SAMPLE_DELAY_MS);
  }

  return (lowCount * 3 >= DO_SAMPLE_COUNT);
}

int readMiddleAverage() {

  long sum = 0;

  for (int i = 0; i < AO_SAMPLE_COUNT; i++) {

    sum += analogRead(FlameMiddle);

    delay(4);
  }

  return sum / AO_SAMPLE_COUNT;
}

bool isFirePresent() {

  bool leftFire = readDOWithSensitivity(FlameLeft);

  bool rightFire = readDOWithSensitivity(FlameRight);

  int middleRaw = readMiddleAverage();

  bool middleNow;

  if (!middleLatched) {

    middleNow = (middleRaw < THRESH_ON);

    if (middleNow) middleLatched = true;

  } else {

    if (middleRaw > THRESH_OFF) {

      middleLatched = false;

      middleNow = false;

    } else {

      middleNow = true;
    }
  }

  return leftFire || rightFire || middleNow;
}

// ======================================================
// AUTOFIRE LOGIC
// ======================================================
void autoFireStateMachine() {

  unsigned long now = millis();

  switch (autoState) {

    // ---------------- WAIT FOR FIRE ----------------
    case A_IDLE:

      if (isFirePresent()) {

        autoState = A_ORIENT_MOVE;
      }

      break;

    // ---------------- TURN TOWARDS FIRE ----------------
    case A_ORIENT_MOVE:

      if (readDOWithSensitivity(FlameLeft)) {

        slightLeft();

        delay(turning_time);

      } else if (readDOWithSensitivity(FlameRight)) {

        slightRight();

        delay(turning_time);
      }

      stopMoving();

      autoState = A_APPROACH_FIRE;

      break;

    // ---------------- MOVE CLOSE TO FIRE ----------------
    case A_APPROACH_FIRE:

      moveForward();

      while (true) {

        int middleRaw = readMiddleAverage();

        // CLOSE ENOUGH TO FIRE
        if (middleRaw < 1200) {

          break;
        }

        // FIRE LOST
        if (!isFirePresent()) {

          stopMoving();

          autoState = A_IDLE;

          return;
        }

        delay(10);
      }

      stopMoving();

      delay(150);

      // START PUMP
      digitalWrite(pumpPin, HIGH);

      sprayStartTime = millis();

      spraySweepAngle = 45;

      spraySweepDir = 1;

      autoState = A_SPRAY;

      break;

    // ---------------- SPRAY WATER ----------------
    case A_SPRAY:

      if (now - lastSprayServoStep >= sprayServoStepMS) {

        lastSprayServoStep = now;

        spraySweepAngle += (5 * spraySweepDir);

        if (spraySweepAngle >= 135) {

          spraySweepAngle = 135;

          spraySweepDir = -1;
        }

        if (spraySweepAngle <= 45) {

          spraySweepAngle = 45;

          spraySweepDir = 1;
        }

        myServo.write(spraySweepAngle);
      }

      // STOP SPRAY AFTER TIME
      if (now - sprayStartTime >= sprayDurationMs) {

        digitalWrite(pumpPin, LOW);

        moveBackward();

        delay(move_backward);

        stopMoving();

        myServo.write(90);

        autoState = A_IDLE;
      }

      break;
  }
}

// ======================================================
// MOTOR CONTROL
// ======================================================
void rotateMotor(int rightMotorSpeed, int leftMotorSpeed) {

  // RIGHT MOTOR
  if (rightMotorSpeed < 0) {

    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);

  } else if (rightMotorSpeed > 0) {

    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);

  } else {

    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
  }

  // LEFT MOTOR
  if (leftMotorSpeed < 0) {

    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);

  } else if (leftMotorSpeed > 0) {

    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);

  } else {

    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
  }

  ledcWrite(channelA, abs(leftMotorSpeed));

  ledcWrite(channelB, abs(rightMotorSpeed));
}

// ======================================================
// MOVEMENT FUNCTIONS
// ======================================================
void stopMoving() {

  rotateMotor(0, 0);
}

void moveForward() {

  rotateMotor(motorSpeed, motorSpeed);
}

void moveBackward() {

  rotateMotor(-motorSpeed, -motorSpeed);
}

// Fast Left Turn
void slightLeft() {

  rotateMotor(-(int)(motorSpeed * 0.95),
              (int)(motorSpeed * 0.95));
}

// Fast Right Turn
void slightRight() {

  rotateMotor((int)(motorSpeed * 0.95),
              -(int)(motorSpeed * 0.95));
}

// ======================================================
// SAFETY STOP
// ======================================================
void safeStopAll() {

  stopMoving();

  digitalWrite(pumpPin, LOW);

  digitalWrite(inbuilt_led, LOW);

  myServo.write(90);

  pumpRunning = false;

  servoRunning = false;

  aligningToStart = false;

  returningHome = false;
}