#ifndef _BASE64_H_
#  define _BASE64_H_

#include <sys/types.h>
#include <string>
#include <bdlib/src/String.h>

class Base64 {
public:
	/* Standard base64 encode/decode */
	static bd::String encode(const bd::String& input);
	static bd::String decode(const bd::String& input);

	/* Eggdrop-compatible base64 (broken variant) */
	static bd::String encode_eggdrop(const bd::String& input);
	static bd::String decode_eggdrop(const bd::String& input);

	/* Integer encoding (eggdrop protocol) */
	static std::string int_encode(unsigned int val);
	static int int_decode(const std::string& encoded);
};

/* C API for existing callers */
char *int_to_base64(unsigned int);
int base64_to_int(const char *) __attribute__((pure));

bd::String broken_base64Encode(const bd::String&);
char *b64enc(const unsigned char *data, size_t len);

bd::String broken_base64Decode(const bd::String&);
char *b64dec(const unsigned char *data, size_t *len);

#endif /* !_BASE64_H_ */
