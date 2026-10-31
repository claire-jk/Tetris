
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <string.h>

// =====================================
// 1. 硬體設定
// =====================================

#define DIN_PIN 23
#define CLK_PIN 18
#define CS_PIN  5

#define JOY_X_PIN 34
#define JOY_Y_PIN 35
#define JOY_SW_PIN 32

#define N_DEVICES 4
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW

MD_MAX72XX display(
  HARDWARE_TYPE,
  DIN_PIN,
  CLK_PIN,
  CS_PIN,
  N_DEVICES
);

const int WIDTH = 32;
const int HEIGHT = 8;

const int GAME_END_X = 23;
const int START_X = 27;
const int START_Y = HEIGHT / 2 - 1;

// =====================================
// 2. 遊戲資料
// =====================================


int score = 0;

// 每消除 5 格直行，自動加快一個速度階段
// 初始 180ms，每階段減少 10ms，最快 100ms
int getMoveDelay() {
  int speedLevel = score / 5;
  int moveDelay = 180 - (speedLevel * 10);

  if (moveDelay < 100) {
    moveDelay = 100;
  }

  return moveDelay;
}

bool board[HEIGHT][WIDTH] = {0};

struct Point {
  int x;
  int y;
};

struct Block {
  Point p[4];
};

// O、I、L、J、S、Z、T
Block TETRIS_SHAPES[7] = {
  {{{0,0}, {1,0}, {0,1}, {1,1}}},
  {{{0,0}, {0,1}, {0,2}, {0,3}}},
  {{{0,0}, {0,1}, {0,2}, {1,2}}},
  {{{1,0}, {1,1}, {1,2}, {0,2}}},
  {{{1,0}, {2,0}, {0,1}, {1,1}}},
  {{{0,0}, {1,0}, {1,1}, {2,1}}},
  {{{0,0}, {1,0}, {2,0}, {1,1}}}
};

// =====================================
// 3. 7-Bag 隨機生成
// =====================================

int bag[7];
int bagIndex = 7;

void refillBag() {
  for (int i = 0; i < 7; i++) {
    bag[i] = i;
  }

  // Fisher-Yates 洗牌
  for (int i = 6; i > 0; i--) {
    int j = random(i + 1);

    int temp = bag[i];
    bag[i] = bag[j];
    bag[j] = temp;
  }

  bagIndex = 0;
}

Block nextBlock() {
  if (bagIndex >= 7) {
    refillBag();
  }

  return TETRIS_SHAPES[bag[bagIndex++]];
}

// =====================================
// 4. LED 顯示
// =====================================

void setPixel(int x, int y, bool state) {
  if (x < 0 || x >= WIDTH ||
      y < 0 || y >= HEIGHT) {
    return;
  }

  display.setPoint(y, x, state);
}

void clearDisplay() {
  display.clear();
}

// =====================================
// 5. 方塊旋轉
// =====================================

Block rotateBlock(Block block) {
  int minX = 99;
  int minY = 99;

  for (int i = 0; i < 4; i++) {
    if (block.p[i].x < minX) {
      minX = block.p[i].x;
    }

    if (block.p[i].y < minY) {
      minY = block.p[i].y;
    }
  }

  Point temp[4];
  int size = 0;

  for (int i = 0; i < 4; i++) {
    temp[i].x = block.p[i].x - minX;
    temp[i].y = block.p[i].y - minY;

    int currentSize =
      max(temp[i].x, temp[i].y) + 1;

    if (currentSize > size) {
      size = currentSize;
    }
  }

  Block rotated;

  for (int i = 0; i < 4; i++) {
    rotated.p[i].x =
      (size - 1) - temp[i].y + minX;

    rotated.p[i].y =
      temp[i].x + minY;
  }

  return rotated;
}

// =====================================
// 6. 碰撞檢查
// =====================================

bool canPlace(Block block, int x, int y) {
  for (int i = 0; i < 4; i++) {
    int nx = x + block.p[i].x;
    int ny = y + block.p[i].y;

    if (nx < 0 || nx >= WIDTH ||
        ny < 0 || ny >= HEIGHT) {
      return false;
    }

    if (board[ny][nx]) {
      return false;
    }
  }

  return true;
}

// =====================================
// 7. 消除直行
// =====================================
void clearLines() {
  int x = 0;
  bool changed = false;

  // 遊戲區域範圍為 0 到 GAME_END_X (即 0~23)
  while (x <= GAME_END_X) {
    bool full = true;

    // 檢查整欄 (y: 0~7) 是否填滿
    for (int y = 0; y < HEIGHT; y++) {
      if (!board[y][x]) {
        full = false;
        break;
      }
    }

    if (full) {
      score++;
      changed = true;

      Serial.print("Score: ");
      Serial.println(score);

      // 將 x 右側的所有固定方塊向左移動一格
      for (int xx = x; xx < GAME_END_X; xx++) {
        for (int y = 0; y < HEIGHT; y++) {
          board[y][xx] = board[y][xx + 1];
        }
      }

      // 最右側邊界欄 (GAME_END_X) 清空
      for (int y = 0; y < HEIGHT; y++) {
        board[y][GAME_END_X] = false;
      }

      // 消除後不增加 x，重新檢查當前這欄 (因為右邊的列已經補上來了)
    } else {
      x++;
    }
  }

  if (changed) {
    delay(50);
  }
}

// =====================================
// 8. Game Over
// =====================================

bool checkGameOver() {
  for (int y = 0; y < HEIGHT; y++) {
    if (board[y][GAME_END_X]) {
      return true;
    }
  }

  return false;
}

// =====================================
// 9. 固定方塊
// =====================================

void lockBlock(Block block, int x, int y) {
  for (int i = 0; i < 4; i++) {
    int nx = x + block.p[i].x;
    int ny = y + block.p[i].y;

    // 只要落在顯示區域內都記錄進陣列，避免邊界點漏寫
    if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
      board[ny][nx] = true;
    }
  }

  clearLines();
}

// =====================================
// 10. 繪製方塊
// =====================================

void drawBlock(Block block, int x, int y) {
  for (int i = 0; i < 4; i++) {
    int nx = x + block.p[i].x;
    int ny = y + block.p[i].y;

    setPixel(nx, ny, true);
  }
}

// =====================================
// 11. 繪製完整畫面
// =====================================

void drawFullBoard(
  Block active,
  int ax,
  int ay,
  bool hasHold,
  Block hold,
  Block next
) {
  clearDisplay();

  // 已固定方塊
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x <= GAME_END_X; x++) {
      if (board[y][x]) {
        setPixel(x, y, true);
      }
    }
  }

  // 活動方塊
  drawBlock(active, ax, ay);

  // Next 預覽
  drawBlock(next, 28, 0);

  // Hold 預覽
  if (hasHold) {
    drawBlock(hold, 28, 4);
  }

  display.update();
}

// =====================================
// 12. 搖桿讀取
// =====================================

int readX() {
  return analogRead(JOY_X_PIN);
}

int readY() {
  return analogRead(JOY_Y_PIN);
}

bool readButton() {
  return digitalRead(JOY_SW_PIN) == LOW;
}

// =====================================
// 13. 字型：SCORE 與數字
// =====================================

const uint8_t* getFont(char c) {

  static const uint8_t SPACE[5] =
    {0, 0, 0, 0, 0};

  static const uint8_t S[5] =
    {0x46, 0x49, 0x49, 0x49, 0x31};

  static const uint8_t C[5] =
    {0x3E, 0x41, 0x41, 0x41, 0x22};

  static const uint8_t O[5] =
    {0x3E, 0x41, 0x41, 0x41, 0x3E};

  static const uint8_t R[5] =
    {0x7F, 0x09, 0x19, 0x29, 0x46};

  static const uint8_t E[5] =
    {0x7F, 0x49, 0x49, 0x49, 0x41};

  static const uint8_t COLON[5] =
    {0, 0x36, 0x36, 0, 0};

  static const uint8_t N0[5] =
    {0x3E, 0x51, 0x49, 0x45, 0x3E};

  static const uint8_t N1[5] =
    {0, 0x42, 0x7F, 0x40, 0};

  static const uint8_t N2[5] =
    {0x62, 0x51, 0x49, 0x49, 0x46};

  static const uint8_t N3[5] =
    {0x22, 0x41, 0x49, 0x49, 0x36};

  static const uint8_t N4[5] =
    {0x18, 0x14, 0x12, 0x7F, 0x10};

  static const uint8_t N5[5] =
    {0x2F, 0x49, 0x49, 0x49, 0x31};

  static const uint8_t N6[5] =
    {0x3E, 0x49, 0x49, 0x49, 0x32};

  static const uint8_t N7[5] =
    {0x01, 0x71, 0x09, 0x05, 0x03};

  static const uint8_t N8[5] =
    {0x36, 0x49, 0x49, 0x49, 0x36};

  static const uint8_t N9[5] =
    {0x26, 0x49, 0x49, 0x49, 0x3E};

  switch (c) {
    case 'S': return S;
    case 'C': return C;
    case 'O': return O;
    case 'R': return R;
    case 'E': return E;
    case ':': return COLON;
    case ' ': return SPACE;

    case '0': return N0;
    case '1': return N1;
    case '2': return N2;
    case '3': return N3;
    case '4': return N4;
    case '5': return N5;
    case '6': return N6;
    case '7': return N7;
    case '8': return N8;
    case '9': return N9;

    default: return SPACE;
  }
}

// =====================================
// 14. 跑馬燈
// =====================================

void scrollText(const char* msg, int delayTime = 100) {
  int textWidth = strlen(msg) * 6;

  for (int shift = -WIDTH; shift < textWidth; shift++) {
    clearDisplay();

    for (int i = 0; msg[i] != '\0'; i++) {
      const uint8_t* font = getFont(msg[i]);

      for (int col = 0; col < 5; col++) {
        int screenX = i * 6 + col - shift;

        if (screenX < 0 || screenX >= WIDTH) {
          continue;
        }

        for (int row = 0; row < 7; row++) {
          if (font[col] & (1 << row)) {
            setPixel(screenX, row, true);
          }
        }
      }
    }

    display.update();
    delay(delayTime);
  }
}

// =====================================
// 15. Game Over 分數
// =====================================

void showScore() {
  Serial.println("====================");
  Serial.println("GAME OVER!");

  Serial.print("Final Score: ");
  Serial.println(score);

  Serial.println("====================");

  char scoreText[20];

  snprintf(
    scoreText,
    sizeof(scoreText),
    " SCORE: %d ",
    score
  );

  while (true) {
    scrollText(scoreText);
    delay(1000);
  }
}

// =====================================
// 16. Hold 功能
// =====================================

void holdFunction(
  Block &active,
  Block &hold,
  Block &next,
  int &x,
  int &y,
  bool &hasHold,
  bool &holdUsed
) {
  if (holdUsed) return;

  if (!hasHold) {
    hold = active;
    active = next;
    next = nextBlock();

    hasHold = true;
  }
  else {
    Block temp = hold;
    hold = active;
    active = temp;
  }

  x = START_X;
  y = START_Y;

  holdUsed = true;
}

// =====================================
// 17. 向左移動或固定方塊
// =====================================

bool moveLeftOrLock(
  Block &block,
  int &x,
  int &y
) {
  if (canPlace(block, x - 1, y)) {
    x--;
    return false;
  }
  else {
    lockBlock(block, x, y);
    return true;
  }
}

// =====================================
// 18. 遊戲主程式
// =====================================

void setup() {
  Serial.begin(115200);

  display.begin();
  display.control(MD_MAX72XX::INTENSITY, 5);
  display.clear();

  pinMode(JOY_SW_PIN, INPUT_PULLUP);

  analogReadResolution(12);

  randomSeed(analogRead(33) ^ micros());

  refillBag();

  Serial.println("Tetris Start!");
}

void loop() {

  // 取得方塊
  Block active = nextBlock();
  Block next = nextBlock();

  Block hold = TETRIS_SHAPES[0];

  bool hasHold = false;
  bool holdUsed = false;

  int x = START_X;
  int y = START_Y;

  // 初始生成位置檢查
  if (!canPlace(active, x, y)) {
    Serial.println("Spawn blocked!");
    showScore();
  }

  // 單一方塊遊戲迴圈
  while (true) {

    // -------------------------------
    // Hold
    // -------------------------------

    if (readButton() && !holdUsed) {
      holdFunction(
        active,
        hold,
        next,
        x,
        y,
        hasHold,
        holdUsed
      );

      if (!canPlace(active, x, y)) {
        Serial.println("Hold spawn blocked!");
        showScore();
      }
    }

    // -------------------------------
    // 讀取搖桿
    // -------------------------------

    int joyX = readX();
    int joyY = readY();

    // -------------------------------
    // 上下移動
    // -------------------------------

    if (joyX < 1500) {
      if (canPlace(active, x, y - 1)) {
        y--;
      }
    }
    else if (joyX > 2500) {
      if (canPlace(active, x, y + 1)) {
        y++;
      }
    }

    // -------------------------------
    // 旋轉
    // -------------------------------

    if (joyY < 1500) {
      Block rotated = rotateBlock(active);

      if (canPlace(rotated, x, y)) {
        active = rotated;
      }
    }

    // -------------------------------
    // 快速左移
    // -------------------------------

    else if (joyY > 2500) {
      bool locked = moveLeftOrLock(active, x, y);

      if (locked) {
        if (checkGameOver()) {
          showScore();
        }

        break;
      }
    }

    // -------------------------------
    // 自動向左移動
    // -------------------------------

    if (canPlace(active, x - 1, y)) {
      x--;
    }
    else {
      lockBlock(active, x, y);

      if (checkGameOver()) {
        showScore();
      }

      break;
    }

    // -------------------------------
    // 更新畫面
    // -------------------------------

    drawFullBoard(
      active,
      x,
      y,
      hasHold,
      hold,
      next
    );

    delay(getMoveDelay());
  }
}
