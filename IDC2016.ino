#include <SoftwareSerial.h>
#include <Servo.h>
#define Rx 10 // DOUT to pin 10
#define Tx 11 // DIN to pin 11

SoftwareSerial Xbee (Rx, Tx);
const int buttonPin = 2;
const int yellowLed = 3;
const int greenLed = 9;
const int redLed = 8;
const int piezo = 7;
const int middleQTI = 4;
const int rightQTI = 5;
const int leftQTI = 6;
const int colorPal = 8;
const int rightServoSensor = 12;
const int leftServoSensor = 13;
const int THRESHOLD = 30; // QTI threshold for black line detection
int buttonState = 0;
Servo servoRight;
Servo servoLeft;
int red;
int green;
int blue;
const int sio = 53; 
const int unused =7; // Non-existant pin for SoftwareSerial
const int sioBaud = 4800;
SoftwareSerial serin(sio, unused);
SoftwareSerial serout(unused, sio);

void setup(){
  Serial.begin(9600);
  setupServos();
  setupLEDs();
}

void loop(){
  lineFollow();
}

void setupLEDs(){
  pinMode(yellowLed, OUTPUT);
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
}

void setupCommunication(){
  Xbee.begin(9600);
  pinMode(buttonPin, INPUT);
}
 
void setupServos(){
  servoRight.attach(rightServoSensor);
  servoLeft.attach(leftServoSensor);
}

void communicate(){
  checkButtonPress();
  checkSerial();
}

void checkButtonPress(){
  buttonState = digitalRead(buttonPin);
  if(buttonState == HIGH){
    char myChar = 'x';
    Xbee.print(myChar);
  }
}
 
void checkSerial(){
  if(Serial.available()){
    char outgoing = Serial.read();
    Xbee.print(outgoing);
  }
}

// Line follows any path
// Specific IDC task: follow a circle and when reach tick, go to center of circle and read color of inner court
// A tick is a horizontal black line that all 3 QTI sensors would detect as black
void lineFollow(){
  boolean tickReached = false;
  int numLeftTurns = 0;
  int numRightTurns = 0;
  while(!tickReached){
    boolean left = rcToBool(leftQTI);
    boolean middle = rcToBool(middleQTI);
    boolean right = rcToBool(rightQTI);
    if(left && middle && right){
      tickReached = true;
      pauseTurnGo(numLeftTurns, numRightTurns);
    }
    else if(!left && !right && !middle){
      goForward();
    }
    else if(middle && right){
      turnRight();
      numRightTurns++;
    }
    else if(left && middle){
      turnLeft();
      numLeftTurns++;
    }
    else if(middle){
      goForward();
    }
    else if(left){
      turnLeft();
      numLeftTurns++;
    }
    else if(right){
      turnRight();
      numRightTurns++;
    }
  }
}

long rcTime(int sensPin){
  long result = 0;
  pinMode(sensPin, OUTPUT);
  digitalWrite(sensPin, HIGH);
  delay(100);
  pinMode(sensPin, INPUT);
  digitalWrite(sensPin, LOW);
  while(digitalRead(sensPin)){
    result++;
  }
  return result;
}

boolean rcToBool(int sensPin){
  long rc = rcTime(sensPin);
  return rc > THRESHOLD;
}

void stop(){
  pauseLeft();
  pauseRight();
}

void pauseTurnGo(int numLeftTurns, int numRightTurns){
  stop();
  delay(1000);
  if(numLeftTurns < numRightTurns){
    turnRight90();
  }
  else{
    turnLeft90();
  }
  goToCenter();
}

void goToCenter(){
  goForward();
  delay(4500);
  pauseLeft();
  pauseRight();
  checkBoardColor();
}
 
void goForward(){
  turnRightCWSlow();
  turnLeftCCWSlow();
}

void goBackward(){
  turnRightCCW();
  turnLeftCW();
}
 
void turnRight(){
  turnLeftCCW();
  pauseRight();
}

void turnRight90(){
  turnRight();
  delay(getTurnDelay());
  pauseLeft();
}

void turnLeft90(){
  turnLeft();
  delay(getTurnDelay());
  pauseRight();
}

int getTurnDelay(){
  return 1750;
}
 
void turnLeft(){
  turnRightCW();
  pauseLeft();
}

void turnFastLeft(){
  turnRightCW();
  turnLeftCCW();
}
 
void turnFastRight(){
  turnLeftCCW();
  turnRightCW();
}
 
void turnRightCW(){
  servoRight.writeMicroseconds(getCWTime());
}
 
void turnRightCCW(){
  servoRight.writeMicroseconds(getCCWTime());
}
 
void turnLeftCW(){
  servoLeft.writeMicroseconds(getCWTime());
}
 
void turnLeftCCW(){
  servoLeft.writeMicroseconds(getCCWTime());
}
 
void pauseLeft(){
  servoLeft.writeMicroseconds(getStopTime());
}
 
void pauseRight(){
  servoRight.writeMicroseconds(getStopTime());  
}

void turnRightCWSlow(){
  servoRight.writeMicroseconds(getFastCWTime());
}

void turnLeftCCWSlow(){
  servoLeft.writeMicroseconds(getFastCCWTime());
}

int getCWTime(){
  return 1450;
}

int getCCWTime(){
  return 1550;
}

int getStopTime(){
  return 1500;
}

int getFastCWTime(){
  return 1480;
}

int getFastCCWTime(){
  return 1520;
}

void detachServos(){
  servoLeft.detach();
  servoRight.detach();
}

void setupColorPAL(){
  detachServos(); // Distributes power to ColorPAL
  resetColorPal();
  serout.begin(sioBaud);
  pinMode(sio, OUTPUT);
  serout.print("= (00 $ m) !"); // Loops print values, specific from ColorPAL documentation
  serout.end(); // Discontinue serial port for transmitting
  serin.begin(sioBaud); // Set up serial port for receiving
  pinMode(sio, INPUT);
}

// IDC specific task: determine color of three possible multi-colored boards
// Board 1: Majority black with some white lines
// Board 2: Majority white with some black lines
// Board 3: Majority grey with some black lines
// Takes 10 trials to determine majority color of board
void checkBoardColor(){
  int numWhites = 0;
  int numGreys = 0;
  int numBlacks = 0;
  int numTrials = 10;
  for(int x=0; x<numTrials; x++){
    setupColorPAL();
    saveRGBs();
    if((green + blue + red) < 120){ // Black detected
      numBlacks++;
    }
    else if(red < 50 && blue > 500 && green > 500){
      numGreys++;
    }
    else if((blue<200 && green<100 && red>100) || (red>100 && green>1000 && blue>1000)){ // White detected
      numWhites++;
    }
    else if(red < 100 && blue > 500 && green > 500){
      numGreys++;
    }
    else if((red>40 && red<125) && (green>40 && green<125) && (blue<125 && blue>40)){
      numGreys++;
    }
    else{
      numBlacks++;
    }
    setupServos(); // Distributes some power to servos so can move again
    inchForward(); // Slight movement to read color on different position on board
  }
  detachServos();
  boolean whiteRead = numWhites > 0;
  boolean blackRead = numBlacks > 0;
  boolean greyRead = numGreys > 0;
  boolean read = false;
  setupCommunication();
  if(whiteRead && blackRead){
    if(max(numWhites, numBlacks) == numWhites){
      while(!read){
        displayWhite();
        read = checkXbee();
      }
    }
    else{
      while(!read){
        displayBlack();
        read = checkXbee();
      }
    }
  }
  else if(greyRead){
    while(!read){
      displayGrey();
      read = checkXbee();
    }
  }
  else if(whiteRead){
    while(!read){
      displayWhite();
      read = checkXbee();
    }
  }
  else if(blackRead){
    while(!read){
      displayBlack();
      read = checkXbee();
    }
  }
}

boolean checkXbee(){
  if(Xbee.available()){
    Serial.println("AVAILABLE");
    char incoming = Xbee.read();
    Serial.println(incoming);
    if (incoming=='x'){
      Serial.println("ANTHEM");
      playAnthem();
      return true;
    }
    else if (incoming=='y'){
      lightShow();
      return true;
    }
    else if (incoming=='z'){
      dance();
      return true;
    }
  }
  return false;
}

// Slight movement to re-position bot to new position on board for new color reading
void inchForward(){
  goForward();
  delay(100);
}

void displayWhite(){
  yellowledOn();
  Xbee.print('c');
  delay(2000);
}

void displayBlack(){
  greenledOn();
  Xbee.print('a');
  delay(2000);
}

void displayGrey(){
  redledOn();
  Xbee.print('b');
  delay(2000);
}

void resetColorPal(){
  delay(200);
  pinMode(sio, OUTPUT);
  digitalWrite(sio, LOW);
  pinMode(sio, INPUT);
  while (digitalRead(sio) != HIGH);
  pinMode(sio, OUTPUT);
  digitalWrite(sio, LOW);
  delay(80);
  pinMode(sio, INPUT);
  delay(200);
}

void saveRGBs(){
  char buffer[32];
  delay(200);
  if (serin.available() > 0){
    // Wait for a $ character, then read three 3 digit hex numbers
    buffer[0] = serin.read();
    if (buffer[0] == '$'){
      for(int i = 0; i < 9; i++){
        while (serin.available() == 0); // Wait for next input character
        buffer[i] = serin.read();
        if (buffer[i] == '$') // Return early if $ character encountered
          return;
      }
      parseAndPrint(buffer);
      delay(10);
    }
  }
}

// Parse the hex data into integers
void parseAndPrint(char * data){
  sscanf (data, "%3x%3x%3x", &red, &green, &blue);
  char buffer[32];
  sprintf(buffer, "R%4.4d G%4.4d B%4.4d", red, green, blue);
  Serial.println(buffer);
}

// LED methods used for displaying results of ColorPAL reading

void yellowledOn(){
  digitalWrite(yellowLed, HIGH);
}
 
void yellowledOff(){
  digitalWrite(yellowLed, LOW);
}

void redledOn(){
  digitalWrite(redLed, HIGH);
}
 
void redledOff(){
  digitalWrite(redLed, LOW);
}

void greenledOn(){
  digitalWrite(greenLed, HIGH);
}
 
void greenledOff(){
  digitalWrite(greenLed, LOW);
}

void yellowledSecond(){
  yellowledOn();
  delay(1000);
  yellowledOff();
}

void yellowledFlicker(){
  for(int x=0; x<5; x++){
    delay(getFlickerDelay());
    yellowledOn();
    delay(getFlickerDelay());
    yellowledOff();
  }
}

void redledFlicker(){
  for(int x=0; x<5; x++){
    delay(getFlickerDelay());
    redledOn();
    delay(getFlickerDelay());
    redledOff();
  }
}

void greenledFlicker(){
  for(int x=0; x<5; x++){
    delay(getFlickerDelay());
    greenledOn();
    delay(getFlickerDelay());
    greenledOff();
  }
}

int getFlickerDelay(){
  return 200;
}

// Celebrations: central robot calculates team score and each bot has to celebrate accordingly

// Silver medal
void lightShow(){
  while(true){
    delay(getFlickerDelay());
    yellowledOn();
    greenledOn();
    redledOn();
    delay(getFlickerDelay());
    yellowledOff();
    redledOff();
    greenledOff();
  }
}

// Bronze medal
void dance(){
  setupServos();
  while(true){
    turnLeft();
  }
}

// Gold medal
//God save the queen
//88 bpm 
//1 beat * (minute/88beats) * (60 s/min) * (1000ms/s) = 681.82 ms
void playAnthem(){
  //durations 
  long quarterNote = 681.8182;
  long halfNote = 2*quarterNote;
  long dottedQuarterNote = 1.5*quarterNote;
  long dottedHalfNote = 3*quarterNote;
  long wholeNote = 4*quarterNote;
  long eighthNote = .5*quarterNote;
  long triplet = .3333*quarterNote;
  //frequencies
  long fs7 = 2959.955;
  long g7 = 3135.963;
  long a7 = 3520.000;
  long b7 = 3941.066;
  long c8 = 4186.009;
  long d8 = 4698.636;
  long e8 = 5274.041;
  long f8 = 5587.652;
  long g8 = 6271.927;
  //tone commands: tone(piezo, frequency, duration) = tone(piezo, noteName, noteType)
  while(true){
    playSound(piezo, g7, quarterNote);
    playSound(piezo, g7, quarterNote);
    playSound(piezo, a7, quarterNote);
    playSound(piezo, fs7, dottedQuarterNote);
    playSound(piezo, g7, eighthNote);
    playSound(piezo, a7, quarterNote);
    playSound(piezo, b7, quarterNote);
    playSound(piezo, b7, quarterNote);
    playSound(piezo, c8, quarterNote);
    playSound(piezo, b7, dottedQuarterNote);
    playSound(piezo, a7, eighthNote);
    playSound(piezo, g7, quarterNote);
    playSound(piezo, a7, quarterNote);
    playSound(piezo, g7, quarterNote);
    playSound(piezo, fs7, quarterNote);
    playSound(piezo, g7, quarterNote);
    playSound(piezo, g7, eighthNote);
    playSound(piezo, a7, eighthNote);
    playSound(piezo, b7, eighthNote);
    playSound(piezo, c8, eighthNote);
    playSound(piezo, d8, quarterNote);
    playSound(piezo, d8, quarterNote);
    playSound(piezo, d8, quarterNote);
    playSound(piezo, d8, dottedQuarterNote);
    playSound(piezo, c8, eighthNote);
    playSound(piezo, b7, quarterNote);
    playSound(piezo, c8, quarterNote);
    playSound(piezo, c8, quarterNote);
    playSound(piezo, c8, quarterNote);
    playSound(piezo, c8, dottedQuarterNote);
    playSound(piezo, b7, eighthNote);
    playSound(piezo, a7, quarterNote);
    playSound(piezo, b7, quarterNote);
    playSound(piezo, c8, eighthNote);
    playSound(piezo, b7, eighthNote);
    playSound(piezo, a7, eighthNote);
    playSound(piezo, g7, eighthNote);
    playSound(piezo, b7, dottedQuarterNote);
    playSound(piezo, c8, eighthNote);
    playSound(piezo, d8, quarterNote);
    playSound(piezo, e8, triplet);
    playSound(piezo, d8, triplet);
    playSound(piezo, c8, triplet);
    playSound(piezo, b7, quarterNote);
    playSound(piezo, a7, quarterNote);
    playSound(piezo, g7, dottedHalfNote);
  }
}

void playSound(long frequency, long note){
  tone(piezo, d8, note);
  delay(note);
}
