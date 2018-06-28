#define DEBUG
#define LEARN
#define TIMEROVERFLOW

#ifdef LEARN
#include <EEPROM.h>
#endif

// constants won't change. They're used here to
// set pin numbers:
const int switch1 = 2;       // main switch
const int switch2 = 3;       // "prolong" switch
const int atxRelay = 4;      // main relay
const int lampRelay = 7;     // standby light relay

#ifdef LEARN
const unsigned long learnMode = 10;  // how long to wait before entering learn
#endif

// variables will change:
unsigned long lightDelay; // how long to wait to turn the light on
// will be loaded from hardcode or from EEPROM
// 1 value corresponds to 1 minute

unsigned long tmp;
int light = 0;
int btn1 = 0, btn2 = 0;
int state = 0; // 0 - off by main button, 1 - on until timer counts down, 2 - off after timer countdown,
// 3 - entering learn mode before 10 sec, 4 - entering learn mode, 10 sec ok, waiting to release button,
// 5 - learning, counting time
unsigned long dateLightSwitch; // variable, millis when it is okay to turn off the light
#ifdef TIMEROVERFLOW
extern volatile unsigned long timer0_millis;
#endif

void setup() {
#ifdef DEBUG
  Serial.begin(9600);
#endif

#ifdef LEARN
  // EEPROM stores minutes to wait.
  // lightDelay stores seconds to wait
  tmp = EEPROM.read(0);
  lightDelay = tmp * 60;
  if (0 == lightDelay) {
    lightDelay = 30 * 60;
    EEPROM.write(0, lightDelay / 60);
  }
#ifdef DEBUG
  Serial.print("read from eeprom "); Serial.print(tmp); Serial.print(", light delay is "); Serial.print(lightDelay);
  Serial.print(", final light delay is "); Serial.println(lightDelay);
#endif // debug
#else // if no learn
  lightDelay = 30 * 60;
#ifdef DEBUG
  Serial.print("light delay is "); Serial.println(lightDelay);
#endif // debug
#endif // learn

  // initialize the relays as outputs:
  pinMode(atxRelay, OUTPUT);
  pinMode(lampRelay, OUTPUT);

  // initialize the pushbutton pins as inputs:
  pinMode(switch1, INPUT);
  pinMode(switch2, INPUT);

  setRelays();

#ifdef TIMEROVERFLOW
  noInterrupts ();
  timer0_millis = -10000;
  interrupts ();
#endif
}

void loop() {
  // read the state of the pushbutton values:
  btn1 = digitalRead(switch1);
  btn2 = digitalRead(switch2);

  if ((HIGH == btn1) && (0 == state)) {
    // 1. goto state 1: it was off, now they turned it on by main button.
    // start the timer, turn on the light
    state = 1; light = 1;
    dateLightSwitch = millis();

#ifdef DEBUG
    Serial.print("s0>1 now is "); Serial.print(millis()); Serial.print(" will turn off in "); Serial.print(lightDelay); Serial.println(" seconds");
#endif
    setRelays();
  }

  if ((LOW == btn1) && (1 == state)) {
    // 2. goto state 1: it was on, timeout did not fire, now they turned it on by main button.
    // turn off the light
    state = 0; light = 0;
#ifdef DEBUG
    Serial.println("s1>0");
#endif
    setRelays();
  }

  if ((HIGH == btn1) && (1 == state)) {
    tmp = millis();
    if (tmp - dateLightSwitch > lightDelay * 1000) {
      // 3. it was on, timer went down, but the button is still on.
      // goto state 2, turn off the light.
      state = 2; light = 0;
#ifdef DEBUG
      Serial.print("s0>2 now is "); Serial.println(tmp);
#endif
      setRelays();
    }
  }

  if ((LOW == btn1) && (2 == state)) {
    // 4. light is off because the timer went out, the light is off,
    // but they turned off the main button. do not switch the light, just goto state 0.
    state = 0; light = 0;
#ifdef DEBUG
    Serial.print("s2->0 now is "); Serial.println(millis());
#endif
    setRelays();
  }

  if ((HIGH == btn2) && (1 == state)) {
    // 5. the light is on, but they pushed the prolong button. rewind the timer, save the state
    state = 1; light = 1;
    dateLightSwitch = millis();
#ifdef DEBUG
    Serial.print("s1>1 prolong "); Serial.print(millis()); Serial.print(" will turn off in "); Serial.print(lightDelay); Serial.println(" seconds");
#endif
    //    setRelays();  // no need to click, cause the light is already on
  }

  if ((HIGH == btn2) && (2 == state)) {
    // 6. the timer went out, the light is off, but they pushed the prolong button.
    // rewind the timer, set the light back on.
    state = 1; light = 1;
    dateLightSwitch = millis();
#ifdef DEBUG
    Serial.print("s2>1 prolong "); Serial.print(millis()); Serial.print(" will turn off in "); Serial.print(lightDelay); Serial.println(" seconds");
#endif
    setRelays();
  }

#ifdef LEARN
  if ((LOW == btn1) && (0 == state) && (HIGH == btn2)) {
    // 7. Learning mode. The light was off and the main button is off, but they pushed the prolong button.
    // we will count 10 seconds to enter learn mode
    dateLightSwitch = millis();
    state = 3; light = 0;
#ifdef DEBUG
    Serial.print("entering learn, now is "); Serial.print(tmp); Serial.print(" will enter in "); Serial.print(learnMode); Serial.println(" seconds");
#endif
  }

  if ((LOW == btn1) && (3 == state) && (LOW == btn2)) {
    // 8. We were entering learn mode but they have released the prolong button.
    // no learning today. goto state 0.
    state = 0; light = 0;
#ifdef DEBUG
    Serial.println("exit learn");
#endif
  }

  if ((LOW == btn1) && (3 == state) && (HIGH == btn2)) {
    tmp = millis();
    if (tmp - dateLightSwitch > learnMode*1000) {
      // 9. We were entering learn mode, 10 seconds with pressed prolong button passed, fine!
      // we are in the learning mode now. Switch the small relay, going to state 4 where we wait for prolong key to release.
#ifdef DEBUG
      Serial.print("entered learn, state 4, now is "); Serial.print(tmp); Serial.print(" entered by "); Serial.println(dateLightSwitch);
#endif

      digitalWrite(lampRelay, !digitalRead(lampRelay));
      delay(1000);
      digitalWrite(lampRelay, !digitalRead(lampRelay));
      state = 4; light = 0;
    }
  }

  if ((LOW == btn1) && (4 == state) && (LOW == btn2)) {
    // 10. We start learning only if the state is right and both keys are released.
    // There we will go to state 5 and save the timer for counting later.
    state = 5; light = 0;
    tmp = millis();
    dateLightSwitch = tmp;
#ifdef DEBUG
    Serial.println("both buttons down, start learn, goto state 5");
#endif
  }

  if ((LOW == btn1) && (5 == state) && (HIGH == btn2)) {
    // 11. If they pushed Prolong button again, that means the learning is finished.
    // We have to count how much time elapsed, set the delay and save it to EEPROM.
    tmp = millis();
    // BEWARE of millis overflow problems.
    lightDelay = (tmp - dateLightSwitch) * 60 / 1000; // секунда за минуту
    if (60 > lightDelay) {
      // if less than 1 second elapsed, think of it as of 1 minute delay.
      lightDelay = 61;
    }
#ifdef DEBUG
    Serial.print("finished learn, state 4, now is "); Serial.print(tmp); Serial.print(", delay learnt is "); Serial.print(lightDelay);
    Serial.print(", writing to eeprom "); Serial.println(lightDelay / 60);
#endif

    EEPROM.write(0, lightDelay / 60);

    // Blink small relay to say the learning is complete.
    digitalWrite(lampRelay, !digitalRead(lampRelay));
    delay(3000);
    digitalWrite(lampRelay, !digitalRead(lampRelay));
    state = 0; light = 0;
    setRelays();
  }

  if ((HIGH == btn1) && (5 == state || 4 == state || 3 == state)) {
    // 11. If they pushed the main button while in learning mode, that means it was a mistake
    // no learning today. Break the learn, no modifications done, forget everything.
    // We will switch from 0 state to 1 on next cycle.

    state = 0; light = 0;
    setRelays();
#ifdef DEBUG
    Serial.println("exit learn by main button");
#endif // debug
  }
#endif // learn
}

void setRelays() {
  // my relays are ON when the pin is LOW and OFF when HIGH
  digitalWrite(atxRelay, (0 == light) ? LOW : HIGH);
  digitalWrite(lampRelay, (0 == light) ? HIGH : LOW);
#ifdef DEBUG
  Serial.print("state is "); Serial.print(state); Serial.print("; light is "); Serial.println(light);
#endif
}
