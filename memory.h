#pragma once

#include <stdio.h>

#define TOTAL_FRAMES 32

typedef struct
{
  int occupied; // 0 for free, 1 for occupied
  int process_id; 
  int page_number;
  int reference_bit; // for page replacement
  int modified_bit; // for page replacement
  int is_pt; // 1 if this frame is used for page table, 0 otherwise
} Frame;


static inline void initialize_frames(Frame *frames){
  for (int i=0; i<TOTAL_FRAMES; i++){
    frames[i].occupied = 0;
    frames[i].process_id = -1;
    frames[i].page_number = -1;
    frames[i].reference_bit = 0;
    frames[i].modified_bit = 0;
    frames[i].is_pt = 0;
  }
}

static inline int getfreeframe(Frame *frames){
  for (int i=0; i<TOTAL_FRAMES; i++){
    if (frames[i].occupied == 0){
      return i;
    }
  }
  return -1; // no free frame
}

static inline void allocate_frame(Frame *frames, int pid, int frame_i, int page_num, int is_pt){
  if (frame_i < 0 || frame_i >= TOTAL_FRAMES){
    printf("Invalid frame index\n");
    return;
  }
  frames[frame_i].occupied = 1;
  frames[frame_i].process_id = pid;
  frames[frame_i].page_number = page_num;
  frames[frame_i].reference_bit = 1; // set reference bit on allocation
  frames[frame_i].modified_bit = 0;
  frames[frame_i].is_pt = is_pt;
}

static inline void free_process_frames(Frame *frames, int pid){
  for (int i=0; i<TOTAL_FRAMES; i++){
    if (frames[i].occupied == 1 && frames[i].process_id == pid){
      frames[i].occupied = 0;
      frames[i].process_id = -1;
      frames[i].page_number = -1;
      frames[i].reference_bit = 0;
      frames[i].modified_bit = 0;
      frames[i].is_pt = 0;
    }
  }
}

