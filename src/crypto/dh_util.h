/* dh_util.h
 *
 */

#ifndef _DH_UTIL_H
#define _DH_UTIL_H 1

#include <sys/types.h>
#include <openssl/dh.h>
#include <bdlib/src/String.h>

struct DH1080KeyPair {
	bd::String private_key;
	bd::String public_key_b64;
};

class DH1080 {
	BIGNUM* prime_;
	BIGNUM* generator_;
public:
	DH1080();
	~DH1080();
	DH1080(const DH1080&) = delete;
	DH1080& operator=(const DH1080&) = delete;

	DH1080KeyPair generate_keypair() const;
	bool compute_shared(const bd::String& private_key, const bd::String& their_public_key_b64, bd::String& shared_key) const;

	/* FiSH base64 helpers */
	static bd::String fish_base64_encode(const bd::String& str);
	static bd::String fish_base64_decode(const bd::String& str);
};

/* C API for existing callers */
bd::String fishBase64Encode(const bd::String& str);
bd::String fishBase64Decode(const bd::String& str);
void DH1080_gen(bd::String& privateKey, bd::String& publicKeyB64);
bool DH1080_comp(const bd::String privateKey, const bd::String theirPublicKeyB64, bd::String& sharedKey);
void DH1080_init();
void DH1080_uninit();
#endif
