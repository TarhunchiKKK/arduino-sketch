// 150501, Барило Константин Сергеевич
// Микропроцессорное устройство контроля параметров горючих смазочных материалов в ёмкости

#include <microDS18B20.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Thread.h>

// константы, описывающие режим работы устройства
#define OUTPUT_ALL 0                      // устройство измеряет все параметры
#define OUTPUT_TEMPERATURE 1              // устройство измеряет только температуру
#define OUTPUT_GAS 2                      // устройство измеряет только газ
#define OUTPUT_FIRE 3                     // устройство измеряет только пламя

// создание объекта модуля вывода
LiquidCrystal_I2C lcd(0x27, 16, 2);

// создание объекта датчика температуры
MicroDS18B20 <A3> temperature_sensor;

// объявление параметров
float temperature = 5.0;                  // значение температуры
bool has_gas = false;                     // флаг наличия газа
bool has_fire = false;                    // флаг наличия пламени

// контакты датчиков
int temperature_contact = A3;             // контакт датчика температуры
int gas_contact = A0;                     // контакт датчика газов
int fire_contact = A2;                    // контакт датчика пламени

// контакты светодиодов
int initial_light_contact = 10;           // контакт зелёного светодиода
int gas_light_contact = 11;               // контакт оранжевого светодиода
int fire_light_contact = 12;              // контакт красного светодиода

// контакты пьезодинамиков
int gas_piezo_contact = 9;
int fire_piezo_contact = 13;

// кнопки модуля клавиатуры
char keypad_matrix[4][3] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};
// контакты микроконтроллера для подключения модуля клавиатуры
byte keypad_rows_contacts[4] = { 6, 5, 4, 3 };
byte keypad_cols_contacts[3] = { 2, 8, 7 };

//создание объекта модуля клавиатуры
Keypad keypad = Keypad(makeKeymap(keypad_matrix), keypad_rows_contacts, keypad_cols_contacts, (byte)4, (byte)3);


bool device_work = false;             // флаг включения устройства
int cycle_counter = 0;                // счетчик итераций главного цикла
int mode = OUTPUT_ALL;                // режим работы устройства


Thread temperature_thread = Thread(); // поток для датчика температуры
Thread gas_thread = Thread();         // поток для датчика газов
Thread fire_thread = Thread();        // поток для датчика пламени
Thread display_thread = Thread();     // поток для модуля вывода
Thread keypad_thread = Thread();      // поток для модуля клавиатуры

// функция инициализации данных
void setup () {
  Serial.begin(9600);     

  // инициализация контактов
  pinMode(temperature_contact, INPUT);    
  pinMode(gas_contact, INPUT);
  pinMode(fire_contact, INPUT);
  pinMode(initial_light_contact, OUTPUT);
  pinMode(gas_light_contact, OUTPUT);
  pinMode(fire_light_contact, OUTPUT);
  pinMode(gas_piezo_contact, OUTPUT);
  pinMode(fire_piezo_contact, OUTPUT);
  for (int i = 0; i < 4; i++) {
    pinMode(keypad_rows_contacts[i], INPUT);
  }
  for (int i = 0; i < 3; i++) {
    pinMode(keypad_cols_contacts[i], INPUT);
  }

   // инициализация модуля вывода
  lcd.init();
  lcd.backlight();

  // инициализация потоков
  temperature_thread.onRun(temperature_handler);    
  temperature_thread.setInterval(1000);
  gas_thread.onRun(gas_handler);
  gas_thread.setInterval(1000);
  fire_thread.onRun(fire_handler);
  fire_thread.setInterval(1000);
  display_thread.onRun(display_handler);
  display_thread.setInterval(1000);
  keypad_thread.onRun(keypad_handler);
  keypad_thread.setInterval(100);

 
}

// главная функция
void loop() {

  // если устройство включено
  if (device_work == true) {

    // если эта итерация первая
    if (cycle_counter == 0) {
      lcd.clear();                                  
      lcd.display();
      lcd.setCursor(0, 0);

      // вывод 3 точек в интервалом в 1000 мс
      lcd.print("Starting");
      for (int i = 0; i < 3; i++) {
        delay(1000);
        lcd.print(".");
      }

      // тройное мигание зеленым светодиодом с интервалом в 500 мс
      for (int i = 0; i < 3; i++){
        digitalWrite(initial_light_contact, HIGH);
        delay(500);
        digitalWrite(initial_light_contact, LOW);
      }
      lcd.clear();
    }
    cycle_counter = cycle_counter + 1;
    Serial.println("Cycle counter: " + String(cycle_counter));
    
    // запуск потока модуля клавиатуры
    if (keypad_thread.shouldRun()) {
      keypad_thread.run();
    }

    // запуск потока датчика температуры
    if (temperature_thread.shouldRun()) {
      temperature_thread.run();
    }

    // запуск потока датчика газов
    if (gas_thread.shouldRun()) {
      gas_thread.run();
    }

    // запуск потока датчика пламени
    if (fire_thread.shouldRun()) {
      fire_thread.run();
    }

    // запуск потока модуля вывода
    if (display_thread.shouldRun()) {
      display_thread.run();
    }
  }  

  // если устройство не включено
  else {
    // очистка дисплея
    lcd.clear();
    lcd.noDisplay();

    // выключение светодиодов и пьезодинамиков
    digitalWrite(initial_light_contact, LOW);
    digitalWrite(gas_light_contact, LOW);
    digitalWrite(fire_light_contact, LOW);
  }

  // запуск потока модуля клавиатуры
  if (keypad_thread.shouldRun()) {
    keypad_thread.run();
  }
}

// функция измерения температуры
void temperature_handler() {
  temperature_sensor.requestTemp();               // запрос температуры
  delay(1000);                                    // ожидание обработки температуры
  if (temperature_sensor.readTemp()) {            // если температура прочитана
    temperature = temperature_sensor.getTemp();   // запомнить тепературу
    Serial.println("Temp sensor value: " +  String(temperature));
  }
}

// функция индикации наличия газов
void gas_handler() {
  int value = analogRead(gas_contact);            // чтение значения датчика газов
  if (value > 400) {                              // газы обнаружены
    has_gas = true;                               // запомнить состоние
    digitalWrite(gas_light_contact, HIGH);        // включить светодиод
    tone(gas_piezo_contact, 1000);
  }
  else {                                          // газы не обнаружены
    has_gas = false;                              // запомнить состояние
    digitalWrite(gas_light_contact, LOW);         // выключить светодиод
    noTone(gas_piezo_contact);
  }
  Serial.println("Gas sensor value: " + String(value));
}

// функция индикации наличия пламени
void fire_handler() {
  int value = analogRead(fire_contact);          // чтение значения датчика пламени
  if (value <= 300) {                            // пламя обнаружено
    has_fire = true;                              // запомнить состояние
    digitalWrite(fire_light_contact, HIGH);       // включить светодиод
    tone(fire_piezo_contact, 1000);
  }
  else {                                          // пламя не обнаружено
    has_fire = false;                             // запомнить состояние
    digitalWrite(fire_light_contact, LOW);        // выключить светодиод
    noTone(fire_piezo_contact);
  }
  Serial.println("Fire sensor value: " + String(value));
}

// функция-обработчик ввода с клавиатуры
void keypad_handler() {
  char pressed_key = keypad.getKey();             // нажатая клавиша

  //Serial.println("Pressed key: " + String(pressed_key));

  // режим вывода информации о температуре и наличии газов и пламени
  if (pressed_key == '0') {
    mode = OUTPUT_ALL;
    Serial.println("Mode: " + String(mode));
    Serial.println("Change to mode OUTPUT_ALL");
  }

  // режим вывода информации о температуре
  if (pressed_key == '1') {
    mode = OUTPUT_TEMPERATURE;
    Serial.println("Mode: " + String(mode));
    Serial.println("Change to mode OUTPUT_TEMPERATURE");
  }

  // режим вывода информации о наличии газов
  if (pressed_key == '2') {
    mode = OUTPUT_GAS;
    Serial.println("Mode: " + String(mode));
    Serial.println("Change to mode OUTPUT_GAS");
  }

  // режим вывода информации о наличии пламени
  if (pressed_key == '3') {
    mode = OUTPUT_FIRE;
    Serial.println("Mode: " + String(mode));
    Serial.println("Change to mode OUTPUT_FIRE");
  }

  // вкючение устройства
  if (pressed_key == '*') {
    device_work = true;
    Serial.println("Device work: " + String(device_work));
    Serial.println("Switch on device");
  }

  // выключение устройства
  if (pressed_key == '#') {
    device_work = false;
    Serial.println("Device work: " + String(device_work));
    Serial.print("Switch device off");
  }
}

// функция звуковой индикации
void sound_signal(int piezo_contact) {

  // включить пьезодинамик
  digitalWrite(piezo_contact, HIGH);
  delay(1000);
  digitalWrite(piezo_contact, LOW);
}

// функция вывода информации
void display_handler() {
  lcd.clear();

  // включен реим вывода информации о всех параметрах
  if (mode == OUTPUT_ALL) {
    
    // вывод информации температуре
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    //lcd.setCursor(0, 1);
    lcd.print(temperature);

    // ввод информации о наличии газов
    if (has_gas == true) {
      lcd.setCursor(2, 1);
      lcd.print("GAS");
      has_gas = false;
    }  

    // вывод информацции о наличии пламени
    if (has_fire == true) {
      lcd.setCursor(10, 1);
      lcd.print("FIRE");
      has_fire = false;
    }
  }

  // включен режим вывода информаии о температуре
  else if (mode == OUTPUT_TEMPERATURE) {
    lcd.setCursor(0, 0);
    lcd.print("Temperature:");
    lcd.setCursor(0, 1);
    lcd.print(temperature);
  }

  // включени режим вывода информации о наличии газов 
  else if (mode == OUTPUT_GAS) {
    if (has_gas == true) {
      lcd.setCursor(6, 0);
      lcd.print("GAS");
      has_gas = false;
    }
    else {
      lcd.setCursor(5, 0);
      lcd.print("NO GAS");
    }
  }

  // включен режим вывода информации о наличии пламени
  else if (mode == OUTPUT_FIRE) {
    if (has_fire == true) {
      lcd.setCursor(6, 0);
      lcd.print("FIRE");
      has_fire = false;
    }
    else {
      lcd.setCursor(5, 0);
      lcd.print("NO FIRE");
    }
  }
}