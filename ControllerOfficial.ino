#include <SPI.h>
#include <Keyboard.h>

#define PIN_SCK          15             // SPI clock
#define PIN_MISO         14             // SPI data input
#define PIN_MOSI         16             // SPI data output
#define PIN_SS1          1             // SPI hardware default SS pin, 4021
#define PIN_595_1        7              // SPI 74HC595
#define PIN_OE           3              //SPI Output Enable (to control brightness through PWM)
#define PIN_S0           0
#define PIN_S1           2
#define PIN_SEC          5

//button bytes
const byte button7 = B10000000; //Miswired :(, have to flop things around, also in the register processor
const byte button6 = B01000000;
const byte button5 = B00100000;
const byte button4 = B00010000;
const byte button3 = B00001000;
const byte button2 = B00000100;
const byte button1 = B00000010;
const byte button0 = B00000001;

const byte switch0 = B00001000; //Miswired :(, have to flop things around, also in the register processor
const byte switch1 = B00010000;
const byte switch2 = B00100000;
const byte switch3 = B01000000;
const byte switch4 = B10000000;

//LEDbytes
//values reversed because bad wiring
const byte led_mute = B00000001;
const byte led_ptt = B10000000;
const byte red_rgb[3] = {B00010010, B00000010, B00010000}; //first is both, 1 and 2 are seperate
const byte green_rgb[3] = {B00100100, B00000100, B00100000};
const byte blue_rgb[3] = {B01001000, B00001000, B01000000};
const byte white_rgb[3] = {B01111110, B00001110, B01110000};

//Logical button toggle register1
const byte totalButtons = 11;
bool buttonToggle[totalButtons];
bool sec_buttonToggle[totalButtons];

//Logical button toggle register2
const byte totalSwitches = 5;
byte buttons_r2;
byte buttons_r2_previous;
byte switches_r2;
byte switches_r2_previous;

//Output variables
byte buttonValue[totalButtons];
byte switchValue[totalSwitches];
byte sec_buttonValue[totalButtons];
byte sec_switchValue[totalSwitches];
byte outputValue = 0;
byte outputValue_previous = 0;

// result byte for 4021
byte firstByte;
byte secondByte;
byte secondByte_previous;

// global vars for button timeout and debounce
long buttonTimeout[totalButtons];
const long debounce = 150;

//process buttons/switches independant of shift registers
long buttonTimeout_Sec;
int previous_mute = HIGH;
int previous_ptt = HIGH;

int secInput;
int muteInput;
int pttInput;

bool secEnabled_switch[totalSwitches];
bool secEnabled_ptt = false;
bool secPrevious = false; //to allow secmode monitoring for led changing
bool secMode = false; //has sec been pressed or consumed
bool vrMode = false;
bool eliteMode = false;

//Deej variables
const int NUM_SLIDERS = 4;
const int analogInputs[NUM_SLIDERS] = {A0, A1, A2, A3}; //leave last analog input for another function to monitor
//const int analogInputs[NUM_SLIDERS] = {A0, A1, A2, A3, A4};
//const int analogInputs[NUM_SLIDERS] = {A0};

const int minValue = 45;
const int maxValue = 970;
int analogSliderValues[NUM_SLIDERS];

//Custom Slider
int customSlider;
int previousValue;

void setup() {
  Serial.begin(9600);
  SPI.begin();

  // set all IC select pins HIGH
  pinMode(PIN_SS1, OUTPUT);
  digitalWrite(PIN_SS1, HIGH);
  pinMode(PIN_595_1, OUTPUT);
  digitalWrite(PIN_595_1, HIGH);

  //enable pullup resistors
  pinMode(PIN_S0, INPUT_PULLUP);
  pinMode(PIN_S1, INPUT_PULLUP);
  pinMode(PIN_SEC, INPUT_PULLUP);

  //Initialize arrays to 0
  for (int i = 0; i < totalButtons; i++) {
    buttonToggle[i] = false;
    buttonTimeout[i] = 0;
  }
  for (int i = 0; i < totalSwitches; i++) {
    secEnabled_switch[i] = false;
  }

  //Assign LED value to inputs to be automatically added to the outputLED value that is sent to the shift register controlling the leds
  buttonValue[0] = 0;
  buttonValue[1] = 0;
  buttonValue[2] = 0;
  buttonValue[3] = 0;
  buttonValue[4] = 0;
  buttonValue[5] = 0;
  buttonValue[6] = 0;
  buttonValue[7] = 0;
  buttonValue[8] = 0;
  buttonValue[9] = 0;
  buttonValue[10] = 0;
  switchValue[0] = red_rgb[0] + blue_rgb[0];
  switchValue[1] = blue_rgb[0] + green_rgb[0];
  switchValue[2] = green_rgb[0];
  switchValue[3] = green_rgb[0];
  switchValue[4] = red_rgb[0] + green_rgb[0];

  sec_buttonValue[0] = 0;
  sec_buttonValue[1] = 0;
  sec_buttonValue[2] = 0;
  sec_buttonValue[3] = 0;
  sec_buttonValue[4] = 0;
  sec_buttonValue[5] = 0;
  sec_buttonValue[6] = 0;
  sec_buttonValue[7] = 0;
  sec_buttonValue[8] = 0;
  sec_buttonValue[9] = 0;
  sec_buttonValue[10] = 0;
  sec_switchValue[0] = red_rgb[0] + blue_rgb[0];
  sec_switchValue[1] = green_rgb[0] + blue_rgb[2];
  sec_switchValue[2] = green_rgb[0] + blue_rgb[2];
  sec_switchValue[3] = 0;
  sec_switchValue[4] = red_rgb[0] + green_rgb[0];

  //Set brightness if OE is connected
  setBrightness(255);
  outputWrite(B01111110); //Set output to RGB leds on

  //Deej Analog Input
  for (int i = 0; i < NUM_SLIDERS; i++) {
    pinMode(analogInputs[i], INPUT);
  }

  Keyboard.begin();
  Serial.println("Start keyboard");
}

void loop() {
  //recieve inputs from shift register in
  inputReceive();

  secInput = digitalRead(PIN_SEC);
  muteInput = digitalRead(PIN_S0);
  pttInput = digitalRead(PIN_S1);

  //****************Check here for Sec button/the two seperate switches***********
  if (secInput == LOW) { //process sec button
    if (millis() - buttonTimeout_Sec > debounce) {
      Serial.println("but_sec");
      buttonTimeout_Sec = millis();
      secPrevious = secMode;
      secMode = not secMode; //handle led at the end
    }
  }

  if (muteInput == LOW && previous_mute == HIGH) { //mute switch on
    pressKey(KEY_F14);
    outputValue += led_mute;
  } else if (muteInput == HIGH && previous_mute == LOW) { //off
    pressKey(KEY_F14);
    outputValue -= led_mute;
  }


  if (pttInput == LOW && previous_ptt == HIGH) { //ptt switch on
    if (!secMode) {
      pressKey(KEY_F15);
      outputValue += led_ptt;
    } else {
      pressKey_c(KEY_F15);
      outputValue += blue_rgb[0];
      secEnabled_ptt = true;
      secMode = false; //consume secMode
    }
  } else if (pttInput == HIGH && previous_ptt == LOW) { //off
    if (!secMode && !secEnabled_ptt) {
      pressKey(KEY_F15);
      outputValue -= led_ptt;
    } else {
      pressKey_s(KEY_F15);
      outputValue -= blue_rgb[0];
      secEnabled_ptt = false;
      secMode = false; //consume secMode
    }
  }

  previous_mute = muteInput;
  previous_ptt = pttInput;

  //process the first and second register
  int operation_r2 = preprocessRegister_2();

  int input_r1 = processRegister_1();
  int input_r2 = processRegister_2(operation_r2);

  int buttonPressed = -1;
  if (operation_r2 == 2) {//buttons on register 2 are different
    buttonPressed = input_r2;
  } else {
    buttonPressed = input_r1;
  }

  /*
    Serial.print("firstByte: ");
    Serial.println(firstByte, BIN);
    Serial.print("secondByte: ");
    Serial.println(secondByte, BIN);
    Serial.print("operation: ");
    Serial.println(operation_r2);
    Serial.print("input_r1: ");
    Serial.println(input_r1);
    Serial.print("input_r2: ");
    Serial.println(input_r2);
    Serial.print("buttonPressed: ");
    Serial.println(buttonPressed);
  */

  //process all buttons, logical toggle

  //NOTE: the whole input process is only designed to recieve one button press at a time.
  //process input as a toggle, and assign value to output
  if (buttonPressed != -1) {//Only process data if an actual button has been pressed.
    outputValue += green_rgb[0];
    outputWrite(outputValue);
    delay(50);
    outputValue -= green_rgb[0];
    outputWrite(outputValue);
    
    if (!secMode) {
      if (!buttonToggle[buttonPressed]) { //if button is in 'off' position
        buttonToggle[buttonPressed] = true;
        outputValue += buttonValue[buttonPressed];
        toggleOn_button(buttonPressed);
      } else { //button is 'on'
        buttonToggle[buttonPressed] = false;
        outputValue -= buttonValue[buttonPressed];
        toggleOff_button(buttonPressed);
      }
    } else { //secMode
      if (!sec_buttonToggle[buttonPressed]) { //if button is in 'off' position
        sec_buttonToggle[buttonPressed] = true;
        outputValue += sec_buttonValue[buttonPressed];
        toggleOn_button(buttonPressed);
      } else { //button is 'on'
        sec_buttonToggle[buttonPressed] = false;
        outputValue -= sec_buttonValue[buttonPressed];
        toggleOff_button(buttonPressed);
      }

      secMode = false; //consume secMode
    }
  }

  //the second register switches are automatically processd in the processRegister_2 function, the output for them is as well

  if (secPrevious != secMode) {
    if (secMode) {//To process changes from other buttons being pressed, this is seperated
      outputValue += red_rgb[0];
    } else {
      outputValue -= red_rgb[0];
    }
    secPrevious = secMode;
  }

  //call function to write to shift register out
  if (outputValue_previous != outputValue) {
    outputWrite(outputValue);
    outputValue_previous = outputValue;
  }

  //Deej functions
  updateSliderValues();
  sendSliderValues(); // Actually send data (all the time)
  // printSliderValues(); // For debug

  //Custom Slider
  customSlider = analogRead(A10);
  setBrightness(customBrightnessScale(customSlider));

  delay(175);
}

void toggleOn_switch(int input) { //******simple add a IF SEC and a different swtich statemetn if so etc.**********
  if (!secMode) {
    switch (input) {
      case 0:
        if (eliteMode) {
          pressKey(KEY_F16); 
        } else {
          pressKey(KEY_F17); 
        }
        break;
      case 1:
        if (vrMode) {
          if (eliteMode) {
            pressKey_c(KEY_F16);
            switchValue[1] = blue_rgb[1];
          } else {
            pressKey_ca(KEY_F19);
            switchValue[1] = blue_rgb[1];
          }
        } else {
          pressKey(KEY_F18); 
          switchValue[1] = blue_rgb[0] + green_rgb[0];
        }
        break;
      case 2:
        pressKey_a(KEY_F19); //Speakers on
        break;
      case 3:
        pressKey(KEY_F19); // VR switch
        vrMode = true;
        break;
      case 4:
        pressKey_a(KEY_F20);
        break;
      default:
        Serial.println("something went from with toggleOn_switch");
        break;
    }
  } else { //sec
    switch (input) {
      case 0:
        pressKey_a(KEY_F17); //Footpedal
        break;
      case 1:
        pressKey_a(KEY_F18); //Misc
        switchValue[1] = green_rgb[0] + blue_rgb[2];
        break;
      case 2:
        break;
      case 3:
        break;
      case 4:
        pressKey_s(KEY_F20);
        delay(300);
        pressKey_a(KEY_F20);
        break;
      default:
        Serial.println("something went from with toggleOn_switchSEC");
        break;
    }
  }

}

void toggleOff_switch(int input) {
  if (!secMode && !secEnabled_switch[input]) {
    switch (input) {
      case 0:
        if (eliteMode) {
          pressKey(KEY_F16); 
        } else {
          pressKey_c(KEY_F17); 
        }
        break;
      case 1:
        if (vrMode) {
          if (eliteMode) {
            pressKey_c(KEY_F16);
            switchValue[1] = blue_rgb[1];
          } else {
            pressKey_s(KEY_F19); //back to VR audio
          }
        } else {
          pressKey_c(KEY_F18); 
        }
        break;
      case 2:
        pressKey_ca(KEY_F19); //Speakers off
        break;
      case 3:
        pressKey_c(KEY_F19); //VR off
        vrMode = false;
        break;
      case 4:
        pressKey_ca(KEY_F20);
        break;
      default:
        Serial.println("something went from with toggleOff_switch");
        break;
    }
  } else {
    switch (input) {
      case 0:
        pressKey_ca(KEY_F17);
        break;
      case 1:
        pressKey_ca(KEY_F18); //Misc
        break;
      case 2:
        break;
      case 3:
        break;
      case 4:
        pressKey_s(KEY_F20);
        delay(300);
        pressKey_ca(KEY_F20);
        break;
      default:
        Serial.println("something went from with toggleOff_switchSEC");
        break;
    }
  }

}

void toggleOn_button(int input) {
  if (!secMode) {
    switch (input) {
      case 0:
        pressKey(KEY_F20);
        break;
      case 1:
        pressKey(KEY_F21);
        break;
      case 2:
        if (vrMode) {
          pressKey(KEY_F22); 
          eliteMode = true;
        } else {
          pressKey_ca(KEY_F19); //Bluetooth
        }
        break;
      case 3:
        pressKey(KEY_F23); //Monitor
        break;
      case 4:
        pressKey(KEY_F24);
        break;
      case 5:
        pressKey_s(KEY_F22);
        break;
      case 6:
        pressKey_s(KEY_F23); //Color change
        //pressKey_c(KEY_F16); //Swap headset
        break;
      case 7:
        pressKey_s(KEY_F24);
        //pressKey(KEY_F16); 
        break;
      case 8:
        pressKey_a(KEY_F22);
        break;
      case 9:
        pressKey_a(KEY_F23);
        break;
      case 10:
        pressKey_a(KEY_F24);
        break;
      default:
        Serial.println("something went wrong with toggleOn_button");
        break;
    }
  } else {
    switch (input) {
      case 0:
        pressKey_c(KEY_F20);
        break;
      case 1:
        pressKey_c(KEY_F21);
        break;
      case 2:
        pressKey_ca(KEY_F19);
        break;
      case 3:
        break;
      case 4:
        pressKey_c(KEY_F24);
        break;
      case 5:
        pressKey_cs(KEY_F22);
        break;
      case 6:
        pressKey_cs(KEY_F23); //Color change two others
        break;
      case 7:
        pressKey_cs(KEY_F24);
        break;
      case 8:
        pressKey_ca(KEY_F22);
        break;
      case 9:
        pressKey_ca(KEY_F23);
        break;
      case 10:
        pressKey_ca(KEY_F24);
        break;
      default:
        Serial.println("something went wrong with toggleOn_buttonSEC");
        break;
    }
  }
}

void toggleOff_button(int input) {
  if (!secMode) {
    switch (input) {
      case 0:
        toggleOn_button(input);
        break;
      case 1:
        toggleOn_button(input);
        break;
      case 2:
        toggleOn_button(input); //Do same thing on both presses
        if (vrMode) {
          eliteMode = false;
        }
        break;
      case 3:
        pressKey_c(KEY_F23); //Monitor
        break;
      case 4:
        toggleOn_button(input);
        break;
      case 5:
        toggleOn_button(input);
        break;
      case 6:
        toggleOn_button(input); //Do same thing on both presses
        break;
      case 7:
        toggleOn_button(input); //Do same thing on both presses
        break;
      case 8:
        toggleOn_button(input);
        break;
      case 9:
        toggleOn_button(input);
        break;
      case 10:
        toggleOn_button(input);
        break;
      default:
        Serial.println("something went wrong with toggleOff_button");
        break;
    }
  } else {
    switch (input) {
      case 0:
        toggleOn_button(input);
        break;
      case 1:
        toggleOn_button(input);
        break;
      case 2:
        toggleOn_button(input);
        break;
      case 3:
        toggleOn_button(input);
        break;
      case 4:
        toggleOn_button(input);
        break;
      case 5:
        toggleOn_button(input);
        break;
      case 6:
        toggleOn_button(input); //Do same thing on both presses
        break;
      case 7:
        toggleOn_button(input);
        break;
      case 8:
        toggleOn_button(input);
        break;
      case 9:
        toggleOn_button(input);
        break;
      case 10:
        toggleOn_button(input);
        break;
      default:
        Serial.println("something went wrong with toggleOff_buttonSEC");
        break;
    }
  }
}

void pressKey(int key) {
  Keyboard.write(key);
}

void pressKey_c(int key) {
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.write(key);
  Keyboard.release(KEY_LEFT_CTRL);
}

void pressKey_s(int key) {
  Keyboard.press(KEY_LEFT_SHIFT);
  Keyboard.write(key);
  Keyboard.release(KEY_LEFT_SHIFT);
}

void pressKey_a(int key) {
  Keyboard.press(KEY_LEFT_ALT);
  Keyboard.write(key);
  Keyboard.release(KEY_LEFT_ALT);
}

void pressKey_ca(int key) {
  Keyboard.press(KEY_LEFT_ALT);
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.write(key);
  Keyboard.release(KEY_LEFT_ALT);
  Keyboard.release(KEY_LEFT_CTRL);
}

void pressKey_cs(int key) {
  Keyboard.press(KEY_LEFT_SHIFT);
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.write(key);
  Keyboard.release(KEY_LEFT_SHIFT);
  Keyboard.release(KEY_LEFT_CTRL);
}

void setBrightness(byte brightness) {// 0 to 255
  analogWrite(PIN_OE, 255 - brightness);
}

//check if any value on register2 was changed
int preprocessRegister_2() {
  if (secondByte_previous == secondByte) {
    return 0; // no change
  } else {
    //clear value, all previous values needed stored in secondByte_previous
    buttons_r2 = 0;
    buttons_r2_previous = 0;
    switches_r2 = 0;
    switches_r2_previous = 0;

    //set value
    //buttons_r2 = secondByte << 5; //B00000100 becomes B10000000
    //buttons_r2_previous = secondByte_previous << 5;
    //switches_r2 = secondByte >> 3; //B10000000 becomes B00010000
    //switches_r2_previous = secondByte_previous >> 3;

    buttons_r2 = secondByte >> 5; //B10000000 becomes B00000100 because i miswired
    buttons_r2_previous = secondByte_previous >> 5;
    switches_r2 = secondByte << 3; //B00010000 becomes B10000000
    switches_r2_previous = secondByte_previous << 3;

    /*
      Serial.print("buttons_r2: ");
      Serial.println(buttons_r2, BIN);
      Serial.print("switches_r2: ");
      Serial.println(switches_r2, BIN);
    */

    if (switches_r2 != switches_r2_previous) {
      return 1; // switches are different
    } else if (buttons_r2 == 0) { //prevent processing the letting go of button
      return 0;
    } else if (buttons_r2 != buttons_r2_previous) {
      return 2; // buttons are different
    } else {
      Serial.println("Something broke in preprocessRegister_2");
    }
  }
}

int processRegister_2(int operation) {
  if (operation == 0) { //no change
    return -1;
  } else if (operation == 1) { //switch was updated
    int inputValue = switches_r2 - switches_r2_previous; //have to filter out any active switches from the one that was just flipped.
    int switchFlipped = r2_process_switch(inputValue);

    if (!secMode && !secEnabled_switch[switchFlipped]) {
      if (inputValue > 0) { //the number returned is positive, so the switch toggled on
        toggleOn_switch(switchFlipped);
        outputValue += switchValue[switchFlipped];
      } else { //the number returned is negative, so the switch toggled off
        toggleOff_switch(switchFlipped);
        outputValue -= switchValue[switchFlipped];
      }
    } else { //secmode is ON, or switch was turned on with SEC
      if (inputValue > 0) { //the number returned is positive, so the switch toggled on
        toggleOn_switch(switchFlipped);
        outputValue += sec_switchValue[switchFlipped];
        secEnabled_switch[switchFlipped] = true;
      } else { //the number returned is negative, so the switch toggled off
        toggleOff_switch(switchFlipped);
        outputValue -= sec_switchValue[switchFlipped];
        secEnabled_switch[switchFlipped] = false;
      }
      secMode = false; //consume secMode
    }

  } else if (operation == 2) { // button was updated
    return r2_process_button();
  } else {
    Serial.println("Something broke in processRegister_2");
  }
}

int r2_process_switch(int input) {
  input = abs(input);
  int switchFlipped;

  switch (input) {
    case switch0:
      Serial.println("s0");
      switchFlipped = 0;
      break;
    case switch1:
      Serial.println("s1");
      switchFlipped =  1;
      break;
    case switch2:
      Serial.println("s2");
      switchFlipped =  2;
      break;
    case switch3:
      Serial.println("s3");
      switchFlipped =  3;
      break;
    case switch4:
      Serial.println("s4");
      switchFlipped =  4;
      break;
    default:
      Serial.println("Something broke in r2_process_switch");
      Serial.print("input: B");
      Serial.println(input, BIN);
      break;
  }

  return switchFlipped;
}

int r2_process_button () {
  int buttonPressed = -1;
  switch (buttons_r2) {
    case button0:
      if (millis() - buttonTimeout[8] > debounce) {
        Serial.println("but8");
        buttonTimeout[8] = millis();
        buttonPressed = 8;
      }
      break;
    case button1:
      if (millis() - buttonTimeout[9] > debounce) {
        Serial.println("but9");
        buttonTimeout[9] = millis();
        buttonPressed = 9;
      }
      break;
    case button2:
      if (millis() - buttonTimeout[10] > debounce) {
        Serial.println("but10");
        buttonTimeout[10] = millis();
        buttonPressed = 10;
      }
      break;
    default:
      Serial.println("Something broke in r2_process_button");
      Serial.print("buttons_r2: B");
      Serial.println(buttons_r2, BIN);
  }

  return buttonPressed;
}

void inputReceive() {
  secondByte_previous = secondByte;

  // select 4021
  digitalWrite(PIN_SS1, LOW);

  // read CD4021 IC
  firstByte = SPI.transfer(0x00);
  secondByte = SPI.transfer(0x00);

  // deselect 4021
  digitalWrite(PIN_SS1, HIGH);

  //Serial.println(firstByte, BIN);
  //Serial.println(secondByte, BIN);
}

int processRegister_1() {
  int buttonPressed = -1;

  //Code for register1, only handles 1 button press at a time
  // button functions and debounces
  // needs refactoring for smaller footprint

  if (firstByte == 0) { //End function early if there is no input
    return -1;
  }

  switch (firstByte) {
    case button0:
      if (millis() - buttonTimeout[0] > debounce) {
        Serial.println("but0");
        buttonTimeout[0] = millis();
        buttonPressed = 0;
      }
      break;
    case button1:
      if (millis() - buttonTimeout[1] > debounce) {
        Serial.println("but1");
        buttonTimeout[1] = millis();
        buttonPressed = 1;
      }
      break;
    case button2:
      if (millis() - buttonTimeout[2] > debounce) {
        Serial.println("but2");
        buttonTimeout[2] = millis();
        buttonPressed = 2;
      }
      break;
    case button3:
      if (millis() - buttonTimeout[3] > debounce) {
        Serial.println("but3");
        buttonTimeout[3] = millis();
        buttonPressed = 3;
      }
      break;
    case button4:
      if (millis() - buttonTimeout[4] > debounce) {
        Serial.println("but4");
        buttonTimeout[4] = millis();
        buttonPressed = 4;
      }
      break;
    case button5:
      if (millis() - buttonTimeout[5] > debounce) {
        Serial.println("but5");
        buttonTimeout[5] = millis();
        buttonPressed = 5;
      }
      break;
    case button6:
      if (millis() - buttonTimeout[6] > debounce) {
        Serial.println("but6");
        buttonTimeout[6] = millis();
        buttonPressed = 6;
      }
      break;
    case button7:
      if (millis() - buttonTimeout[7] > debounce) {
        Serial.println("but7");
        buttonTimeout[7] = millis();
        buttonPressed = 7;
      }
      break;
    default:
      Serial.println("Something went wrong with processRegister_1");
      Serial.println("------------------------------");
      Serial.print("firstByte: ");
      Serial.println(firstByte);
      Serial.print("buttonPressed: ");
      Serial.println(buttonPressed);
      break;
  }
  return buttonPressed;
}



void outputWrite(byte outputLED) {
  //************************Potential for boolean and time check so after X time color is changed back on temp button presses or something

  //default litholeds to on when no color is specified
  const byte litho_on = B11111100;

  byte litholeds = outputLED >> 1; //Remove led_PTT
  litholeds = litholeds << 2; //Remove led_Mute and leftshift
  byte muteled = outputLED << 7;
  byte pttled = outputLED >> 7;
  pttled = pttled << 7;

  if (litholeds == 0) {
    outputLED += white_rgb[0];
  }

  if (muteInput == HIGH && muteled) { //ensure that these two leds stay off if not specifically enabled, due to unaccounted for addition
    outputLED -= led_mute;
  }
  if ((pttInput == HIGH || secEnabled_ptt) && pttled) {
    outputLED -= led_ptt;
  }

  // SS1 = HIGH -> 4021 is gathering data from parallel inputs

  // select 595
  digitalWrite(PIN_595_1, LOW);

  // send BIN number to 595 to light leds
  SPI.transfer(outputLED);

  // deselect 595
  digitalWrite(PIN_595_1, HIGH);

}

byte customBrightnessScale(int input) {
  if (abs(input - previousValue) < 20) { //filter noisy input
    input = previousValue;
  } else {
    previousValue = input;
  }

  if (input < minValue) {
    input = 0;
  } else if (input > maxValue) {
    input = 1023;
  }

  return map(input, 0, 1023, 0, 255);
}

//Deej functions, code to print analog sliders to the program deej which adjusts windows audio values, these functions taken from the deej github page------------------------------------------------
int customAudioScale(int input) {
  if (input < minValue) {
    return 0;
  } else if (input > maxValue) {
    return 1023;
  } else {
    return input;
  }
}

void updateSliderValues() {
  for (int i = 0; i < NUM_SLIDERS; i++) {
    analogSliderValues[i] = customAudioScale(analogRead(analogInputs[i]));
    //analogSliderValues[i] = analogRead(analogInputs[i]);
  }
}

void sendSliderValues() {
  String builtString = String("");

  for (int i = 0; i < NUM_SLIDERS; i++) {
    builtString += String((int)analogSliderValues[i]);

    if (i < NUM_SLIDERS - 1) {
      builtString += String("|");
    }
  }

  Serial.println(builtString);
}

void printSliderValues() {
  for (int i = 0; i < NUM_SLIDERS; i++) {
    String printedString = String("Slider #") + String(i + 1) + String(": ") + String(analogSliderValues[i]) + String(" mV");
    Serial.write(printedString.c_str());

    if (i < NUM_SLIDERS - 1) {
      Serial.write(" | ");
    } else {
      Serial.write("\n");
    }
  }
}
//End Deej functions --------------------------------
