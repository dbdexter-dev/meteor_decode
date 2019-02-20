#include <assert.h>
#include <locale.h>
#include <ncurses.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include "tui.h"
#include "utils.h"

#define COLS 60

static void print_banner(WINDOW *win);
static void windows_init(int rows, int cols);

static unsigned _upd_interval;
struct {
	WINDOW *banner;
	WINDOW *phys, *pktinfo;
	WINDOW *info;
} tui;

enum {
	PAIR_DEF = 1,
	PAIR_GREEN_DEF = 2,
	PAIR_RED_DEF = 3
};

void
tui_init(unsigned upd_interval)
{
	int rows, cols;

	_upd_interval = upd_interval;
	setlocale(LC_ALL, "");

	initscr();
	noecho();
	cbreak();
	curs_set(0);

	use_default_colors();
	start_color();
	init_pair(PAIR_DEF, -1, -1);
	init_pair(PAIR_RED_DEF, COLOR_RED, -1);
	init_pair(PAIR_GREEN_DEF, COLOR_GREEN, -1);

	getmaxyx(stdscr, rows, cols);

	windows_init(rows, cols);
	print_banner(tui.banner);
}

void
tui_deinit()
{
	delwin(tui.pktinfo);
	delwin(tui.phys);
	delwin(tui.banner);
	delwin(tui.info);
	endwin();
}

void
tui_update_phys(uint32_t syncword, int rs_count, int total, int recv_count)
{
	assert(tui.phys);

	werase(tui.phys);
	wmove(tui.phys, 0, 0);
	wattrset(tui.phys, A_BOLD);
	wprintw(tui.phys, "Data link layer\n");
	wattroff(tui.phys, A_BOLD);
	wprintw(tui.phys, "Total frames: %d\n", total);
	wprintw(tui.phys, "Valid frames: %d\n", recv_count);
	wprintw(tui.phys, "Syncword:  0x%08X\n", syncword);
	if (rs_count < 0) {
		wprintw(tui.phys, "RS errors: N/A\n");
	} else {
		wprintw(tui.phys, "RS errors: %d\n", rs_count);
	}
	wrefresh(tui.phys);
}

void
tui_update_pktinfo(int seq, int apid, uint32_t tstamp)
{
	assert(tui.pktinfo);

	werase(tui.pktinfo);
	wmove(tui.pktinfo, 0, 0);
	wattrset(tui.pktinfo, A_BOLD);
	wprintw(tui.pktinfo, "Last decoded packet\n");
	wattroff(tui.pktinfo, A_BOLD);
	wprintw(tui.pktinfo, "Seq:  %d\n", seq);
	wprintw(tui.pktinfo, "APID: %d\n", apid);
	wprintw(tui.pktinfo, "Time: %s\n", timeofday(tstamp));
	wrefresh(tui.pktinfo);
}

int
tui_print_info(const char *msg, ...)
{
	time_t t;
	va_list ap;
	struct tm* tm;
	char timestr[] = "HH:MM:SS";

	assert(tui.info);

	t = time(NULL);
	tm = localtime(&t);
	strftime(timestr, sizeof(timestr), "%T", tm);
	wprintw(tui.info, "(%s) ", timestr);

	va_start(ap, msg);
	vwprintw(tui.info, msg, ap);
	va_end(ap);

	return 0;
}

int
tui_process_input()
{
	int ch;
	ch = wgetch(tui.info);

	switch(ch) {
	case KEY_RESIZE:
		break;
	case 'q':
		return 1;
		break;
	default:
		break;
	}
	wrefresh(tui.info);
	return 0;
}

int
tui_wait_for_user_input()
{
	int ret;

	wtimeout(tui.info, -1);
	ret = wgetch(tui.info);
	wtimeout(tui.info, _upd_interval);

	return ret;
}

/* Static functions {{{ */
void
print_banner(WINDOW *win)
{
	mvwprintw(win, 0, 0, "\t~ LRPT Decoder v%s ~", VERSION);
	wrefresh(win);
}

void
windows_init(int rows, int cols)
{
	cols = MIN(COLS, cols);
	tui.banner = newwin(1, cols, 0, 0);
	tui.phys = newwin(5, cols/2, 2, 0);
	tui.pktinfo = newwin(5, cols/2, 2, cols/2);
	tui.info = newwin(rows - 7, cols, 8, 0);
	scrollok(tui.info, TRUE);
	wtimeout(tui.info, _upd_interval);
}
/* }}} */
