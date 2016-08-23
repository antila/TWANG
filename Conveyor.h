#include "Arduino.h"

class Conveyor
{
  public:
      void Spawn(int startPoint, int endPoint, char dir);
      void Kill();
      bool Alive();
      int _startPoint;
      int _endPoint;
      char _dir;
  private:
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

bool Conveyor::Alive(){
    return _alive;
}
