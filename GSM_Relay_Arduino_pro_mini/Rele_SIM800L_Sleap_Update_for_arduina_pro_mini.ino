/*   
  arduino       SIM800L
    D2           RING
    A2           Relay 1
    A3           Relay 2
    D6           DTR
    D9           LED
    A7           Sensor 1
    D10          Sensor 1 VCC
    A6           Sensor 2
    D11          Sensor 2 VCC
    D12          RX
    D13          TX
    A1           Voltage_sensor 1
    A0           Voltage_sensor 2
*/

#include <SoftwareSerial.h>                                           // Библиотека для эмуляции на цифровом пине Serial
#include <GyverPower.h>

#include "relay_manager.h"
#include "voltage_manager.h"
#include "sensor_manager.h"

// Раздефайнить или задефайнить для использования
#define DEBUG_ENABLE

#ifdef DEBUG_ENABLE
#define DEBUG_PRINT(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#endif

RelayManager R1(A2, true);                                            // Экземпляр реле 1
RelayManager R2(A3, true);                                            // Экземпляр реле 2

SensorManager S1(A7, 10);                                             // Экземпляр сенсора 1
SensorManager S2(A6, 11);                                             // Экземпляр сенсора 2

VoltageManager U1(A0, 2200000, 100000, 1.03526);                      // Экземпляр сенсора питания
VoltageManager U2(A1, 2200000, 100000, 1.03526);                      // Экземпляр сенсора питания


// Настройка пинов
const int RING_PIN = 3;                                               // Подключаем к ring пину gsm модуля для обработки звонка
const int DTR_PIN = 4;                                                // Подкючение к dtr пину для управление сном модуля
const int TX = 13;                                                    // Подключение RX SIM800L
const int RX = 12;                                                    // Подключение TX SIM800L

#define BATTERY_1_VOLTAGE 3.5                                         // Минимальное напряжение для U1
#define BATTERY_2_VOLTAGE 3.5                                         // Минимальное напряжение для U2

uint32_t sleepTime = 3600000;

SoftwareSerial SIM800L(RX, TX);                                       // Выводы SIM800L Tx & Rx подключены к выводам Arduino 3 и 2

// Переменнй для работы с GSM модемом
int countTry = 0;                                                     // Количество неудачных попыток авторизации
String _response = "";                                                // Переменная для хранения ответов модуля
String result = "";                                                   // Переменная для хранения вводимых данных через DTMF
String _temp_phone = "";                                              // Переменная для временного хранения телефона звонящего
String _phone = "";                                                   // Переменная для хранения телефона звонящего
const String pass = "1923";                                           // Пароль для авторизации

// Флаги
bool load_1_flag = true;                                              // Флаг для единоразового предупреждения о низком заряде U1
bool load_2_flag = true;                                              // Флаг для единоразового предупреждения о низком заряде U2
bool flag_pre_auth = true;                                            // Флаг авторизации
bool callState = false;                                               // Флаг наличия звонка
volatile bool interruptFlag1 = false;
volatile bool interruptFlag2 = false;

uint32_t callTime;


void setup() {                                                        // Первоночальная настройка 
  power.autoCalibrate();
  // power.setSleepMode(STANDBY_SLEEP);                                  // По умолчанию POWERDOWN_SLEEP
  analogReference(INTERNAL);                                          // Задаем опорное напряжение. INTERNAL: опорное напряжение равно напряжению 1.1.
  
  pinMode(DTR_PIN, OUTPUT);                                           // Меняем режим работы пина на вывод сигнала
  pinMode(RING_PIN, INPUT);

  digitalWrite(RING_PIN, HIGH);
  digitalWrite(DTR_PIN, LOW);                                         // Пробуждаем GSM модуль

  #ifdef DEBUG_ENABLE
    Serial.begin(9600);                                               // Настройка скорости передачи Serial порта 
    while (!Serial) {}
    DEBUG_PRINT(F("[DEBUG SERIAL] -> Connect"));
  #endif
  
  init_gsm_model();

  delay(2000);

  // Включение прерываний на пинах D3, A6 и A7
  attachInterrupt(digitalPinToInterrupt(RING_PIN), wakeUp, FALLING);
  attachInterrupt(digitalPinToInterrupt(S1.get_pin()), sensor1_interrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(S2.get_pin()),sensor2_interrupt, FALLING);
}

void loop() {
  handler_gsm();                                                     // Обработчик ответов GSM
  handler_sensor();                                                  // Обработчик ответов от Сенсеров
  handler_voltage();                                                 // Обработчик таймеров
  EnterSleep();
}

// Отдельная функция для пробуждения gsm модуля
void wakeUp() {
  DEBUG_PRINT(F("WakeUp"));                                          // Проснулись
  power.wakeUp();
  digitalWrite(DTR_PIN, LOW);                                        // Пробуждаем GSM модуль
  delay(500);
}

// Обработчик срабатывания датчика 1
void sensor1_interrupt() {
  wakeUp();
  interruptFlag1 = true;
}

// Обработчик срабатывания датчика 2
void sensor2_interrupt() {
  wakeUp();
  interruptFlag2 = true;
}

// Отдельная функция для погружения arduina nano в сон
void EnterSleep() {
  digitalWrite(DTR_PIN, LOW);
  delay(300);
  power.sleepDelay(sleepTime);
  delay(300);
}

// Отдельная функция для настройки gsm модуля
void init_gsm_model() {
  SIM800L.begin(2400);                                                // Настройка скорости передачи Serial порта 
  SIM800L.setTimeout(100);

  while(!SIM800L) {}

  DEBUG_PRINT(F("[GSM DEBUG] -> Подключение GSM модуля SIM800L"));
  do {
    _response = sendATCommand(F("AT"), true);                         // Настраиваем скорость обмена
    _response.trim();                                                 // Убираем пробельные символы в начале и конце
    delay(1000);
  } while (_response != F("OK"));                                     // Не пускать дальше, пока модем не вернет ОК
  DEBUG_PRINT(F("[GSM DEBUG] -> Подключено!"));                       // Печатаем текст
  
  DEBUG_PRINT(F("[GSM DEBUG] -> Ожидание регистрации SIM-карты в сети...."));
  do {
    _response = sendATCommand(F("AT+CREG?"), true);                   // Проверка регистрации в сети
    _response.trim();                                                 // Убираем пробельные символы в начале и конце
    delay(1000);
  } while (_response.substring(_response.indexOf(F(",")) + 1, _response.length()).toInt() != 1);
  DEBUG_PRINT(F("[GSM DEBUG] -> SIM-карта прошла регистрацию."));
  
  DEBUG_PRINT(F("[GSM DEBUG] -> Настройка модуля."));

  do {
    _response = sendATCommand(F("AT+DDET=1,500,0"), true);            // Включение DTMF (тонального набора)
    _response.trim();                                                 // Убираем пробельные символы в начале и конце
  } while (_response != "OK");                                        // Не пускать дальше, пока модем не вернет ОК

  do {
    _response = sendATCommand(F("AT+CLIP=1"), true);                  // Включаем АОН
    _response.trim();                                                 // Убираем пробельные символы в начале и конце
  } while (_response != "OK");                                        // Не пускать дальше, пока модем не вернет ОК

  sendATCommand(F("AT+CMGF=1"), true);                                // Включение TextMode для SMS
  sendATCommand(F("AT+CSCLK=1"), true);                               // Включаем режим сна, когда на DTR пине +3.3 модуль спит, -3.3 пробуждается                                                                                                              
  
  DEBUG_PRINT(F("[GSM DEBUG] -> Настройка модуля завершена."));
}

// Отдельная функция для отправки комманд в GSM
String sendATCommand(String cmd, bool waiting) {
  String _resp = "";                                                  // Переменная для хранения результата
  DEBUG_PRINT("[GSM DEBUG sendATCommand SEND] -> " + cmd);            // Дублируем команду в монитор порта
  SIM800L.println(cmd);                                               // Отправляем команду модулю
  if (waiting) {                                                      // Если необходимо дождаться ответа...
    _resp = waitResponse();                                           // ... ждем, когда будет передан ответ
    // Если Echo Mode выключен (ATE0), то эти 3 строки можно закомментировать
    if (_resp.startsWith(cmd)) {  // Убираем из ответа дублирующуюся команду
      _resp = _resp.substring(_resp.indexOf("\r", cmd.length()) + 2);
    }
    _resp.trim();
    DEBUG_PRINT("[GSM DEBUG sendATCommand RESPONSE] -> " + _resp);   // Дублируем ответ в монитор порта
  }
  return _resp;                                                      // Возвращаем результат. Пусто, если проблема
}

// Отдельная функция для получения ответа от GSM
String waitResponse() {
  String _resp = "";                                                 // Переменная для хранения результата
  long _timeout = millis() + 10000;                                  // Переменная для отслеживания таймаута (10 секунд)
  while (!SIM800L.available() && millis() < _timeout)  {};           // Ждем ответа 10 секунд, если пришел ответ или наступил таймаут, то...
  if (SIM800L.available()) {                                         // Если есть, что считывать...
    _resp = SIM800L.readString();                                    // ... считываем и запоминаем
  }
  else {                                                             // Если пришел таймаут, то...
    DEBUG_PRINT((String)"[GSM DEBUG waitResponse RESPONSE] -> Timeout...");               // ... оповещаем об этом и...
  }
  _resp.trim();
  return _resp;                                                      // ... возвращаем результат. Пусто, если проблема
}

// Отдельная функция для обработки данных поступаемых от GSM
void handler_gsm() {                                                 // Обработчик ответов GSM
  do {
    if (SIM800L.available()) {                                       // Если модем, что-то отправил...
      _response = waitResponse();                                    // Получаем ответ от модема для анализа
      DEBUG_PRINT("[GSM DEBUG handler_gsm RESP] -> " + _response);   // Если нужно выводим в монитор порта

      int index = -1;
      do  {                                                          // Перебираем построчно каждый пришедший ответ
        index = _response.indexOf("\r\n");                           // Получаем идекс переноса строки
        String submsg = "";
        if (index > -1) {                                            // Если перенос строки есть, значит
          submsg = _response.substring(0, index);                    // Получаем первую строку
          _response = _response.substring(index + 2);                // И убираем её из пачки
        }
        else {                                                       // Если больше переносов нет
          submsg = _response;                                        // Последняя строка - это все, что осталось от пачки
          _response = "";                                            // Пачку обнуляем
        }
        submsg.trim();                                               // Убираем пробельные символы справа и слева
        if (submsg != "") {                                          // Если строка значимая (не пустая), то распознаем уже её
          DEBUG_PRINT("[GSM DEBUG handler_gsm] -> submessage: " + submsg);
          if (submsg.startsWith("+DTMF:")) {                         // Если ответ начинается с "+DTMF:" тогда:
            String symbol = submsg.substring(7, 8);                  // Выдергиваем символ с 7 позиции длиной 1 (по 8)
            processingDTMF(symbol);                                  // Логику выносим для удобства в отдельную функцию
          }

          if (submsg.startsWith("RING")) {                           // Есть входящий вызов
            ring();
          }
          if (submsg.startsWith(F("NO CARRIER")) ||
              submsg.startsWith(F("NO BUSY")) ||
              submsg.startsWith(F("NO DIALTONE")) ||
              submsg.startsWith(F("NO ANSWER"))) {                   // Завершение звонка
            callOut();
          }
        }
      } while (index > -1);                                          // Пока индекс переноса строки действителен
    }
  } while (callState && millis() - callTime <= 120000);              // Пока звонок активен и не превышает 2 минут
  callOut();                                                
}

// Отдельная функция для принятия вызова
void ring() {
  int phoneindex = _response.indexOf("+CLIP: \"");                  // Есть ли информация об определении номера, если да, то phoneindex>-1
  String innerPhone = "";                                           // Переменная для хранения определенного номера
  if (phoneindex >= 0) {                                            // Если информация была найдена
    phoneindex += 8;                                                // Парсим строку и ...
    innerPhone = _response.substring(
      phoneindex, 
      _response.indexOf("\"", phoneindex));                         // ...получаем номер
    DEBUG_PRINT("[GSM DEBUG handler_gsm] -> Number: " + innerPhone);  // Выводим номер в монитор порта
  }
                                                            
  if (innerPhone.length() >= 7) {                                   // Проверяем, чтобы длина номера была больше 6 цифр, и номер должен быть в списке
    _temp_phone = innerPhone;
    sendATCommand(F("ATA"), true);                                  // Если да, то отвечаем на вызов

    delay(2000);

    if (_phone != innerPhone && _phone) {
      DEBUG_PRINT("[GSM DEBUG handler_gsm] -> new_phone");
      flag_pre_auth == true;
      sendATCommand(F("AT+VTS=\"1,4,3\""), true);
    }
    
    countTry = 0;
    callState = true;
    callTime = millis();
  }
  else {
    callOut();
  }
}

// Отдельная функция для завершения вызова
void callOut() {
  _temp_phone = "";
  sendATCommand(F("ATH"), true); 
  callState = false;
  delay(1000);
}

// Отдельная функция для обработки логики DTMF
void processingDTMF(String symbol) {
  DEBUG_PRINT("[GSM DEBUG processingDTMF] -> Key: " + symbol);                                     // Выводим в Serial для контроля, что ничего не потерялось
  if (!flag_pre_auth) {
    if (symbol == "#") {
      bool correct;                                                  // Для оптимизации кода, переменная корректности команды
      correct = handler_command(result);
      if (!correct) DEBUG_PRINT("[GSM DEBUG processingDTMF] -> Incorrect command: " + result);     // Если команда некорректна, выводим сообщение
      result = "";                                                   // После каждой решетки сбрасываем вводимую комбинацию
    }
    else {
      result += symbol;                                              // Если нет, добавляем в конец
    }
  }
  else {
   auth(symbol);
  }
}

// Отдельная функция для обработки комманд DTMF
bool handler_command(String command) {
  // Контроль реле
  // Включение реле 1
  if (command == F("111")) {
    sendATCommand(F("AT+VTS=\"1,4\""), true); 
    DEBUG_PRINT(F("Реле 1 включено"));
    R1.set_state(true);
    return true;
  }
  // Выключение реле 1
  if (command == F("110")) {
    sendATCommand(F("AT+VTS=\"1,4\""), true); 
    DEBUG_PRINT(F("Реле 1 выключено"));
    R1.set_state(false);
    return true;
  }
  // Включение реле 2
  if (command == F("121")) {
    sendATCommand(F("AT+VTS=\"1,4\""), true); 
    DEBUG_PRINT(F("Реле 2 включено"));
    R2.set_state(true);
    return true;
  }
  // Выключение реле 2
  if (command == F("120")) {
    sendATCommand(F("AT+VTS=\"1,4\""), true); 
    DEBUG_PRINT(F("Реле 2 выключено"));
    R2.set_state(false);
    return true;
  }
  // Выключить реле 1 и 2
  if (command == F("130")) {
    sendATCommand(F("AT+VTS=\"1,4\""), true); 
    DEBUG_PRINT(F("Реле 1 и Реле 2 выключены"));
    R1.set_state(false);
    R2.set_state(false);
    return true;
  }
  // Включить реле 1 и 2
  if (command == F("131")) {
    sendATCommand(F("AT+VTS=\"1,4\""), true); 
    DEBUG_PRINT(F("Реле 1 и Реле 2 включены"));
    R1.set_state(true);
    R2.set_state(true);
    return true;
  }
  // Перезагрузка реле 1 и 2
  if (command == F("132")) {
    sendATCommand(F("AT+VTS=\"1,4\""), true); 
    DEBUG_PRINT(F("Реле 1 и Реле 2 перезагружены"));
    R1.restart();
    R2.restart();
    return true;
  }
  // Контроль напряжения
  // Замер напряжения U1
  if (command == F("21")) {
    sendATCommand(F("AT+VTS=\"1,4\""), true); 
    DEBUG_PRINT(F("Напряжения U1"));
    float voltage = 0.5;
    voltage = U1.get_voltage();

    DEBUG_PRINT((String)"U1\nvoltage: " + voltage + "v");
    sendSMS(_phone, (String)"U1\nvoltage: " + voltage + "v");
  
    return true;
  }
  // Замер напряжения U2
  if (command == F("22")) {
    sendATCommand(F("AT+VTS=\"1,4\""), true); 
    DEBUG_PRINT(F("Напряжения U2"));
    float voltage = 0.5;
    voltage = U2.get_voltage();

    DEBUG_PRINT((String)"U2\nvoltage: " + voltage + "v");
    sendSMS(_phone, (String)"U2\nvoltage: " + voltage + "v");
  
    return true;
  }
  // Контроль сенсеров
  // Включение сенсера 1
  if (command == F("311")) {
    sendATCommand(F("AT+VTS=\"1,4\""), true); 
    DEBUG_PRINT(F("Датчик 1 включен"));
    S1.set_state(true);

    return true;
  }
  // Выключение сенсера 1
  if (command == F("310")) {
    sendATCommand(F("AT+VTS=\"1,4\""), true); 
    DEBUG_PRINT(F("Датчик 1 выключен"));
    S1.set_state(false);

    return true;
  }
  // Включение сенсера 2
  if (command == F("321")) {
    sendATCommand(F("AT+VTS=\"1,4\""), true); 
    DEBUG_PRINT(F("Датчик 2 включен"));
    S2.set_state(true);
         
    return true;
  }
  // Выключение сенсера 2
  if (command == F("320")) {
    sendATCommand(F("AT+VTS=\"1,4\""), true); 
    DEBUG_PRINT(F("Датчик 2 выключен"));
    S2.set_state(false);
         
    return true;
  }
  // Включение сенсера 1 и 2
  if (command == F("331")) {
    sendATCommand(F("AT+VTS=\"1,4\""), true); 
    DEBUG_PRINT(F("Сигнализация включена"));
    S1.set_state(true);
    S2.set_state(true);

    return true;
  }
  // Выключение сенсера 1 и 2
  if (command == F("330")) {
    sendATCommand(F("AT+VTS=\"1,4\""), true); 
    DEBUG_PRINT(F("Сигнализация выключена"));
    S1.set_state(false);
    S2.set_state(false);

    return true;
  }
  
  return false;
}

// Отдельная функция для обработки авторизации
void auth(String symbol) {
  if (countTry < 3) {                                                // Если 3 неудачных попытки, перестаем реагировать на нажатия
    DEBUG_PRINT((String)"[GSM DEBUG AUTH]" + result);
    if (symbol == "#") {
      bool correct = false;  
      if (result == pass) {                                          // Введенная строка совпадает с заданным паролем
        DEBUG_PRINT("[GSM DEBUG AUTH] -> The correct password is entered: " + result);   // Информируем о корректном вводе пароля
        countTry = 0;                                                // Обнуляем счетчик неудачных попыток ввода
        flag_pre_auth = false;                                                
        _phone = _temp_phone;
        _temp_phone = "";
        sendATCommand(F("AT+VTS=\"1,4\""), true);   
      }
      else {
        countTry += 1;                                               // Увеличиваем счетчик неудачных попыток на 1
        DEBUG_PRINT(F("[GSM DEBUG AUTH] -> Incorrect password"));                        // Неверный пароль
        DEBUG_PRINT("[GSM DEBUG AUTH] -> Counter:" + (String)countTry);                  // Количество неверных попыток
      }
      result = "";                                                   // После каждой решетки сбрасываем вводимую комбинацию 
    }
    else {
      result += symbol;
    }
  }
  else {
    callOut();                                                        // Если количество неудачных папыток привысило 3, то отклоняем вызов
  }
}

// Отдельная функция для отправки sms
void sendSMS(String phone, String message) {
  if (phone != "") {                                                 // Проверка номера на пустату
    delay(5000);
    sendATCommand("AT+CMGS=\"" + phone + "\"", true);                // Переходим в режим ввода текстового сообщения
    sendATCommand(message + "\r\n" + (String)((char)26), true);      // После текста отправляем перенос строки и Ctrl+Z
  }
  else {
    DEBUG_PRINT(F("[GSM DEBUG sendSMS] -> Номер пустой"));                                  // Номер пустой
  }
}

// Отдельная функция для обработки таймеров
void handler_voltage() {
  float voltage = 0.3;

  voltage = U1.get_voltage();
      
  if (1 < voltage && voltage < BATTERY_1_VOLTAGE && load_1_flag != false) {    // Сравниваем напряжение акб с заданным минимальным значением
    load_1_flag = !load_1_flag;
    DEBUG_PRINT((String)"[GSM DEBUG handler_voltage] -> U1_voltage: " + voltage + "v");
    sendSMS(_phone, (String)"U1_voltage: " + voltage + "v");                                     
  }

  voltage = U2.get_voltage();
      
  if (1 < voltage && voltage < BATTERY_2_VOLTAGE && load_2_flag != false) {    // Сравниваем напряжение акб с заданным минимальным значением
    load_2_flag = !load_2_flag;
    DEBUG_PRINT((String)"[GSM DEBUG handler_voltage] -> U2_voltage: " + voltage + "v");
    sendSMS(_phone, (String)"U2_voltage: " + voltage + "v");                                     
  }
}

// Отдельная функция для обработки сенсеров
void handler_sensor() {
  if (interruptFlag1) {
    interruptFlag1 = !interruptFlag1;
    DEBUG_PRINT("[GSM DEBUG handler_sensor] -> Door open!!!");
    sendSMS(_phone, F("Door open!!!"));
    sendATCommand((String)"ATD+" + _phone + ";", true);
    return;
  }

  if (interruptFlag2) {
    interruptFlag2 = !interruptFlag2;
    DEBUG_PRINT("[GSM DEBUG handler_sensor] -> Movement!!!");
    sendSMS(_phone, F("Movement!!!"));
    sendATCommand((String)"ATD+" + _phone + ";", true);
    return;
  }
}










