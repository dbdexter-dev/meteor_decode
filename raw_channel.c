#include <stdint.h>
#include "raw_channel.h"

void
raw_channel_init(RawChannel *ch, const char *fname)
{
	ch->fd = fopen(fname, "wb");
}

void
raw_channel_write(RawChannel *ch, uint8_t *data, int len)
{
	fwrite(data, len, 1, ch->fd);
}

void
raw_channel_close(RawChannel *ch)
{
	fclose(ch->fd);
}
