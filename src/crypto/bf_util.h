/* bf_util.h
 *
 */

#ifndef _BF_UTIL_H
#define _BF_UTIL_H 1

#include <sys/types.h>

namespace bd {
  class String;
}

class BlowfishCipher {
	BF_KEY enc_key_;
	BF_KEY dec_key_;
public:
	explicit BlowfishCipher(const bd::String& key);
	~BlowfishCipher();

	/* Eggdrop-compatible */
	bd::String egg_encrypt(const bd::String& plaintext) const;
	bd::String egg_decrypt(const bd::String& ciphertext) const;

	/* FiSH/mIRC-compatible (CBC with random IV) */
	bd::String fish_encrypt(const bd::String& plaintext) const;
	bd::String fish_decrypt(const bd::String& b64ciphertext) const;
};

/* C API for existing callers */
bd::String egg_bf_encrypt(bd::String in, const bd::String& key);
bd::String egg_bf_decrypt(bd::String in, const bd::String& key);
bd::String fish_bf_cbc_encrypt(const bd::String& key, const bd::String& plaintext);
bd::String fish_bf_cbc_decrypt(const bd::String& key, const bd::String& b64ciphertext);
#endif
