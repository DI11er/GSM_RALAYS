#include "relay_manager.h" // влючаем заголовок обязательно

// реализация методов
RelayManager::RelayManager(byte pin, bool state, bool flag_inverted) {
  _pin = pin;
  
  _flag_inverted = flag_inverted;

  pinMode(_pin, OUTPUT);

  if (flag_inverted) {state = !state;}

  digitalWrite(_pin, state);
} // конструктор

void RelayManager::toggle() { 
  _state = !_state; 
  digitalWrite(_pin, _state);
}

void RelayManager::set_state(bool state) {
  _state = state;

  if (_flag_inverted) {_state = !_state;}

  digitalWrite(_pin, _state);
}

void RelayManager::restart() {
  _state = false;

  if (_flag_inverted) {_state = !_state;}

  digitalWrite(_pin, _state);
  _state = true;

  if (_flag_inverted) {_state = !_state;}

  delay(10000);
  digitalWrite(_pin, _state);  
}

bool RelayManager::get_state() { 
  if (_flag_inverted) {return !digitalRead(_pin);}
  return digitalRead(_pin); 
}