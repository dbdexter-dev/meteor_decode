#include <stdint.h>
#include "raw_channel.h"

int
raw_channel_init(RawChannel *ch, const char *fname)
{
	return !(ch->fd = fopen(fname, "wb"));
}

void
raw_channel_write(RawChannel *ch, uint8_t *data, int len)
{
	if (!ch) return;
	fwrite(data, len, 1, ch->fd);
}

void
raw_channel_close(RawChannel *ch)
{
	fclose(ch->fd);
}
