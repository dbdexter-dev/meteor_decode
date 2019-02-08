#ifndef LRPTDEC_TUI_H
#define LRPTDEC_TUI_H

#include <stdint.h>

void tui_init(unsigned upd_interval);
void tui_deinit();
int  tui_process_input();

void tui_update_phys(uint32_t syncword, int rs_count, int total, int recv_count);
void tui_update_pktinfo(int seq, int apid, uint32_t timestamp);
int  tui_print_info(const char *msg, ...);
int  tui_wait_for_user_input();

#endif
