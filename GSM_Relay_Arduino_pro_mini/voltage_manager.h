#pragma once
#include <Arduino.h>

// описание класса
class VoltageManager {  // класс VoltageManager
  public:
    // список членов, доступных в программе
    VoltageManager(byte pin, long r1, long r2, float ratio); // конструктор
    float get_voltage();  // метод для получения напряжения
  private:
    // список членов для использования внутри класса
    byte _pin;  // номер пина
    long _r1;  // верхнее плечо делителя
    long _r2;  // нижнее плечо делителя
    float _voltage; // переменная для хранения напряжения
    int _number_measurements = 60;  // количество выборок
    float _VREF = 1.1;  // опорное напряжение
    float _ratio; // коэффициент коррекции измеряемого напряжения
};