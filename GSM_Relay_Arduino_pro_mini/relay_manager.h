#pragma once
#include <Arduino.h>

// описание класса
class RelayManager {  // класс RelayManager
  public:
    // список членов, доступных в программе
    RelayManager(byte pin, bool state=true, bool flag_inverted=false); // конструктор
    void toggle(); // метод для переключения состояния реле
    void set_state(bool state); // метод для установки состояния реле
    void restart();
    bool get_state(); // метод для проверки состояния реле
  private:
    // список членов для использования внутри класса
    byte _pin;  // номер пина
    bool _state; // состояние реле
    bool _flag_inverted; // флаг для инвертирования сигнала
};