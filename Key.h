#include "Arduino.h"

class Key
{
  public:
    void Spawn(int pos);
    void Collect();
    void Kill();
    bool isCollected();
    bool isAlive();
    int _pos = 0;
  private:
    bool _collected = false;
    bool _alive = false;
};

void Key::Spawn(int pos) {
    _pos = pos;
    _alive = true;
}

bool Key::isCollected() {
    return _collected;
}

bool Key::isAlive() {
    return _alive;
}

void Key::Collect() {
    _collected = true;
}

void Key::Kill() {
    _alive = false;
    _collected = false;
}
