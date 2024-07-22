#pragma once
#include <Arduino.h>

// описание класса
class SensorManager {
  public:
    // список членов, доступных в программе
    SensorManager(byte pin_sensor, byte pin_power_sensor, bool state=false, bool flag_inverted=false); // конструктор
    void toggle(); // метод для переключения состояния сенсора
    void set_state(bool state); // метод для установки состояния сенсора
    bool get_state(); // метод для получения состояния сенсорп
    bool get_value(); // метод для получения значения сенсера
    int get_pin(); // метод для получения номера пина
  private:
    // список членов для использования внутри класса
    byte _pin_sensor;  // номер пина
    byte _pin_power_sensor;  // номер пина
    bool _state; // состояние сенсора
    bool _sensor_flag = true; // флаг сенсора
    bool _flag_inverted;
};