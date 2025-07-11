#ifndef SCREEN_MSC_H
#define SCREEN_MSC_H

#include <stdint.h>

// MSC screen hook functions
void screen_msc_reset(void);
void screen_msc_write_start(uint32_t num_blocks);
void screen_msc_write_block(uint32_t block_num);
void screen_msc_write_complete(void);
void screen_msc_set_block_data(uint8_t* data);

#endif