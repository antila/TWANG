// Required libs
//#include <EEPROM.h>
//#include <LiquidCrystal.h>
#include "FastLED.h"
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"
#include "toneAC.h"
#include "RunningMedian.h"

// Included libs
#include "Enemy.h"
#include "Particle.h"
#include "Spawner.h"
#include "Lava.h"
#include "Boss.h"
#include "Conveyor.h"

// MPU
MPU6050 accelgyro;
int16_t ax, ay, az;
int16_t gx, gy, gz;

// toneAC - Speaker
// ATmega328 / ATmega128, Uno:      Pin 9, 10
// ATmega2560 / ATmega1280:         Pin 11, 12
// ATmega1284P / ATmega644:         Pin 12, 13

// LED setup
#define NUM_LEDS                   287
#define DATA_PIN                   3
#define LED_TYPE                   WS2812B
#define LED_COLOR_ORDER            GRB
#define BRIGHTNESS                 250
#define DIRECTION                  1         // 0 = right to left, 1 = left to right
#define MIN_REDRAW_INTERVAL        20        // Min redraw interval (ms) 33 = 30fps / 16 = 63fps
#define USE_GRAVITY                0         // 0/1 use gravity (LED strip going up wall)
#define BEND_POINT                 550       // 0/1000 point at which the LED strip goes up the wall
#define MAX_LIVES                  3

// GAME
enum Stage { SCREENSAVER, PLAY, COMPLETE, DEAD, WIN, GAMEOVER };

long previousMillis = 0;                   // Time of the last redraw
int levelNumber =                      0;
long lastInputTime = 0;
#define TIMEOUT                        60000
#define LEVEL_COUNT                    14
#define MAX_VOLUME                     10

// JOYSTICK
#define JOYSTICK_ORIENTATION     1         // 0, 1 or 2 to set the angle of the joystick
#define JOYSTICK_DIRECTION       1         // 0/1 to flip joystick direction
#define ATTACK_THRESHOLD         24000     // The threshold that triggers an attack
#define JOYSTICK_DEADZONE        20        // Angle to ignore
int joystickTilt = 0;                      // Stores the angle of the joystick
int joystickWobble = 0;                    // Stores the max amount of acceleration (wobble)

// LCD and stats
//LiquidCrystal lcd(52, 53, 50, 51, 48, 49);
//byte stats_playthroughs = 0;
//int stats_besttime = 0;
//int lastPlaythrough = 0;
//long gameStartTime = 0;

// WOBBLE ATTACK
#define ATTACK_WIDTH             70         // Width of the wobble attack, world is 1000 wide
#define ATTACK_DURATION          500        // Duration of a wobble attack (ms)
long attackMillis = 0;                      // Time the attack started
bool attacking = 0;                         // Is the attack in progress?
#define BOSS_WIDTH               40

// PLAYER
#define MAX_PLAYER_SPEED         12         // Max move speed of the player
Stage stage;                                // what stage the game is at (PLAY/DEAD/WIN/GAMEOVER)
long stageStartTime;                        // Stores the time the stage changed for stages that are time based
int playerPosition;                         // Stores the player position
int playerPositionModifier;                 // +/- adjustment to player position
bool playerAlive;
long killTime;
int lives = MAX_LIVES;
int moveAmount = 0;


// POOLS
int const enemyCount = 10;
int const particleCount = 40;
int const spawnCount = 2;
int const lavaCount = 5;
int const conveyorCount = 2;

Enemy enemyPool[enemyCount];
Particle particlePool[particleCount];
Spawner spawnPool[spawnCount];
Lava lavaPool[lavaCount];
Conveyor conveyorPool[conveyorCount];
Boss boss = Boss();

CRGB leds[NUM_LEDS];
RunningMedian MPUAngleSamples = RunningMedian(5);
RunningMedian MPUWobbleSamples = RunningMedian(5);

void setup() {
	//Serial.begin(9600);
	//while (!Serial);

	//stats_playthroughs = EEPROM.read(0);
	//stats_besttime = EEPROM_readint(1);

	// MPU
	Wire.begin();
	accelgyro.initialize();

	// Fast LED
	FastLED.addLeds<LED_TYPE, DATA_PIN, LED_COLOR_ORDER>(leds, NUM_LEDS);//.setCorrection(TypicalLEDStrip);
	FastLED.setBrightness(BRIGHTNESS);
	FastLED.setDither(0);

	//lcd.begin(20, 4);
	//updateStats();

	// Life LEDs
	loadLevel();
}

void loop() {
	long mm = millis();
	int brightness = 0;

	if (stage == PLAY) {
		if (attacking) {
			SFXattacking();
		}
		else {
			SFXtilt(joystickTilt);
		}
	}
	else if (stage == DEAD) {
		SFXdead();
	}

	if (mm - previousMillis >= MIN_REDRAW_INTERVAL) {
		getInput();
		long frameTimer = mm;
		previousMillis = mm;

		if (abs(joystickTilt) > JOYSTICK_DEADZONE) {
			lastInputTime = mm;
			if (stage == SCREENSAVER) {
				levelNumber = -1;
				stageStartTime = mm;
				stage = WIN;
			}
		}
		else {
			if (lastInputTime + TIMEOUT < mm) {
				stage = SCREENSAVER;
			}
		}

		switch (stage) {
		case SCREENSAVER:
			screenSaverTick();
			break;
		case PLAY:
			// PLAYING
			if (attacking && attackMillis + ATTACK_DURATION < mm) attacking = 0;

			// If not attacking, check if they should be
			if (!attacking && joystickWobble > ATTACK_THRESHOLD) {
				attackMillis = mm;
				attacking = 1;
			}

			moveAmount = (joystickTilt / 6.0);
			if (DIRECTION) moveAmount = -moveAmount;
			moveAmount = constrain(moveAmount, -MAX_PLAYER_SPEED, MAX_PLAYER_SPEED);

			// If still not attacking, move!
			playerPosition += playerPositionModifier;
			if (!attacking) {
				playerPosition -= moveAmount;
				if (playerPosition < 0) playerPosition = 0;
				if (playerPosition >= 1000 && !boss.Alive()) {
					// Reached exit!
					levelComplete();
					return;
				}
			}

			if (inLava(playerPosition)) {
				die();
			}

			// Ticks and draw calls
			FastLED.clear();
			tickConveyors();
			tickSpawners();
			tickBoss();
			tickLava();
			tickEnemies();
			drawPlayer();
			drawAttack();
			drawExit();
			drawLives();
			break;
		case DEAD:
			// DEAD
			FastLED.clear();
			if (!tickParticles()) {
				loadLevel();
			}
			break;
		case WIN:
			// LEVEL COMPLETE
			FastLED.clear();
			if (stageStartTime + 500 > mm) {
				int n = max(map(((mm - stageStartTime)), 0, 500, NUM_LEDS, 0), 0);
				for (int i = NUM_LEDS; i >= n; i--) {
					brightness = 255;
					leds[i] = CRGB(0, brightness, 0);
				}
				SFXwin();
			}
			else if (stageStartTime + 1000 > mm) {
				int n = max(map(((mm - stageStartTime)), 500, 1000, NUM_LEDS, 0), 0);
				for (int i = 0; i < n; ++i) {
					brightness = 255;
					leds[i] = CRGB(0, brightness, 0);
				}
				SFXwin();
			}
			else if (stageStartTime + 1200 > mm) {
				leds[0] = CRGB(0, 255, 0);
			}
			else {
				nextLevel();
			}
			break;
		case COMPLETE:
			FastLED.clear();
			SFXcomplete();
			if (stageStartTime + 500 > mm) {
				int n = max(map(((mm - stageStartTime)), 0, 500, NUM_LEDS, 0), 0);
				for (int i = NUM_LEDS; i >= n; i--) {
					brightness = (sin(((i * 10) + mm) / 500.0) + 1) * 255;
					leds[i].setHSV(brightness, 255, 50);
				}
			}
			else if (stageStartTime + 5000 > mm) {
				for (int i = NUM_LEDS; i >= 0; i--) {
					brightness = (sin(((i * 10) + mm) / 500.0) + 1) * 255;
					leds[i].setHSV(brightness, 255, 50);
				}
			}
			else if (stageStartTime + 5500 > mm) {
				int n = max(map(((mm - stageStartTime)), 5000, 5500, NUM_LEDS, 0), 0);
				for (int i = 0; i < n; ++i) {
					brightness = (sin(((i * 10) + mm) / 500.0) + 1) * 255;
					leds[i].setHSV(brightness, 255, 50);
				}
			}
			else {
				nextLevel();
			}
			break;
		case GAMEOVER:
			// GAME OVER!
			FastLED.clear();
			stageStartTime = 0;
			break;
		}

		//Serial.print(millis() - mm);
		//Serial.print(" - ");
		FastLED.show();
		//Serial.println(millis() - mm);
	}
}


// ---------------------------------
// ------------ LEVELS -------------
// ---------------------------------
void loadLevel() {
	cleanupLevel();
	playerPosition = 0;
	playerAlive = 1;
	switch (levelNumber) {
	case 0:
    // Left or right?
		playerPosition = 200;
		spawnEnemy(1, 0, 0, 0);
		//gameStartTime = millis();
		//lastPlaythrough = 0;
		//updateStats();
		break;
	case 1:
		// Slow moving enemy
		spawnEnemy(900, 0, 1, 0);
		break;
	case 2:
		// Spawning enemies at exit every 2 seconds
		spawnPool[0].Spawn(1000, 3000, 2, 0, 0);
		break;
	case 3:
		// Lava intro
		spawnLava(400, 490, 2000, 2000, 0, false);
		spawnPool[0].Spawn(1000, 5500, 3, 0, 0);
		break;
	case 4:
		// Lava run
		spawnLava(195, 300, 2000, 2000, 0, false);
		spawnLava(360, 455, 2000, 2000, 0, false);
		spawnLava(520, 610, 2000, 2000, 0, false);
    spawnLava(670, 770, 2000, 2000, 0, false);
		spawnPool[0].Spawn(1000, 3800, 4, 0, 0);
		break;
	case 5:
		// Sin enemy
		spawnEnemy(700, 1, 7, 275);
		spawnEnemy(500, 1, 5, 250);
		break;
	case 6:
		// Conveyor
		spawnConveyor(100, 600, -1);
		spawnEnemy(800, 0, 0, 0);
		spawnEnemy(500, 1, 7, 275);
		break;
	case 7:
		// Drainage
		spawnConveyor(100, 600, 1);
		spawnConveyor(600, 1000, -1);
		spawnEnemy(600, 0, 0, 0);
		spawnPool[0].Spawn(1000, 5500, 3, 0, 0);
		break;
	case 8:
		// Conveyor of enemies
		spawnConveyor(50, 1000, 1);
		spawnEnemy(300, 0, 0, 0);
		spawnEnemy(400, 0, 0, 0);
		spawnEnemy(500, 0, 0, 0);
		spawnEnemy(600, 0, 0, 0);
		spawnEnemy(700, 0, 0, 0);
		spawnEnemy(800, 0, 0, 0);
		spawnEnemy(900, 0, 0, 0);
		break;
	case 9:
		// Sin enemy #2
		spawnEnemy(700, 1, 7, 275);
		spawnEnemy(500, 1, 5, 250);
		spawnPool[0].Spawn(1000, 5500, 4, 0, 3000);
		spawnPool[1].Spawn(0, 5500, 5, 1, 10000);
		spawnConveyor(100, 900, -1);
		break;
	case 10:
		// less lava, more enemies
		spawnLava(350, 455, 2000, 2000, 0, false);
		spawnLava(660, 760, 2000, 2000, 0, false);
		spawnPool[0].Spawn(1000, 3800, 4, 0, 270);
		spawnEnemy(800, 0, 0, 0);
		break;
	case 11:
		// pushed towards lava
		spawnConveyor(100, 800, 1);
		spawnLava(800, 850, 1000, 2000, 0, false);
		spawnPool[0].Spawn(1000, 2000, 4, 0, 0);
		break;
	case 12:
		// quick lava
		spawnPool[0].Spawn(0, 2300, 6, 1, 7000);
		spawnLava(200, 400, 1000, 2000, 0, false);
		spawnLava(600, 800, 1000, 2000, 0, false);
		spawnPool[1].Spawn(1000, 2500, 6, 0, 1000);
		break;
	case 13:
		// Boss
		spawnEnemy(800, 0, 0, 0);
		break;
	case 14:
		// Boss
		spawnBoss();
		break;
	}
	stageStartTime = millis();
	stage = PLAY;
}

void spawnBoss() {
	boss.Spawn();
	moveBoss();
}

void moveBoss() {
	int spawnSpeed = 2100;
	if (boss._lives == 2) spawnSpeed = 1800;
	if (boss._lives == 1) spawnSpeed = 1400;
	spawnPool[0].Spawn(boss._pos, spawnSpeed, 3, 0, 0);
	spawnPool[1].Spawn(boss._pos, spawnSpeed, 3, 1, 0);
}

void spawnEnemy(int pos, int dir, int sp, int wobble) {
	for (int e = 0; e < enemyCount; ++e) {
		if (!enemyPool[e].Alive()) {
			enemyPool[e].Spawn(pos, dir, sp, wobble);
			enemyPool[e].playerSide = pos > playerPosition ? 1 : -1;
			return;
		}
	}
}

void spawnLava(int left, int right, int ontime, int offtime, int offset, bool isOn) {
	for (int i = 0; i < lavaCount; ++i) {
		if (!lavaPool[i].Alive()) {
			lavaPool[i].Spawn(left, right, ontime, offtime, offset, isOn);
			return;
		}
	}
}

void spawnConveyor(int startPoint, int endPoint, int dir) {
	for (int i = 0; i < conveyorCount; ++i) {
		if (!conveyorPool[i].Alive()) {
			conveyorPool[i].Spawn(startPoint, endPoint, dir);
			return;
		}
	}
}

void cleanupLevel() {
	for (int i = 0; i < enemyCount; ++i) {
		enemyPool[i].Kill();
	}
	for (int i = 0; i < particleCount; ++i) {
		particlePool[i].Kill();
	}
	for (int i = 0; i < spawnCount; ++i) {
		spawnPool[i].Kill();
	}
	for (int i = 0; i < lavaCount; ++i) {
		lavaPool[i].Kill();
	}
	for (int i = 0; i < conveyorCount; ++i) {
		conveyorPool[i].Kill();
	}
	boss.Kill();
}

void levelComplete() {
	stageStartTime = millis();
	stage = WIN;
	if (levelNumber == LEVEL_COUNT)
	{
		stage = COMPLETE;
	/*
		++stats_playthroughs;
		EEPROM.write(0, stats_playthroughs);
		lastPlaythrough = (int)((millis() - gameStartTime) / 1000);
		if (lastPlaythrough < stats_besttime) {
			stats_besttime = lastPlaythrough;
			EEPROM_writeint(1, stats_besttime);
		}
		updateStats();
	*/
	}
	lives = 3; //min(MAX_LIVES, lives + 1);
}

void nextLevel() {
	++levelNumber;
	if (levelNumber > LEVEL_COUNT) levelNumber = 0;
	loadLevel();
}

void gameOver() {
	levelNumber = 0;
	loadLevel();
}

void die() {
	playerAlive = 0;
	if (levelNumber > 0) --lives;
	if (lives == 0) {
		levelNumber = 0;
		lives = MAX_LIVES;
	}
	for (int p = 0; p < particleCount; ++p) {
		particlePool[p].Spawn(playerPosition);
	}
	stageStartTime = millis();
	stage = DEAD;
	killTime = millis();
}

// ----------------------------------
// -------- TICKS & RENDERS ---------
// ----------------------------------
void tickEnemies() {
	for (int i = 0; i < enemyCount; ++i) {
		if (enemyPool[i].Alive()) {
			enemyPool[i].Tick();
			// Hit attack?
			if (attacking) {
				if (enemyPool[i]._pos > playerPosition - (ATTACK_WIDTH / 2) && enemyPool[i]._pos < playerPosition + (ATTACK_WIDTH / 2)) {
					enemyPool[i].Kill();
					SFXkill();
				}
			}
			if (inLava(enemyPool[i]._pos)) {
				enemyPool[i].Kill();
				SFXkill();
			}
			// Draw (if still alive)
			if (enemyPool[i].Alive()) {
				leds[getLED(enemyPool[i]._pos)].setRGB(255, 0, 0);
			}
			// Hit player?
			if (
				(enemyPool[i].playerSide == 1 && enemyPool[i]._pos <= playerPosition) ||
				(enemyPool[i].playerSide == -1 && enemyPool[i]._pos >= playerPosition)
				) {
				die();
				return;
			}
		}
	}
}

void tickBoss() {
	// DRAW
	if (boss.Alive()) {
		++boss._ticks;
		for (int i = getLED(boss._pos - BOSS_WIDTH / 2); i <= getLED(boss._pos + BOSS_WIDTH / 2); i++) {
			leds[i] = CRGB::DarkRed;
			leds[i] %= 100;
		}
		// CHECK COLLISION
		if (getLED(playerPosition) > getLED(boss._pos - BOSS_WIDTH / 2) && getLED(playerPosition) < getLED(boss._pos + BOSS_WIDTH)) {
			die();
			return;
		}
		// CHECK FOR ATTACK
		if (attacking) {
			if (
				(getLED(playerPosition + (ATTACK_WIDTH / 2)) >= getLED(boss._pos - BOSS_WIDTH / 2) && getLED(playerPosition + (ATTACK_WIDTH / 2)) <= getLED(boss._pos + BOSS_WIDTH / 2)) ||
				(getLED(playerPosition - (ATTACK_WIDTH / 2)) <= getLED(boss._pos + BOSS_WIDTH / 2) && getLED(playerPosition - (ATTACK_WIDTH / 2)) >= getLED(boss._pos - BOSS_WIDTH / 2))
				) {
				boss.Hit();
				if (boss.Alive()) {
					moveBoss();
				}
				else {
					spawnPool[0].Kill();
					spawnPool[1].Kill();
				}
			}
		}
	}
}

void drawPlayer() {
	leds[getLED(playerPosition)].setRGB(0, 255, 0);
}

void drawExit() {
	if (!boss.Alive()) {
		leds[NUM_LEDS - MAX_LIVES - 1].setRGB(0, 0, 255);
	}
}

void drawLives() {
	// Updates the life LEDs to show how many lives the player has left
	for (int i = 1; i <= MAX_LIVES; ++i) {
		int val = lives >= i ? 100 : 0;
		leds[NUM_LEDS - i].setRGB(val, 0, val);
	}
}

void tickSpawners() {
	long mm = millis();
	for (int s = 0; s < spawnCount; ++s) {
		if (spawnPool[s].Alive() && spawnPool[s]._activate < mm) {
			if (spawnPool[s]._lastSpawned + spawnPool[s]._rate < mm || spawnPool[s]._lastSpawned == 0) {
				spawnEnemy(spawnPool[s]._pos, spawnPool[s]._dir, spawnPool[s]._sp, 0);
				spawnPool[s]._lastSpawned = mm;
			}
		}
	}
}

void tickLava() {
	int A, B, p, i, brightness, flicker;
	long mm = millis();
	for (i = 0; i < lavaCount; ++i) {
		flicker = random8(5);
		Lava &LP = lavaPool[i];
		if (LP.Alive()) {
			A = getLED(LP._left);
			B = getLED(LP._right);
			if (LP._isOn) {
				if (LP._lastOn + LP._ontime < mm) {
					LP._isOn = false;
					LP._lastOn = mm;
				}
				for (p = A; p <= B; ++p) {
					leds[p].setRGB(150 + flicker, 100 + flicker, 0);
				}
			}
			else {
				if (LP._lastOn + LP._offtime < mm) {
					LP._isOn = true;
					LP._lastOn = mm;
				}
				for (p = A; p <= B; ++p) {
					leds[p].setRGB(3 + flicker, (3 + flicker) / 1.5, 0);
				}
			}
		}
	}
}

bool tickParticles() {
	bool stillActive = false;
	for (int p = 0; p < particleCount; ++p) {
		if (particlePool[p].Alive()) {
			particlePool[p].Tick(USE_GRAVITY);
			leds[getLED(particlePool[p]._pos)] += CRGB(particlePool[p]._power, 0, 0);
			stillActive = true;
		}
	}
	return stillActive;
}

void tickConveyors() {
	int b, dir, n, i, ss, ee, led;
	long m = 10000 + millis();
	playerPositionModifier = 0;

	for (i = 0; i < conveyorCount; ++i) {
		if (conveyorPool[i].Alive()) {
			dir = conveyorPool[i]._dir;
			ss = getLED(conveyorPool[i]._startPoint);
			ee = getLED(conveyorPool[i]._endPoint);
			for (led = ss; led < ee; led++) {
				b = 5;
				n = (-led + (m / 100)) % 5;
				if (dir == -1) n = (led + (m / 100)) % 5;
				b = (5 - n) / 2.0;
				if (b > 0) leds[led].setRGB(0, 0, b);
			}

			if (playerPosition > conveyorPool[i]._startPoint && playerPosition < conveyorPool[i]._endPoint) {
				if (dir == -1) {
					playerPositionModifier = -(MAX_PLAYER_SPEED - 4);
					if (attacking && moveAmount < 0)
						playerPositionModifier /= 3;
				}
				else {
					playerPositionModifier = (MAX_PLAYER_SPEED - 4);
					if (attacking && moveAmount > 0)
						playerPositionModifier /= 3;
				}
			}
		}
	}
}

void drawAttack() {
	if (!attacking) return;
	int n = map(millis() - attackMillis, 0, ATTACK_DURATION, 100, 5);
	for (int i = getLED(playerPosition - (ATTACK_WIDTH / 2)) + 1; i <= getLED(playerPosition + (ATTACK_WIDTH / 2)) - 1; ++i) {
		leds[i].setRGB(0, 0, n);
	}
	if (n > 90) {
		n = 255;
		leds[getLED(playerPosition)].setRGB(255, 255, 255);
	}
	else {
		n = 0;
		leds[getLED(playerPosition)].setRGB(0, 255, 0);
	}
	leds[getLED(playerPosition - (ATTACK_WIDTH / 2))].setRGB(n, n, 255);
	leds[getLED(playerPosition + (ATTACK_WIDTH / 2))].setRGB(n, n, 255);
}

int getLED(int pos) {
	// The world is 1000 pixels wide, this converts world units into an LED number
	return constrain((int)map(pos, 0, 1000, 0, NUM_LEDS - MAX_LIVES - 1), 0, NUM_LEDS - MAX_LIVES - 1);
}

bool inLava(int pos) {
	// Returns if the player is in active lava
	int i;
	Lava LP;
	for (i = 0; i < lavaCount; ++i) {
		LP = lavaPool[i];
		if (LP.Alive() && LP._isOn) {
			if (LP._left < pos && LP._right > pos) return true;
		}
	}
	return false;
}

// ---------------------------------
// --------- SCREENSAVER -----------
// ---------------------------------
void screenSaverTick() {
	int n, b, c, i;
	long mm = millis();
	int mode = (mm / 20000) % 2;

	for (i = 0; i < NUM_LEDS; ++i) {
		leds[i].nscale8(250);
	}
	if (mode == 0) {
		// Marching green <> orange
		n = (mm / 250) % 10;
		b = 10 + ((sin(mm / 500.00) + 1) * 20.00);
		c = 20 + ((sin(mm / 5000.00) + 1) * 33);
		for (i = 0; i < NUM_LEDS; ++i) {
			if (i % 10 == n) {
				leds[i].setHSV(c, 255, 150);
			}
		}
	}
	else if (mode == 1) {
		// Random flashes
		randomSeed(mm);
		for (i = 0; i < NUM_LEDS; ++i) {
			if (random8(200) == 0) {
				leds[i].setHSV(25, 255, 100);
			}
		}
	}
}

// ---------------------------------
// --------- LCD / Stats -----------
// ---------------------------------
/*
int EEPROM_readint(long address) {
	return ((EEPROM.read(address) << 0) & 0xFF) + ((EEPROM.read(address + 1) << 8) & 0xFFFF);
}

void EEPROM_writeint(long address, int value) {
	EEPROM.write(address, ((value >> 0) & 0xFF));
	EEPROM.write(address + 1, ((value >> 8) & 0xFF));
}

void updateStats() {
	lcd.setCursor(0, 1);
	lcd.print("Playthroughs: ");
	lcd.print(stats_playthroughs);

	lcd.setCursor(0, 2);
	lcd.print("Best time:    ");
	printTime(stats_besttime);
	
	if (lastPlaythrough > 0)
	{
		lcd.setCursor(0, 3);
		lcd.print("Your time:    ");
		printTime(lastPlaythrough);
	}
}

void printTime(long seconds) {
	int v = int(seconds / 60);
	if (v < 10)
		lcd.print("0");
	lcd.print(v);
	lcd.print(":");
	v = seconds - v * 60;
	if (v < 10)
		lcd.print("0");
	lcd.print(v);
}
*/
// ---------------------------------
// ----------- JOYSTICK ------------
// ---------------------------------
void getInput() {
	// This is responsible for the player movement speed and attacking.
	// You can replace it with anything you want that passes a -90>+90 value to joystickTilt
	// and any value to joystickWobble that is greater than ATTACK_THRESHOLD (defined at start)
	// For example you could use 3 momentery buttons:
	// if(digitalRead(leftButtonPinNumber) == HIGH) joystickTilt = -90;
	// if(digitalRead(rightButtonPinNumber) == HIGH) joystickTilt = 90;
	// if(digitalRead(attackButtonPinNumber) == HIGH) joystickWobble = ATTACK_THRESHOLD;
	/*
		joystickWobble = 0;
		joystickTilt = 0;
		if(digitalRead(PAD_UP_PIN) == HIGH) joystickTilt = -90;
		if(digitalRead(PAD_DOWN_PIN) == HIGH) joystickTilt = 90;
		if(digitalRead(PAD_ATK_PIN) == HIGH) joystickWobble = ATTACK_THRESHOLD+1;
		return;
	*/

	accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
	int a = (JOYSTICK_ORIENTATION == 0 ? ax : (JOYSTICK_ORIENTATION == 1 ? ay : az)) / 166;
	int g = (JOYSTICK_ORIENTATION == 0 ? gx : (JOYSTICK_ORIENTATION == 1 ? gy : gz));
	if (abs(a) < JOYSTICK_DEADZONE) a = 0;
	if (a > 0) a -= JOYSTICK_DEADZONE;
	if (a < 0) a += JOYSTICK_DEADZONE;
	MPUAngleSamples.add(a);
	MPUWobbleSamples.add(g);

	joystickTilt = MPUAngleSamples.getMedian();
	if (JOYSTICK_DIRECTION == 1) {
		joystickTilt = 0 - joystickTilt;
	}
	joystickWobble = abs(MPUWobbleSamples.getHighest());
}


// ---------------------------------
// -------------- SFX --------------
// ---------------------------------
void SFXtilt(int amount) {
	int f = map(abs(amount), 0, 90, 80, 900) + random8(100);
	if (playerPositionModifier < 0) f -= 500;
	if (playerPositionModifier > 0) f += 200;
	toneAC(f, min(min(abs(amount) / 9, 5), MAX_VOLUME));

}
void SFXattacking() {
	int freq = map(sin(millis() / 2.0) * 1000.0, -1000, 1000, 500, 600);
	if (random8(5) == 0) {
		freq *= 3;
	}
	toneAC(freq, MAX_VOLUME);
}
void SFXdead() {
	int freq = max(1000 - (millis() - killTime), 10);
	freq += random8(200);
	int vol = max(10 - (millis() - killTime) / 200, 0);
	toneAC(freq, MAX_VOLUME);
}
void SFXkill() {
	toneAC(2000, MAX_VOLUME, 1000, true);
}
void SFXwin() {
	int freq = (millis() - stageStartTime) / 3.0;
	freq += map(sin(millis() / 20.0) * 1000.0, -1000, 1000, 0, 20);
	int vol = 10;//max(10 - (millis()-stageStartTime)/200, 0);
	toneAC(freq, MAX_VOLUME);
}

void SFXcomplete() {
	noToneAC();
}









