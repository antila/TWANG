#include "Arduino.h"

class Lava
{
public:
	void Spawn(int left, int right, int ontime, int offtime, int offset, bool isOn);
	void Kill();
	bool Alive();
	bool contains(int pos);
	int _left;
	int _right;
	int _ontime;
	int _offtime;
	int _offset;
	long _lastOn;
	bool _isOn;
private:
	bool _alive;
};

void Lava::Spawn(int left, int right, int ontime, int offtime, int offset, bool isOn) {
	_left = left;
	_right = right;
	_ontime = ontime;
	_offtime = offtime;
	_offset = offset;
	_alive = true;
	_lastOn = millis() - offset;
	_isOn = isOn;
}

void Lava::Kill() {
	_alive = false;
}

bool Lava::Alive() {
	return _alive;
}

bool Lava::contains(int pos) {
	return Alive() && _isOn && _left < pos && _right > pos;
}