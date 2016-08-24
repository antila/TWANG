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
	void setRGB(int i, const CRGB& rgb);
	void setRGB(int i, uint8_t r, uint8_t g, uint8_t b);
	void addRGB(int i, uint8_t r, uint8_t g, uint8_t b);
	void setHSV(int i, uint8_t h, uint8_t s, uint8_t v);
	void nscale8(int i, uint8_t amount);
	int mapPos(int rel);
	const int numLeds = NUM_LEDS;
private:
	int mapEnd = 0;
	CRGB leds[NUM_LEDS];
};

void Display::init(int reserved) {
	mapEnd = NUM_LEDS - reserved - 1;
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

void Display::setRGB(int i, const CRGB& rgb) {
	leds[i] = rgb;
}

void Display::setRGB(int i, uint8_t r, uint8_t g, uint8_t b) {
	leds[i].setRGB(r, g, b);
}

void Display::addRGB(int i, uint8_t r, uint8_t g, uint8_t b) {
	leds[i] += CRGB(r, g, b);
}

void Display::setHSV(int i, uint8_t h, uint8_t s, uint8_t v) {
	leds[i].setHSV(h, s, v);
}

void Display::nscale8(int i, uint8_t amount) {
	leds[i].nscale8(amount);
}

int Display::mapPos(int pos) {
	return constrain((int)map(pos, 0, 1000, 0, mapEnd), 0, mapEnd);
}
