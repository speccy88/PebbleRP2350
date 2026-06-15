#include "drivers/display/display.h"

void display_init(void) {
}

void display_clear(void) {
}

void display_set_enabled(bool enabled) {
}

void display_set_rotated(bool rotated) {

}

bool display_update_in_progress(void) {
  return false;
}

void display_update(NextRowCallback nrcb, UpdateCompleteCallback uccb) {
}

void display_update_boot_frame(uint8_t *framebuffer) {
  (void)framebuffer;
}
