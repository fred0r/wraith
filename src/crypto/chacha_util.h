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

	/* Status-returning variants. allow_short_key exists only for the legacy
	 * SALT2 handshake, which pads a 16-byte key the same way older peers do.
	 * Link data requires an exact 32-byte key. */
	static bool encrypt_checked(const bd::String& key, const bd::String& data,
		const bd::String& nonce, bd::String& out, bool allow_short_key = false);
	static bool decrypt_checked(const bd::String& key, const bd::String& b64data,
		bd::String& out, bool allow_short_key = false);
private:
	static bd::String derive_key(const bd::String& key);
};

/* C API for existing callers */
bd::String encrypt_chacha20_poly1305(const bd::String& key, const bd::String& data, const bd::String& nonce);
bd::String decrypt_chacha20_poly1305(const bd::String& key, const bd::String& data);
bool encrypt_chacha20_poly1305_checked(const bd::String& key, const bd::String& data,
	const bd::String& nonce, bd::String& out, bool allow_short_key = false);
bool decrypt_chacha20_poly1305_checked(const bd::String& key, const bd::String& data,
	bd::String& out, bool allow_short_key = false);

} /* namespace crypto */

#endif
