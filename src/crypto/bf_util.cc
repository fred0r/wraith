/* bf_util.cc
 *
 */

#include "src/libcrypto.h"
#include "src/compat/compat.h"
#include <bdlib/src/String.h>
#include <bdlib/src/base64.h>

#define CRYPT_BLOCKSIZE BF_BLOCK

static const char eggdrop_blowfish_base64[65] = "./0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const int eggdrop_blowfish_base64_index[256] = {
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  1,
   2,  3,  4,  5,  6,  7,  8,  9, 10, 11, -1, -1, -1, -1, -1, -1,
  -1, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
  53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, -1, -1, -1, -1, -1,
  -1, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
  27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

union bf_data {
  struct {
    BF_LONG left;
    BF_LONG right;
  } lr;
  BF_LONG bf_long;
};

/* BlowfishCipher class implementation */
BlowfishCipher::BlowfishCipher(const bd::String& key)
{
  if (key.length()) {
    BF_set_key(&enc_key_, key.length(), reinterpret_cast<const unsigned char*>(key.cbegin()));
    BF_set_key(&dec_key_, key.length(), reinterpret_cast<const unsigned char*>(key.cbegin()));
  } else {
    memset(&enc_key_, 0, sizeof(enc_key_));
    memset(&dec_key_, 0, sizeof(dec_key_));
  }
}

BlowfishCipher::~BlowfishCipher()
{
  OPENSSL_cleanse(&enc_key_, sizeof(enc_key_));
  OPENSSL_cleanse(&dec_key_, sizeof(dec_key_));
}

bd::String BlowfishCipher::egg_encrypt(const bd::String& plaintext) const
{
  if (!plaintext.length()) return plaintext;

  bd::String in(plaintext);
  size_t datalen = in.length();
  bd::String out(static_cast<size_t>(datalen * 1.5));
  if (datalen % 8 != 0) {
    datalen += 8 - (datalen % 8);
    in.resize(datalen, 0);
  }
  bf_data data;
  size_t part;
  const unsigned char *s = reinterpret_cast<const unsigned char*>(in.cbegin());
  for (size_t i = 0; i < in.length(); i += 8) {
    data.lr.left = *s++ << 24;
    data.lr.left += *s++ << 16;
    data.lr.left += *s++ << 8;
    data.lr.left += *s++;
    data.lr.right = *s++ << 24;
    data.lr.right += *s++ << 16;
    data.lr.right += *s++ << 8;
    data.lr.right += *s++;
    BF_encrypt(&data.bf_long, &enc_key_);
    for (part = 0; part < 6; part++) {
      out += eggdrop_blowfish_base64[data.lr.right & 0x3f];
      data.lr.right = data.lr.right >> 6;
    }
    for (part = 0; part < 6; part++) {
      out += eggdrop_blowfish_base64[data.lr.left & 0x3f];
      data.lr.left = data.lr.left >> 6;
    }
  }
  return out;
}

bd::String BlowfishCipher::egg_decrypt(const bd::String& ciphertext) const
{
  bd::String in(ciphertext);
  // Skip over '+OK '
  if (in(0, 4) == "+OK ")
    in += static_cast<size_t>(4);
  bd::String out(static_cast<size_t>(in.length() * .9));
  // Too small to process
  if (in.size() < 12) return out;

  // Not valid base64
  if (eggdrop_blowfish_base64_index[int(in[0])] == -1) return out;

  int cut_off = in.length() % 12;
  if (cut_off > 0)
    in.resize(in.length() - cut_off);

  bf_data data;
  int val;
  size_t part;
  const char *s = in.cbegin();
  for (size_t i = 0; i < in.length(); i += 12) {
    data.lr.left = 0;
    data.lr.right = 0;
    for (part = 0; part < 6; part++) {
      if ((val = eggdrop_blowfish_base64_index[int(*s++)]) == -1) return out;
      data.lr.right |= (char)val << part * 6;
    }
    for (part = 0; part < 6; part++) {
      if ((val = eggdrop_blowfish_base64_index[int(*s++)]) == -1) return out;
      data.lr.left |= (char)val << part * 6;
    }
    BF_decrypt(&data.bf_long, &dec_key_);
    for (part = 0; part < 4; part++) {
      const char decrypted_char = char((data.lr.left & (0xff << ((3 - part) * 8))) >> ((3 - part) * 8));
      if (decrypted_char) {
        out += decrypted_char;
      }
    }
    for (part = 0; part < 4; part++) {
      const char decrypted_char = char((data.lr.right & (0xff << ((3 - part) * 8))) >> ((3 - part) * 8));
      if (decrypted_char) {
        out += decrypted_char;
      }
    }
  }

  return out;
}

bd::String BlowfishCipher::fish_encrypt(const bd::String& plaintext) const
{
  if (!plaintext.length()) return plaintext;

  // Generate random 8-byte IV
  bd::String iv;
  iv.resize(BF_BLOCK);
  if (RAND_bytes(reinterpret_cast<unsigned char*>(iv.begin()), BF_BLOCK) != 1)
    return bd::String();

  // Pad plaintext to multiple of BF_BLOCK bytes
  bd::String data(plaintext);
  size_t datalen = data.length();
  if (datalen % BF_BLOCK != 0)
    datalen += BF_BLOCK - (datalen % BF_BLOCK);
  data.resize(datalen, 0);

  // Encrypt (copy IV since BF_cbc_encrypt modifies it in-place)
  bd::String out;
  out.resize(datalen);
  bd::String iv_copy(iv);
  BF_cbc_encrypt(
      reinterpret_cast<const unsigned char*>(data.cbegin()),
      reinterpret_cast<unsigned char*>(out.begin()),
      static_cast<long>(datalen), &enc_key_,
      reinterpret_cast<unsigned char*>(iv_copy.begin()),
      BF_ENCRYPT
  );

  return bd::base64Encode(iv + out);
}

bd::String BlowfishCipher::fish_decrypt(const bd::String& b64ciphertext) const
{
  if (!b64ciphertext.length()) return b64ciphertext;

  bd::String decoded(bd::base64Decode(b64ciphertext));

  // Need at least IV (BF_BLOCK bytes) + 1 block
  if (decoded.length() < static_cast<size_t>(BF_BLOCK * 2)) return bd::String();

  // Extract IV from the first BF_BLOCK bytes
  bd::String iv;
  iv.resize(BF_BLOCK);
  memcpy(iv.begin(), decoded.cbegin(), BF_BLOCK);

  size_t cipherlen = decoded.length() - BF_BLOCK;
  cipherlen -= cipherlen % BF_BLOCK;
  if (cipherlen == 0) return bd::String();

  bd::String out;
  out.resize(cipherlen);
  BF_cbc_encrypt(
      reinterpret_cast<const unsigned char*>(decoded.cbegin()) + BF_BLOCK,
      reinterpret_cast<unsigned char*>(out.begin()),
      static_cast<long>(cipherlen), &dec_key_,
      reinterpret_cast<unsigned char*>(iv.begin()),
      BF_DECRYPT
  );

  // Trim trailing NUL padding bytes
  while (out.length() > 0 && out[out.length() - 1] == '\0')
    --out;

  return out;
}

/* C API wrappers */
bd::String egg_bf_encrypt(bd::String in, const bd::String& key)
{
  if (!key.length()) return in;
  BlowfishCipher cipher(key);
  return cipher.egg_encrypt(in);
}

bd::String egg_bf_decrypt(bd::String in, const bd::String& key)
{
  if (!key.length()) return in;
  BlowfishCipher cipher(key);
  return cipher.egg_decrypt(in);
}

bd::String fish_bf_cbc_encrypt(const bd::String& key, const bd::String& plaintext)
{
  if (!key.length()) return plaintext;
  BlowfishCipher cipher(key);
  return cipher.fish_encrypt(plaintext);
}

bd::String fish_bf_cbc_decrypt(const bd::String& key, const bd::String& b64ciphertext)
{
  if (!key.length()) return b64ciphertext;
  BlowfishCipher cipher(key);
  return cipher.fish_decrypt(b64ciphertext);
}

/* vim: set sts=2 sw=2 ts=8 et: */
