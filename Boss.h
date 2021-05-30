#include "Arduino.h"

class Boss
{
  public:
    void Spawn();
    void Hit();
    void Kill();
    bool Alive();
    int _pos;
    byte _lives;
  private:
    bool _alive;
};

void Boss::Spawn() {
    _pos = 800;
    _lives = 3;
    _alive = true;
}

void Boss::Hit() {
    _lives --;
    if(_lives == 0) {
        Kill();
        return;
    }
    if(_lives == 2) {
        _pos = 200;
    } else if(_lives == 1) {
        _pos = 600;
    }
}

bool Boss::Alive() {
    return _alive;
}

void Boss::Kill() {
    _alive = false;
}
