/*
 * chacha.c
 * Implementación mínima de wrapper para XChaCha20-Poly1305 (libsodium).
 * Proporciona dos funciones: chacha_encrypt y chacha_decrypt.
 *
 * Notas de seguridad:
 * - Usar siempre la versión AEAD (XChaCha20-Poly1305) para evitar ataques de
 *   manipulación o reordenamiento (Poly1305 verifica integridad).
 * - No reutilizar la misma (clave, nonce) para distintos mensajes.
 */

#include "chacha.h"
#include <sodium.h>
#include <string.h>

int chacha_encrypt(const unsigned char *key,
                   const unsigned char *msg, unsigned long long msg_len,
                   unsigned char *nonce_out, unsigned char *ciphertext, unsigned long long *cipher_len)
{
    /* Validación básica de parámetros */
    if (!key || !msg || !nonce_out || !ciphertext || !cipher_len) return -1;

    /* Genera un nonce aleatorio XChaCha (24 bytes) y lo escribe en nonce_out */
    randombytes_buf(nonce_out, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);

    unsigned long long clen = 0;

    /* Cifrado AEAD: no hay datos adicionales (ad=NULL, adlen=0) y no usamos nonce
     * asociado; la función escribe el ciphertext + tag en `ciphertext`.
     */
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(ciphertext, &clen,
                                                   msg, msg_len,
                                                   NULL, 0,
                                                   NULL, nonce_out, key) != 0)
    {
        /* Error de cifrado (inusual con libsodium) */
        return -1;
    }

    *cipher_len = clen;
    return 0;
}

int chacha_decrypt(const unsigned char *key,
                   const unsigned char *ciphertext, unsigned long long cipher_len,
                   const unsigned char *nonce, unsigned char *decrypted, unsigned long long *decrypted_len)
{
    /* Validación de parámetros */
    if (!key || !ciphertext || !nonce || !decrypted || !decrypted_len) return -1;

    unsigned long long mlen = 0;

    /* Descifrado AEAD. Si la verificación de tag falla, la función devuelve != 0. */
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(decrypted, &mlen,
                                                   NULL,
                                                   ciphertext, cipher_len,
                                                   NULL, 0,
                                                   nonce, key) != 0)
    {
        /* Tag inválido o datos corruptos */
        return -1;
    }

    *decrypted_len = mlen;
    return 0;
}
