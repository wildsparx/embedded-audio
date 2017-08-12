/* Copyright 2017 Asher Blum */

#include "Arduino.h"
#include <Sd2Card.h>

/*** PINS ***/
#define PWM_PIN 3
#define START_PIN 14 // A0
#define STOP_PIN 15 // A1
#define CHIP_SELECT_PIN 10

#define BLOCK_SIZE 512 /* Bytes per block in SD cards */
#define N_META_BLOCKS 2 /* Reserve first two blocks for metadata */
#define CMD_NULL 0 /* Default command byte - no-op */
#define CMD_STOP 2
#define CMD_LOOP 3 /* Play from beginning */



// we have 2K of RAM on 328P

void handle_cmd(uint8_t cmd);


class Source {
public:
  uint8_t buf[2 * BLOCK_SIZE];
  volatile uint16_t BUF_POS;
  uint16_t blockno; // allows 17 minutes
  uint8_t want_reset; // reset on next sample

  Source(): BUF_POS(0), blockno(N_META_BLOCKS), want_reset(0) {}
  
  void reset() {
    blockno = N_META_BLOCKS;
    BUF_POS = 1; /* skip first command byte */
    want_reset = 0;
    memset(buf, 0, sizeof(buf));
  }

  uint8_t get_sample() {
    if(want_reset) {
      reset();
      
    }
    uint8_t rv = buf[BUF_POS];
    BUF_POS ++;
    if(BUF_POS == 512) { // skip middle control byte
      BUF_POS = 513;
    }

    if(BUF_POS >= sizeof(buf)) {
      BUF_POS = 1; // skip first control byte
    }
    return rv;
  }

  void wait_for_first_half() {
    while(BUF_POS < sizeof(buf)/2) {}
  }

  void wait_for_second_half() {
    while(BUF_POS >= sizeof(buf)/2) {}
  }

  void reset_on_sample() {
    want_reset = 1;
    
  }
  /* loop until the specified buffer half is free */

  void wait_for_buffer_half(uint8_t half) {
    if(!half) {
      while(BUF_POS < sizeof(buf)/2) {}
      return;
    }
    while(BUF_POS >= sizeof(buf)/2) {}
  }

  void read_block_from_card(Sd2Card *sd_card, uint8_t half) {
    wait_for_buffer_half(half);
    sd_card->readBlock(blockno, buf + half * BLOCK_SIZE);
    handle_cmd(buf[half * BLOCK_SIZE]);
    blockno ++;
  }
};

/*** Globals ***/

Sd2Card card;
Source source;
uint8_t PLAYING;

/* Gets called once per frame - approx 62x per second */
inline void handle_cmd(uint8_t cmd) {
  switch(cmd) {
    case CMD_NULL:
      break;
    case CMD_STOP:
      Serial.println("STOP");
      PLAYING = 0;
      break;
    case CMD_LOOP:
      Serial.println("loop");
      source.reset();
      break;
    default:
      Serial.print("cmd ");
      Serial.println((int)cmd);
  }
}

void wait_for_serial_connect() {
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
}

void fatal_error(const char *msg) {
  Serial.print("ERROR: ");
  Serial.println(msg);
  while(1) {};
}

inline uint8_t pressedStop() {
  return digitalRead(STOP_PIN) ? 0 : 1;
}

/*********** ao audio output using pwm *************/
void ao_enable_interrupt() {
  TIMSK2 |= (1<<TOIE2);
}

void ao_disable_interrupt() {
  TIMSK2 &= ~(1<<TOIE2);
}

/* per-sample callback - called at sample rate */
ISR(TIMER2_OVF_vect) {
  if(!PLAYING) {
    return;
  }
  OCR2B = source.get_sample();
}

void ao_init() {
  pinMode(PWM_PIN, OUTPUT);
  TCCR2A = _BV(COM2B1) | _BV(WGM20); // ph correct pwm
  TCCR2B = _BV(CS20); // full clock

  // OCR2A is pin 11; can't use, connects to SD card DI
  OCR2B = 127; // pin 3
}

/************************/


void init_card() {
  if (!card.init(SPI_FULL_SPEED, CHIP_SELECT_PIN)) {
    fatal_error("card init");
  }
}

/* called by Arduino framework at startup */
void setup() {
  Serial.begin(9600);
  Serial.println("initializing");
  pinMode(CHIP_SELECT_PIN, OUTPUT);     // change this to 53 on a mega
  init_card();
  ao_init();
  pinMode(START_PIN, INPUT_PULLUP);
  pinMode(STOP_PIN, INPUT_PULLUP);
}


/* Play audio by enabling interrupt-based player
   and feeding data from sd card to Source. Does
   not return until done playing */

void play() {
  uint8_t half = 0;
  source.reset();
  PLAYING = 1;

  while(1) {
    if(!PLAYING) {
      return;
    }
    source.read_block_from_card(&card, half);

    if(pressedStop()) {
      PLAYING = 0;
      return;
    }
    half = 1 - half;
  }
}

/* called by Arduino framework */
void loop() {
  PLAYING = 0;
  uint8_t start_val, stop_val;
  ao_enable_interrupt();
  while(1) {
    start_val = digitalRead(START_PIN);
    stop_val = digitalRead(STOP_PIN);
    if(!start_val && stop_val) {
      play();
    }
  }
}

