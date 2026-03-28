#include <Arduino.h>

#include "font.h"
#include <SPI.h>

#define CCLK   13
#define CD     11
#define CT      4
#define RCLK    5
#define RD      6
#define RT      7
#define NUMBER_OF_MATRIX 8
#define ROWS             8
#define COLS (8 * NUMBER_OF_MATRIX)
#define CHAR_W 9         // 8+1 (1 col for gap)
#define TEXT_BUF_SIZE 80 // each char costs 1 byte of SRAM

volatile unsigned char display[ROWS][NUMBER_OF_MATRIX] = {{0}};
volatile uint8_t mat_i = 0;

int MSG_W = 0;
uint8_t serialIdx = 0;
char textBuf[TEXT_BUF_SIZE] = "HELLO WORLD "; // active display text
char serialBuf[TEXT_BUF_SIZE];


bool newText = false;


void Output(unsigned char row);
const uint8_t *getFont(char ch);
uint8_t getPixel(uint8_t row, int col);
void render(int offset);
void CheckSerial();
void Right();
void Left();



ISR(TIMER1_COMPA_vect)
{
  Output(mat_i);
  mat_i++;
  if (mat_i >= 8)
  {
    mat_i = 0;
  }
}

void setup()
{
  // TODO: put your setup code here, to run once:
  pinMode(CCLK, OUTPUT);
  pinMode(CD, OUTPUT);
  pinMode(CT, OUTPUT);
  pinMode(RCLK, OUTPUT);
  pinMode(RD, OUTPUT);
  pinMode(RT, OUTPUT);

  SPI.begin();
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  TCNT1 = 0;
  TCCR1A = 0;
  TCCR1B = 0;
  OCR1A = 125;
  TCCR1B = (1 << WGM12) | (1 << CS10) | (1 << CS11);
  TIMSK1 = (1 << OCIE1A);
  TIFR1 = (1 << OCF1A);


  Serial.begin(9600);
  Serial.println("STARTED");
}





void loop()
{
  static int scrollOffset = 0;

  CheckSerial();

  if (newText)
  {

    size_t len = strlen(serialBuf); // actual message length
    noInterrupts();
    memcpy(textBuf, serialBuf, len);
    memset(textBuf + len, 0, TEXT_BUF_SIZE - len); // zero everything after
    interrupts();
    scrollOffset = 0; // restart scroll from beginning
    newText = false;
    Serial.print(F("Showing: "));
    Serial.println(textBuf);
  }

  MSG_W = strlen(textBuf) * CHAR_W;

  render(scrollOffset);
  delay(40);

  if (++scrollOffset > MSG_W + COLS)
  {
    scrollOffset = 0;
  }
}




void Output(unsigned char row)
{
  digitalWrite(RT, LOW);
  digitalWrite(CT, LOW);
  shiftOut(RD, RCLK, MSBFIRST, ~(1 << row));

  for (int i = NUMBER_OF_MATRIX - 1; i >= 0; i--)
  {
    // shiftOut(CD,CCLK,MSBFIRST, display[row][i]);
    SPI.transfer(display[row][i]);
  }
  digitalWrite(RT, HIGH);
  digitalWrite(CT, HIGH);
}


const uint8_t *getFont(char ch)
{
  if (ch >= 'A' && ch <= 'Z')
    return FONT[ch - 'A'];
  else if (ch >= 'a' && ch <= 'z')
    return FONT[ch - 'a'];
  else
    return FONT_SPACE;
}

uint8_t getPixel(uint8_t row, int col)
{
  if (col < 0)
    return 0;

  uint8_t charIndex = col / CHAR_W; // Character Index
  uint8_t bitcol = col % CHAR_W;    // bttPosition
  if (bitcol == 8)
    return 0;

  if (!textBuf[charIndex])
    return 0;

  uint8_t fontByte = getFont(textBuf[charIndex])[row];
  return (fontByte >> (7 - bitcol)) & 1;
}



void render(int offset)
{

  uint8_t next[ROWS][NUMBER_OF_MATRIX] = {{0}};

  for (int r = 0; r < ROWS; r++)
  {

    for (int screenCol = 0; screenCol < COLS; screenCol++)
    {

      int msgCol = offset + screenCol - COLS;

      if (msgCol >= 0 && getPixel(r, msgCol))
      {

        // Which Module
        int whichModule = screenCol / 8;
        // Which bit inside the the module
        int bitPos = screenCol % 8;

        next[r][whichModule] |= (1 << bitPos);
      }
    }
  }

  noInterrupts();
  memcpy((void *)display, next, sizeof(next));
  interrupts();
}


void CheckSerial()
{
  while (Serial.available())
  {
    char c = Serial.read();

    if (c == '\n' || c == '\r')
    {
      if (serialIdx > 0)
      {
        serialBuf[serialIdx] = '\0';
        memset(serialBuf + serialIdx + 1, 0, TEXT_BUF_SIZE - serialIdx - 1);

        serialIdx = 0;
        newText = true;
      }
    }
    else
    {

      if (c >= 'a' && c <= 'z')
        c -= 32; //<< Force Upper case
      if (serialIdx < TEXT_BUF_SIZE - 1)
      {

        serialBuf[serialIdx++] = c;
      }
    }
  }
}


// Scroll From Left to Right
void Left()
{
  static int scrollOffset = (MSG_W + COLS) - 1;
  render(scrollOffset);
  delay(40);
  scrollOffset--;
  if (scrollOffset <= 0)
  {
    scrollOffset = (MSG_W + COLS) - 1;
  }
}


// Scroll From Right to Left


void Right()
{
  static int scrollOffset = 0;
  render(scrollOffset);
  delay(40);
  ++scrollOffset;
  if (scrollOffset > (MSG_W + COLS))
  {
    scrollOffset = 0;
  }
}
