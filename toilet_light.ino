// constants won't change. They're used here to
// set pin numbers:

//#define DEBUG
#define LEARN

#ifdef LEARN
#include <EEPROM.h>
#endif

const int switch1 = 2;       // выключатель у входа
const int switch2 = 3;       // выключатель внутри в ванной
const int atxRelay = 4;      // реле блока питания компьютера
const int lampRelay = 7;     // реле лампы дежурного освещения

const unsigned long afterSwitchDelay = 100;  // задержка после включения света, чтобы не дребезжало
const unsigned long learnMode = 10;  // сколько секунд ждать для входа в режим обучения

// variables will change:
unsigned long lightDelay; // задержка выключения света,
// будет загружена из EEPROM или взята из хардкода,
// 1 секунда соответствует 1 минуте - масштаб 1:60

unsigned long tmp;
int light = 0;
int btn1 = 0, btn2 = 0;
int state = 0; // состояние: 0 - выключено нормально, 1 - включено до таймаута, 2 - выключено из-за таймаута
// 3 - вход в режим обучения, 4 - переход в режим обучения, держат кнопку, 5 - обучение, ждём считаем секунды
unsigned long dateLightSwitch; // когда пора выключать свет

void setup() {
#ifdef DEBUG
  Serial.begin(9600);
#endif

  // в EEPROM хранится число минут ожидания.
  // в lightDelay число реальных секунд

#ifdef LEARN
  tmp = EEPROM.read(0);
  lightDelay = tmp * 60;
#ifdef DEBUG
  Serial.print("read from eeprom "); Serial.print(tmp); Serial.print(", light delay is "); Serial.print(lightDelay);
#endif //debug

  if (0 == lightDelay) {
    lightDelay = 20 * 60;
    EEPROM.write(0, lightDelay / 60);
  }
#ifdef DEBUG
  Serial.print(", final light delay is "); Serial.println(lightDelay);
#endif //debug
#else
  lightDelay = 30 * 60; //tmp*60;
#endif //learn

  // initialize the relays as outputs:
  pinMode(atxRelay, OUTPUT);
  pinMode(lampRelay, OUTPUT);

  // initialize the pushbutton pin as an input:
  pinMode(switch1, INPUT);
  pinMode(switch2, INPUT);

  setRelays();
}

void loop() {
  // read the state of the pushbutton value:
  btn1 = digitalRead(switch1);
  btn2 = digitalRead(switch2);

  if ((HIGH == btn1) && (0 == state)) {
    // 1 было выключено полностью, включили кнопкой. зажигаем свет, заводим таймер
    state = 1; light = 1;
    tmp = millis();
    dateLightSwitch = tmp + lightDelay * 1000;
#ifdef DEBUG
    Serial.print("s0>1 now is "); Serial.print(millis()); Serial.print(" will turn off at "); Serial.println(dateLightSwitch);
#endif
    setRelays();
  }

  if ((LOW == btn1) && (1 == state)) {
    // 2 было включено до таймаута, выключили кнопкой. гасим свет
    state = 0; light = 0;
#ifdef DEBUG
    Serial.println("s1>0");
#endif
    setRelays();
  }

  tmp = millis();
  if ((HIGH == btn1) && (1 == state) && (tmp > dateLightSwitch)) {
    // 3 включено, кнопка ещё нажата, а таймер вышел. гасим свет.
    state = 2; light = 0;
#ifdef DEBUG
    Serial.print("s0>2 now is "); Serial.println(millis());
#endif
    setRelays();
  }

  if ((LOW == btn1) && (2 == state)) {
    // 4 таймер вышел, свет потушен, а теперь отпустили кнопку. всё уже выключено, сбрасываем в 0.
    state = 0; light = 0;
#ifdef DEBUG
    Serial.print("s2->0 now is "); Serial.println(millis());
#endif
    setRelays();
  }

  if ((HIGH == btn2) && (1 == state)) {
    // 5 свет горит, нажали на кнопку продления. переводим таймер.
    state = 1; light = 1;
    tmp = millis();
    dateLightSwitch = tmp + lightDelay * 1000;
#ifdef DEBUG
    Serial.print("s1>1 prolong "); Serial.print(millis()); Serial.print(" will turn off at "); Serial.println(dateLightSwitch);
#endif
    //    setRelays();  // no need to click, cause the light is already on
  }

  if ((HIGH == btn2) && (2 == state)) {
    // 6 свет потух, но нажали на кнопку продления. переводим таймер.
    state = 1; light = 1;
    tmp = millis();
    dateLightSwitch = tmp + lightDelay * 1000;
#ifdef DEBUG
    Serial.print("s2>1 prolong "); Serial.print(millis()); Serial.print(" will turn off at "); Serial.println(dateLightSwitch);
#endif
    setRelays();
  }

#ifdef LEARN
  if ((LOW == btn1) && (0 == state) && (HIGH == btn2)) {
    // 7 режим обучения, считаем 10 секунд для входа
    tmp = millis();
    dateLightSwitch = tmp + learnMode * 1000;
    state = 3; light = 0;
#ifdef DEBUG
    Serial.print("entering learn, now is "); Serial.print(tmp); Serial.print(" will enter at "); Serial.println(dateLightSwitch);
#endif
  }

  if ((LOW == btn1) && (3 == state) && (LOW == btn2)) {
    // вышли из режима обучения не дождавшись десяти секунд потому что отпущена кнопка 2
    state = 0; light = 0;
#ifdef DEBUG
        Serial.println("exit learn");
#endif
  }

  tmp = millis();
  if ((LOW == btn1) && (3 == state) && (HIGH == btn2) && (tmp > dateLightSwitch)) {
    // 8 переходим в режим обучения, мигаем дежуркой, отпустите кнопку 2
#ifdef DEBUG
    Serial.print("entered learn, state 4,now is "); Serial.print(tmp); Serial.print(" entered by "); Serial.println(dateLightSwitch);
#endif

    digitalWrite(lampRelay, !digitalRead(lampRelay));
    delay(1000);
    digitalWrite(lampRelay, !digitalRead(lampRelay));
    state = 4; light = 0;
  }

  if ((LOW == btn1) && (4 == state) && (LOW == btn2)) {
    // вошли в режим обучения, обе кнопки отпущены, можно начинать считать время
    state = 5; light = 0;
    tmp = millis();
    dateLightSwitch = tmp;
#ifdef DEBUG
    Serial.println("both buttons down, goto state 5");
#endif
  }

  if ((LOW == btn1) && (5 == state) && (HIGH == btn2)) {
    // если в режиме обучения опять нажали вторую кнопку,
    // значит обучение закончено. надо посчитать сколько прошло времени
    // и записать это в EEPROM для использования
    tmp = millis();
    // ОПАСНО! Возможны глюки при переполнении millis
    lightDelay = (tmp - dateLightSwitch) * 60 / 1000; // секунда за минуту
    if (60 > lightDelay) {
      lightDelay = 61;
    }
#ifdef DEBUG
    Serial.print("finished learn, state 4, now is "); Serial.print(tmp); Serial.print(", delay learnt is "); Serial.print(lightDelay);
    Serial.print(", writing to eeprom "); Serial.println(lightDelay / 60);
#endif

    EEPROM.write(0, lightDelay / 60);

    digitalWrite(lampRelay, !digitalRead(lampRelay));
    delay(3000);
    digitalWrite(lampRelay, !digitalRead(lampRelay));
    state = 0; light = 0;
    setRelays();
  }

  if ((HIGH == btn1) && (5 == state || 4 == state || 3 == state)) {
    // если в режимах обучения нажали первую кнопку,
    // значит обучение прервано, выходим и забываем.

#ifdef DEBUG
    Serial.println("exit learn by main button");
#endif

    state = 0; light = 0;
    setRelays();
    // на следующем цикле из нулевого состояния перейдём в первое.
  }
#endif
}

void setRelays() {
  digitalWrite(atxRelay, (0 == light) ? HIGH : LOW);
  digitalWrite(lampRelay, (0 == light) ? LOW : HIGH);
  //  delay(afterSwitchDelay);
#ifdef DEBUG
  Serial.print("state is "); Serial.print(state); Serial.print("; light is "); Serial.println(light);
#endif
}
