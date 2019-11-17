#ifndef __VIDEO_H
#define __VIDEO_H

#include <lcom/lcf.h>

int video_set_mode(uint16_t mode);
int vg_draw_pattern(uint8_t no_rectangles, uint32_t first_color, uint8_t step);

#endif /* __VIDEO_H */