/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2014 Bryan Drewery
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/* enclink.c
 *
 */


#include "common.h"
#include "enclink.h"
#include "dcc.h"
#include "net.h"
#include "misc.h"
#include "set.h"
#include "src/crypto/chacha_util.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <bdlib/src/base64.h>

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <stdarg.h>

static bool link_random_key(char out[ENC_KEY_LEN + 1])
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

  if (encoded.length() != ENC_KEY_LEN)
    return false;

  memcpy(out, encoded.c_str(), ENC_KEY_LEN);
  out[ENC_KEY_LEN] = '\0';
  return true;
}

static bool link_random_seed(int& seed)
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

static bool parse_link_seed(const char *text, int& seed)
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

bool link_type_offered(int type)
{
  if (link_find_by_type(type) == -1)
    return false;

  if (type == LINK_CLEARTEXT && !link_cleartext)
    return false;

  return true;
}

static void ghost_link_case(int idx, direction_t direction)
{
  int snum = findanysnum(dcc[idx].sock);

  if (likely(snum >= 0)) {
    char initkey[33] = "", *tmp2 = NULL;
    char *keyp = NULL, *nick1 = NULL, *nick2 = NULL;
    in_port_t port = 0;
    const char salt1[] = SALT1;
    const char salt2[] = SALT2;

    if (direction == TO) {
      keyp = socklist[snum].ikey;
      nick1 = strdup(dcc[idx].nick);
      for (int j = 0; j < dcc_total; j++) {
       if (dcc[j].type && dcc[j].sock == dcc[idx].u.relay->sock && dcc[j].type == &DCC_RELAYING) {
         nick2 = strdup(dcc[j].nick);
         break;
       }
      }
      if (!nick2)
        nick2 = strdup(conf.bot->nick);
      port = htons(dcc[idx].port);
    } else if (direction == FROM) {
      keyp = socklist[snum].okey;
      nick1 = strdup(conf.bot->nick);
      nick2 = strdup(dcc[idx].nick);

      struct sockaddr_in sa;
      socklen_t socklen = sizeof(sa);

      bzero(&sa, socklen);
      getsockname(socklist[snum].sock, (struct sockaddr *) &sa, &socklen);
      if (sa.sin_family == AF_UNIX)
        port = 0;
      else
        port = sa.sin_port;
    }

    /* initkey-gen */
    /* salt1 salt2 port hubnick leafnick */
    char tmp[SALT1LEN + 1 + SALT2LEN + 1 + 4 + 1 + HANDLEN + 1 + HANDLEN + 1] = "";
    simple_snprintf(tmp, sizeof(tmp), STR("%s@%s@%4x@%s@%s"), salt1, salt2, port, strtoupper(nick1), strtoupper(nick2));

    free(nick1);
    free(nick2);
    strlcpy(keyp, SHA1(tmp), ENC_KEY_LEN + 1);
#ifdef DEBUG
    putlog(LOG_DEBUG, "@", "Link hash for %s: %s", dcc[idx].nick, tmp);
    putlog(LOG_DEBUG, "@", "outkey (%zu): %s", strlen(keyp), keyp);
#endif
    OPENSSL_cleanse(tmp, sizeof(tmp));
    SHA1(NULL);

    if (direction == FROM) {
      int seed = 0;

      if (!link_random_key(initkey) || !link_random_seed(seed)) {
        OPENSSL_cleanse(initkey, sizeof(initkey));
        putlog(LOG_WARN, "*", STR("Unable to generate a link key for %s"), dcc[idx].nick);
        killsock(dcc[idx].sock);
        lostdcc(idx);
        return;
      }

      socklist[snum].oseed = seed;
      socklist[snum].iseed = seed;
      tmp2 = encrypt_string(salt2, initkey);
      if (!tmp2) {
        OPENSSL_cleanse(initkey, sizeof(initkey));
        putlog(LOG_WARN, "*", STR("Unable to encrypt the link handshake for %s"), dcc[idx].nick);
        killsock(dcc[idx].sock);
        lostdcc(idx);
        return;
      }
      putlog(LOG_BOTS, "*", STR("Sending encrypted link handshake to %s..."), dcc[idx].nick);

      socklist[snum].encstatus = 1;
      socklist[snum].gz = 1;

      link_send(idx, STR("elink %s %d\n"), tmp2, socklist[snum].oseed);
      free(tmp2);
      strlcpy(socklist[snum].okey, initkey, ENC_KEY_LEN + 1);
      strlcpy(socklist[snum].ikey, initkey, ENC_KEY_LEN + 1);
      socklist[snum].oseed = socklist[snum].iseed;
      OPENSSL_cleanse(initkey, sizeof(initkey));
    } else {
      socklist[snum].encstatus = 1;
      socklist[snum].gz = 1;
    }
  } else {
    putlog(LOG_MISC, "*", STR("Couldn't find socket for %s connection?? Shouldn't happen :/"), dcc[idx].nick);
    killsock(dcc[idx].sock);
    lostdcc(idx);
  }
}

static int
prand(int *seed, int range)
{
  long long i1 = *seed;

  i1 = (i1 * 0x08088405 + 1) & 0xFFFFFFFF;
  *seed = i1;
  return ((i1 * range) >> 32);
}

static void
rotate_key(char* key, int& seed)
{
  if (seed) {
    *(uint32_t *) & key[0] = prand(&seed, 0xFFFFFFFF);
    *(uint32_t *) & key[4] = prand(&seed, 0xFFFFFFFF);
    *(uint32_t *) & key[8] = prand(&seed, 0xFFFFFFFF);
    *(uint32_t *) & key[12] = prand(&seed, 0xFFFFFFFF);

    if (!seed)
      seed++;
  }
}

/* HMAC-SHA256 based key rotation for chacha-poly-v2 links.
 * Output is base64-encoded to fit the char[33] C-string key storage
 * (no embedded nulls, strlen always returns 32). The 4-byte counter is
 * intentionally serialized in host byte order to stay wire-compatible with
 * existing peers.
 */
static bool
chacha_rotate(char* key, int& seed)
{
  if (seed == 0)
    seed = 1;

  uint32_t counter_value = static_cast<uint32_t>(seed);
  unsigned char counter[sizeof(counter_value)];
  memcpy(counter, &counter_value, sizeof(counter));

  unsigned char hmac_out[32];
  unsigned int outlen = 0;

  if (!HMAC(EVP_sha256(),
            (const unsigned char *)key, ENC_KEY_LEN,
            counter, sizeof(counter),
            hmac_out, &outlen)
      || outlen != sizeof(hmac_out)) {
    OPENSSL_cleanse(hmac_out, sizeof(hmac_out));
    return false;
  }

  bd::String b64key;
  if (!bd::base64Encode(
          bd::String((const char *)hmac_out, 24), b64key)) {
    OPENSSL_cleanse(hmac_out, sizeof(hmac_out));
    return false;
  }
  OPENSSL_cleanse(hmac_out, sizeof(hmac_out));

  if (b64key.length() != ENC_KEY_LEN)
    return false;

  memcpy(key, b64key.c_str(), ENC_KEY_LEN);
  key[ENC_KEY_LEN] = '\0';
  if (seed == INT_MAX)
    seed = INT_MIN;
  else
    seed++;
  return true;
}

static int ghost_read(int snum, char *src)
{
  char *line = decrypt_string(socklist[snum].ikey, src);

  strlcpy(src, line, SGRAB + 1);
  OPENSSL_cleanse(line, strlen(line) + 1);
  free(line);
  rotate_key(socklist[snum].ikey, socklist[snum].iseed);
  return OK;
}

static const char *ghost_write(int snum, const char *src, size_t *len)
{
  static char buf[SGRAB * 2 + 64] = "";
  char *srcbuf = NULL, *line = NULL, *eol = NULL, *eline = NULL;

  const size_t bufsiz = *len + 9 + 1;
  srcbuf = (char *) calloc(1, bufsiz);
  if (!srcbuf) {
    *len = 0;
    return NULL;
  }
  strlcpy(srcbuf, src, bufsiz);
  line = srcbuf;
  buf[0] = 0;

  eol = strchr(line, '\n');
  while (eol) {
    *eol++ = 0;
    eline = encrypt_string(socklist[snum].okey, line);
    rotate_key(socklist[snum].okey, socklist[snum].oseed);
    strlcat(buf, eline, sizeof(buf));
    free(eline);
    *len = strlcat(buf, "\n", sizeof(buf));
    line = eol;
    eol = strchr(line, '\n');
  }
  if (line[0]) {
    eline = encrypt_string(socklist[snum].okey, line);
    rotate_key(socklist[snum].okey, socklist[snum].oseed);
    strlcat(buf, eline, sizeof(buf));
    free(eline);
    *len = strlcat(buf, "\n", sizeof(buf));
  }
  OPENSSL_cleanse(srcbuf, bufsiz);
  free(srcbuf);

  return buf;
}

void ghost_parse(int idx, int snum, char *buf)
{
  /* putlog(LOG_DEBUG, "*", "Got elink: %s %s", code, buf); */
  /* Set the socket key and we're linked */

  char *code = newsplit(&buf);

  if (!strcasecmp(code, STR("elink"))) {
    const char salt2[] = SALT2;
    char *tmp = decrypt_string(salt2, newsplit(&buf));
    int seed = 0;

    if (!tmp || strlen(tmp) != ENC_KEY_LEN || !parse_link_seed(buf, seed)) {
      if (tmp) {
        OPENSSL_cleanse(tmp, strlen(tmp));
        free(tmp);
      }
      putlog(LOG_WARN, "*", STR("Invalid encrypted link handshake from %s"), dcc[idx].nick);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    }

    strlcpy(socklist[snum].okey, tmp, ENC_KEY_LEN + 1);
    OPENSSL_cleanse(tmp, strlen(tmp));
    free(tmp);

    strlcpy(socklist[snum].ikey, socklist[snum].okey, ENC_KEY_LEN + 1);

    socklist[snum].iseed = seed;
    socklist[snum].oseed = seed;
    putlog(LOG_BOTS, "*", STR("Handshake with %s succeeded, we're linked."), dcc[idx].nick);
    link_done(idx);
  }
}

static void chacha20_poly1305_link_case(int idx, direction_t direction)
{
  int snum = findanysnum(dcc[idx].sock);

  if (likely(snum >= 0)) {
    char initkey[33] = "";
    char *keyp = NULL, *nick1 = NULL, *nick2 = NULL;
    in_port_t port = 0;
    const char salt1[] = SALT1;
    const char salt2[] = SALT2;

    if (direction == TO) {
      keyp = socklist[snum].ikey;
      nick1 = strdup(dcc[idx].nick);
      for (int j = 0; j < dcc_total; j++) {
       if (dcc[j].type && dcc[j].sock == dcc[idx].u.relay->sock && dcc[j].type == &DCC_RELAYING) {
         nick2 = strdup(dcc[j].nick);
         break;
       }
      }
      if (!nick2)
        nick2 = strdup(conf.bot->nick);
      port = htons(dcc[idx].port);
    } else if (direction == FROM) {
      keyp = socklist[snum].okey;
      nick1 = strdup(conf.bot->nick);
      nick2 = strdup(dcc[idx].nick);

      struct sockaddr_in sa;
      socklen_t socklen = sizeof(sa);

      bzero(&sa, socklen);
      getsockname(socklist[snum].sock, (struct sockaddr *) &sa, &socklen);
      if (sa.sin_family == AF_UNIX)
        port = 0;
      else
        port = sa.sin_port;
    }

    char tmp[SALT1LEN + 1 + SALT2LEN + 1 + 4 + 1 + HANDLEN + 1 + HANDLEN + 1] = "";
    simple_snprintf(tmp, sizeof(tmp), STR("%s@%s@%4x@%s@%s"), salt1, salt2, port, strtoupper(nick1), strtoupper(nick2));
    free(nick1);
    free(nick2);
    strlcpy(keyp, SHA1(tmp), ENC_KEY_LEN + 1);
    OPENSSL_cleanse(tmp, sizeof(tmp));
    SHA1(NULL);

    if (direction == FROM) {
      int seed = 0;

      if (!link_random_key(initkey) || !link_random_seed(seed)) {
        OPENSSL_cleanse(initkey, sizeof(initkey));
        putlog(LOG_WARN, "*", STR("Unable to generate a link key for %s"), dcc[idx].nick);
        killsock(dcc[idx].sock);
        lostdcc(idx);
        return;
      }

      socklist[snum].oseed = seed;
      socklist[snum].iseed = seed;

      bd::String encrypted;
      if (!crypto::encrypt_chacha20_poly1305_checked(
              bd::String(salt2), bd::String(initkey, ENC_KEY_LEN), bd::String(),
              encrypted, true) || !encrypted.length()) {
        OPENSSL_cleanse(initkey, sizeof(initkey));
        putlog(LOG_WARN, "*", STR("Unable to encrypt the link handshake for %s"), dcc[idx].nick);
        killsock(dcc[idx].sock);
        lostdcc(idx);
        return;
      }
      putlog(LOG_BOTS, "*", STR("Sending encrypted link handshake to %s..."), dcc[idx].nick);

      link_send(idx, STR("elink %s %d\n"), encrypted.c_str(), seed);

      socklist[snum].gz = 1;
      strlcpy(socklist[snum].okey, initkey, ENC_KEY_LEN + 1);
      strlcpy(socklist[snum].ikey, initkey, ENC_KEY_LEN + 1);
      OPENSSL_cleanse(initkey, sizeof(initkey));
    } else {
      socklist[snum].gz = 1;
    }
  } else {
    putlog(LOG_MISC, "*", STR("Couldn't find socket for %s connection?? Shouldn't happen :/"), dcc[idx].nick);
    killsock(dcc[idx].sock);
    lostdcc(idx);
  }
}


static void chacha20_poly1305_parse(int idx, int snum, char *buf)
{
  char *code = newsplit(&buf);

  if (!strcasecmp(code, STR("elink"))) {
    const char salt2[] = SALT2;
    char *payload = newsplit(&buf);
    bd::String decrypted;
    int seed = 0;

    if (!payload[0]
        || !crypto::decrypt_chacha20_poly1305_checked(
               bd::String(salt2), payload, decrypted, true)
        || decrypted.length() != ENC_KEY_LEN
        || memchr(decrypted.c_str(), '\0', ENC_KEY_LEN) != NULL
        || !parse_link_seed(buf, seed)) {
      putlog(LOG_WARN, "*", "Invalid encrypted link handshake from %s", dcc[idx].nick);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    }

    strlcpy(socklist[snum].okey, decrypted.c_str(), ENC_KEY_LEN + 1);
    strlcpy(socklist[snum].ikey, socklist[snum].okey, ENC_KEY_LEN + 1);

    socklist[snum].iseed = seed;
    socklist[snum].oseed = seed;
    putlog(LOG_BOTS, "*", STR("Handshake with %s succeeded, we're linked."), dcc[idx].nick);
    link_done(idx);
    socklist[snum].encstatus = 1;
  }
}

static int chacha20_poly1305_read(int snum, char *src)
{
  bd::String decrypted;

  if (!crypto::decrypt_chacha20_poly1305_checked(socklist[snum].ikey, src,
                                                 decrypted, false)
      || decrypted.length() > SGRAB) {
    putlog(LOG_WARN, "*", "MAC verification failed on link");
    int sock = socklist[snum].sock;
    killsock(sock);
    for (int j = 0; j < dcc_total; j++) {
      if (dcc[j].sock == sock) {
        lostdcc(j);
        break;
      }
    }
    return -1;
  }

  strlcpy(src, decrypted.c_str(), SGRAB + 1);
  if (!chacha_rotate(socklist[snum].ikey, socklist[snum].iseed)) {
    putlog(LOG_WARN, "*", "Unable to rotate the link key");
    int sock = socklist[snum].sock;
    killsock(sock);
    for (int j = 0; j < dcc_total; j++) {
      if (dcc[j].sock == sock) {
        lostdcc(j);
        break;
      }
    }
    return -1;
  }
  return OK;
}

static const size_t CHACHA_NONCE_LEN = 12;
static const size_t CHACHA_TAG_LEN = 16;
static const size_t CHACHA_WIRE_MAX = SGRAB - 2;

static bool chacha_wire_len(size_t plaintext_len, size_t& wire_len)
{
  const size_t max_size = std::numeric_limits<size_t>::max();
  const size_t overhead = CHACHA_NONCE_LEN + CHACHA_TAG_LEN;

  if (plaintext_len > max_size - overhead - 2)
    return false;

  size_t groups = (plaintext_len + overhead + 2) / 3;
  if (groups > max_size / 4)
    return false;

  wire_len = groups * 4;
  return wire_len + 1 <= CHACHA_WIRE_MAX;
}

static bool chacha_wire_plan(const char *src, size_t& total)
{
  const size_t max_size = std::numeric_limits<size_t>::max();
  const char *line = src;

  total = 0;
  for (;;) {
    const char *eol = strchr(line, '\n');
    size_t line_len = eol ? static_cast<size_t>(eol - line) : strlen(line);

    if (line_len) {
      size_t wire_len = 0;
      if (!chacha_wire_len(line_len, wire_len))
        return false;
      if (total > max_size - (wire_len + 1))
        return false;
      total += wire_len + 1;
    }

    if (!eol)
      break;
    line = eol + 1;
  }
  return true;
}

static const char *chacha20_poly1305_write(int snum, const char *src, size_t *len)
{
  static std::string wire;
  size_t srclen = *len;

  wire.clear();
  *len = 0;

  if (!src || srclen > std::numeric_limits<size_t>::max() - 10)
    return NULL;

  const size_t bufsiz = srclen + 10;
  char *srcbuf = (char *) calloc(1, bufsiz);
  if (!srcbuf)
    return NULL;
  strlcpy(srcbuf, src, bufsiz);

  size_t total = 0;
  if (!chacha_wire_plan(srcbuf, total)) {
    putlog(LOG_WARN, "*", "Refusing an oversized message on an encrypted link");
    OPENSSL_cleanse(srcbuf, bufsiz);
    free(srcbuf);
    return NULL;
  }

  wire.reserve(total);

  bool failed = false;
  auto process_line = [&](char *line) -> bool {
    if (!line[0])
      return true;

    bd::String encrypted;
    if (!crypto::encrypt_chacha20_poly1305_checked(
            socklist[snum].okey, bd::String(line), bd::String(),
            encrypted, false))
      return false;

    if (encrypted.length() + 1 > CHACHA_WIRE_MAX
        || wire.size() + encrypted.length() + 1 > wire.capacity())
      return false;

    if (!chacha_rotate(socklist[snum].okey, socklist[snum].oseed))
      return false;

    wire.append(encrypted.c_str(), encrypted.length());
    wire.push_back('\n');
    return true;
  };

  char *line = srcbuf;
  char *eol = strchr(line, '\n');
  while (eol) {
    *eol++ = 0;
    if (!process_line(line)) {
      failed = true;
      break;
    }
    line = eol;
    eol = strchr(line, '\n');
  }
  if (!failed && line[0] && !process_line(line))
    failed = true;

  OPENSSL_cleanse(srcbuf, bufsiz);
  free(srcbuf);

  if (failed)
    putlog(LOG_WARN, "*", "Unable to encrypt a message on an encrypted link");

  if (wire.empty())
    return NULL;

  *len = wire.size();
  return wire.c_str();
}

void link_send(int idx, const char *format, ...)
{
  char s[2001] = "";
  va_list va;

  va_start(va, format);
  egg_vsnprintf(s, sizeof(s) - 1, format, va);
  va_end(va);
  remove_crlf(s);

  dprintf(-dcc[idx].sock, STR("neg! %s\n"), s);
}

void link_done(int idx)
{
  dprintf(-dcc[idx].sock, "%s", STR("neg.\n"));
}

int link_find_by_type(int type)
{
  int i = 0;

  for (i = 0; enclink[i].name; i++)
    if (type == enclink[i].type)
      return i;

  return -1;
}

void link_link(int idx, int type, int i, direction_t direction)
{
  if (i == -1 && type != -1) {
    for (i = 0; enclink[i].name; i++) {
      if (enclink[i].type == type)
        break;
    }
  }

  if (i != -1 && enclink[i].link)
    (enclink[i].link) (idx, direction);
  else if (direction == TO)		/* problem finding function, just assume we're done */
    link_done(idx);

  return;
}

int link_read(int snum, char *buf)
{
  int i = socklist[snum].enclink;

  if (i != -1 && enclink[i].read)
    return (enclink[i].read) (snum, buf);

  return -1;
}

const char *link_write(int snum, const char *buf, size_t *len)
{
  int i = socklist[snum].enclink;

  if (i != -1 && enclink[i].write) {
    const char *result = (enclink[i].write) (snum, buf, len);
    if (result)
      return result;
    *len = 0;
    return buf;
  }

  return buf;
}

void link_challenge_to(int idx, char *buf) {
  int snum = findanysnum(dcc[idx].sock);

  if (snum >= 0) {
    char *rand = newsplit(&buf), *tmp = strdup(buf), *tmpp = tmp, *p = NULL;
    int i = -1;
    bool v2 = false;

    while ((p = newsplit(&tmp))[0]) {
      if (!strcmp(p, STR("v2"))) {
        v2 = true;
        continue;
      }
      if (str_isdigit(p)) {
        int type = atoi(p);
        int found = link_find_by_type(type);

        /* Take the first enabled match but keep scanning for "v2" token */
        if (found != -1 && link_type_offered(type) && i == -1)
          i = found;
      }
    }

    // No shared type!
    if (i == -1) {
      sdprintf(STR("No shared cipher with %s"), dcc[idx].nick);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      free(tmpp);
      return;
    }

    sdprintf(STR("Choosing '%s' (%d/%d) for link to %s (v2=%d)"), enclink[i].name, enclink[i].type, i, dcc[idx].nick, v2);
    if (v2)
      link_hash_new(idx, rand, buf);
    else
      link_hash(idx, rand, NULL);
    dprintf(-dcc[idx].sock, STR("neg %s %d %s\n"), dcc[idx].shahash, enclink[i].type, dcc[idx].nick);
    socklist[snum].enclink = i;
    link_link(idx, -1, i, TO);
    free(tmpp);
  }
}

void link_hash(int idx, char *rand, const char *ciphers)
{
  char hash[1024] = "";

  simple_snprintf(hash, sizeof(hash), STR("enclink%s%s"), rand, ciphers ? ciphers : "");
  strlcpy(dcc[idx].shahash, SHA1(hash), SHA_HASH_LENGTH + 1);
  SHA1(NULL);
  OPENSSL_cleanse(hash, sizeof(hash));
  return;
}

void link_hash_new(int idx, char *rand, const char *ciphers)
{
  char hash[1024] = "";

  /* Include cipher list in hash to prevent downgrade attacks */
  simple_snprintf(hash, sizeof(hash), STR("enclink%s%s"), rand, ciphers);
  strlcpy(dcc[idx].shahash, SHA1(hash), SHA_HASH_LENGTH + 1);
  SHA1(NULL);
  OPENSSL_cleanse(hash, sizeof(hash));
  return;
}

void link_parse(int idx, char *buf)
{
  int snum = findanysnum(dcc[idx].sock);
  if (snum < 0)
    return;
  int i = socklist[snum].enclink;

  if (i >= 0 && enclink[i].parse)
    (enclink[i].parse) (idx, snum, buf);

  return;
}

void link_get_method(int idx)
{
  if (!dcc[idx].type)
    return;

  int n = dcc[idx].u.enc->method_number;

  if (enclink[n].name)
    dcc[idx].u.enc->method = &(enclink[n]);

  dcc[idx].u.enc->method_number++;
}

/* the order of entries here determines which will be picked */
struct enc_link enclink[] = {
  { "chacha-poly", LINK_CHACHA20_POLY1305, chacha20_poly1305_link_case, chacha20_poly1305_write, chacha20_poly1305_read, chacha20_poly1305_parse },
  { "aes-ecb", LINK_GHOSTCASE3, ghost_link_case, ghost_write, ghost_read, ghost_parse },
  { "cleartext", LINK_CLEARTEXT, NULL, NULL, NULL, NULL },
  { NULL, 0, NULL, NULL, NULL, NULL }
};
/* vim: set sts=2 sw=2 ts=8 et: */
