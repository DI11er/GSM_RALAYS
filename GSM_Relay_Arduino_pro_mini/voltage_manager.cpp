#include "voltage_manager.h"

// реализация методов
VoltageManager::VoltageManager(byte pin, long r1, long r2, float ratio) {
  _pin = pin;
  
  _r1 = r1;
  _r2 = r2;

  _ratio = ratio;

  pinMode(_pin, INPUT);
} // конструктор

float VoltageManager::get_voltage() {
  _voltage = 0.0;

  for (int i = 0; i < _number_measurements; i++) {
    _voltage += (float)analogRead(_pin) * _VREF * ((_r1 + _r2) / _r2) / 1024;
    delay(10);
  }

  _voltage /= _number_measurements;

  if (_voltage < 0.25) { _voltage = 0; }
  else if (_voltage > 1) { _voltage *= _ratio; } 

  return _voltage;
}