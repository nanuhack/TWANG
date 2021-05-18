#include "Arduino.h"

class Particle
{
  public:
    void Spawn(int pos);
    void Tick(int USE_GRAVITY, int BEND_POINT);
    void Kill();
    bool Alive();
    int _pos;
    int _power;
  private:
    int _life;
    bool _alive;
    char _sp;
};

void Particle::Spawn(int pos){
    _pos = pos;
    _sp = random(-120, 120);
    _power = 255;
    _alive = true;
    _life = 140 - abs(_sp);
}

void Particle::Tick(int USE_GRAVITY, int BEND_POINT){
    if(_alive){
        _life ++;
        if(_sp > 0){
            _sp -= _life/10;
        } else{
            _sp += _life/10;
        }
        
        if(USE_GRAVITY && _pos > BEND_POINT) _sp -= 10;
        
        _power = 130 - _life;
        
        if(_power <= 10){
            Kill(); 
        } else{
            _pos += _sp/7.0;
            if(_pos > 1000){
                _pos = 1000;
                _sp = 0-(_sp/2);
            } else if(_pos < 0){
                _pos = 0;
                _sp = 0-(_sp/2);
            }
        }
    }
}

bool Particle::Alive(){
    return _alive;
}

void Particle::Kill(){
    _alive = false;
}
