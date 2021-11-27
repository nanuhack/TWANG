// Required libs
#include "FastLED.h"

#include "Wire.h"

#include "I2Cdev.h"
#include "MPU6050.h"

#include "toneAC.h"

#include "RunningMedian.h"

// Included game libs
#include "Enemy.h"
#include "Particle.h"
#include "Spawner.h"
#include "Lava.h"
#include "Boss.h"
#include "Conveyor.h"
#include "Key.h"

// MPU
MPU6050 accelgyro;
int16_t ax, ay, az;
int16_t gx, gy, gz;

// LED setup
#define NUM_LEDS             300
#define DATA_PIN             3
//#define CLOCK_PIN            4     
#define LED_COLOR_ORDER      GRB   //if colours aren't working, try GRB or GBR
#define BRIGHTNESS           150   //Use a lower value for lower current power supplies(<2 amps)
#define DIRECTION            1     // 0 = right to left, 1 = left to right
#define MIN_REDRAW_INTERVAL  20    // Min redraw interval (ms) 33 = 30fps / 16 = 63fps
#define USE_GRAVITY          1     // 0/1 use gravity (LED strip going up wall)
#define BEND_POINT           500   // 0/1000 point at which the LED strip goes up the wall
#define GRAVITY_PLAYER_MOD   1.25   // factor player movement is slowed down/ speed up at wall (>1.0)
#define LED_TYPE             WS2812//type of LED strip to use(APA102 - DotStar, WS2811 - NeoPixel) For Neopixels, uncomment line #108 and comment out line #106

// GAME
#define START_LVL_NUM        0     // start level (after gameover), for debuging only! ;) default: 0
long previousMillis = 0;           // Time of the last redraw
char levelNumber = START_LVL_NUM;
long lastInputTime = 0;
#define TIMEOUT              50000
#define LEVEL_COUNT          13

// JOYSTICK
#define JOYSTICK_ORIENTATION 1     // 0, 1 or 2 to set the angle of the joystick
#define JOYSTICK_DIRECTION   1     // 0/1 to flip joystick direction
#define ATTACK_THRESHOLD     30000 // The threshold that triggers an attack
#define JOYSTICK_DEADZONE    10    // Angle to ignore
char joystickTilt = 0;             // Stores the angle of the joystick
int joystickWobble = 0;            // Stores the max amount of acceleration (wobble)

// WOBBLE ATTACK
#define ATTACK_WIDTH        50     // Width of the wobble attack, world is 1000 wide
#define ATTACK_DURATION     400    // Duration of a wobble attack (ms)
long attackMillis = 0;             // Time the attack started
bool attacking = 0;                // Is the attack in progress?
#define BOSS_WIDTH          40

// PLAYER
#define MAX_PLAYER_SPEED    8     // Max move speed of the player
byte lives = 3;

byte stage;                        // what stage the game is at (0 - SCREENSAVER/ 1 - PLAYING/ 2 - DEAD/ 3 - LEVEL WIN/ 4 - GAMEOVER/ 5 - COMPLETE GAME)
long stageStartTime;               // Stores the time the stage changed for stages that are time based
int playerPosition = 0;                // Stores the player position
char playerPositionModifier;       // +/- adjustment to player position
bool playerAlive;                  // Stores if player is still alive
bool beforeKey = true;             // Stores relative position of player to key
long killTime;
int moveAmount = 0;


byte lifeLEDs[] = {7, 6, 5};

// POOLS
Enemy enemyPool[] = {
    Enemy(), Enemy(), Enemy(), Enemy(), Enemy(), Enemy(), Enemy()
};
#define ENEMY_COUNT          7

Particle particlePool[] = {
    Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(),
    Particle(), Particle(), Particle(), Particle(), Particle()
};
#define PARTICLE_COUNT       15

Spawner spawnPool[] = {
    Spawner(), Spawner()
};
#define SPAWN_COUNT          2

Lava lavaPool[] = {
    Lava(), Lava(), Lava(), Lava(), Lava()
};
#define LAVA_COUNT           5

Conveyor conveyorPool[] = {
    Conveyor(), Conveyor(), Conveyor()
};
#define CONVEYOR_COUNT       3

Boss boss = Boss();
Key key = Key();

// SFX
#define MAX_VOLUME           10
/*
int melody[] = { 262, 0, 196, 0, 196, 0, 220, 0, 196, 0, 0, 0, 247, 0, 262, 0 };
#define MELODY_LENGTH        16
byte melody_pos = 0;
int noteTime = 0;
*/

CRGB leds[NUM_LEDS];
RunningMedian MPUAngleSamples = RunningMedian(5);
RunningMedian MPUWobbleSamples = RunningMedian(5);

void setup() {
    Wire.begin();
    
    //Serial.begin(9600);
    //while (!Serial);
    
    accelgyro.initialize();
    
    // Fast LED
    //FastLED.addLeds<LED_TYPE, DATA_PIN, CLOCK_PIN, LED_COLOR_ORDER>(leds, NUM_LEDS);
    // If using Neopixels, use
    FastLED.addLeds<LED_TYPE, DATA_PIN, LED_COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.setDither(1);
    
    // Life LEDs
    for(int i = 0; i<3; i++) {
        pinMode(lifeLEDs[i], OUTPUT);
        digitalWrite(lifeLEDs[i], HIGH);
    }

    random16_set_seed(millis());
  
    loadLevel();
}

void loop() {
    long mm = millis();
    int brightness = 0;
    
    if (mm - previousMillis >= MIN_REDRAW_INTERVAL) {
        getInput();
        previousMillis = mm;

        if(abs(joystickTilt) > JOYSTICK_DEADZONE) {
            lastInputTime = mm;
            if(stage == 0) {
                levelNumber = -1;
                stageStartTime = mm;
                stage = 3;
                lives = 3;
            }
        } else{
            if(lastInputTime+TIMEOUT < mm) {
                if(stage != 0) {
                    for(int i = 0; i<3; i++) {
                        digitalWrite(lifeLEDs[i], LOW);
                    }
                    stage = 0;
                }
            }
        }
        
        
        if(stage == 0) {
            // SCREENSAVER
            screenSaverTick();
            
        } else if(stage == 1) {
            // PLAYING
            if(attacking && attackMillis+ATTACK_DURATION < mm) attacking = 0;
            
            // If not attacking, check if they should be
            if(!attacking && joystickWobble > ATTACK_THRESHOLD) {
                attackMillis = mm;
                attacking = 1;
            }

            
            // If still not attacking, move!
            playerPosition += playerPositionModifier;
            if(!attacking) {
                moveAmount = (joystickTilt/6.0);
                if(DIRECTION) moveAmount = -moveAmount;
                moveAmount = constrain(moveAmount, -MAX_PLAYER_SPEED, MAX_PLAYER_SPEED);
                if(USE_GRAVITY && playerPosition > BEND_POINT) {
                  if(moveAmount > 0) moveAmount *= GRAVITY_PLAYER_MOD;
                  else moveAmount *= 1.0 / GRAVITY_PLAYER_MOD;
                }
                playerPosition -= moveAmount;
                if(playerPosition <= 0) playerPosition = 0;
                if(playerPosition >= 1000) {
                  if(!boss.Alive() && (key.isCollected() || !key.isAlive())) {
                    // Reached exit!
                    levelComplete();
                    return;
                  } else {
                    playerPosition = 1000;
                  }
                }
                
                if(key.isAlive() && !key.isCollected() && ((beforeKey && playerPosition >= key._pos) || (!beforeKey && playerPosition <= key._pos))) {
                  key.Collect();
                  SFXkey();
                }
            }
            
            if(inLava(playerPosition)) {
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
            drawKey();

        } else if(stage == 2) {
            // DEAD
            FastLED.clear();
            if(!tickParticles()) {
                loadLevel();
            }

        } else if(stage == 3) {
            // LEVEL COMPLETE
            FastLED.clear();
            if(stageStartTime+500 > mm) {
                int n = max(map(((mm-stageStartTime)), 0, 500, NUM_LEDS, 0), 0);
                for(int i = NUM_LEDS; i>= n; i--) {
                    leds[i] = CRGB(0, 255, 0);
                }
                SFXwin();
            } else if(stageStartTime+1000 > mm) {
                int n = max(map(((mm-stageStartTime)), 500, 1000, NUM_LEDS, 0), 0);
                for(int i = 0; i< n; i++) {
                    leds[i] = CRGB(0, 255, 0);
                }
                SFXwin();
            } else if(stageStartTime+1200 > mm) {
                leds[0] = CRGB(0, 255, 0);
            } else{
                nextLevel();
            }
        } else if(stage == 5) {
            FastLED.clear();
            noToneAC();
            if(stageStartTime+500 > mm) {
                int n = max(map(((mm-stageStartTime)), 0, 500, NUM_LEDS, 0), 0);
                for(int i = NUM_LEDS; i>= n; i--) {
                    brightness = (sin(((i*10)+mm)/500.0)+1)*255;
                    leds[i].setHSV(brightness, 255, 50);
                }
            } else if(stageStartTime+5000 > mm) {
                for(int i = NUM_LEDS; i>= 0; i--) {
                    brightness = (sin(((i*10)+mm)/500.0)+1)*255;
                    leds[i].setHSV(brightness, 255, 50);
                }
            } else if(stageStartTime+5500 > mm) {
                int n = max(map(((mm-stageStartTime)), 5000, 5500, NUM_LEDS, 0), 0);
                for(int i = 0; i< n; i++) {
                    brightness = (sin(((i*10)+mm)/500.0)+1)*255;
                    leds[i].setHSV(brightness, 255, 50);
                }
            } else{
                nextLevel();
            }
        } else if(stage == 4) {
            // GAME OVER!
            FastLED.clear();
            if(stageStartTime+500 > mm) {
                int n = max(map(((mm-stageStartTime)), 0, 500, NUM_LEDS, 0), 0);
                for(int i = NUM_LEDS; i>= n; i--) {
                    leds[i] = CRGB(255, 0, 0);
                }
                SFXloose();
            } else if(stageStartTime+1000 > mm) {
                int n = max(map(((mm-stageStartTime)), 500, 1000, NUM_LEDS, 0), 0);
                for(int i = 0; i< n; i++) {
                    leds[i] = CRGB(255, 0, 0);
                }
                SFXloose();
            } else if(stageStartTime+1200 > mm) {
                leds[0] = CRGB(255, 0, 0);
            } else{
                nextLevel();
            }
        }
        
        FastLED.show();
    }

    if(stage == 1) {
        // PLAYING
        
        if(attacking) {
            SFXattacking();
        } else{
            SFXtilt(joystickTilt);
            //SFXmusic();
        }
    } else if(stage == 2) {
        // DEAD
        SFXdead();
    }
}


// ---------------------------------
// ------------ LEVELS -------------
// ---------------------------------
void loadLevel() {
    random16_add_entropy(millis());
    updateLives();
    cleanupLevel();
    playerAlive = 1;
    //int startPos = 0;  //startPosition default 0 on lvl entry
    playerPosition = 0;
    
    switch(levelNumber) {
        case 0:
            // Left or right?
            playerPosition = 200;
            spawnEnemy(1, 0, 0, 0, playerPosition);
            break;
        case 1:
            // Key
            playerPosition = 1000;
            spawnKey(200);
            break;
        case 2:
            // Slow moving enemy
            spawnEnemy(900, 0, 1, 0, playerPosition);
            break;
        case 3:
            // Spawning enemies at exit every 2 seconds
            spawnPool[0].Spawn(1000, 3000, 2, 0, 0);
            break;
        case 4:
            // Lava intro
            spawnLava(random16(395,405), random16(485,495), random16(1900,2100), random16(1700,2100), 0, 1);
            spawnPool[0].Spawn(1000, 5500, 3, 0, 0);
            break;
        //case 5:
            //Key respawning
          //  playerPosition = 1000;
            //spawnLava(0, BEND_POINT, 10, 10, 0, 1);
           // spawnKey(getLEDrev(getLED(BEND_POINT)+1));
            //spawnPool[0].Spawn(BEND_POINT + (1000 - BEND_POINT)/2, 2000, 3, 1, 0);
        case 5:
            // Sin enemy
            spawnEnemy(random16(350, 500), 1, random8(6,9), random16(200,300), playerPosition);
            spawnEnemy(random16(500, 700), 1, random8(6,9), random16(200,300), playerPosition);
            break;
        case 6:
            // bad Conveyor
            spawnConveyor(100, 600, -1);
            spawnEnemy(650, 0, 0, 0, playerPosition);
            break;
        case 7:
            // Lava run
            playerPosition = 1000;
            spawnKey(0);
            spawnLava(200, 300, 2000, 2000, 0, 1);
            spawnLava(350, 450, 1800, 1800, 0, 0);
            spawnLava(500, 600, 1600, 1600, 0, 1);
            spawnLava(650, 750, 1400, 1400, 0, 0);
            spawnLava(800, 900, 1200, 1200, 0, 1);
            break;
        case 8:
            // Conveyor of enemies
            spawnConveyor(50, 1000, 1);
            spawnEnemy(300, 0,  0,  0, playerPosition);
            spawnEnemy(400, 0,  2, 10, playerPosition);
            spawnEnemy(500, 0,  4, 20, playerPosition);
            spawnEnemy(600, 0,  8, 30, playerPosition);
            spawnEnemy(700, 0, 16, 40, playerPosition);
            spawnEnemy(800, 0, 24, 50, playerPosition);
            spawnEnemy(900, 0, 32, 60, playerPosition);
            break;
        case 9:
            // Lava run #2
            spawnPool[0].Spawn(0, 3500, 7, 1, 10000);
            spawnLava(200, 300, random16(1500,2000), random16(1500,2000), 0, 1);
            spawnLava(350, 460, random16(1500,2000), random16(1500,2000), 400, 0);
            spawnLava(500, 620, random16(1500,2000), random16(1500,2000), 1200, 1);
            spawnKey(635);
            spawnLava(650, 780, random16(1500,2000), random16(1500,2000), 800, 0);
            spawnPool[1].Spawn(1000, 3500, 7, 0, 0);
            break;
        case 10:
            //Lavas + Conveyors
            spawnEnemy(400, 0, 4, 0, playerPosition);
            spawnEnemy(400, 0, 5, 0, playerPosition);
            spawnEnemy(400, 0, 6, 0, playerPosition);
            spawnEnemy(400, 0, 7, 0, playerPosition);
            spawnConveyor(100, 500, 1);
            spawnLava(500, 600, random16(1500,2000), random16(1500,2000), 0, 0);
            spawnConveyor(600, 700, -1);
            spawnLava(700, 850, random16(1800,2200), random16(1800,2200), 0, 0);
            spawnConveyor(850, 1000, -1);
            break;
        case 11:
            // Sin enemy #2
            spawnPool[1].Spawn(0, 5500, 4, 1, 10000);
            spawnEnemy(random16(650, 750), 1, random8(5,7), random16(150,200), playerPosition);
            spawnEnemy(random16(450, 550), 1, random8(4,6), random16(200,300), playerPosition);
            spawnPool[0].Spawn(1000, 5500, 4, 0, 3000);
            spawnConveyor(100, 900, -1);
            break;
        case 12:
            // Boss
            spawnBoss();
            break;
    }
    
    //Use collected Key as Checkpoint
    if(key.isAlive() && key.isCollected()) playerPosition = key._pos;
        
    stageStartTime = millis();
    stage = 1;
}

void spawnBoss() {
    boss.Spawn();
    moveBoss();
}

void moveBoss() {
    word spawnSpeed = 2500;
    if(boss._lives == 2) spawnSpeed = 2000;
    if(boss._lives == 1) spawnSpeed = 1500;
    spawnPool[0].Spawn(boss._pos, spawnSpeed, 3, 0, 0);
    spawnPool[1].Spawn(boss._pos, spawnSpeed, 3, 1, 0);
}

void spawnEnemy(int pos, char dir, char sp, int wobble, int playerPos) {
    for(int e = 0; e<ENEMY_COUNT; e++) {
        if(!enemyPool[e].Alive()) {
            enemyPool[e].Spawn(pos, dir, sp, wobble);
            enemyPool[e].playerSide = pos > playerPos?1:-1;
            return;
        }
    }
}

void spawnLava(int left, int right, int ontime, int offtime, int offset, bool state) {
    for(int i = 0; i<LAVA_COUNT; i++) {
        if(!lavaPool[i].Alive()) {
            lavaPool[i].Spawn(left, right, ontime, offtime, offset, state);
            return;
        }
    }
}

void spawnConveyor(int startPoint, int endPoint, char dir) {
    for(int i = 0; i<CONVEYOR_COUNT; i++) {
        if(!conveyorPool[i]._alive) {
            conveyorPool[i].Spawn(startPoint, endPoint, dir);
            return;
        }
    }
}

void spawnKey(int pos) {
  key.Spawn(pos);
  beforeKey = playerPosition <= pos;
}

void cleanupLevel() {
    for(int i = 0; i<ENEMY_COUNT; i++) {
        enemyPool[i].Kill();
    }
    for(int i = 0; i<PARTICLE_COUNT; i++) {
        particlePool[i].Kill();
    }
    for(int i = 0; i<SPAWN_COUNT; i++) {
        spawnPool[i].Kill();
    }
    for(int i = 0; i<LAVA_COUNT; i++) {
        lavaPool[i].Kill();
    }
    for(int i = 0; i<CONVEYOR_COUNT; i++) {
        conveyorPool[i].Kill();
    }
    boss.Kill();
}

void levelComplete() {
    stageStartTime = millis();
    stage = 3;
    if(levelNumber == LEVEL_COUNT) stage = 5;
    lives = 3;
    updateLives();
}

void nextLevel() {
    levelNumber ++;
    if(levelNumber > LEVEL_COUNT || levelNumber < START_LVL_NUM)
      levelNumber = START_LVL_NUM;
    
    key.Kill();
    
    loadLevel();
}

void die() {
    playerAlive = 0;
    stage = 2;
    //char partColor[] = {255, 0, 0};
    
    if(levelNumber >= 0) lives --;
      updateLives();
    
    if(lives == 0) {
        levelNumber = -1;
        lives = 3;
        stage = 4;
    }
    for(int p = 0; p < PARTICLE_COUNT; p++) {
        particlePool[p].Spawn(playerPosition);
    }
    stageStartTime = millis();
    killTime = millis();
}

// ----------------------------------
// -------- TICKS & RENDERS ---------
// ----------------------------------
void tickEnemies() {
    for(int i = 0; i<ENEMY_COUNT; i++) {
        if(enemyPool[i].Alive()) {
            enemyPool[i].Tick();
            // Hit attack?
            if(attacking) {
                if(enemyPool[i]._pos > playerPosition-(ATTACK_WIDTH/2) && enemyPool[i]._pos < playerPosition+(ATTACK_WIDTH/2)) {
                   enemyPool[i].Kill();
                   SFXkill();
                }
            }
            if(inLava(enemyPool[i]._pos)) {
                enemyPool[i].Kill();
                SFXkill();
            }
            // Draw (if still alive)
            if(enemyPool[i].Alive()) {
                leds[getLED(enemyPool[i]._pos)] = CRGB(255, 0, 0);
            }
            // Hit player?
            if(
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
    if(boss.Alive()) {
        for(int i = getLED(boss._pos-BOSS_WIDTH/2); i<=getLED(boss._pos+BOSS_WIDTH/2); i++) {
            leds[i] = CRGB::DarkRed;
            leds[i] %= 100;
        }
        // CHECK COLLISION
        if(getLED(playerPosition) > getLED(boss._pos - BOSS_WIDTH/2) && getLED(playerPosition) < getLED(boss._pos + BOSS_WIDTH)) {
            die();
            return; 
        }
        // CHECK FOR ATTACK
        if(attacking) {
            if(
              (getLED(playerPosition+(ATTACK_WIDTH/2)) >= getLED(boss._pos - BOSS_WIDTH/2) && getLED(playerPosition+(ATTACK_WIDTH/2)) <= getLED(boss._pos + BOSS_WIDTH/2)) ||
              (getLED(playerPosition-(ATTACK_WIDTH/2)) <= getLED(boss._pos + BOSS_WIDTH/2) && getLED(playerPosition-(ATTACK_WIDTH/2)) >= getLED(boss._pos - BOSS_WIDTH/2))
            ) {
               boss.Hit();
               if(boss.Alive()) {
                   moveBoss();
               } else{
                   spawnPool[0].Kill();
                   spawnPool[1].Kill();
               }
            }
        }
    }
}

void drawPlayer() {
    leds[getLED(playerPosition)] = CRGB(0, 255, 0);
}

void drawExit() {
    if(!boss.Alive() && (key.isCollected() || !key.isAlive())) {
        leds[NUM_LEDS-1] = CRGB(0, 0, 255);
        //particlePool[0].Spawn(NUM_LEDS-1, CRGB(0, 0, 255));
    }
}

void drawKey() {
    if(key.isAlive() && !key.isCollected())
      leds[getLED(key._pos)] = CRGB(255, 0, 255);
}

void tickSpawners() {
    long mm = millis();
    for(int s = 0; s<SPAWN_COUNT; s++) {
        if(spawnPool[s].Alive() && spawnPool[s]._activate < mm) {
            if(spawnPool[s]._lastSpawned + spawnPool[s]._rate < mm || spawnPool[s]._lastSpawned == 0) {
                spawnEnemy(spawnPool[s]._pos, spawnPool[s]._dir, spawnPool[s]._sp, 0, playerPosition);
                spawnPool[s]._lastSpawned = mm;
            }
        }
    }
}

void tickLava() {
    int A, B, p, i, flicker;
    long mm = millis();
    Lava LP;
    for(i = 0; i<LAVA_COUNT; i++) {
        flicker = random8(5);
        LP = lavaPool[i];
        if(LP.Alive()) {
            A = getLED(LP._left);
            B = getLED(LP._right);
            if(LP._state == 0) {
                if(LP._lastOn + LP._offtime < mm) {
                    LP._state = 1;
                    LP._lastOn = mm;
                }
                for(p = A; p<= B; p++) {
                    leds[p] = CRGB(3+flicker, (3+flicker)/1.5, 0);
                }
            } else if(LP._state == 1) {
                if(LP._lastOn + LP._ontime < mm) {
                    LP._state = 0;
                    LP._lastOn = mm;
                }
                for(p = A; p<= B; p++) {
                    leds[p] = CRGB(150+flicker, 100+flicker, 0);
                }
            }
        }
        lavaPool[i] = LP;
    }
}

bool tickParticles() {
    bool stillActive = false;
    for(int p = 0; p < PARTICLE_COUNT; p++) {
        if(particlePool[p].Alive()) {
            particlePool[p].Tick(USE_GRAVITY, BEND_POINT);
            leds[getLED(particlePool[p]._pos)] += CRGB(particlePool[p]._power, 0, 0);// * particlePool[p]._color;
            stillActive = true;
        }
    }
    return stillActive;
}

void tickConveyors() {
    int b, dir, n, i, ss, ee;
    long m = 10000+millis();
    playerPositionModifier = 0;
    
    for(i = 0; i<CONVEYOR_COUNT; i++) {
        if(conveyorPool[i]._alive) {
            dir = conveyorPool[i]._dir;
            ss = getLED(conveyorPool[i]._startPoint);
            ee = getLED(conveyorPool[i]._endPoint);
            for(int led = ss; led<ee; led++) {
                b = 5;
                if(dir == -1) n = (led + (m/100)) % 5;
                else n = (-led + (m/100)) % 5;
                b = (5-n)/2.0;
                if(b > 0) leds[led] = CRGB(0, 0, b);
            }
            
            if(playerPosition > conveyorPool[i]._startPoint && playerPosition < conveyorPool[i]._endPoint) {
                if(dir == -1) {
                    playerPositionModifier = -(MAX_PLAYER_SPEED-4);
                } else{
                    playerPositionModifier = (MAX_PLAYER_SPEED-4);
                }
            }
        }
    }
}

void drawAttack() {
    if(!attacking) return;
    int n = map(millis() - attackMillis, 0, ATTACK_DURATION, 100, 5);
    for(int i = getLED(playerPosition-(ATTACK_WIDTH/2))+1; i<=getLED(playerPosition+(ATTACK_WIDTH/2))-1; i++) {
        leds[i] = CRGB(0, 0, n);
    }
    if(n > 90) {
        n = 255;
        leds[getLED(playerPosition)] = CRGB(255, 255, 255);
    } else{
        n = 0;
        leds[getLED(playerPosition)] = CRGB(0, 255, 0);
    }
    leds[getLED(playerPosition-(ATTACK_WIDTH/2))] = CRGB(n, n, 255);
    leds[getLED(playerPosition+(ATTACK_WIDTH/2))] = CRGB(n, n, 255);
}

int getLED(int pos) {
    // The world is 1000 pixels wide, this converts world units into an LED number
    return constrain((int)map(pos, 0, 1000, 0, NUM_LEDS-1), 0, NUM_LEDS-1);
}

int getLEDrev(int led) {
    //reverses getLED(); x = getLEDrev(getLED(x))
    return constrain((int)map(led, 0, NUM_LEDS-1, 0, 1000), 0, 1000);
}

bool inLava(int pos) {
    // Returns if pos is in active lava
    int i;
    Lava LP;
    for(i = 0; i<LAVA_COUNT; i++) {
        LP = lavaPool[i];
        if(LP.Alive() && LP._state == 1) {
            if(LP._left < pos && LP._right > pos) return true;
        }
    }
    return false;
}

void updateLives() {
    // Updates the life LEDs to show how many lives the player has left
    for(int i = 0; i<3; i++) {
       digitalWrite(lifeLEDs[i], lives>i?HIGH:LOW);
    }
}


// ---------------------------------
// --------- SCREENSAVER -----------
// ---------------------------------
void screenSaverTick() {
    int n, c, i;
    long mm = millis();
    int mode = (mm/20000)%2;
    
    for(i = 0; i<NUM_LEDS; i++) {
        leds[i].nscale8(250);
    }
    if(mode == 0) {
        // Marching green <> orange
        n = (mm/250)%10;
        //b = 10+((sin(mm/500.00)+1)*20.00);
        c = 20+((sin(mm/5000.00)+1)*33);
        for(i = 0; i<NUM_LEDS; i++) {
            if(i%10 == n) {
                leds[i] = CHSV(c, 255, 150);
            }
        }
    } else if(mode == 1) {
        // Random flashes
        randomSeed(mm);
        for(i = 0; i<NUM_LEDS; i++) {
            if(random8(200) == 0) {
                leds[i] = CHSV(25, 255, 100);
            }
        }
    }

    //Life LED Screensaver
    i = (mm/500)%3;
    digitalWrite(lifeLEDs[i], random8(2) == 0?HIGH:LOW);
}

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
    
    accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    
    int a = (JOYSTICK_ORIENTATION == 0?ax:(JOYSTICK_ORIENTATION == 1?ay:az))/166;
    int g = (JOYSTICK_ORIENTATION == 0?gx:(JOYSTICK_ORIENTATION == 1?gy:gz));
    if(abs(a) < JOYSTICK_DEADZONE) a = 0;
    if(a > 0) a -= JOYSTICK_DEADZONE;
    if(a < 0) a += JOYSTICK_DEADZONE;
    MPUAngleSamples.add(a);
    MPUWobbleSamples.add(g);
    
    joystickTilt = MPUAngleSamples.getMedian();
    if(JOYSTICK_DIRECTION == 1) joystickTilt = -joystickTilt;
    joystickWobble = abs(MPUWobbleSamples.getHighest());
}


// ---------------------------------
// -------------- SFX --------------
// ---------------------------------
void SFXtilt(int amount) { 
    int f = map(abs(amount), 0, 90, 80, 900)+random8(100);
    if(playerPositionModifier < 0) f -= 400;
    if(playerPositionModifier > 0) f += 200;
    if(USE_GRAVITY && playerPosition > BEND_POINT) {
      if(amount > 0) f -= 150;
      else f += 150;
    }
    toneAC(f, min(min(abs(amount)/9, 5), MAX_VOLUME));
}
void SFXattacking() {
    int freq = map(sin(millis()/2.0)*1000.0, -1000, 1000, 500, 600)+random8(100);
    if(random8(5)== 0) {
      freq *= 3;
    }
    toneAC(freq, MAX_VOLUME);
}
void SFXdead() {
    int freq = max(1000 - (millis()-killTime), 10)+random8(100);
    freq += random8(200);
    int vol = max(MAX_VOLUME - (millis()-killTime)/200, 0);
    toneAC(freq, vol);
}
void SFXkill() {
    toneAC(2000+random8(100), MAX_VOLUME, 1500, true);
}
void SFXkey() {
    toneAC(1800+random8(100), MAX_VOLUME, 2000, true);
}
void SFXwin() {
    int freq = (millis()-stageStartTime)/3.0+random8(100);
    freq += map(sin(millis()/20.0)*1000.0, -1000, 1000, 0, 20);
    int vol max(MAX_VOLUME - (millis()-stageStartTime)/200, 0);
    toneAC(freq, vol);
}
void SFXloose() {
    int freq = (1000 - (millis()-stageStartTime))/3.0+random8(100);
    freq += map(sin(millis()/20.0)*1000.0, -1000, 1000, 0, 20);
    int vol max(MAX_VOLUME - (millis()-stageStartTime)/200, 0);
    toneAC(freq, vol);
}

/*
void SFXmusic() {
    if(melody[melody_pos] != 0)
        toneAC(melody[melody_pos], max(MAX_VOLUME - 2,0) + map(sin(millis()%20)*1000.0, -1000, 1000, 20, 0));
            
    if(millis()-noteTime > 1000) {
        noteTime = millis();
        melody_pos++;
        if(melody_pos >= MELODY_LENGTH) melody_pos = 0;
    }
}*/
