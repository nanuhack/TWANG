#include "Arduino.h"

class Conveyor
{
  public:
      void Spawn(int startPoint, int endPoint, char dir);
      void Kill();
      int _startPoint;
      int _endPoint;
      char _dir;
      bool _alive;
};

void Conveyor::Spawn(int startPoint, int endPoint, char dir){
    _startPoint = startPoint;
    _endPoint = endPoint;
    _dir = dir;
    _alive = true;
}

void Conveyor::Kill(){
    _alive = false;
}
