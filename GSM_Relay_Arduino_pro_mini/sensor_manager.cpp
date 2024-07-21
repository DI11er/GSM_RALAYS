#include "Arduino.h"
#include "sensor_manager.h"  // подключаем заголовок обязательно

// реализация методов
SensorManager::SensorManager(byte pin_sensor, byte pin_power_sensor, bool state, bool flag_inverted) {
  _pin_sensor = pin_sensor;
  _pin_power_sensor = pin_power_sensor;

  _state = state;

  _flag_inverted = flag_inverted;

  pinMode(_pin_sensor, INPUT);
  pinMode(_pin_power_sensor, OUTPUT);

  digitalWrite(_pin_sensor, HIGH);
} // конструктор

void SensorManager::toggle() {
  _state = !_state;
  digitalWrite(_pin_power_sensor, _state); 
}

void SensorManager::set_state(bool state) {
  _state = state;
  digitalWrite(_pin_power_sensor, _state); 
}

bool SensorManager::get_state() { return _state; }

bool SensorManager::get_value() {
  if (_flag_inverted) { return !digitalRead(_pin_sensor); } 
  else { return digitalRead(_pin_sensor); }
}