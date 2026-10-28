import base64
import hashlib
import subprocess


INT_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789[]"


def int_to_base64(value):
    if value == 0:
        return "A"
    result = ""
    while value:
        result = INT_ALPHABET[value & 0x3F] + result
        value >>= 6
    return result


def _md5(value):
    return hashlib.md5(value.encode()).hexdigest()


def _aes_ecb(key, data):
    padding = (-len(data)) % 16
    padded = data + b"\0" * padding
    result = subprocess.run(
        [
            "openssl", "enc", "-aes-256-ecb", "-nopad",
            "-K", key.encode().hex(),
        ],
        input=padded, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
    )
    return result.stdout


def cookie_key(salt1, salt2, channel, opper_handle, opper_userhost, random):
    return (
        random[0] + salt1[5] + random[3] + opper_handle.upper()
        + random[2] + salt1[4] + salt1[0] + salt1[1] + salt1[3]
        + salt1[6] + channel.upper() + salt1[10] + random[1]
        + salt2[15] + salt2[13] + salt1[10] + salt2[3] + salt2[1]
        + opper_userhost
    )


def cookie_hash(salt2, ts, random, opped_nick, opped_userhost, key):
    value = (
        salt2[0] + ts + random[0] + random[1] + random[2] + "\n"
        + random[3] + salt2[15] + opped_nick + opped_userhost + key
    )
    return _md5(value)


def make_cookie(salt1, salt2, channel, opper_handle, opper_userhost,
                opped_nick, opped_userhost, now, random="Ab3d", counter=1):
    timestamp = "%010d" % now
    key = cookie_key(salt1, salt2, channel, opper_handle, opper_userhost, random)
    digest = cookie_hash(
        salt2, timestamp[4:], random, opped_nick, opped_userhost, key)
    clear = random + timestamp[3:] + int_to_base64(counter)
    encrypted = _aes_ecb(_md5(key), clear.encode())
    encoded = base64.b64encode(encrypted).decode()
    return "%s!%s@%s" % (
        digest[8] + digest[16] + digest[18], random, encoded)
