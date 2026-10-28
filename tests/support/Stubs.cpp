#include <cstdarg>
#include <cstddef>
#include <cstring>

#include "src/compat/strlcpy.h"

extern "C" size_t my_strlcpy(char *dst, const char *src, size_t size)
{
  const size_t src_len = strlen(src);

  if (size) {
    const size_t copy_len = src_len >= size ? size - 1 : src_len;
    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
  }

  return src_len;
}

extern "C" size_t my_strlcat(char *dst, const char *src, size_t size)
{
  const size_t dst_len = strnlen(dst, size);
  const size_t src_len = strlen(src);

  if (dst_len == size)
    return size + src_len;

  const size_t space = size - dst_len;
  const size_t copy_len = src_len >= space ? space - 1 : src_len;
  memcpy(dst + dst_len, src, copy_len);
  dst[dst_len + copy_len] = '\0';

  return dst_len + src_len;
}

void sdprintf(const char *, ...)
{
}
