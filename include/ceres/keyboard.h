// Keyboard device (0xFF050000). Reports key *events* - a code plus a
// pressed/released flag - unlike the terminal's character stream.
// See CeresASM docs/07-IO-Devices-and-Ports.md. Codes are in ceres/keys.h.
//
// The device holds 64 events; a full queue DROPS new ones, so drain it every frame. Each event also
// raises interrupt 19 (IRQ_KEYBOARD), which a program may attach a handler to (ceres/irq.h).
// Without a window (plain `ceres run`) nothing ever arrives: use the terminal instead.

#pragma once

#include "../ceres.h"
#include "keys.h"

#define KBD_STATUS         (KEYBOARD_BASE + 0x00)  // read: bit0 set when an event is queued
#define KBD_EVENT          (KEYBOARD_BASE + 0x04)  // read: pop one event
#define KBD_BLOCK_READ_CNT (KEYBOARD_BASE + 0x10)  // read: events the last block read drained
#define KBD_BLOCK_ADDR     (KEYBOARD_BASE + 0xF0)
#define KBD_BLOCK_LEN      (KEYBOARD_BASE + 0xF4)
#define KBD_BLOCK_CMD      (KEYBOARD_BASE + 0xF8)  // write: 1 drains the queue into RAM

#define KBD_EVENT_READY     0x01
#define KBD_EVENT_PRESSED   0x80000000u   // bit31: 1 = pressed, 0 = released
#define KBD_EVENT_CODE_MASK 0x7FFFFFFFu   // bits 30:0 hold the key code
#define KBD_BLOCK_CMD_READ  0x01
#define KBD_QUEUE_SIZE      64            // events the device can hold

int  kbd_event_ready(void);              // nonzero when an event is queued
unsigned int kbd_read_event(void);       // pop one event; 0 when empty
int  kbd_is_pressed(unsigned int event); // nonzero if the pressed bit is set
int  kbd_keycode(unsigned int event);    // bits 30:0

// Drains up to `max` events (at most KBD_QUEUE_SIZE) into `events` with ONE block transfer, oldest
// first, and returns how many. 0 when the queue is empty.
int  kbd_read_block(unsigned int* events, int max);
int  kbd_flush(void);                    // throws every queued event away; returns how many
