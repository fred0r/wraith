#ifndef _UPDATE_STREAM_H
#define _UPDATE_STREAM_H

#include <bdlib/src/Stream.h>
#include <bdlib/src/String.h>

enum UpdateChunkResult {
  UPDATE_CHUNK_OK,
  UPDATE_CHUNK_MISSING,
  UPDATE_CHUNK_BAD_SIZE,
  UPDATE_CHUNK_BAD_ENCODING
};

UpdateChunkResult parse_update_chunk(const char *size, const char *payload,
    bd::String& decoded);
bool next_update_chunk(bd::Stream& stream, bd::String& encoded, bool& at_eof);

#endif
