#include <machine/int86.h>
#include <lcom/vbe.h>
#include <lcom/lcf.h>
#include <stdbool.h>
#include <limits.h>

#include "video.h"
#include "vbe_constants.h"

#include "res/Ship.xpm"

extern void *frame_buffer;
static vbe_mode_info_t info;
static uint8_t bytes_per_pixel;

void *(vg_init)(uint16_t mode) {
  static void *video_mem;

  memset(&info, 0, sizeof(info)); // Zero the struct

  if (vbe_return_mode_info(mode, &info)) {
    return MAP_FAILED;
  }

  bytes_per_pixel = info.BitsPerPixel / 8;
  if (info.BitsPerPixel % 8 != 0) {
    // Bits per pixel isn't a multiple of 8: add an extra byte
    ++bytes_per_pixel;
  }

  unsigned int vram_size = info.XResolution * info.YResolution * bytes_per_pixel;
  
  struct minix_mem_range mr;
  mr.mr_base = info.PhysBasePtr;
  mr.mr_limit = mr.mr_base + vram_size;

  if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != OK) {
    printf("Error when calling sys_privctl.\n");
    return MAP_FAILED;
  }

  video_mem = vm_map_phys(SELF, (void *) mr.mr_base, vram_size);

  if (video_set_mode(mode)) {
    printf("Error when calling video_set_mode.\n");
    return MAP_FAILED;
  }

  return video_mem;
}


int vbe_return_mode_info(uint16_t mode, vbe_mode_info_t *info_ptr) {
  if (info_ptr == NULL) {
    printf("Error occurred: null pointer.\n");
    return 1;
  }

  mmap_t map;
  lm_alloc(sizeof(*info_ptr), &map);
  phys_bytes buf = map.phys;

  struct reg86 reg;
  memset(&reg, 0, sizeof(reg)); // Zero the reg86 struct

  /* Set register values */
  reg.intno = SERVICE_VIDEO_CARD;
  reg.ax = VBE_FUNC_AX(FUNC_RETURN_VBE_MODE_INFO);
  reg.cx = VBE_MODE_LINEAR(mode);
  reg.es = PB2BASE(buf);
  reg.di = PB2OFF(buf);

  /* BIOS call */
  if (sys_int86(&reg) != OK) {
    lm_free(&map);
    printf("Error when calling sys_int86.\n");
    return 1;
  }

  /* Check return value  */
  if (reg.ah != VBE_RETURN_SUCCESS) {
    lm_free(&map);
    printf("Error occurred: VBE function not successful (AH = 0x%x).\n", reg.ah);
    return 1;
  }

  if (reg.al != VBE_RETURN_FUNC_SUPPORTED) {
    lm_free(&map);
    printf("Error occurred: VBE function not supported.\n");
    return 1;
  }

  *info_ptr = *((vbe_mode_info_t *)(map.virt));
  lm_free(&map);

  return 0;
}


int video_set_mode(uint16_t mode) {
  struct reg86 reg;
  memset(&reg, 0, sizeof(reg)); // Zero the struct

  /* Set register values */
  reg.intno = SERVICE_VIDEO_CARD;
  reg.ax = VBE_FUNC_AX(FUNC_SET_VBE_MODE);
  reg.bx = VBE_MODE_LINEAR(mode);

  /* BIOS call */
  if (sys_int86(&reg) != OK) {
    printf("Error when calling sys_int86.\n");
    return 1;
  }

  /* Check return value */
  if (reg.al != VBE_RETURN_FUNC_SUPPORTED) {
    printf("Error occurred: VBE function not supported.\n");
    return 1;
  }

  if (reg.ah != VBE_RETURN_SUCCESS) {
    printf("Error occurred: VBE function not successful.\n");
    return 1;
  }

  return 0;
}


int vg_draw_pixel(uint16_t x, uint16_t y, uint32_t color, void** buffer) {
  if (buffer == NULL) {
    printf("Error occurred: buffer not set.\n");
    return 1;
  }

  color &= BITMASK(0, info.BitsPerPixel);

  uint8_t *addr = (uint8_t *)(*buffer) + bytes_per_pixel * (x + y * info.XResolution);
  memcpy(addr, &color, bytes_per_pixel);

  return 0;
}


int vg_draw_xpm(xpm_image_t img, uint16_t x, uint16_t y, void** buffer) {
  if (img.bytes == NULL) {
    printf("Error occurred: null pixmap.\n");
    return 1;
  }

  if (*buffer == NULL) {
    printf("Error occurred: frame buffer not set.\n");
    return 1;
  }

  uint32_t transparency_color = xpm_transparency_color(img.type);
  uint32_t current_color;

  /* Draw pixelmap */
  for (uint16_t row = 0; row < img.height; row++) {
    for (uint16_t col = 0; col < img.width; col++) {
      switch (img.type) {
        case XPM_INDEXED:
          current_color = img.bytes[col + row * img.width];
          break;
        default:
          break;
      }
      
      if (current_color != transparency_color) {
        vg_draw_pixel(x + col, y + row, img.bytes[col + row * img.width], buffer);
      }
    }
  }

  return 0;
}
