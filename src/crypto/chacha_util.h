#ifndef CRYPTO_CHACHA_UTIL_H
#define CRYPTO_CHACHA_UTIL_H

#include <sys/types.h>

namespace bd {
class String;
}

namespace crypto {

class ChaCha20Poly1305 {
	static constexpr size_t KEY_LEN = 32;
	static constexpr size_t NONCE_LEN = 12;
	static constexpr size_t TAG_LEN = 16;
public:
	static bd::String encrypt(const bd::String& key, const bd::String& data, const bd::String& nonce = bd::String());
	static bd::String decrypt(const bd::String& key, const bd::String& b64data);
private:
	static bd::String derive_key(const bd::String& key);
};

/* C API for existing callers */
bd::String encrypt_chacha20_poly1305(const bd::String& key, const bd::String& data, const bd::String& nonce);
bd::String decrypt_chacha20_poly1305(const bd::String& key, const bd::String& data);

} /* namespace crypto */

#endif
