/* aes_util.h
 *
 */

#ifndef _AES_UTIL_H
#define _AES_UTIL_H 1

#include <sys/types.h>
#include <string>

namespace bd {
  class String;
};

class AesCipher {
	AES_KEY enc_key_;
	AES_KEY dec_key_;
public:
	explicit AesCipher(const std::string& key);
	~AesCipher();

	/* ECB mode */
	std::string encrypt_ecb(const std::string& data) const;
	std::string decrypt_ecb(const std::string& data) const;

	/* CBC mode (IV is passed by value, modified in-place for compatibility) */
	std::string encrypt_cbc(const std::string& data, std::string iv) const;
	std::string decrypt_cbc(const std::string& data, std::string iv) const;

	/* Raw binary API */
	unsigned char *encrypt_ecb_raw(const unsigned char *in, size_t *inlen) const;
	unsigned char *decrypt_ecb_raw(const unsigned char *in, size_t *len) const;
	unsigned char *encrypt_cbc_raw(const unsigned char *in, size_t *inlen, unsigned char *ivec) const;
	unsigned char *decrypt_cbc_raw(const unsigned char *in, size_t *len, unsigned char *ivec) const;
};

/* C API for existing callers */
unsigned char *aes_encrypt_ecb_binary(const char *, unsigned char *, size_t *);
unsigned char *aes_decrypt_ecb_binary(const char *, unsigned char *, size_t *);
unsigned char *aes_encrypt_cbc_binary(const char *, unsigned char *, size_t *, unsigned char *);
unsigned char *aes_decrypt_cbc_binary(const char *, unsigned char *, size_t *, unsigned char *);
bd::String encrypt_string(const bd::String&, const bd::String&);
bd::String encrypt_string_cbc(const bd::String&, bd::String, bd::String);
bd::String decrypt_string(const bd::String&, const bd::String&);
bd::String decrypt_string_cbc(const bd::String&, bd::String, bd::String);
#endif
