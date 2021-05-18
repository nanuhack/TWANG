#include "Arduino.h"

class Lava
{
  public:
    void Spawn(int left, int right, int ontime, int offtime, int offset, bool state);
    void Kill();
    int Alive();
    int _left;
    int _right;
    int _ontime;
    int _offtime;
    int _offset;
    long _lastOn;
    bool _state;
  private:
    bool _alive;
};

void Lava::Spawn(int left, int right, int ontime, int offtime, int offset, bool state){
    _left = left;
    _right = right;
    _ontime = ontime;
    _offtime = offtime;
    _offset = offset;
    _alive = true;
    _lastOn = millis()-offset;
    _state = state;
}

void Lava::Kill(){
    _alive = false;
}

int Lava::Alive(){
    return _alive;
}
