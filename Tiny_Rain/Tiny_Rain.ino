/*
Final Project for the Integration Workshop I subject.

This game was made by:
João Vitor Caversan
Alfons de Andrade
Guilherme Koller 

If you have any questions, contact us throught this e-mail:
joaopassos@alunos.utfpr.edu.br

All functions functionalities are commented in front of their headers or over the actual
implementation. 
*/

#include <EEPROM.h>
#include <time.h>
#include "font6x8AJ.h"
#include <avr/sleep.h>
#include <avr/interrupt.h> // needed for the additional interrupt

#define DIGITAL_WRITE_HIGH(PORT) PORTB |= (1 << PORT)
#define DIGITAL_WRITE_LOW(PORT) PORTB &= ~(1 << PORT)

// Routines to set and clear bits (used in the sleep code)
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

// Defines for OLED output
#define SSD1306XLED_H
#define SSD1306_SCL   PORTB3  // SCL, Pin 4 on SSD1306 Board - for webbogles board
#define SSD1306_SDA   PORTB4 // SDA, Pin 3 on SSD1306 Board - for webbogles board
#define SSD1306_SA    0x78  // Slave address

#define WINSCORE 7

//Struct used for the rain falling, based on a doubly linked list
typedef struct rain
{
  int posX, posY;
  struct rain* previous;
  struct rain* next;
}List;

// Functions for moving bytes bit per bit in order to draw everything correctly
byte doDrawLS(byte);
byte doDrawRS(byte);
byte doDrawLSP(byte, byte);
byte doDrawRSP(byte, byte);
// Functions used for this game expecifically
void playTinyRain(void);
void beep(int, int);
void drawPlayer(byte startRow, byte endRow);
void drawRainBlock(byte startRow, byte endRow, List* r);// Explanation over the actual function 
void drawShot(byte startRow, byte endRow, int* s);
void updateRain(List* r, int* s);
void updateShots(int* s);
//Functions used in doubly linked list
List* createList ();
List* insertRearRain (List *q, int x);
int kill (List* dead);// free one node
void murder (List* r);// free all nodes int the list           


//Functions used for management of the data sent to the display
void doNumber (int x, int y, int value);
void ssd1306_init(void);
void ssd1306_xfer_start(void);
void ssd1306_xfer_stop(void);
void ssd1306_send_byte(uint8_t byte);
void ssd1306_send_bit(void);
void ssd1306_send_command(uint8_t command);
void ssd1306_send_data_start(void);
void ssd1306_send_data_stop(void);
void ssd1306_setpos(uint8_t x, uint8_t y);
void ssd1306_fillscreen(uint8_t fill_Data);
void ssd1306_char_f6x8(uint8_t x, uint8_t y, const char ch[]);
void ssd1306_draw_bmp(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t bitmap[]);

/*
  General variables used throughout the code
*/
int playerOffset = 0; // y offset of the top of the player
int playerXposition = 0; // player's offset according to the left side of the screen
int score = 0;
int top = 0;
int fire = 0; // Used for limiting the firepower
int totalRun = 0;
int spawnTimer = 100;// The frequency of which the blocks spawn

boolean newHigh = 0;
boolean stopAnimate = 0;
boolean mute = 0;
byte i;


// Interrupt handlers
ISR(PCINT0_vect) { // PB0 pin button interrupt
}

void playerIncRain() { // PB2 pin button interrupt
}

// Arduino stuff - setup
void setup() {
  DDRB = 0b00000010;    // set PB1 as output (for the speaker)
  PCMSK = 0b00000001; // pin change mask: listen to portb bit 1
  GIMSK |= 0b00100000;  // enable PCINT interrupt
  sei();          // enable all interrupts
}

// Arduino stuff - loop
void loop() {
  ssd1306_init();
  int flag = 0;

  while (flag == 0)
  {
    ssd1306_fillscreen(0x00);

    /* Since no lower case characters, nor symbols, are used in the game, all of them have been "commented" from the ASCII table,
    but the hexadecimal numbers used are still there. You can change it at any time by going to font6x8.h file.
    */
    ssd1306_char_f6x8(8, 1, "   --------------");
    ssd1306_char_f6x8(8, 2, "        TINY     ");
    ssd1306_char_f6x8(8, 4, "        RAIN     ");
    ssd1306_char_f6x8(8, 5, "   --------------");
    ssd1306_char_f6x8(8, 7, "       GRUPO 1   ");

    long startT = millis();
    long nowT =0;
    boolean sChange = 0;
    while(digitalRead(0) == HIGH) {
      nowT = millis();
      if (nowT - startT > 2000) {
        sChange = 1;     
        if (digitalRead(2) == HIGH) {
          EEPROM.write(0,0);
          EEPROM.write(1,0);
          ssd1306_char_f6x8(8, 0, "-HIGH SCORE RESET-");  
        } else if (mute == 0) { mute = 1; ssd1306_char_f6x8(32, 0, "-- MUTE --"); } else { mute = 0; ssd1306_char_f6x8(23, 0, "-- SOUND ON --");  }    
        break;
      }
      if (sChange == 1) break;
    }  
    while(digitalRead(0) == HIGH);

    if (sChange == 0) {
      delay(400);

      for (int k = 2; k>=0;k--){
        for (playerOffset = 55; playerOffset>0+(k*10);playerOffset--) {
          drawPlayer(0,8);
          beep(2,200+playerOffset);
          delay(2);
        }
        for (playerOffset = 0+(k*10); playerOffset<55;playerOffset++) {
          drawPlayer(0,8);
          beep(2,200+playerOffset);
          delay(2);
        }
      }

      delay(600);
      ssd1306_init();
      ssd1306_fillscreen(0x00);
      stopAnimate = 0;

      srand(time(NULL));
      playTinyRain();

      ssd1306_fillscreen(0x00);
      ssd1306_char_f6x8(11, 1, "----------------");
      ssd1306_char_f6x8(11, 2, "G A M E  O V E R");
      ssd1306_char_f6x8(11, 3, "----------------");
      ssd1306_char_f6x8(37, 5, "SCORE:");
      doNumber(75, 5, score);
      for (int i = 700; i>200; i = i - 50){
      beep(30,i);
      };
      if (!newHigh) {
        ssd1306_char_f6x8(21, 7, "HIGH SCORE:");
        doNumber(88, 7, top);
      }
      delay(2000);
      if (newHigh) {
        ssd1306_fillscreen(0x00);
        ssd1306_char_f6x8(10, 1, "----------------");
        ssd1306_char_f6x8(10, 3, " NEW HIGH SCORE ");
        ssd1306_char_f6x8(10, 7, "----------------");
        doNumber(50,5,top);
        for (int i = 700; i>200; i = i - 50){
        beep(30,i);
        }
        newHigh = 0;
        delay(2700);    
      }

      ssd1306_fillscreen(0x00);
      
        ssd1306_char_f6x8(10, 1, "    CONTINUE    ");
        ssd1306_char_f6x8(10, 3, "   LEFT   YES    ");
        ssd1306_char_f6x8(10, 5, "   RIGHT  NO   ");

      while (flag == 0)
      {
        if (digitalRead(0) == HIGH)
        {
          break;
        }
        if (digitalRead(2) == HIGH)
        {
          flag = 1;
          system_sleep();
          break;
        }
      } 
    }
  }

  system_sleep();
}

void doNumber (int x, int y, int value) {
  char temp[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  itoa(value, temp, 10);
  ssd1306_char_f6x8(x, y, temp);
}

void ssd1306_init(void) {
  DDRB |= (1 << SSD1306_SDA); // Set port as output
  DDRB |= (1 << SSD1306_SCL); // Set port as output

  ssd1306_send_command(0xAE); // display off
  ssd1306_send_command(0x00); // Set Memory Addressing Mode
  ssd1306_send_command(0x10); // 00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
  ssd1306_send_command(0x40); // Set Page Start Address for Page Addressing Mode,0-7
  ssd1306_send_command(0x81); // Set COM Output Scan Direction
  ssd1306_send_command(0xCF); // ---set low column address
  ssd1306_send_command(0xA1); // ---set high column address
  ssd1306_send_command(0xC8); // --set start line address
  ssd1306_send_command(0xA6); // --set contrast control register
  ssd1306_send_command(0xA8);
  ssd1306_send_command(0x3F); // --set segment re-map 0 to 127
  ssd1306_send_command(0xD3); // --set normal display
  ssd1306_send_command(0x00); // --set multiplex ratio(1 to 64)
  ssd1306_send_command(0xD5); //
  ssd1306_send_command(0x80); // 0xa4,Output follows RAM content;0xa5,Output ignores RAM content
  ssd1306_send_command(0xD9); // -set display offset
  ssd1306_send_command(0xF1); // -not offset
  ssd1306_send_command(0xDA); // --set display clock divide ratio/oscillator frequency
  ssd1306_send_command(0x12); // --set divide ratio
  ssd1306_send_command(0xDB); // --set pre-charge period
  ssd1306_send_command(0x40); //
  ssd1306_send_command(0x20); // --set com pins hardware configuration
  ssd1306_send_command(0x02);
  ssd1306_send_command(0x8D); // --set vcomh
  ssd1306_send_command(0x14); // 0x20,0.77xVcc
  ssd1306_send_command(0xA4); // --set DC-DC enable
  ssd1306_send_command(0xA6); //
  ssd1306_send_command(0xAF); // --turn on oled panel
}

void ssd1306_xfer_start(void) {
  DIGITAL_WRITE_HIGH(SSD1306_SCL);  // Set to HIGH
  DIGITAL_WRITE_HIGH(SSD1306_SDA);  // Set to HIGH
  DIGITAL_WRITE_LOW(SSD1306_SDA);   // Set to LOW
  DIGITAL_WRITE_LOW(SSD1306_SCL);   // Set to LOW
}

void ssd1306_xfer_stop(void) {
  DIGITAL_WRITE_LOW(SSD1306_SCL);   // Set to LOW
  DIGITAL_WRITE_LOW(SSD1306_SDA);   // Set to LOW
  DIGITAL_WRITE_HIGH(SSD1306_SCL);  // Set to HIGH
  DIGITAL_WRITE_HIGH(SSD1306_SDA);  // Set to HIGH
}

void ssd1306_send_bit(boolean thisbit) {
  if (thisbit) {
    DIGITAL_WRITE_HIGH(SSD1306_SDA);
  } else {
    DIGITAL_WRITE_LOW(SSD1306_SDA);
  }
  DIGITAL_WRITE_HIGH(SSD1306_SCL);
  DIGITAL_WRITE_LOW(SSD1306_SCL);
  DIGITAL_WRITE_HIGH(SSD1306_SDA);
  DIGITAL_WRITE_HIGH(SSD1306_SCL);
  DIGITAL_WRITE_LOW(SSD1306_SCL);
}

void ssd1306_send_byte(uint8_t byte) {
  uint8_t i;
  for (i = 0; i < 8; i++)
  {
    if ((byte << i) & 0x80)
      DIGITAL_WRITE_HIGH(SSD1306_SDA);
    else
      DIGITAL_WRITE_LOW(SSD1306_SDA);

    DIGITAL_WRITE_HIGH(SSD1306_SCL);
    DIGITAL_WRITE_LOW(SSD1306_SCL);
  }
  DIGITAL_WRITE_HIGH(SSD1306_SDA);
  DIGITAL_WRITE_HIGH(SSD1306_SCL);
  DIGITAL_WRITE_LOW(SSD1306_SCL);
}

void ssd1306_send_command(uint8_t command) {
  ssd1306_xfer_start();
  ssd1306_send_byte(SSD1306_SA);  // Slave address, SA0=0
  ssd1306_send_byte(0x00);  // write command
  ssd1306_send_byte(command);
  ssd1306_xfer_stop();
}

void ssd1306_send_data_start(void) {
  ssd1306_xfer_start();
  ssd1306_send_byte(SSD1306_SA);
  ssd1306_send_byte(0x40);  //write data
}

void ssd1306_send_data_stop(void) {
  ssd1306_xfer_stop();
}

void ssd1306_setpos(uint8_t x, uint8_t y)
{
  if (y > 7) return;
  ssd1306_xfer_start();
  ssd1306_send_byte(SSD1306_SA);  //Slave address,SA0=0
  ssd1306_send_byte(0x00);  //write command

  ssd1306_send_byte(0xb0 + y);
  ssd1306_send_byte(((x & 0xf0) >> 4) | 0x10); // |0x10
  ssd1306_send_byte((x & 0x0f) | 0x01); // |0x01

  ssd1306_xfer_stop();
}

void ssd1306_fillscreen(uint8_t fill_Data) {
  uint8_t m, n;
  for (m = 0; m < 8; m++)
  {
    ssd1306_send_command(0xb0 + m); //page0-page1
    ssd1306_send_command(0x00);   //low column start address
    ssd1306_send_command(0x10);   //high column start address
    ssd1306_send_data_start();
    for (n = 0; n < 128; n++)
    {
      ssd1306_send_byte(fill_Data);
    }
    ssd1306_send_data_stop();
  }
}

void ssd1306_char_f6x8(uint8_t x, uint8_t y, const char ch[]) {
  uint8_t c, i, j = 0;
  while (ch[j] != '\0')
  {
    c = ch[j] - 32;
    if (c > 0) c = c - 12;
    if (c > 15) c = c - 6;
    if (c > 40) c = c - 6;
    if (x > 126)
    {
      x = 0;
      y++;
    }
    ssd1306_setpos(x, y);
    ssd1306_send_data_start();
    for (i = 0; i < 6; i++)
    {
      ssd1306_send_byte(pgm_read_byte(&ssd1306xled_font6x8[c * 6 + i]));
    }
    ssd1306_send_data_stop();
    x += 6;
    j++;
  }
}

void system_sleep() {
  ssd1306_fillscreen(0x00);
  ssd1306_send_command(0xAE);
  cbi(ADCSRA, ADEN);                   // switch Analog to Digitalconverter OFF
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // sleep mode is set here
  sleep_enable();
  sleep_mode();                        // System actually sleeps here
  sleep_disable();                     // System continues execution here when watchdog timed out
  sbi(ADCSRA, ADEN);                   // switch Analog to Digitalconverter ON
  ssd1306_send_command(0xAF);
}

void beep(int bCount, int bDelay) {
  if (mute) return;
  for (int i = 0; i <= bCount; i++) {
    digitalWrite(1, HIGH);
    for (int i2 = 0; i2 < bDelay; i2++) {
      __asm__("nop\n\t");
    } digitalWrite(1, LOW);
    for (int i2=0; i2<bDelay; i2++) {
      __asm__("nop\n\t");
    }
  }
}

/* ------------------------
    Tiny Rain game code
*/
void playTinyRain() {
  
  totalRun = 0;
  spawnTimer = 100;
  playerOffset = 56;
  playerXposition = 2;
  newHigh = 0;
  score = 0; 
  stopAnimate = 0;
  fire = 0;
  List* storm = NULL;
  int shots[3] = {0}; // shots[0] == permission to shoot, shots[1] == shot x position, shots[2] == shot y position
  
  randomSeed(0);
  
  while (stopAnimate == 0) {
    // Total distance isn't really distance - it's a measure of how long the game's been running - used to stop the game among other things
    totalRun++;

    // Increment score every 10 cycles
    if (totalRun%10 == 0)
      score++;
    
    // Draws the score of this run
    doNumber(0, 1, score);

    /*
     * NORMAL BUTTON USE
     */
    // With the left button down - the ball goes left
    if (digitalRead(0) == HIGH) {
      playerXposition -= 1;
      if (playerXposition < 1)
        playerXposition = 1;
    }
   
    // With the right button pressed - the ball goes right 
    if (digitalRead(2) == HIGH) {
      playerXposition += 1;
      if (playerXposition > 117 )
        playerXposition = 117;
    }

    // With the fire button pressed - it fires
    if (analogRead(0) < 940) fire = 1;

    if ((fire == 1) && (shots[0] == 0))
    {
      shots[0] = 1;
      shots[1] = playerXposition+6;
      shots[2] = playerOffset;
    }

    // Spawning the raindrops
    if (totalRun % spawnTimer == 0)
    {
      storm = insertRearRain(storm, rand()%120);
      fire = 0;
    }
    
    
    // Updating the spawnTimer
    if (totalRun % 1000 == 0)
    {
      spawnTimer -= 3;
    }   

    // Fill in the bird in the top couple of rows
    drawPlayer(7, 8);
    // Updates and draws the shots taken
    updateShots(shots);
    // Updates and draws the rain
    updateRain(storm, shots);
  }
  murder(storm);
  free(shots);
  delay(1500);
  top = EEPROM.read(0);
  top = top << 8;
  top = top |  EEPROM.read(1);

  if (score > top) { 
    top = score;
    EEPROM.write(1,score & 0xFF); 
    EEPROM.write(0,(score>>8) & 0xFF); 
    newHigh = 1;
  }
}

byte doDrawRS(byte P2) {
  return(B00000011 >> P2);
}
byte doDrawLS(byte P2) {
  return(B00000011 << P2);
}


byte doDrawRSP(byte column, byte P2) {
  switch(column) {
  case 0:
  return((B01000000) >> P2);
  break;
  case 1:
  return((B01100000) >> P2);
  break;
  case 2:
  return((B01110000) >> P2);
  break;
  case 3:
  return((B00111000) >> P2);
  break;
  case 4:
  return((B11111100) >> P2);
  break;
  case 5:
  return((B10011111) >> P2);
  break;
  case 6:
  return((B00100101) >> P2);
  break;
  default:
  return((B00000110) >> P2);
  break;
  }
}

byte doDrawLSP(byte column, byte P2) {
  switch(column) {
  case 0:
  return((B01000000) << P2);
  break;
  case 1:
  return((B01100000) << P2);
  break;
  case 2:
  return((B01110000) << P2);
  break;
  case 3:
  return((B00111000) << P2);
  break;
  case 4:
  return((B11111100) << P2);
  break;
  case 5:
  return((B10011111) << P2);
  break;
  case 6:
  return((B00100101) << P2);
  break;
  default:
  return((B00000110) << P2);
  break;
  }
}

void drawPlayer(byte startRow, byte endRow) {
    for (byte l = startRow; l <= endRow; l++) {
        ssd1306_setpos(playerXposition, l);
        ssd1306_send_data_start();
        ssd1306_send_byte(B00000000);
        ssd1306_send_byte(B00000000);
        for (byte r = 8; r<16; r++) {
          // bird with LS only'
          if (l == playerOffset/8) {
            ssd1306_send_byte(doDrawLSP(r-8, playerOffset % 8));
          // bird with RS only
          } 
          else if (l == playerOffset/8 + 1) {
            ssd1306_send_byte(doDrawRSP(r-8, 8- playerOffset % 8));
          } 
          else {
            ssd1306_send_byte(B00000000);
          }
        }
        ssd1306_send_byte(B00000000);
        ssd1306_send_byte(B00000000);
        ssd1306_send_data_stop();
    }
}
/*
Each 8 bits (a byte) fills 8 pixels in a downward line and automatically jumps to the nexte column, 
filling it up with each bit that you send.
When a block is within 2 lines (8 pixels wide each), that's when this function shines.
It moves the block "n" pixels down in the given line, and "8-n" pixels up in the next line, completing 
the draw from the bottom.
The same logic is applied to all drawn things in this game.
*/
void drawRainBlock(byte startRow, byte endRow, List* r)
{
  for (byte l=startRow ; l<=endRow ; l++)
  {
    ssd1306_setpos(r->posX, l);
    ssd1306_send_data_start();
    for (byte c=0 ; c<3 ; c++)
    {
      if (l == r->posY/8)
        ssd1306_send_byte(B11111111 << ((r->posY)%8));
      else if (l == r->posY/8 + 1)
        ssd1306_send_byte(B11111111 >> (8 - ((r->posY)%8)));
      else 
        ssd1306_send_byte(B00000000);
    }
    ssd1306_send_data_stop();
  }
}

void drawShot(byte startRow, byte endRow, int* s)
{
  for (byte l=startRow ; l<=endRow ; l++)
  {
    ssd1306_setpos(s[1], l);
    ssd1306_send_data_start();

    if (l == s[2]/8)
      ssd1306_send_byte(B11100000 >> (8 - ((s[2])%8)));
    else if (l == s[2]/8 - 1)
      ssd1306_send_byte(B11100000 << ((s[2])%8));
    else
      ssd1306_send_byte(B00000000);

    ssd1306_send_data_stop();
  }
}

void updateRain(List* r, int* s)
{
  int flagContinue = 0;
  List* aux = r;
  while (aux != NULL)
  {
    if (totalRun%10 == 0)
      aux->posY += 1;

    if (aux->posY <= 8)
      drawRainBlock(1, 2, aux);
    else if (aux->posY < 55)
      drawRainBlock(((aux->posY)/8)-1, ((aux->posY)/8)+1, aux);
    else
      drawRainBlock(7, 8, aux);

    //Collision with player
    if (((aux->posY)+8) > playerOffset && (playerXposition-(aux->posX) < 3) && (playerXposition-(aux->posX) > (-8)))
    {
      stopAnimate = 1;
      delay(500);
    }
    //Collision with shots
    if (s[0] != 0)
    {
      if ((aux->posY + 8) >= (s[2] - 3) && ((s[1])-(aux->posX) <= 3) && ((s[1])-(aux->posX) >= (-1)))
      {
        s[0] = 0;
        s[1] = 0;
        s[2] = 7;
        beep(30, 200);
        ssd1306_fillscreen(0);
        flagContinue = 1;
        List* aux2 = aux;
        if (kill(aux2) == 1)
        {
          aux = aux->next;
          free(aux2);
        }
        break;
      }
    }
    if (flagContinue == 1)
    {
      flagContinue = 0;
      continue;
    }
    
    //Erasing the raindrops if they fall on the ground
    if ((aux->posY + 8) > 62)
    {
      List* aux3 = aux;
      if (kill(aux3) == 1)
      {
        aux = aux->next;
        free(aux3);
      }
      ssd1306_fillscreen(0);
      continue;
    }
    aux = aux->next;
  }
}

void updateShots(int* s)
{
  if (s[0] != 0)
  {
    (s[2]) >= 8 ? drawShot(((s[2])/8)-1, ((s[2])/8)+1, s) : drawShot(1, 2, s);
    // Dealing with hiting the ceiling
    if (s[2] == 0) 
    {
      ssd1306_setpos(s[1],0);
      uint8_t temp = B00000000;
      ssd1306_send_data_start();
      ssd1306_send_byte(temp);  
      ssd1306_send_data_stop();                                                          
      s[0] = 0;
      fire = 0;
    }
    else  
      s[2] -= 1;
  }
}

/*************

Functions used in doubly linked list

*************/
List* createList (){return NULL;}

List* insertRearRain (List *q, int x)
{
  List* neu = (List*)malloc(sizeof(List));
  neu->posX = x;
  neu->posY = 8;
  neu->previous = NULL;
  neu->next = q;
  if (q != NULL)
    q->previous = neu;

  return neu;
}  

int kill (List* dead)
{
  if (dead->previous == NULL && dead->next != NULL)
  {
    dead->next->previous = NULL;
    return 1;
  }
  else if (dead->next == NULL && dead->previous != NULL)
  {
    dead->previous->next = NULL;
    return 1;
  }
  else if (dead->next != NULL && dead->previous != NULL)
  {
    dead->previous->next = dead->next;
    dead->next->previous = dead->previous;
    return 1;
  }
  else
  {  
    dead = NULL;
    return 0;
  }
}

void murder (List* r)
{
  if (r != NULL)
  {
    if (r->next != NULL)
    {
      List* aux = r->next;      
      while (aux->next != NULL)
      {
        free(r);
        r = aux;
        aux = aux->next;
      }
      free(r);
      free(aux);
    }
    else
      free(r);
  }
}                    