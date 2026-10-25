#include "src/enclink_util.h"

#include <bdlib/src/base64.h>

#include <openssl/crypto.h>
#include <openssl/rand.h>

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>

bool link_random_key(char out[LINK_KEY_LEN + 1])
{
  unsigned char raw[24];

  if (RAND_bytes(raw, sizeof(raw)) != 1)
    return false;

  bd::String encoded;
  if (!bd::base64Encode(
          bd::String(reinterpret_cast<const char *>(raw), sizeof(raw)),
          encoded)) {
    OPENSSL_cleanse(raw, sizeof(raw));
    return false;
  }
  OPENSSL_cleanse(raw, sizeof(raw));

  if (encoded.length() != LINK_KEY_LEN)
    return false;

  memcpy(out, encoded.c_str(), LINK_KEY_LEN);
  out[LINK_KEY_LEN] = '\0';
  return true;
}

bool link_random_seed(int& seed)
{
  uint32_t value = 0;

  if (RAND_bytes(reinterpret_cast<unsigned char *>(&value), sizeof(value)) != 1)
    return false;

  value &= 0x7ffffffeU;
  if (value == 0)
    value = 1;

  seed = static_cast<int>(value);
  return true;
}

bool parse_link_seed(const char *text, int& seed)
{
  if (!text)
    return false;

  while (*text == ' ' || *text == '\t' || *text == '\r')
    ++text;

  if (!*text)
    return false;

  errno = 0;
  char *end = NULL;
  long value = strtol(text, &end, 10);

  if (errno || end == text)
    return false;

  while (*end == ' ' || *end == '\t' || *end == '\r')
    ++end;

  if (*end || value < 0 || value > INT_MAX)
    return false;

  seed = static_cast<int>(value);
  return true;
}

static void strip_crlf(bd::String& message)
{
  if (!message.length())
    return;

  char *p = NULL;
  if ((p = strchr(message.mdata(), '\n')))
    *p = '\0';
  if ((p = strchr(message.mdata(), '\r')))
    *p = '\0';

  message.resize(strlen(message.c_str()));
}

bd::String format_link_message_va(const char *format, va_list va)
{
  bd::String message = bd::String::vprintf(format, va);
  strip_crlf(message);
  return message;
}

bd::String format_link_message(const char *format, ...)
{
  va_list va;

  va_start(va, format);
  bd::String message = format_link_message_va(format, va);
  va_end(va);

  return message;
}
