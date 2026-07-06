/* dh_util.cc
 *
 * Adapted from ZNC-fish
 */

#include "src/libcrypto.h"
#include "src/compat/compat.h"
#include <bdlib/src/String.h>
#include <bdlib/src/base64.h>
#include "dh_util.h"

/* Global instance for C API */
static DH1080 *g_dh1080 = NULL;

/* DH1080 class implementation */
DH1080::DH1080() : prime_(NULL), generator_(NULL)
{
  const char *prime1080 = "FBE1022E23D213E8ACFA9AE8B9DFADA3EA6B7AC7A7B7E95AB5EB2DF858921FEADE95E6AC7BE7DE6ADBAB8A783E7AF7A7FA6A2B7BEB1E72EAE2B72F9FA2BFB2A2EFBEFAC868BADB3E828FA8BADFADA3E4CC1BE7E8AFE85E9698A783EB68FA07A77AB6AD7BEB618ACF9CA2897EB28A6189EFA07AB99A8A7FA9AE299EFA7BA66DEAFEFBEFBF0B7D8B";

  if (!BN_hex2bn(&prime_, prime1080)) {
    sdprintf("BAD PRIME");
    return;
  }

  if (!BN_dec2bn(&generator_, "2")) {
    sdprintf("BAD GENERATOR");
    return;
  }
}

DH1080::~DH1080()
{
  BN_clear_free(prime_);
  BN_clear_free(generator_);
}

bd::String DH1080::fish_base64_encode(const bd::String& str) {
  bd::String result(bd::base64Encode(str));

  // No padding, add an A on the end (base64-encoded NULL-terminator)
  if (result.rfind('=') == result.npos) {
    result += 'A';
  } else {
    // Remove padding
    while (result.rfind('=') != result.npos) {
      --result;
    }
  }
  return result;
}

bd::String DH1080::fish_base64_decode(const bd::String& str) {
  bd::String temp(str);

  // Remove the 'A' NULL-terminator if present
  if (temp.length() % 4 == 1 && temp(-1, 1) == 'A') {
    --temp;
  }

  while (temp.length() % 4) {
    temp += '=';
  }

  return bd::base64Decode(temp);
}

DH1080KeyPair DH1080::generate_keypair() const {
  DH1080KeyPair result;
  DH *dh = NULL;
  const BIGNUM *priv_key, *pub_key;

  dh = DH_new();
#if (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER >= 0x30500000L) || \
    (!defined(LIBRESSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER >= 0x10100000L)
  if (prime_ == NULL || generator_ == NULL ||
      !DH_set0_pqg(dh, BN_dup(prime_), NULL, BN_dup(generator_)))
    return result;
#else
  dh->p = BN_dup(prime_);
  dh->g = BN_dup(generator_);
#endif

  if (!DH_generate_key(dh)) {
    DH_free(dh);
    return result;
  }

  // Get private key
#if (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER >= 0x30500000L) || \
    (!defined(LIBRESSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER >= 0x10100000L)
  DH_get0_key(dh, &pub_key, &priv_key);
#else
  priv_key = dh->priv_key;
  pub_key = dh->pub_key;
#endif
  result.private_key.resize(BN_num_bytes(priv_key), 0);
  BN_bn2bin(priv_key, reinterpret_cast<unsigned char*>(result.private_key.begin()));

  // Get public key
  bd::String publicKey;
  publicKey.resize(static_cast<size_t>(BN_num_bytes(pub_key)));
  BN_bn2bin(pub_key, reinterpret_cast<unsigned char*>(publicKey.begin()));;

  // base64 encode
  result.public_key_b64 = fish_base64_encode(publicKey);

  DH_free(dh);
  return result;
}

bool DH1080::compute_shared(const bd::String& privateKey, const bd::String& theirPublicKeyB64, bd::String& sharedKey) const {
  BIGNUM *b_myPrivkey = NULL, *b_HisPubkey = NULL;
  DH *dh = NULL;

  dh = DH_new();
#if (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER >= 0x30500000L) || \
    (!defined(LIBRESSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER >= 0x10100000L)
  if (prime_ == NULL || generator_ == NULL ||
      !DH_set0_pqg(dh, BN_dup(prime_), NULL, BN_dup(generator_)))
    return false;
#else
  dh->p = BN_dup(prime_);
  dh->g = BN_dup(generator_);
#endif

  // Setup my private key
  b_myPrivkey = BN_bin2bn(reinterpret_cast<const unsigned char*>(privateKey.cbegin()), privateKey.length(), NULL);
#if (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER >= 0x30500000L) || \
    (!defined(LIBRESSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER >= 0x10100000L)
  DH_set0_key(dh, NULL, b_myPrivkey);
#else
  dh->priv_key = b_myPrivkey;
#endif

  // Prep their public key
  bd::String theirPublicKey(fish_base64_decode(theirPublicKeyB64));
  b_HisPubkey = BN_bin2bn(reinterpret_cast<const unsigned char*>(theirPublicKey.cbegin()), theirPublicKey.length(), NULL);

  // Compute the Shared key
  char *key = (char *)calloc(1, DH_size(dh));
  size_t len = DH_compute_key((unsigned char *)key, b_HisPubkey, dh);
  DH_free(dh);
  BN_clear_free(b_HisPubkey);
  if (len == static_cast<size_t>(-1)) {
    unsigned long err = ERR_get_error();
    sdprintf("** DH Error: %s", ERR_error_string(err, NULL));
    free(key);

    sharedKey = ERR_error_string(err, NULL);
    return false;
  }

  SHA256_CTX c;
  bd::String SHA256Digest(static_cast<size_t>(SHA256_DIGEST_LENGTH));
  SHA256Digest.resize(SHA256_DIGEST_LENGTH);

  SHA256_Init(&c);
  SHA256_Update(&c, key, len);
  SHA256_Final(reinterpret_cast<unsigned char*>(SHA256Digest.begin()), &c);
  sharedKey = fish_base64_encode(SHA256Digest);

  OPENSSL_cleanse(key, len);
  free(key);

  return true;
}

/* C API wrappers */
bd::String fishBase64Encode(const bd::String& str) {
  return DH1080::fish_base64_encode(str);
}

bd::String fishBase64Decode(const bd::String& str) {
  return DH1080::fish_base64_decode(str);
}

void DH1080_init() {
  g_dh1080 = new DH1080();
}

void DH1080_uninit() {
  delete g_dh1080;
  g_dh1080 = NULL;
}

void DH1080_gen(bd::String& privateKey, bd::String& publicKeyB64) {
  if (!g_dh1080) return;
  DH1080KeyPair kp = g_dh1080->generate_keypair();
  privateKey = kp.private_key;
  publicKeyB64 = kp.public_key_b64;
}

bool DH1080_comp(const bd::String privateKey, const bd::String theirPublicKeyB64, bd::String& sharedKey) {
  if (!g_dh1080) return false;
  return g_dh1080->compute_shared(privateKey, theirPublicKeyB64, sharedKey);
}

/* vim: set sts=2 sw=2 ts=8 et: */
