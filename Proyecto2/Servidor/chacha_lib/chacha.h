/*
 * chacha.h
 *
 * Pequeña API wrapper para XChaCha20-Poly1305 (AEAD) usando libsodium.
 * Proporciona funciones simples de cifrado y descifrado que generan/usan
 * nonces y devuelven longitudes de datos cifrados/descifrados.
 *
 * Seguridad: esta API utiliza la construcción AEAD recomendada
 * `crypto_aead_xchacha20poly1305_ietf_*` de libsodium, que proporciona
 * confidencialidad y autenticidad (tag Poly1305). No use operaciones
 * de stream sin autenticar en producción.
 */

#ifndef CHACHA_H
#define CHACHA_H

#include <stddef.h>
#include <sodium.h>

/* Tamaño de la clave (32 bytes) y del nonce público (24 bytes para XChaCha) */
#define CHACHA_KEYBYTES crypto_aead_xchacha20poly1305_ietf_KEYBYTES
#define CHACHA_NONCEBYTES crypto_aead_xchacha20poly1305_ietf_NPUBBYTES

/*
 * chacha_encrypt
 * - key: buffer de `CHACHA_KEYBYTES` bytes con la clave simétrica.
 * - msg, msg_len: datos a cifrar.
 * - nonce_out: buffer donde se escribe el nonce generado (debe tener CHACHA_NONCEBYTES).
 * - ciphertext: buffer de salida donde se escriben los datos cifrados (incluye tag).
 * - cipher_len: puntero donde se escribirá la longitud de salida.
 *
 * Devuelve 0 en éxito, -1 en error.
 * Nota: genera un nonce aleatorio y lo escribe en `nonce_out`. El receptor necesita
 * el mismo nonce y la misma clave para descifrar.
 */
int chacha_encrypt(const unsigned char *key,
                   const unsigned char *msg, unsigned long long msg_len,
                   unsigned char *nonce_out, unsigned char *ciphertext, unsigned long long *cipher_len);

/*
 * chacha_decrypt
 * - key: buffer de `CHACHA_KEYBYTES` bytes con la clave simétrica.
 * - ciphertext, cipher_len: datos cifrados recibidos (incluye tag).
 * - nonce: nonce usado durante el cifrado (CHACHA_NONCEBYTES).
 * - decrypted: buffer de salida donde se escribirá el texto claro.
 * - decrypted_len: puntero donde se escribirá la longitud del texto claro.
 *
 * Devuelve 0 en éxito (autenticidad verificada), -1 en error (p. ej. tag inválido).
 */
int chacha_decrypt(const unsigned char *key,
                   const unsigned char *ciphertext, unsigned long long cipher_len,
                   const unsigned char *nonce, unsigned char *decrypted, unsigned long long *decrypted_len);

#endif // CHACHA_H
