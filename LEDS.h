#include "Arduino.h"
#include "FastLED.h"

#define NUM_LEDS                   287
#define DATA_PIN                   3
#define LED_TYPE                   WS2812B
#define LED_COLOR_ORDER            GRB
#define BRIGHTNESS                 150

class Display
{
public:
	void init(int reserved);
	void clear();
	void show();
	void setRGB(int i, CRGB rgb);
	void setRGB(int i, byte r, byte g, byte b);
	void addRGB(int i, byte r, byte g, byte b);
	void setHSV(int i, byte h, byte s, byte v);
	void nscale8(int i, int amount);
	int mapPos(int rel);
	const int numLeds = NUM_LEDS;
private:
	int reserved = 0;
	CRGB leds[NUM_LEDS];
};

void Display::init(int reserved) {
	this->reserved = reserved;
	// Fast LED
	FastLED.addLeds<LED_TYPE, DATA_PIN, LED_COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
	FastLED.setBrightness(BRIGHTNESS);
	FastLED.setDither(1);
}

void Display::clear() {
	FastLED.clear();
}

void Display::show() {
	FastLED.show();
}

void Display::setRGB(int i, CRGB rgb) {
	leds[i] = rgb;
}

void Display::setRGB(int i, byte r, byte g, byte b) {
	leds[i].setRGB(r, g, b);
}

void Display::addRGB(int i, byte r, byte g, byte b) {
	leds[i] += CRGB(r, g, b);
}

void Display::setHSV(int i, byte h, byte s, byte v) {
	leds[i].setHSV(h, s, v);
}

void Display::nscale8(int i, int amount) {
	leds[i].nscale8(amount);
}

int Display::mapPos(int pos) {
	constrain((int)map(pos, 0, 1000, 0, NUM_LEDS - reserved - 1), 0, NUM_LEDS - reserved - 1);
}
