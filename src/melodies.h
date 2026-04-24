#ifndef MELODIES_H
#define MELODIES_H

#include "pitches.h"

#define BUZZ_PIN 13

typedef struct note_model {
  int note;
  int duration;
} Note;

Note welcomeMelody[] = {
  {NOTE_AS4, 170},
  {NOTE_GS5, 150},
  {-1, -1}
};

Note readMelody[] = {
  {NOTE_AS4, 170},
  {NOTE_AS4, 170},
  {-1, -1}
};


void playMelody(Note *melody) {

  // if the melody isn't marked as empty
  if(0 > melody->duration) return;

  do {
    tone(BUZZ_PIN, melody->note, melody->duration);

    delay(melody->duration * 1.30);
    noTone(BUZZ_PIN);
    
    // while we have another note
  } while (0 <= (++melody)->duration);
}

#endif
