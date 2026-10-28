#include "CryptoTest.h"

#include <string>

#include <bdlib/src/String.h>
#include "src/libcrypto.h"
#include "src/crypto/aes_util.h"
#include "src/crypto/bf_util.h"
#include "src/crypto/chacha_util.h"
#include "src/crypto/dh_util.h"

CPPUNIT_TEST_SUITE_REGISTRATION(CryptoCipherTest);
CPPUNIT_TEST_SUITE_REGISTRATION(DH1080Test);

static const char AES_TEST_KEY[] = "0123456789abcdef0123456789abcdef";
static const char CHACHA_KEY[] = "0123456789abcdef0123456789abcdef";

void CryptoCipherTest::aesEcbKnownVector()
{
  const char expected_raw[] = {
    '\xf8', '\x3c', '\x9a', '\x60', '\xdc', '\x0c', '\xdb', '\x98',
    '\x21', '\x9f', '\x79', '\xd6', '\xd5', '\xdb', '\x16', '\x35'
  };
  const bd::String plaintext("0123456789abcdef");
  const bd::String expected(expected_raw, sizeof(expected_raw));

  AesCipher cipher(AES_TEST_KEY);
  CPPUNIT_ASSERT_EQUAL(expected, bd::String(cipher.encrypt_ecb(plaintext.c_str())));
  CPPUNIT_ASSERT_EQUAL(plaintext, bd::String(cipher.decrypt_ecb(expected.c_str())));
}

void CryptoCipherTest::aesRoundtrip()
{
  AesCipher cipher(AES_TEST_KEY);
  const bd::String plaintext("hello aes world");

  CPPUNIT_ASSERT_EQUAL(plaintext, bd::String(cipher.decrypt_ecb(cipher.encrypt_ecb(plaintext.c_str()))));

  const bd::String encrypted_string = encrypt_string(AES_TEST_KEY, plaintext);
  CPPUNIT_ASSERT(!encrypted_string.isEmpty());
  CPPUNIT_ASSERT_EQUAL(plaintext, decrypt_string(AES_TEST_KEY, encrypted_string));

  const bd::String cbc_plaintext("hello cbc world");
  const bd::String iv("abcdefghijklmnop");
  const std::string encrypted = cipher.encrypt_cbc(cbc_plaintext.c_str(), iv.c_str());
  CPPUNIT_ASSERT_EQUAL(cbc_plaintext,
      bd::String(cipher.decrypt_cbc(encrypted, iv.c_str())));
}

void CryptoCipherTest::blowfishRoundtrip()
{
  BlowfishCipher cipher("blowfish test key");
  const bd::String plaintext("hello fish world");

  CPPUNIT_ASSERT_EQUAL(plaintext, cipher.egg_decrypt(cipher.egg_encrypt(plaintext)));
  CPPUNIT_ASSERT_EQUAL(plaintext, cipher.fish_decrypt(cipher.fish_encrypt(plaintext)));
  CPPUNIT_ASSERT_EQUAL(bd::String(), cipher.fish_decrypt("%%%%"));
  CPPUNIT_ASSERT_EQUAL(bd::String(), cipher.fish_decrypt("AAAA"));
}

void CryptoCipherTest::blowfishEcbKnownVector()
{
  const BlowfishCipher cipher("0123456789abcdef");
  const bd::String plaintext("hello fish world");
  const bd::String expected("1GDXj/uTlvx1V45oW0UFg0U.");

  CPPUNIT_ASSERT_EQUAL(expected, cipher.egg_encrypt(plaintext));
  CPPUNIT_ASSERT_EQUAL(expected, egg_bf_encrypt(plaintext, "0123456789abcdef"));
  CPPUNIT_ASSERT_EQUAL(plaintext, cipher.egg_decrypt(expected));
  CPPUNIT_ASSERT_EQUAL(plaintext, egg_bf_decrypt(expected, "0123456789abcdef"));
  CPPUNIT_ASSERT_EQUAL(plaintext, cipher.egg_decrypt(bd::String("+OK ") + expected));
}

void CryptoCipherTest::blowfishCbcKnownVector()
{
  const BlowfishCipher cipher("0123456789abcdef");
  const bd::String plaintext("hello fish world");
  const bd::String encrypted("MDEyMzQ1NjeZh8QVo0K+dyiYTqgqP39t");

  CPPUNIT_ASSERT_EQUAL(plaintext, cipher.fish_decrypt(encrypted));
  CPPUNIT_ASSERT_EQUAL(plaintext,
      fish_bf_cbc_decrypt("0123456789abcdef", encrypted));
}

void CryptoCipherTest::blowfishBlockBoundaries()
{
  const BlowfishCipher cipher("0123456789abcdef");
  const char *plaintexts[] = {
    "12345678",
    "1234567890abcdef",
    "1234567890abcdef!",
  };

  for (const char *plaintext : plaintexts) {
    const bd::String value(plaintext);
    CPPUNIT_ASSERT_EQUAL(value, cipher.egg_decrypt(cipher.egg_encrypt(value)));
    CPPUNIT_ASSERT_EQUAL(value, cipher.fish_decrypt(cipher.fish_encrypt(value)));
  }
}

void CryptoCipherTest::blowfishRejectsMalformed()
{
  const BlowfishCipher cipher("blowfish test key");

  bd::String first;
  first.resize(12, 'A');
  first[0] = static_cast<char>(0x80);
  CPPUNIT_ASSERT_EQUAL(bd::String(), cipher.egg_decrypt(first));

  bd::String middle;
  middle.resize(12, 'A');
  middle[7] = static_cast<char>(0x80);
  CPPUNIT_ASSERT_EQUAL(bd::String(), cipher.egg_decrypt(middle));
}

void CryptoCipherTest::blowfishCbcRejectsMalformed()
{
  const BlowfishCipher cipher("blowfish test key");

  CPPUNIT_ASSERT_EQUAL(bd::String(), cipher.fish_decrypt(""));
  CPPUNIT_ASSERT_EQUAL(bd::String(), cipher.fish_decrypt("%%%%"));
  CPPUNIT_ASSERT_EQUAL(bd::String(), cipher.fish_decrypt("MDEyMzQ1Njc="));
  CPPUNIT_ASSERT_EQUAL(bd::String(), cipher.fish_decrypt("MDEyMzQ1Njc4OWFiY2Rl"));
}

void CryptoCipherTest::blowfishEmptyKey()
{
  CPPUNIT_ASSERT_EQUAL(bd::String("plaintext"),
      fish_bf_cbc_encrypt("", "plaintext"));
  CPPUNIT_ASSERT_EQUAL(bd::String("ciphertext"),
      fish_bf_cbc_decrypt("", "ciphertext"));
  CPPUNIT_ASSERT_EQUAL(bd::String("plaintext"),
      egg_bf_encrypt("plaintext", ""));
  CPPUNIT_ASSERT_EQUAL(bd::String("ciphertext"),
      egg_bf_decrypt("ciphertext", ""));
}

void CryptoCipherTest::chachaRoundtrip()
{
  const bd::String key(CHACHA_KEY);
  const bd::String nonce("abcdefghijkl");
  const bd::String plaintext("hello chacha world");

  bd::String encrypted;
  bd::String decrypted;
  CPPUNIT_ASSERT(crypto::ChaCha20Poly1305::encrypt_checked(key, plaintext, nonce, encrypted));
  CPPUNIT_ASSERT(!encrypted.isEmpty());
  CPPUNIT_ASSERT(crypto::ChaCha20Poly1305::decrypt_checked(key, encrypted, decrypted));
  CPPUNIT_ASSERT_EQUAL(plaintext, decrypted);

  bd::String encrypted_again;
  CPPUNIT_ASSERT(crypto::ChaCha20Poly1305::encrypt_checked(key, plaintext, nonce, encrypted_again));
  CPPUNIT_ASSERT_EQUAL(encrypted, encrypted_again);

  bd::String empty_encrypted;
  bd::String empty_decrypted;
  CPPUNIT_ASSERT(crypto::ChaCha20Poly1305::encrypt_checked(key, bd::String(), nonce, empty_encrypted));
  CPPUNIT_ASSERT(crypto::ChaCha20Poly1305::decrypt_checked(key, empty_encrypted, empty_decrypted));
  CPPUNIT_ASSERT(empty_decrypted.isEmpty());
}

void CryptoCipherTest::chachaKeyLengths()
{
  const bd::String short_key("0123456789abcdef");
  const bd::String full_key(CHACHA_KEY);
  const bd::String nonce("abcdefghijkl");
  const bd::String plaintext("key length test");
  bd::String encrypted;
  bd::String decrypted;

  CPPUNIT_ASSERT(!crypto::ChaCha20Poly1305::encrypt_checked(short_key, plaintext, nonce, encrypted));
  CPPUNIT_ASSERT(crypto::ChaCha20Poly1305::encrypt_checked(short_key, plaintext, nonce, encrypted, true));
  CPPUNIT_ASSERT(!encrypted.isEmpty());

  CPPUNIT_ASSERT(crypto::ChaCha20Poly1305::decrypt_checked(short_key, encrypted, decrypted, true));
  CPPUNIT_ASSERT_EQUAL(plaintext, decrypted);

  CPPUNIT_ASSERT(crypto::ChaCha20Poly1305::encrypt_checked(full_key, plaintext, nonce, encrypted));
}

void CryptoCipherTest::chachaRejectsTampering()
{
  const bd::String key(CHACHA_KEY);
  const bd::String nonce("abcdefghijkl");
  const bd::String plaintext("tamper test");
  bd::String encrypted;
  bd::String out;

  CPPUNIT_ASSERT(crypto::ChaCha20Poly1305::encrypt_checked(key, plaintext, nonce, encrypted));
  CPPUNIT_ASSERT(!encrypted.isEmpty());

  encrypted[0] = encrypted[0] == 'A' ? 'B' : 'A';
  CPPUNIT_ASSERT(!crypto::ChaCha20Poly1305::decrypt_checked(key, encrypted, out));
  CPPUNIT_ASSERT(out.isEmpty());
}

void CryptoCipherTest::chachaRejectsMalformedBase64()
{
  const bd::String key(CHACHA_KEY);
  bd::String out;

  CPPUNIT_ASSERT(!crypto::ChaCha20Poly1305::decrypt_checked(key, "!!!!", out));
  CPPUNIT_ASSERT(!crypto::ChaCha20Poly1305::decrypt_checked(key, "AAAA\n", out));
  CPPUNIT_ASSERT(out.isEmpty());
}

void DH1080Test::fishBase64Roundtrip()
{
  const bd::String plaintext("hello dh1080");

  CPPUNIT_ASSERT_EQUAL(plaintext,
      DH1080::fish_base64_decode(DH1080::fish_base64_encode(plaintext)));
  CPPUNIT_ASSERT_EQUAL(bd::String(),
      DH1080::fish_base64_decode(DH1080::fish_base64_encode(bd::String())));
  CPPUNIT_ASSERT_EQUAL(bd::String(), DH1080::fish_base64_decode("%%%%"));
}

void DH1080Test::sharedSecretSymmetry()
{
  DH1080 dh;
  const DH1080KeyPair first = dh.generate_keypair();
  const DH1080KeyPair second = dh.generate_keypair();

  CPPUNIT_ASSERT(!first.private_key.isEmpty());
  CPPUNIT_ASSERT(!first.public_key_b64.isEmpty());
  CPPUNIT_ASSERT(!second.private_key.isEmpty());
  CPPUNIT_ASSERT(!second.public_key_b64.isEmpty());

  bd::String first_secret;
  bd::String second_secret;
  CPPUNIT_ASSERT(dh.compute_shared(first.private_key, second.public_key_b64, first_secret));
  CPPUNIT_ASSERT(dh.compute_shared(second.private_key, first.public_key_b64, second_secret));
  CPPUNIT_ASSERT(!first_secret.isEmpty());
  CPPUNIT_ASSERT_EQUAL(first_secret, second_secret);
}

void DH1080Test::rejectsInvalidPeerKey()
{
  DH1080 dh;
  const DH1080KeyPair pair = dh.generate_keypair();
  bd::String shared("untouched");

  CPPUNIT_ASSERT(!dh.compute_shared(pair.private_key, bd::String(), shared));
  CPPUNIT_ASSERT_EQUAL(bd::String("untouched"), shared);
  CPPUNIT_ASSERT(!dh.compute_shared(pair.private_key, "%%%%", shared));
}
