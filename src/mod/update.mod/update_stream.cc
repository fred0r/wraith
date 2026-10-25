#include "update_stream.h"

#include <bdlib/src/base64.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>

UpdateChunkResult parse_update_chunk(const char *size, const char *payload,
    bd::String& decoded)
{
  decoded = bd::String();

  if (!size || !size[0] || !payload || !payload[0])
    return UPDATE_CHUNK_MISSING;

  errno = 0;
  char *end = NULL;
  const unsigned long chunk_len = strtoul(size, &end, 10);
  const size_t payload_len = strlen(payload);

  if (errno || end == size || *end || chunk_len != payload_len)
    return UPDATE_CHUNK_BAD_SIZE;

  bd::String chunk(payload, chunk_len);
  if (!bd::base64Decode(chunk, decoded, false))
    return UPDATE_CHUNK_BAD_ENCODING;

  return UPDATE_CHUNK_OK;
}

bool next_update_chunk(bd::Stream& stream, bd::String& encoded, bool& at_eof)
{
  encoded = bd::String();
  at_eof = stream.eof();

  if (at_eof)
    return true;

  bd::String data = stream.read(1024);
  if (!bd::base64Encode(data, encoded)) {
    encoded = bd::String();
    return false;
  }

  at_eof = stream.eof();
  return true;
}
