#include "Arduino.h"

class Spawner
{
  public:
    void Spawn(int pos, int rate, char sp, char dir, long activate);
    void Kill();
    bool Alive();
    int _pos;
    int _rate;
    char _sp;
    int _dir;
    long _lastSpawned;
    long _activate;
  private:
    bool _alive;
};

void Spawner::Spawn(int pos, int rate, char sp, char dir, long activate) {
    _pos = pos;
    _rate = rate;
    _sp = sp;
    _dir = dir;
    _activate = millis()+activate;
    _alive = true;
}

void Spawner::Kill() {
    _alive = false;
    _lastSpawned = 0;
}

bool Spawner::Alive() {
    return _alive;
}
