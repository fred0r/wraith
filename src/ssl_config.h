/*
 * Copyright (C) 2002 - 2014 Bryan Drewery
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*
 * ssl_config.h -- SSL/TLS configuration constants
 *
 * This header centralizes SSL configuration settings that were previously
 * hardcoded throughout the codebase. This makes it easier to adjust security
 * settings and maintain consistent behavior.
 */

#ifndef _WRAITH_SSL_CONFIG_H
#define _WRAITH_SSL_CONFIG_H

#include <openssl/ssl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * TLS Method Selection
 * ============================================================================ */

#if (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER > 0x20020002L) || \
    (!defined(LIBRESSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER >= 0x10100000L)
#define WRAITH_SSL_CLIENT_METHOD() TLS_client_method()
#else
#define WRAITH_SSL_CLIENT_METHOD() SSLv23_client_method()
#endif

/* ============================================================================
 * Cipher Configuration — TLS 1.3
 * ============================================================================ */

/*
 * WRAITH_SSL_CIPHERSUITES
 *
 * TLS 1.3 cipher suites. These must be configured via
 * SSL_CTX_set_ciphersuites() (OpenSSL 1.1.1+), not SSL_CTX_set_cipher_list().
 */
#define WRAITH_SSL_CIPHERSUITES \
  "TLS_AES_256_GCM_SHA384:" \
  "TLS_CHACHA20_POLY1305_SHA256:" \
  "TLS_AES_128_GCM_SHA256"

/* ============================================================================
 * Cipher Configuration — TLS 1.2 and below
 * ============================================================================ */

/*
 * WRAITH_SSL_CIPHER_LIST
 *
 * TLS 1.2 ECDHE ciphers with AEAD (GCM, ChaCha20-Poly1305), plus legacy
 * CBC-mode fallbacks for compatibility with older IRC servers.
 * TLS 1.3 ciphers are configured separately via SSL_CTX_set_ciphersuites().
 *
 * Exclusions (!X means "exclude X"):
 *   - !aNULL: No anonymous authentication
 *   - !eNULL: No unencrypted ciphers
 *   - !EXPORT: No export-grade weak ciphers
 *   - !DES, !3DES: No DES or Triple-DES
 *   - !RC4: No RC4 stream cipher
 *   - !MD5: No MD5 hashing
 *   - !PSK: No pre-shared key ciphers
 *   - !aECDH: No anonymous ECDH
 *   - !EDH-DSS-DES-CBC3-SHA: Specific weak cipher
 *   - !KRB5-DES-CBC3-SHA: Specific weak Kerberos cipher
 */
#define WRAITH_SSL_CIPHER_LIST \
  "ECDHE-ECDSA-AES256-GCM-SHA384:" \
  "ECDHE-RSA-AES256-GCM-SHA384:" \
  "ECDHE-ECDSA-CHACHA20-POLY1305:" \
  "ECDHE-RSA-CHACHA20-POLY1305:" \
  "ECDHE-ECDSA-AES128-GCM-SHA256:" \
  "ECDHE-RSA-AES128-GCM-SHA256:" \
  "!aNULL:!eNULL:!EXPORT:!DES:!3DES:!RC4:!MD5:!PSK:!aECDH:" \
  "!EDH-DSS-DES-CBC3-SHA:!KRB5-DES-CBC3-SHA:" \
  "ECDHE-ECDSA-AES256-SHA:" \
  "ECDHE-RSA-AES256-SHA:" \
  "ECDHE-ECDSA-AES128-SHA:" \
  "ECDHE-RSA-AES128-SHA:" \
  "DHE-RSA-AES256-SHA256:" \
  "DHE-RSA-AES128-SHA256:" \
  "DHE-RSA-AES256-SHA:" \
  "DHE-RSA-AES128-SHA"

/* ============================================================================
 * SSL Options
 * ============================================================================ */

/*
 * WRAITH_SSL_OPTIONS
 *
 * SSL options to enable for security hardening. TLS 1.0 and 1.1 are kept
 * enabled for compatibility with older IRC servers. Only SSLv2 and SSLv3
 * are fully disabled (broken protocols).
 *   - SSL_OP_NO_SSLv2: Disable SSLv2 (broken, never use)
 *   - SSL_OP_NO_SSLv3: Disable SSLv3 (POODLE attack)
 *   - SSL_OP_SINGLE_DH_USE: Generate new DH key for each connection
 */
#define WRAITH_SSL_OPTIONS \
  (SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_SINGLE_DH_USE)

/* ============================================================================
 * SSL Modes
 * ============================================================================ */

/*
 * WRAITH_SSL_MODES
 *
 * SSL mode flags for behavior control:
 *   - SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER: Allow the write buffer to move
 *     between SSL_write() calls (improves flexibility)
 *   - SSL_MODE_ENABLE_PARTIAL_WRITE: Allow SSL_write() to return after
 *     writing partial data (improves non-blocking I/O)
 */
#define WRAITH_SSL_MODES \
  (SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER | SSL_MODE_ENABLE_PARTIAL_WRITE)

#ifdef __cplusplus
}
#endif

#endif /* _WRAITH_SSL_CONFIG_H */
/* vim: set sts=2 sw=2 ts=8 et: */
