#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <RotaryEncoder.h>
#include <SoftwareSerial.h>
#include <Wire.h>

#define BUZZER_PIN 5
// input 1 on the encoder
#define CLK 2
// input 2 on the encoder
#define DT 3
// button on the encoder
#define SW 4

#define PLAYER_MODE_MENU 0
#define GAMEMODE_MENU 1
#define PLAYER_COUNT_MENU 3
#define TIMER_MENU 4
#define START_GAME 5
#define GAME_OVER 6
#define START_GAME_COMMAND 's'
#define TIMER_MODE_COMMAND 't'
#define GAME_ONGOING 'o'
#define STOP_GAME_COMMAND 'q'
#define TRANSMIT_END 'e'
#define RESET_SCORES_COMMAND '-'
#define RECEIVE_READY 'r'
#define I2C_POLL_DELAY 100
#define TIMER_POLL_DELAY 1000
#define CONTROLLER_COUNT 2

// LCD pins: SDA - A4, SCL - A5
LiquidCrystal_I2C lcd(0x27, 16, 2);
RotaryEncoder *encoder = nullptr;

void checkPosition() {
  encoder->tick(); // just call tick() to check the state.
}

bool competitiveMode = false;
bool timerMode = false;
int lastPos = 0;
int menuSelection = 0;
int menuNumber = 0;
int players = 0;
int timeMinutes = 0;
unsigned long endTimeMilis = 0;
char tetrominoList[100];
int playerScores[8];
unsigned long lastTimePoll = 0;
unsigned long lastTimeTimer = 0;
unsigned long currentTime = 0;

void startGame() {
  menuNumber = START_GAME;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Starting game...");
  // fill in tetromino list with random numbers
  for (int i = 0; i < 100; i++) {
    tetrominoList[i] = random(0, 7);
  }
  for (int controllerAddress = 8; controllerAddress <= 7 + CONTROLLER_COUNT;
       controllerAddress++) {

    Wire.beginTransmission(controllerAddress);
    Wire.write(START_GAME_COMMAND);

    int errorCode = Wire.endTransmission(false);
    if (errorCode != 0) {
      Serial.println("Error sending start game command");
      Serial.println(errorCode);
    }
    Serial.println("Sent start game command");

    // ask controller if ready to recevie tetromino list

    bool ready = false;
    while (!ready) {
      Wire.requestFrom(controllerAddress, 1);
      Serial.println("Waiting for controller to be ready");
      while (Wire.available() == 0) {
        delay(100);
      }
      char c = Wire.read();
      if (c == 'r') {
        // controller is ready to receive tetromino list
        Serial.println("Controller ready");
        ready = true;
        break;
      }
    }

    // send list of generated tetrominos to controller in batches of 32
    for (int i = 0; i < 100; i++) {
      if (i % 32 == 0) {
        Wire.beginTransmission(controllerAddress);
      }
      Wire.write(tetrominoList[i]);
      if (i % 32 == 31) {
        Wire.endTransmission();
      }
    }
    Wire.endTransmission();
    // send end of list signal
    Wire.beginTransmission(controllerAddress);
    Wire.write(TRANSMIT_END);
    Wire.endTransmission();

    if (timerMode) {
      endTimeMilis = millis() + timeMinutes * 60000;
    }

    delay(100);

    // game started
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Game started!");
    lcd.setCursor(0, 1);
    lcd.print("Good luck!");
  }
}

void moveMenu(int menu) {
  if (menu == PLAYER_MODE_MENU) {
    menuNumber = PLAYER_MODE_MENU;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Player Mode: ");
    lcd.setCursor(0, 1);
    lcd.print("> Classic");
    menuSelection = 0;
  } else if (menu == GAMEMODE_MENU) {
    menuNumber = GAMEMODE_MENU;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Gamemode: ");
    lcd.setCursor(0, 1);
    lcd.print("> Classic");
    menuSelection = 0;
  } else if (menu == PLAYER_COUNT_MENU) {
    menuNumber = PLAYER_COUNT_MENU;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Player amount: ");
    lcd.setCursor(0, 1);
    lcd.print("> 2");
    menuSelection = 0;
  } else if (menu == TIMER_MENU) {
    menuNumber = TIMER_MENU;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Time per round: ");
    lcd.setCursor(0, 1);
    lcd.print("> 1 minute");
    menuSelection = 1;
  } else if (menu == START_GAME) {
    startGame();
  } else if (menu == GAME_OVER) {
    menuNumber = GAME_OVER;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Collecting points");
    lcd.setCursor(0, 1);
    lcd.print("please wait");
  }
}

void setup() {
  Wire.begin();
  // initialize lcd i2c library
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Hello World!");

  encoder = new RotaryEncoder(CLK, DT, RotaryEncoder::LatchMode::TWO03);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SW, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(CLK), checkPosition, CHANGE);
  attachInterrupt(digitalPinToInterrupt(DT), checkPosition, CHANGE);
  Serial.begin(9600);

  // make welcoming screeen to tetris game
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("=== Tetris ===");
  lcd.setCursor(0, 1);
  lcd.print("Press Button");
  while (digitalRead(SW) == HIGH) {
    delay(100);
  }
  moveMenu(PLAYER_MODE_MENU);

  delay(100);
}

bool previousButtonPressed = true;

void loop() {
  encoder->tick();
  bool buttonPressed = false;
  
  bool changed = false;

  int newPos = encoder->getPosition();
  if (newPos > lastPos && newPos % 2 == 0) {
    menuSelection++;
    lastPos = newPos;
    changed = true;
  } else if (newPos < lastPos && newPos % 2 == 0) {
    menuSelection--;
    lastPos = newPos;
    changed = true;
  }

  if (digitalRead(SW) == LOW && !previousButtonPressed) {
    buttonPressed = true;
    previousButtonPressed = true;
  }
  if (digitalRead(SW) == HIGH) {
    previousButtonPressed = false;
  }

  // make a menu selection with roatry encoder for a game mode selection menu in
  // tetris with either
  // 1. casual mode
  // 2. competitive mode
  // afterwards ask for another gamemode to play the game in
  // 1. classic mode
  // 2. timer mode
  // if in competitive mode, ask for amount of players from 2 to 8 players, then
  // make a single-elimination bracket with the amount of players if in timer
  // mode, ask for amount of time from 1 to 8 minutes lastly send a signal to
  // the controller to start the gme

  if (menuNumber == PLAYER_MODE_MENU) {
    if (menuSelection < 0) {
      menuSelection = 0;
    }
    menuSelection = menuSelection % 2;
    String menuSelectionString =
        menuSelection == 0 ? "Casual     " : "Competitive    ";
    if (changed) {
      lcd.setCursor(2, 1);
      lcd.print(menuSelectionString);
    }
    if (buttonPressed) {
      moveMenu(GAMEMODE_MENU);

      if (menuSelection == 1) {
        competitiveMode = true;
      }
    }
  } else if (menuNumber == GAMEMODE_MENU) {
    if (menuSelection < 0) {
      menuSelection = 0;
    }
    menuSelection = menuSelection % 2;
    String menuSelectionString =
        menuSelection == 0 ? "Classic   " : "Timer    ";
    if (changed) {
      lcd.setCursor(2, 1);
      lcd.print(menuSelectionString);
    }
    if (buttonPressed) {
      if (menuSelection == 1) {
        timerMode = true;
      }
      menuSelection = 0;
      if (competitiveMode) {
        moveMenu(PLAYER_COUNT_MENU);
      } else if (timerMode) {
        moveMenu(TIMER_MENU);
      } else {
        moveMenu(START_GAME);
      }
    }
  } else if (menuNumber == PLAYER_COUNT_MENU) {
    if (menuSelection < 0) {
      menuSelection = 0;
    } else {
      menuSelection = menuSelection % 7;
    }
    if (changed) {
      lcd.setCursor(2, 1);
      lcd.print("    ");
      lcd.setCursor(2, 1);
      lcd.print(menuSelection + 2);
    }
    if (buttonPressed) {
      players = menuSelection + 2;
      if (timerMode && timeMinutes == 0) {
        moveMenu(TIMER_MENU);
      } else {
        moveMenu(START_GAME);
      }
    }

  } else if (menuNumber == TIMER_MENU) {
    if (menuSelection < 1) {
      menuSelection = 1;
    } else {
      menuSelection = menuSelection % 8;
    }
    if (changed) {
      lcd.setCursor(2, 1);
      lcd.print("            ");
      lcd.setCursor(2, 1);
      String timeString = String(menuSelection) + " minutes";
      lcd.print(timeString);
    }
    if (buttonPressed) {
      timeMinutes = menuSelection;
      if (competitiveMode && players == 0) {
        moveMenu(PLAYER_COUNT_MENU);
      } else {
        moveMenu(START_GAME);
      }
    }
  } else if (menuNumber == START_GAME) {
    // poll controllers if any has lost asynchroneously
    currentTime = millis();
    if (currentTime - lastTimePoll > I2C_POLL_DELAY) {
      lastTimePoll = currentTime;
      for (int controllerAddress = 8; controllerAddress <= 7 + CONTROLLER_COUNT;
           controllerAddress++) {
        Wire.requestFrom(controllerAddress, 1);
        while (Wire.available() == 0) {
          delay(100);
        }
        char c = Wire.read();
        Serial.print("Received from controller: ");
        Serial.println(c);
        // check if c is a number ascii code
        if (c != GAME_ONGOING) {
          moveMenu(GAME_OVER);
        }
      }
    }
    if (timerMode) {
      // check if timer has run out
      currentTime = millis();
      if (currentTime - lastTimeTimer > TIMER_POLL_DELAY) {
        lastTimeTimer = currentTime;

        // display timer to lcd
        int timeLeftMinutes = (endTimeMilis - currentTime) / 60000;
        int timeLeftSeconds = ((endTimeMilis - currentTime) % 60000) / 1000;
        lcd.setCursor(0, 1);
        lcd.print("Time left: ");
        lcd.print(timeLeftMinutes);
        lcd.print(":");
        if (timeLeftSeconds < 10) {
          lcd.print("0");
        }
        lcd.print(timeLeftSeconds);

        if (currentTime > endTimeMilis) {
          moveMenu(GAME_OVER);
          Serial.println("Timer ran out");
        }
      }
    }

  } else if (menuNumber == GAME_OVER) {
    // make regular dots animation with delay
    lcd.setCursor(12, 1);
    lcd.print("   ");
    lcd.setCursor(12, 1);
    for (int i = 0; i < 4; i++) {
      lcd.print(".");
      delay(250);
    }

    // empty playerscores array
    for (int i = 0; i < 8; i++) {
      playerScores[i] = 0;
    }

    Serial.println("sending stop game command");
    // send game over signal to controllers
    for (int controllerAddress = 8; controllerAddress <= 7 + CONTROLLER_COUNT;
         controllerAddress++) {
      Wire.beginTransmission(controllerAddress);
      Wire.write(STOP_GAME_COMMAND);
      Wire.endTransmission();
    }

    Serial.println("collecting scores");
    // collect scores from controllers
    for (int controllerAddress = 8; controllerAddress <= 7 + CONTROLLER_COUNT;
         controllerAddress++) {
      Wire.requestFrom(controllerAddress, 1);
      while (Wire.available() == 0) {
        delay(100);
      }
      char c = Wire.read();
      if (c != GAME_ONGOING) {
        // get score from controller

        // clear buffer
        while (Wire.available() > 0) {
          Wire.read();
        }

        // request score
        Wire.requestFrom(controllerAddress, 2);
        while (Wire.available() < 2) {
          delay(100);
        }
        byte HiByte = Wire.read();
        byte LoByte = Wire.read();
        int score = (HiByte << 8) | LoByte;
        Serial.print("Received score from controller: ");
        Serial.println(score);
        playerScores[controllerAddress - 8] = score;
      }

      // reset scores on controller
      Wire.beginTransmission(controllerAddress);
      Wire.write(RESET_SCORES_COMMAND);
      Wire.endTransmission();
    }

    // check to see who has the highest score
    int highestScore = 0;
    for (int i = 0; i < 8; i++) {
      Serial.print(playerScores[i]);
      if (playerScores[i] > highestScore) {
        highestScore = playerScores[i];
      }
    }

    // display winner
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Winner: ");
    for (int i = 0; i < CONTROLLER_COUNT; i++) {
      if (playerScores[i] == highestScore) {
        lcd.setCursor(0, 1);
        lcd.print("Player ");
        lcd.print(i + 1);
        lcd.print(" ");
      }
    }
    while (digitalRead(SW) == HIGH) {
      delay(100);
    }
    // reset scores array
    for (int i = 0; i < 8; i++) {
      playerScores[i] = 0;
    }

    // reset game settings
    competitiveMode = false;
    timerMode = false;
    players = 0;
    timeMinutes = 0;
    endTimeMilis = 0;

    moveMenu(PLAYER_MODE_MENU);
  }

  if (buttonPressed) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
  delay(100);
}