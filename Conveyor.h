#include "Arduino.h"

class Conveyor
{
public:
	void Spawn(int startPoint, int endPoint, char dir);
	void Kill();
	bool Alive();
	bool contains(int position);
	void Animate(Display &leds);
	int _startPoint;
	int _endPoint;
	char _dir;
private:
	bool _alive;
};

void Conveyor::Spawn(int startPoint, int endPoint, char dir) {
	_startPoint = startPoint;
	_endPoint = endPoint;
	_dir = dir;
	_alive = true;
}

void Conveyor::Kill() {
	_alive = false;
}

bool Conveyor::Alive() {
	return _alive;
}

bool Conveyor::contains(int position) {
	return position > _startPoint && position < _endPoint;
}

void Conveyor::Animate(Display &leds) {
	int b, n, ss, ee, led;
	long m = 10000 + millis();
	ss = leds.mapPos(_startPoint);
	ee = leds.mapPos(_endPoint);
	for (led = ss; led < ee; led++) {
		b = 5;
		n = (-led + (m / 100)) % 5;
		if (_dir == -1) n = (led + (m / 100)) % 5;
		b = (5 - n) / 2.0;
		if (b > 0) leds.setRGB(led, 0, 0, b);
	}
}
