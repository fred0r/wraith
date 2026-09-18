/* aes_util.cc
 *
 */

#include "src/libcrypto.h"
#include "src/compat/compat.h"
#include <bdlib/src/String.h>

#define CRYPT_BLOCKSIZE AES_BLOCK_SIZE
#define CRYPT_KEYBITS 256
#define CRYPT_KEYSIZE (CRYPT_KEYBITS >> 3)

/* AesCipher class implementation */
AesCipher::AesCipher(const std::string& key)
{
  if (!key.empty()) {
    char padded_key[CRYPT_KEYSIZE + 1] = "";
    strlcpy(padded_key, key.c_str(), sizeof(padded_key));
    AES_set_encrypt_key((const unsigned char *)padded_key, CRYPT_KEYBITS, &enc_key_);
    AES_set_decrypt_key((const unsigned char *)padded_key, CRYPT_KEYBITS, &dec_key_);
    OPENSSL_cleanse(padded_key, sizeof(padded_key));
  } else {
    memset(&enc_key_, 0, sizeof(enc_key_));
    memset(&dec_key_, 0, sizeof(dec_key_));
  }
}

AesCipher::~AesCipher()
{
  OPENSSL_cleanse(&enc_key_, sizeof(enc_key_));
  OPENSSL_cleanse(&dec_key_, sizeof(dec_key_));
}

std::string AesCipher::encrypt_ecb(const std::string& data) const
{
  size_t len = data.length();
  unsigned char *out = encrypt_ecb_raw((const unsigned char *)data.c_str(), &len);
  std::string result((const char *)out, len);
  OPENSSL_cleanse(out, len);
  free(out);
  return result;
}

std::string AesCipher::decrypt_ecb(const std::string& data) const
{
  size_t len = data.length();
  unsigned char *out = decrypt_ecb_raw((const unsigned char *)data.c_str(), &len);
  std::string result((const char *)out, len);
  OPENSSL_cleanse(out, len);
  free(out);
  return result;
}

std::string AesCipher::encrypt_cbc(const std::string& data, std::string iv) const
{
  /* Make a mutable copy for CBC (IV is modified in-place) */
  std::string padded_data = data;
  size_t padding = CRYPT_BLOCKSIZE;
  if (padded_data.length() % CRYPT_BLOCKSIZE)
    padding = (CRYPT_BLOCKSIZE - (padded_data.length() % CRYPT_BLOCKSIZE));
  padded_data.append(padding, (char)padding);

  unsigned char *out = (unsigned char *)calloc(1, padded_data.length());
  memcpy(out, padded_data.c_str(), padded_data.length());
  AES_cbc_encrypt(out, out, padded_data.length(), &enc_key_, (unsigned char *)iv.c_str(), AES_ENCRYPT);

  std::string result((const char *)out, padded_data.length());
  OPENSSL_cleanse(out, padded_data.length());
  free(out);
  return result;
}

std::string AesCipher::decrypt_cbc(const std::string& data, std::string iv) const
{
  std::string result = data;
  unsigned char *buf = (unsigned char *)calloc(1, result.length());
  memcpy(buf, result.c_str(), result.length());

  AES_cbc_encrypt(buf, buf, result.length(), &dec_key_, (unsigned char *)iv.c_str(), AES_DECRYPT);

  /* Remove PKCS padding */
  size_t len = result.length();
  size_t padding = buf[len - 1];
  if (!padding || padding > CRYPT_BLOCKSIZE)
    len = strlen((char *)buf);
  else
    len -= padding;

  result.assign((const char *)buf, len);
  OPENSSL_cleanse(buf, result.length());
  free(buf);
  return result;
}

unsigned char *
AesCipher::encrypt_ecb_raw(const unsigned char *in, size_t *inlen) const
{
  size_t len = *inlen;
  int blocks = 0, block = 0;
  unsigned char *out = NULL;

  if (len % CRYPT_BLOCKSIZE)
    len += (CRYPT_BLOCKSIZE - (len % CRYPT_BLOCKSIZE));

  out = (unsigned char *) calloc(1, len + 1);
  memcpy(out, in, *inlen);
  *inlen = len;

  blocks = len / CRYPT_BLOCKSIZE;
  for (block = blocks - 1; block >= 0; --block)
    AES_encrypt(&out[block * CRYPT_BLOCKSIZE], &out[block * CRYPT_BLOCKSIZE], &enc_key_);

  out[len] = 0;
  return out;
}

unsigned char *
AesCipher::decrypt_ecb_raw(const unsigned char *in, size_t *len) const
{
  int blocks = 0, block = 0;
  unsigned char *out = NULL;

  *len -= *len % CRYPT_BLOCKSIZE;
  out = (unsigned char *) calloc(1, *len + 1);
  memcpy(out, in, *len);

  blocks = *len / CRYPT_BLOCKSIZE;
  for (block = blocks - 1; block >= 0; --block)
    AES_decrypt(&out[block * CRYPT_BLOCKSIZE], &out[block * CRYPT_BLOCKSIZE], &dec_key_);

  *len = strlen((char*) out);
  out[*len] = 0;
  return out;
}

unsigned char *
AesCipher::encrypt_cbc_raw(const unsigned char *in, size_t *inlen, unsigned char *ivec) const
{
  size_t len = *inlen;
  unsigned char *out = NULL;

  size_t padding = 16;
  if (len % CRYPT_BLOCKSIZE)
    padding = (CRYPT_BLOCKSIZE - (len % CRYPT_BLOCKSIZE));
  len += padding;

  out = (unsigned char *) calloc(1, len);
  memset(out + *inlen, padding, padding);
  memcpy(out, in, *inlen);
  *inlen = len;

  AES_cbc_encrypt(out, out, len, &enc_key_, ivec, AES_ENCRYPT);
  return out;
}

unsigned char *
AesCipher::decrypt_cbc_raw(const unsigned char *in, size_t *len, unsigned char *ivec) const
{
  unsigned char *out = NULL;

  *len -= *len % CRYPT_BLOCKSIZE;
  out = (unsigned char *) calloc(1, *len + 1);
  memcpy(out, in, *len);

  AES_cbc_encrypt(out, out, *len, &dec_key_, ivec, AES_DECRYPT);

  size_t padding = out[*len - 1];
  if (!padding)
    *len = strlen((char*) out);
  else
    *len -= padding;

  return out;
}

/* C API wrappers */
bd::String encrypt_string(const bd::String& key, const bd::String& data) {
  if (!key) return data;
  AesCipher cipher{std::string(key.c_str(), key.length())};
  return bd::String(cipher.encrypt_ecb(std::string(data.c_str(), data.length())).c_str());
}

bd::String encrypt_string_cbc(const bd::String& key, bd::String data, bd::String IV) {
  if (!key) return data;
  AesCipher cipher{std::string(key.c_str(), key.length())};
  std::string result = cipher.encrypt_cbc(
    std::string(data.c_str(), data.length()),
    std::string(IV.c_str(), IV.length())
  );
  return bd::String(result.c_str(), result.length());
}

bd::String decrypt_string(const bd::String& key, const bd::String& data) {
  if (!key) return data;
  AesCipher cipher{std::string(key.c_str(), key.length())};
  return bd::String(cipher.decrypt_ecb(std::string(data.c_str(), data.length())).c_str());
}

bd::String decrypt_string_cbc(const bd::String& key, bd::String data, bd::String IV) {
  if (!key) return data;
  AesCipher cipher{std::string(key.c_str(), key.length())};
  std::string result = cipher.decrypt_cbc(
    std::string(data.c_str(), data.length()),
    std::string(IV.c_str(), IV.length())
  );
  return bd::String(result.c_str(), result.length());
}

unsigned char *
aes_encrypt_ecb_binary(const char *keydata, unsigned char *in, size_t *inlen)
{
  if (!keydata || !*keydata) {
    unsigned char *out = (unsigned char *) calloc(1, *inlen + 1);
    memcpy(out, in, *inlen);
    return out;
  }
  AesCipher cipher{std::string(keydata)};
  return cipher.encrypt_ecb_raw(in, inlen);
}

unsigned char *
aes_decrypt_ecb_binary(const char *keydata, unsigned char *in, size_t *len)
{
  if (!keydata || !*keydata) {
    unsigned char *out = (unsigned char *) calloc(1, *len + 1);
    memcpy(out, in, *len);
    return out;
  }
  AesCipher cipher{std::string(keydata)};
  return cipher.decrypt_ecb_raw(in, len);
}

unsigned char *
aes_encrypt_cbc_binary(const char *keydata, unsigned char *in, size_t *inlen, unsigned char *ivec)
{
  if (!keydata || !*keydata) {
    size_t len = *inlen;
    size_t padding = 16;
    if (len % CRYPT_BLOCKSIZE)
      padding = (CRYPT_BLOCKSIZE - (len % CRYPT_BLOCKSIZE));
    len += padding;
    unsigned char *out = (unsigned char *) calloc(1, len);
    memcpy(out, in, *inlen);
    *inlen = len;
    return out;
  }
  AesCipher cipher{std::string(keydata)};
  return cipher.encrypt_cbc_raw(in, inlen, ivec);
}

unsigned char *
aes_decrypt_cbc_binary(const char *keydata, unsigned char *in, size_t *len, unsigned char *ivec)
{
  if (!keydata || !*keydata) {
    unsigned char *out = (unsigned char *) calloc(1, *len + 1);
    memcpy(out, in, *len);
    return out;
  }
  AesCipher cipher{std::string(keydata)};
  return cipher.decrypt_cbc_raw(in, len, ivec);
}

/* vim: set sts=2 sw=2 ts=8 et: */
