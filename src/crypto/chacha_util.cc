#include <openssl/evp.h>
#include <openssl/rand.h>
#include <bdlib/src/String.h>
#include <bdlib/src/base64.h>
#include <climits>
#include <cstring>
#include <vector>
#include "src/libcrypto.h"
#include "src/crypto/chacha_util.h"

namespace crypto {

static const size_t CHACHA_KEYLEN = 32;
static const size_t CHACHA_NONCELEN = 12;
static const size_t CHACHA_TAGLEN = 16;

static bool key_valid(const bd::String& key, bool allow_short_key)
{
  if (!key)
    return false;

  return allow_short_key ? key.length() >= 16 : key.length() == CHACHA_KEYLEN;
}

bd::String ChaCha20Poly1305::derive_key(const bd::String& key)
{
  if (key.length() >= CHACHA_KEYLEN)
    return bd::String(key.c_str(), CHACHA_KEYLEN);

  std::vector<unsigned char> full_key(CHACHA_KEYLEN, 0);
  memcpy(full_key.data(), key.c_str(), key.length());
  bd::String result(reinterpret_cast<const char*>(full_key.data()), CHACHA_KEYLEN);
  OPENSSL_cleanse(full_key.data(), full_key.size());
  return result;
}

bool ChaCha20Poly1305::encrypt_checked(const bd::String& key, const bd::String& data,
	const bd::String& nonce, bd::String& out, bool allow_short_key)
{
  out = bd::String();

  if (!key_valid(key, allow_short_key) || data.length() > INT_MAX)
    return false;

  bd::String expanded_key = derive_key(key);

  unsigned char nonce_bytes[CHACHA_NONCELEN];
  if (nonce && nonce.length() >= CHACHA_NONCELEN)
    memcpy(nonce_bytes, nonce.c_str(), CHACHA_NONCELEN);
  else if (RAND_bytes(nonce_bytes, CHACHA_NONCELEN) != 1)
    return false;

  const EVP_CIPHER *cipher = EVP_get_cipherbyname("chacha20-poly1305");
  if (!cipher)
    return false;

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx)
    return false;

  if (EVP_EncryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return false;
  }

  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, CHACHA_NONCELEN, nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return false;
  }

  if (EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                         (const unsigned char*)expanded_key.c_str(), nonce_bytes) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return false;
  }

  int outlen = 0;
  std::vector<unsigned char> outbuf(data.length() + CHACHA_TAGLEN);

  if (EVP_EncryptUpdate(ctx, outbuf.data(), &outlen,
                        (const unsigned char*)data.c_str(),
                        static_cast<int>(data.length())) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(outbuf.data(), outbuf.size());
    return false;
  }

  int finlen = 0;
  if (EVP_EncryptFinal_ex(ctx, outbuf.data() + outlen, &finlen) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(outbuf.data(), outbuf.size());
    return false;
  }

  unsigned char tag[CHACHA_TAGLEN];
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, CHACHA_TAGLEN, tag) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(outbuf.data(), outbuf.size());
    return false;
  }

  EVP_CIPHER_CTX_free(ctx);

  size_t total = CHACHA_NONCELEN + outlen + finlen + CHACHA_TAGLEN;
  std::vector<unsigned char> wire(total);

  memcpy(wire.data(), nonce_bytes, CHACHA_NONCELEN);
  memcpy(wire.data() + CHACHA_NONCELEN, outbuf.data(), outlen + finlen);
  memcpy(wire.data() + CHACHA_NONCELEN + outlen + finlen, tag, CHACHA_TAGLEN);

  bd::String result(reinterpret_cast<const char*>(wire.data()), total);
  OPENSSL_cleanse(outbuf.data(), outbuf.size());
  OPENSSL_cleanse(wire.data(), wire.size());

  out = bd::base64Encode(result);
  return true;
}

bd::String ChaCha20Poly1305::encrypt(const bd::String& key, const bd::String& data, const bd::String& nonce)
{
  bd::String out;
  encrypt_checked(key, data, nonce, out, true);
  return out;
}

bool ChaCha20Poly1305::decrypt_checked(const bd::String& key, const bd::String& data,
	bd::String& out, bool allow_short_key)
{
  out = bd::String();

  if (!key_valid(key, allow_short_key))
    return false;

  bd::String decoded = bd::base64Decode(data);

  if (decoded.length() < CHACHA_NONCELEN + CHACHA_TAGLEN)
    return false;

  bd::String expanded_key = derive_key(key);

  size_t ciphertext_len = decoded.length() - CHACHA_NONCELEN - CHACHA_TAGLEN;
  if (ciphertext_len > INT_MAX)
    return false;

  const unsigned char* nonce = reinterpret_cast<const unsigned char*>(decoded.c_str());
  const unsigned char* ciphertext = nonce + CHACHA_NONCELEN;
  const unsigned char* tag = ciphertext + ciphertext_len;

  const EVP_CIPHER *cipher = EVP_get_cipherbyname("chacha20-poly1305");
  if (!cipher)
    return false;

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx)
    return false;

  if (EVP_DecryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return false;
  }

  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, CHACHA_NONCELEN, nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return false;
  }

  if (EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                         (const unsigned char*)expanded_key.c_str(), nonce) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return false;
  }

  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, CHACHA_TAGLEN, (void*)tag) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return false;
  }

  std::vector<unsigned char> outbuf(ciphertext_len + 1);

  int outlen = 0;
  if (EVP_DecryptUpdate(ctx, outbuf.data(), &outlen, ciphertext,
                        static_cast<int>(ciphertext_len)) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(outbuf.data(), outbuf.size());
    return false;
  }

  int finlen = 0;
  if (EVP_DecryptFinal_ex(ctx, outbuf.data() + outlen, &finlen) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(outbuf.data(), outbuf.size());
    return false;
  }

  EVP_CIPHER_CTX_free(ctx);

  out = bd::String(reinterpret_cast<const char*>(outbuf.data()), outlen + finlen);
  OPENSSL_cleanse(outbuf.data(), outbuf.size());
  return true;
}

bd::String ChaCha20Poly1305::decrypt(const bd::String& key, const bd::String& data)
{
  bd::String out;
  decrypt_checked(key, data, out, true);
  return out;
}

/* C API wrappers */
bd::String encrypt_chacha20_poly1305(const bd::String& key, const bd::String& data, const bd::String& nonce)
{
  return ChaCha20Poly1305::encrypt(key, data, nonce);
}

bd::String decrypt_chacha20_poly1305(const bd::String& key, const bd::String& data)
{
  return ChaCha20Poly1305::decrypt(key, data);
}

bool encrypt_chacha20_poly1305_checked(const bd::String& key, const bd::String& data,
	const bd::String& nonce, bd::String& out, bool allow_short_key)
{
  return ChaCha20Poly1305::encrypt_checked(key, data, nonce, out, allow_short_key);
}

bool decrypt_chacha20_poly1305_checked(const bd::String& key, const bd::String& data,
	bd::String& out, bool allow_short_key)
{
  return ChaCha20Poly1305::decrypt_checked(key, data, out, allow_short_key);
}

}
