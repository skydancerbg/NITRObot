#include <Arduino.h>

// The rangefinders work well to show the distance to objects from around
// 1 inch (2 cm) to around 9 feet away (3 meters), but they have trouble when
// they aren't approximatley at a right angle to the object they are detecting.
// If the angle is too great (over about 15 degrees) not enough of the sound
// bounces back for it to get a reliable range.

// Тук е секцията за Библиотеки и хедър файлове които се инклудват:

#include <Servo.h>

// PROBEN TEKST

// Тук е секцията с описание на използваните пинове:

// дефинициите са изцяло с главни букви
#define LEFT_FOR 9    // PWMB
#define LEFT_BACK 5   // DIRB  ---  Left
#define RIGHT_FOR 6   // PWMA
#define RIGHT_BACK 10 // DIRA  ---  Right

// константите започват с главна буква, а всяка следваща дума с главна буква
const int LeftIrAvoidancePin = 12;
const int RightIrAvoidancePin = A5;
const int UltrasonicPin = 3;
const int RgbPin = 2;
// и тези пинове трябва да са константи - не се променят
const int ServoPin = 13;
const int LedPin = 33;

// Robot parameters:
const int RobotLenght = 25;     // реални размери 25.0 см.
const int robotWidth = 16;      // реални размери 16.7 см.
const int brakingDistance = 10; // спирачен път в см.
// Maze parameters:
const int MazeCorridorWidth = 50; //   ШИРОЧИНАТА НА КОРИДОРЕ  Е 3 ШИРИНИ НА РОБОТА = 50см. (robotWidth * 3) + 2;

// Tresholds:

const float FrontDistanceTreshold = MazeCorridorWidth / 2 + brakingDistance;
const float WallToCorridorMiddle = MazeCorridorWidth / 2;
const float SideCorridorTreshold = MazeCorridorWidth;

//? С теаи параметри правим нещо като ръчен PID алгоритъм за следване на централната линия спрямо измереното разстояние до страничната стена
const float CenterLineTolerance = 2.5; // plus/minus how many cm are acceptable to consider the movement to be on the center line...
                                       // +- 1 cm from centerline is considered straight movement!!!
const float SharpTurnTreshold = 15.0;  // TODO Да се определи спрямо размера робота и коридора !!!!!

//? От тук задаваме дали ще следваме дясна или лява стена за да е универсале алгоритъма
const int WallFollowingSide = -90; //Set: -90 for right wall following or +90 for left wall following
                                   //we will add this value to the servo position i.e. myservo.write(90 + WallFollowingSide);
                                   // in order to set to which side the servo should move (0 or 180 degrees)
//Servo parameters
const int FrontServoAngle = 90;
const int SideServoAngle = FrontServoAngle + WallFollowingSide; //(0 or 180 degrees)
const int FrontServoDelay = 150;
const int SideServoDelay = 150;

const int LeftSpeed = 90;
const int RightSpeed = 90;

// Тук започват ГЛОБАЛНИТЕ променливи, които се използват в кода:
// променливите започват с малка буква, а всяка следваща дума с главна буква
// ....
float maxDistance = 130;
int speedLeft = LeftSpeed;
int speedRight = RightSpeed;
bool directionCompensation = false;
// Тук инициализираме обектите:
Servo myservo;

// Тук слагаме прототипите на функциите:
void moveForward();
void turnRight();
void moveBackward();
void makeSlightLeftTurn();
void turnSlightRight();
void stopMoving();
float getDistance(int servoAngle, int delayAfterServoMovement); //read the Ultasonic Sensor pointing at the given servo angle

//-----------------------------------------------

void setup()
{
  pinMode(ServoPin, OUTPUT);
  pinMode(LEFT_FOR, OUTPUT);
  pinMode(LEFT_BACK, OUTPUT);
  pinMode(RIGHT_FOR, OUTPUT);
  pinMode(RIGHT_BACK, OUTPUT);
  pinMode(UltrasonicPin, OUTPUT);
  pinMode(LedPin, OUTPUT);
  Serial.begin(9600);
  myservo.attach(ServoPin);
  myservo.write(90); //Move the servo to center position

  moveForward();
  delay(500);
}

//---------------------------------------------------------

void loop()
{

  float frontDistance, sideDistance;
  // states:
  int currentState; //? Правим проверките универсални - за следване на стена отдясно или отляво в зависимост от избрания WallFollowingSide - параметър по-горе.
  // Състоянието (state) след прочитане на разстоянията може да бъде:
  // 1 Приближили сме стена отпред (<= FrontDistanceTreshold), отдясно/отляво има стена (< SideCorridorTreshold) - завой на 180 градуса
  // 2 Пррближили сме стена отпред (<= FrontDistanceTreshold), има коридор надясно/наляво (>= SideCorridorTreshold) - завой на 90 градуса надясно/наляво
  // 3 Напред стената е далече (> FrontDistanceTreshold), има коридор в дясно/ляво (>= SideCorridorTreshold) - завой на 90 градуса надясно/наляво
  // 4 Има стена отдясно/отляво (< SideCorridorTreshold), напред е свободно (> FrontDistanceTreshold), в рамките на +- CenterLineTolerance от централната линия сме - движение право напред
  // 5 Има стена отдясно/отляво (< SideCorridorTreshold), напред е свободно (> FrontDistanceTreshold), на повече от + SharpTurnTreshold от централната линия (по-близо до лявата/дясната стена) сме - движение напред с остър завой наляво/надясно
  // 6 Има стена отдясно/отляво (< SideCorridorTreshold), напред е свободно (> FrontDistanceTreshold), на повече от - SharpTurnTreshold от централната линия сме (по-близо до дясната/лявата стена) сме - движение напред с остър завой надясно/наляво
  // 7 Има стена отдясно/отляво (< SideCorridorTreshold), напред е свободно (> FrontDistanceTreshold), на повече от + tolerance от централната линия (по-близо до лявата/дясната стена) сме - движение напред със завой леко наляво/надясно
  // 8 Има стена отдясно/отляво (< SideCorridorTreshold), напред е свободно (> FrontDistanceTreshold), на повече от - tolerance от централната линия сме (по-близо до дясната/лявата стена) сме - движение напред със завой леко надясно/наляво

  sideDistance = getDistance(SideServoAngle, SideServoDelay);
  frontDistance = getDistance(FrontServoAngle, FrontServoDelay);
  Serial.println(sideDistance);
  if (frontDistance <= WallToCorridorMiddle - brakingDistance) //Стената отпред е близко SideCorridorTreshold

  {
    digitalWrite(LedPin, LOW);
    currentState = 1; // turn

    //------------------------------------------
    //if (sideDistance < SideCorridorTreshold)
    //{
    // 1 Приближили сме стена отпред (<= FrontDistanceTreshold), отдясно/отляво има стена (< SideCorridorTreshold)
    // - завой на 90 градуса
    //  currentState = 1; // turn 90 degrees
    // }
    // else if (sideDistance >= SideCorridorTreshold )
    // {
    // 2 Пррближили сме стена отпред (<= FrontDistanceTreshold), има коридор надясно/наляво (>= SideCorridorTreshold)
    // - завой на 90 градуса надясно/наляво
    //   currentState = 2; // turn 90 degrees
    // }
    //----------------------------------------------
  }
  else if (frontDistance >= 50) //Стената отпред е далече
  {
    digitalWrite(LedPin, LOW);
    if (sideDistance >= 50) //SideCorridorTreshold
    {
      // 3 Напред стената е далече (> FrontDistanceTreshold), има коридор в дясно/ляво (>= SideCorridorTreshold)
      // - завой на 90 градуса надясно/наляво

      currentState = 3; // turn 90 degrees
    }
  }

  else if (sideDistance < 50) // В коридора сме!

  {

    digitalWrite(LedPin, HIGH);
    if (24 < sideDistance && sideDistance < 26) //(sideDistance < 23 && sideDistance > 27sideDistance < WallToCorridorMiddle + CenterLineTolerance && sideDistance > WallToCorridorMiddle - CenterLineTolerance)
    {
      // 4 Има стена отдясно/отляво (< SideCorridorTreshold), напред е свободно (> FrontDistanceTreshold), в рамките на +- CenterLineTolerance
      // от централната линия сме  - движение право напред

      currentState = 4; //Close to the centerline - go forwad
    }
    else if (35 > sideDistance && sideDistance >= 26) //(sideDistance >= WallToCorridorMiddle + SharpTurnTreshold)
    {
      // 5 Има стена отдясно/отляво (< SideCorridorTreshold), напред е свободно (> FrontDistanceTreshold), на повече от + SharpTurnTreshold от
      // централната линия (по-близо до лявата/дясната стена) сме - движение напред с остър завой наляво/надясно

      currentState = 5; //Close to the other wall - hard turn to centerline
    }
    else if (15 < sideDistance && sideDistance <= 24) //(sideDistance <= WallToCorridorMiddle - SharpTurnTreshold)
    {
      // 6 Има стена отдясно/отляво (< SideCorridorTreshold), напред е свободно (> FrontDistanceTreshold), на повече от - SharpTurnTreshold
      //  от централната линия сме (по-близо до дясната/лявата стена) сме - движение напред с остър завой наляво/надясно

      currentState = 6; //Close to the wall we are following - hard turn to centerline
    }
    else if (sideDistance <= 15)
    {
      // 8 Има стена отдясно/отляво (< SideCorridorTreshold), напред е свободно (> FrontDistanceTreshold), на повече от - tolerance
      //  въртим се с while докаро изправим робота
      currentState = 7;
    }
    else if (sideDistance >= 35)
    {
      // 8 Има стена отдясно/отляво (< SideCorridorTreshold), напред е свободно (> FrontDistanceTreshold), на повече от - tolerance
      //  въртим се с while докаро изправим робота
      currentState = 8;
    }
  }
  switch (currentState)
  {
  case 1: /* завой на 90 градуса */
    Serial.println(currentState);
    stopMoving();
    makeSlightLeftTurn();
    delay(550);
    directionCompensation = false;
    speedLeft = LeftSpeed;
    speedRight = RightSpeed;
    break;
  // case 2: /* завой на 90 градуса надясно/наляво */
  //       stopMoving();
  //   moveBackward();
  //   delay(200);
  //   turnSlightRight();
  //   delay(500);
  //   directionCompensation = false;
  //   break;
  case 3: /* завой на 90 градуса надясно/наляво */
    Serial.println(currentState);
    stopMoving();
    moveBackward();
    delay(200);
    for (size_t i = 0; i < 4; i++)
    {
      speedLeft = 100;
      speedRight = 100;
      moveForward();
      delay(20);
      speedLeft = 255;
      speedRight = 0;
      moveForward();
      delay(200);
    }
    // speedLeft = LeftSpeed * 1.0;
    // speedRight = RightSpeed * 1.0;
    // moveForward();
    // delay(1500);
    directionCompensation = false;
    speedLeft = LeftSpeed;
    speedRight = RightSpeed;
    break;
  case 4:
    Serial.println(currentState);
    speedLeft = LeftSpeed;
    speedRight = RightSpeed;
    directionCompensation = false;
    /* движение право напред */
    break;
  case 5:
    Serial.println(currentState);
    /* движение напред с остър завой наляво/надясно */
    speedLeft = LeftSpeed * 2.5;
    speedRight = RightSpeed * .8;
    directionCompensation = true;
    break;
  case 6:
    Serial.println(currentState);
    speedLeft = LeftSpeed * .8;
    speedRight = RightSpeed * 2.1;
    directionCompensation = true;
    /* движение напред с остър завой надясно/наляво */
    break;
  case 7:
    Serial.println(currentState);
    stopMoving();
    while (sideDistance <= 15)
    {
      makeSlightLeftTurn();
      sideDistance = getDistance(SideServoAngle, SideServoDelay);
    }
    speedLeft = LeftSpeed;
    speedRight = RightSpeed;
    break;
  case 8:
    Serial.println(currentState);
    stopMoving();
    while (sideDistance >= 35)
    {
      turnSlightRight();
      sideDistance = getDistance(SideServoAngle, SideServoDelay);
    }
    speedLeft = LeftSpeed;
    speedRight = RightSpeed;
    break;
  default:
    break;
  }
  moveForward();
}
//==================================== VOID =====================================================

void moveForward()
{
  analogWrite(LEFT_FOR, abs(speedLeft));
  analogWrite(LEFT_BACK, LOW);
  analogWrite(RIGHT_FOR, abs(speedRight));
  analogWrite(RIGHT_BACK, LOW);
}

void turnRight()
{
  analogWrite(LEFT_FOR, 180);
  analogWrite(LEFT_BACK, LOW);
  analogWrite(RIGHT_FOR, LOW);
  analogWrite(RIGHT_BACK, 0);
}

void moveBackward()
{
  analogWrite(LEFT_FOR, LOW);
  analogWrite(LEFT_BACK, abs(speedLeft));
  analogWrite(RIGHT_FOR, LOW);
  analogWrite(RIGHT_BACK, abs(speedRight));
}

void makeSlightLeftTurn()
{
  analogWrite(LEFT_FOR, LOW);
  analogWrite(LEFT_BACK, 130);
  analogWrite(RIGHT_FOR, 130);
  analogWrite(RIGHT_BACK, LOW);
}

void turnSlightRight()
{
  analogWrite(LEFT_FOR, 110);
  analogWrite(LEFT_BACK, LOW);
  analogWrite(RIGHT_FOR, LOW);
  analogWrite(RIGHT_BACK, 90);
}

void stopMoving()
{
  analogWrite(LEFT_FOR, HIGH);
  analogWrite(LEFT_BACK, HIGH);
  analogWrite(RIGHT_FOR, HIGH);
  analogWrite(RIGHT_BACK, HIGH);
}

float getDistance(int servoAngle, int delayAfterServoMovement)
{
  float distance;
  myservo.write(servoAngle);
  delay(delayAfterServoMovement);
  pinMode(UltrasonicPin, OUTPUT);
  digitalWrite(UltrasonicPin, LOW);
  delayMicroseconds(2);
  digitalWrite(UltrasonicPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(UltrasonicPin, LOW);
  pinMode(UltrasonicPin, INPUT);
  distance = pulseIn(UltrasonicPin, HIGH) / 58.00;
  return distance;
}