/*
 * song.h
 *
 *  Created on: 10 kwi 2025
 *      Author: student
 */

#ifndef SONG_H_
#define SONG_H_

static void playNote(uint32_t note, uint32_t durationMs);
static uint32_t getNote(uint8_t ch);
static uint32_t getDuration(uint8_t ch);
static uint32_t getPause(uint8_t ch);
static void playSong(uint8_t *song);


#endif /* SONG_H_ */
