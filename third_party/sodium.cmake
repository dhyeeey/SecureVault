# CMake wrapper for the official libsodium git submodule (1.0.20-RELEASE).

option(SODIUM_MINIMAL "Only compile the minimum set of functions required for the high-level API" OFF)
option(SODIUM_ENABLE_BLOCKING_RANDOM "Enable this switch only if /dev/urandom is totally broken on the target platform" OFF)

set(SODIUM_ROOT "${CMAKE_CURRENT_LIST_DIR}/libsodium")
set(_s "${SODIUM_ROOT}/src/libsodium")

if(NOT EXISTS "${_s}/include/sodium.h")
    message(FATAL_ERROR
        "libsodium sources are missing at ${SODIUM_ROOT}. "
        "Clone with git clone --recurse-submodules, or run: git submodule update --init --recursive")
endif()

add_library(sodium
    ${_s}/crypto_aead/aegis128l/aead_aegis128l.c
    ${_s}/crypto_aead/aegis128l/aegis128l_aesni.c
    ${_s}/crypto_aead/aegis128l/aegis128l_armcrypto.c
    ${_s}/crypto_aead/aegis128l/aegis128l_soft.c
    ${_s}/crypto_aead/aegis256/aead_aegis256.c
    ${_s}/crypto_aead/aegis256/aegis256_aesni.c
    ${_s}/crypto_aead/aegis256/aegis256_armcrypto.c
    ${_s}/crypto_aead/aegis256/aegis256_soft.c
    ${_s}/crypto_aead/aes256gcm/aesni/aead_aes256gcm_aesni.c
    ${_s}/crypto_aead/aes256gcm/aead_aes256gcm.c
    ${_s}/crypto_aead/chacha20poly1305/aead_chacha20poly1305.c
    ${_s}/crypto_aead/xchacha20poly1305/aead_xchacha20poly1305.c
    ${_s}/crypto_auth/crypto_auth.c
    ${_s}/crypto_auth/hmacsha256/auth_hmacsha256.c
    ${_s}/crypto_auth/hmacsha512/auth_hmacsha512.c
    ${_s}/crypto_auth/hmacsha512256/auth_hmacsha512256.c
    ${_s}/crypto_box/crypto_box.c
    ${_s}/crypto_box/crypto_box_easy.c
    ${_s}/crypto_box/crypto_box_seal.c
    ${_s}/crypto_box/curve25519xsalsa20poly1305/box_curve25519xsalsa20poly1305.c
    ${_s}/crypto_core/ed25519/ref10/ed25519_ref10.c
    ${_s}/crypto_core/ed25519/ref10/fe_25_5/base.h
    ${_s}/crypto_core/ed25519/ref10/fe_25_5/base2.h
    ${_s}/crypto_core/ed25519/ref10/fe_25_5/constants.h
    ${_s}/crypto_core/ed25519/ref10/fe_25_5/fe.h
    ${_s}/crypto_core/ed25519/ref10/fe_51/base.h
    ${_s}/crypto_core/ed25519/ref10/fe_51/base2.h
    ${_s}/crypto_core/ed25519/ref10/fe_51/constants.h
    ${_s}/crypto_core/ed25519/ref10/fe_51/fe.h
    ${_s}/crypto_core/hchacha20/core_hchacha20.c
    ${_s}/crypto_core/hsalsa20/core_hsalsa20.c
    ${_s}/crypto_core/hsalsa20/ref2/core_hsalsa20_ref2.c
    ${_s}/crypto_core/salsa/ref/core_salsa_ref.c
    ${_s}/crypto_core/softaes/softaes.c
    ${_s}/crypto_generichash/blake2b/generichash_blake2.c
    ${_s}/crypto_generichash/blake2b/ref/blake2.h
    ${_s}/crypto_generichash/blake2b/ref/blake2b-compress-avx2.c
    ${_s}/crypto_generichash/blake2b/ref/blake2b-compress-avx2.h
    ${_s}/crypto_generichash/blake2b/ref/blake2b-compress-ref.c
    ${_s}/crypto_generichash/blake2b/ref/blake2b-compress-sse41.c
    ${_s}/crypto_generichash/blake2b/ref/blake2b-compress-sse41.h
    ${_s}/crypto_generichash/blake2b/ref/blake2b-compress-ssse3.c
    ${_s}/crypto_generichash/blake2b/ref/blake2b-compress-ssse3.h
    ${_s}/crypto_generichash/blake2b/ref/blake2b-load-avx2.h
    ${_s}/crypto_generichash/blake2b/ref/blake2b-load-sse2.h
    ${_s}/crypto_generichash/blake2b/ref/blake2b-load-sse41.h
    ${_s}/crypto_generichash/blake2b/ref/blake2b-ref.c
    ${_s}/crypto_generichash/blake2b/ref/generichash_blake2b.c
    ${_s}/crypto_generichash/crypto_generichash.c
    ${_s}/crypto_hash/crypto_hash.c
    ${_s}/crypto_hash/sha256/cp/hash_sha256_cp.c
    ${_s}/crypto_hash/sha256/hash_sha256.c
    ${_s}/crypto_hash/sha512/cp/hash_sha512_cp.c
    ${_s}/crypto_hash/sha512/hash_sha512.c
    ${_s}/crypto_kdf/blake2b/kdf_blake2b.c
    ${_s}/crypto_kdf/crypto_kdf.c
    ${_s}/crypto_kdf/hkdf/kdf_hkdf_sha256.c
    ${_s}/crypto_kdf/hkdf/kdf_hkdf_sha512.c
    ${_s}/crypto_kx/crypto_kx.c
    ${_s}/crypto_onetimeauth/crypto_onetimeauth.c
    ${_s}/crypto_onetimeauth/poly1305/donna/poly1305_donna.c
    ${_s}/crypto_onetimeauth/poly1305/donna/poly1305_donna.h
    ${_s}/crypto_onetimeauth/poly1305/donna/poly1305_donna32.h
    ${_s}/crypto_onetimeauth/poly1305/donna/poly1305_donna64.h
    ${_s}/crypto_onetimeauth/poly1305/onetimeauth_poly1305.c
    ${_s}/crypto_onetimeauth/poly1305/onetimeauth_poly1305.h
    ${_s}/crypto_onetimeauth/poly1305/sse2/poly1305_sse2.c
    ${_s}/crypto_onetimeauth/poly1305/sse2/poly1305_sse2.h
    ${_s}/crypto_pwhash/argon2/argon2-core.c
    ${_s}/crypto_pwhash/argon2/argon2-core.h
    ${_s}/crypto_pwhash/argon2/argon2-encoding.c
    ${_s}/crypto_pwhash/argon2/argon2-encoding.h
    ${_s}/crypto_pwhash/argon2/argon2-fill-block-avx2.c
    ${_s}/crypto_pwhash/argon2/argon2-fill-block-avx512f.c
    ${_s}/crypto_pwhash/argon2/argon2-fill-block-ref.c
    ${_s}/crypto_pwhash/argon2/argon2-fill-block-ssse3.c
    ${_s}/crypto_pwhash/argon2/argon2.c
    ${_s}/crypto_pwhash/argon2/argon2.h
    ${_s}/crypto_pwhash/argon2/blake2b-long.c
    ${_s}/crypto_pwhash/argon2/blake2b-long.h
    ${_s}/crypto_pwhash/argon2/blamka-round-avx2.h
    ${_s}/crypto_pwhash/argon2/blamka-round-avx512f.h
    ${_s}/crypto_pwhash/argon2/blamka-round-ref.h
    ${_s}/crypto_pwhash/argon2/blamka-round-ssse3.h
    ${_s}/crypto_pwhash/argon2/pwhash_argon2i.c
    ${_s}/crypto_pwhash/argon2/pwhash_argon2id.c
    ${_s}/crypto_pwhash/crypto_pwhash.c
    ${_s}/crypto_scalarmult/crypto_scalarmult.c
    ${_s}/crypto_scalarmult/curve25519/ref10/x25519_ref10.c
    ${_s}/crypto_scalarmult/curve25519/ref10/x25519_ref10.h
    ${_s}/crypto_scalarmult/curve25519/sandy2x/consts_namespace.h
    ${_s}/crypto_scalarmult/curve25519/sandy2x/curve25519_sandy2x.c
    ${_s}/crypto_scalarmult/curve25519/sandy2x/curve25519_sandy2x.h
    ${_s}/crypto_scalarmult/curve25519/sandy2x/fe.h
    ${_s}/crypto_scalarmult/curve25519/sandy2x/fe51.h
    ${_s}/crypto_scalarmult/curve25519/sandy2x/fe51_invert.c
    ${_s}/crypto_scalarmult/curve25519/sandy2x/fe51_namespace.h
    ${_s}/crypto_scalarmult/curve25519/sandy2x/fe_frombytes_sandy2x.c
    ${_s}/crypto_scalarmult/curve25519/sandy2x/ladder.h
    ${_s}/crypto_scalarmult/curve25519/sandy2x/ladder_namespace.h
    ${_s}/crypto_scalarmult/curve25519/scalarmult_curve25519.c
    ${_s}/crypto_scalarmult/curve25519/scalarmult_curve25519.h
    ${_s}/crypto_secretbox/crypto_secretbox.c
    ${_s}/crypto_secretbox/crypto_secretbox_easy.c
    ${_s}/crypto_secretbox/xsalsa20poly1305/secretbox_xsalsa20poly1305.c
    ${_s}/crypto_secretstream/xchacha20poly1305/secretstream_xchacha20poly1305.c
    ${_s}/crypto_shorthash/crypto_shorthash.c
    ${_s}/crypto_shorthash/siphash24/ref/shorthash_siphash24_ref.c
    ${_s}/crypto_shorthash/siphash24/ref/shorthash_siphash_ref.h
    ${_s}/crypto_shorthash/siphash24/shorthash_siphash24.c
    ${_s}/crypto_sign/crypto_sign.c
    ${_s}/crypto_sign/ed25519/ref10/keypair.c
    ${_s}/crypto_sign/ed25519/ref10/open.c
    ${_s}/crypto_sign/ed25519/ref10/sign.c
    ${_s}/crypto_sign/ed25519/ref10/sign_ed25519_ref10.h
    ${_s}/crypto_sign/ed25519/sign_ed25519.c
    ${_s}/crypto_stream/chacha20/dolbeau/chacha20_dolbeau-avx2.c
    ${_s}/crypto_stream/chacha20/dolbeau/chacha20_dolbeau-avx2.h
    ${_s}/crypto_stream/chacha20/dolbeau/chacha20_dolbeau-ssse3.c
    ${_s}/crypto_stream/chacha20/dolbeau/chacha20_dolbeau-ssse3.h
    ${_s}/crypto_stream/chacha20/dolbeau/u0.h
    ${_s}/crypto_stream/chacha20/dolbeau/u1.h
    ${_s}/crypto_stream/chacha20/dolbeau/u4.h
    ${_s}/crypto_stream/chacha20/dolbeau/u8.h
    ${_s}/crypto_stream/chacha20/ref/chacha20_ref.c
    ${_s}/crypto_stream/chacha20/ref/chacha20_ref.h
    ${_s}/crypto_stream/chacha20/stream_chacha20.c
    ${_s}/crypto_stream/chacha20/stream_chacha20.h
    ${_s}/crypto_stream/crypto_stream.c
    ${_s}/crypto_stream/salsa20/ref/salsa20_ref.c
    ${_s}/crypto_stream/salsa20/ref/salsa20_ref.h
    ${_s}/crypto_stream/salsa20/stream_salsa20.c
    ${_s}/crypto_stream/salsa20/stream_salsa20.h
    ${_s}/crypto_stream/salsa20/xmm6/salsa20_xmm6.c
    ${_s}/crypto_stream/salsa20/xmm6/salsa20_xmm6.h
    ${_s}/crypto_stream/salsa20/xmm6int/salsa20_xmm6int-avx2.c
    ${_s}/crypto_stream/salsa20/xmm6int/salsa20_xmm6int-avx2.h
    ${_s}/crypto_stream/salsa20/xmm6int/salsa20_xmm6int-sse2.c
    ${_s}/crypto_stream/salsa20/xmm6int/salsa20_xmm6int-sse2.h
    ${_s}/crypto_stream/salsa20/xmm6int/u0.h
    ${_s}/crypto_stream/salsa20/xmm6int/u1.h
    ${_s}/crypto_stream/salsa20/xmm6int/u4.h
    ${_s}/crypto_stream/salsa20/xmm6int/u8.h
    ${_s}/crypto_stream/xsalsa20/stream_xsalsa20.c
    ${_s}/crypto_verify/verify.c
    ${_s}/include/sodium.h
    ${_s}/include/sodium/core.h
    ${_s}/include/sodium/crypto_aead_aegis128l.h
    ${_s}/include/sodium/crypto_aead_aegis256.h
    ${_s}/include/sodium/crypto_aead_aes256gcm.h
    ${_s}/include/sodium/crypto_aead_chacha20poly1305.h
    ${_s}/include/sodium/crypto_aead_xchacha20poly1305.h
    ${_s}/include/sodium/crypto_auth.h
    ${_s}/include/sodium/crypto_auth_hmacsha256.h
    ${_s}/include/sodium/crypto_auth_hmacsha512.h
    ${_s}/include/sodium/crypto_auth_hmacsha512256.h
    ${_s}/include/sodium/crypto_box.h
    ${_s}/include/sodium/crypto_box_curve25519xchacha20poly1305.h
    ${_s}/include/sodium/crypto_box_curve25519xsalsa20poly1305.h
    ${_s}/include/sodium/crypto_core_ed25519.h
    ${_s}/include/sodium/crypto_core_hchacha20.h
    ${_s}/include/sodium/crypto_core_hsalsa20.h
    ${_s}/include/sodium/crypto_core_ristretto255.h
    ${_s}/include/sodium/crypto_core_salsa20.h
    ${_s}/include/sodium/crypto_core_salsa2012.h
    ${_s}/include/sodium/crypto_core_salsa208.h
    ${_s}/include/sodium/crypto_generichash.h
    ${_s}/include/sodium/crypto_generichash_blake2b.h
    ${_s}/include/sodium/crypto_hash.h
    ${_s}/include/sodium/crypto_hash_sha256.h
    ${_s}/include/sodium/crypto_hash_sha512.h
    ${_s}/include/sodium/crypto_kdf.h
    ${_s}/include/sodium/crypto_kdf_blake2b.h
    ${_s}/include/sodium/crypto_kx.h
    ${_s}/include/sodium/crypto_onetimeauth.h
    ${_s}/include/sodium/crypto_onetimeauth_poly1305.h
    ${_s}/include/sodium/crypto_pwhash.h
    ${_s}/include/sodium/crypto_pwhash_argon2i.h
    ${_s}/include/sodium/crypto_pwhash_argon2id.h
    ${_s}/include/sodium/crypto_pwhash_scryptsalsa208sha256.h
    ${_s}/include/sodium/crypto_scalarmult.h
    ${_s}/include/sodium/crypto_scalarmult_curve25519.h
    ${_s}/include/sodium/crypto_scalarmult_ed25519.h
    ${_s}/include/sodium/crypto_scalarmult_ristretto255.h
    ${_s}/include/sodium/crypto_secretbox.h
    ${_s}/include/sodium/crypto_secretbox_xchacha20poly1305.h
    ${_s}/include/sodium/crypto_secretbox_xsalsa20poly1305.h
    ${_s}/include/sodium/crypto_secretstream_xchacha20poly1305.h
    ${_s}/include/sodium/crypto_shorthash.h
    ${_s}/include/sodium/crypto_shorthash_siphash24.h
    ${_s}/include/sodium/crypto_sign.h
    ${_s}/include/sodium/crypto_sign_ed25519.h
    ${_s}/include/sodium/crypto_sign_edwards25519sha512batch.h
    ${_s}/include/sodium/crypto_stream.h
    ${_s}/include/sodium/crypto_stream_chacha20.h
    ${_s}/include/sodium/crypto_stream_salsa20.h
    ${_s}/include/sodium/crypto_stream_salsa2012.h
    ${_s}/include/sodium/crypto_stream_salsa208.h
    ${_s}/include/sodium/crypto_stream_xchacha20.h
    ${_s}/include/sodium/crypto_stream_xsalsa20.h
    ${_s}/include/sodium/crypto_verify_16.h
    ${_s}/include/sodium/crypto_verify_32.h
    ${_s}/include/sodium/crypto_verify_64.h
    ${_s}/include/sodium/export.h
    ${_s}/include/sodium/private/chacha20_ietf_ext.h
    ${_s}/include/sodium/private/common.h
    ${_s}/include/sodium/private/ed25519_ref10.h
    ${_s}/include/sodium/private/ed25519_ref10_fe_25_5.h
    ${_s}/include/sodium/private/ed25519_ref10_fe_51.h
    ${_s}/include/sodium/private/implementations.h
    ${_s}/include/sodium/private/mutex.h
    ${_s}/include/sodium/private/sse2_64_32.h
    ${_s}/include/sodium/randombytes.h
    ${_s}/include/sodium/randombytes_internal_random.h
    ${_s}/include/sodium/randombytes_sysrandom.h
    ${_s}/include/sodium/runtime.h
    ${_s}/include/sodium/utils.h
    ${_s}/randombytes/internal/randombytes_internal_random.c
    ${_s}/randombytes/randombytes.c
    ${_s}/randombytes/sysrandom/randombytes_sysrandom.c
    ${_s}/sodium/codecs.c
    ${_s}/sodium/core.c
    ${_s}/sodium/runtime.c
    ${_s}/sodium/utils.c
    ${_s}/sodium/version.c
)

if(NOT SODIUM_MINIMAL)
    target_sources(sodium
        PRIVATE
            ${_s}/crypto_box/curve25519xchacha20poly1305/box_curve25519xchacha20poly1305.c
            ${_s}/crypto_box/curve25519xchacha20poly1305/box_seal_curve25519xchacha20poly1305.c
            ${_s}/crypto_core/ed25519/core_ed25519.c
            ${_s}/crypto_core/ed25519/core_ristretto255.c
            ${_s}/crypto_pwhash/scryptsalsa208sha256/crypto_scrypt-common.c
            ${_s}/crypto_pwhash/scryptsalsa208sha256/crypto_scrypt.h
            ${_s}/crypto_pwhash/scryptsalsa208sha256/nosse/pwhash_scryptsalsa208sha256_nosse.c
            ${_s}/crypto_pwhash/scryptsalsa208sha256/pbkdf2-sha256.c
            ${_s}/crypto_pwhash/scryptsalsa208sha256/pbkdf2-sha256.h
            ${_s}/crypto_pwhash/scryptsalsa208sha256/pwhash_scryptsalsa208sha256.c
            ${_s}/crypto_pwhash/scryptsalsa208sha256/scrypt_platform.c
            ${_s}/crypto_pwhash/scryptsalsa208sha256/sse/pwhash_scryptsalsa208sha256_sse.c
            ${_s}/crypto_scalarmult/ed25519/ref10/scalarmult_ed25519_ref10.c
            ${_s}/crypto_scalarmult/ristretto255/ref10/scalarmult_ristretto255_ref10.c
            ${_s}/crypto_secretbox/xchacha20poly1305/secretbox_xchacha20poly1305.c
            ${_s}/crypto_shorthash/siphash24/ref/shorthash_siphashx24_ref.c
            ${_s}/crypto_shorthash/siphash24/shorthash_siphashx24.c
            ${_s}/crypto_sign/ed25519/ref10/obsolete.c
            ${_s}/crypto_stream/salsa2012/ref/stream_salsa2012_ref.c
            ${_s}/crypto_stream/salsa2012/stream_salsa2012.c
            ${_s}/crypto_stream/salsa208/ref/stream_salsa208_ref.c
            ${_s}/crypto_stream/salsa208/stream_salsa208.c
            ${_s}/crypto_stream/xchacha20/stream_xchacha20.c
    )
endif()

set_target_properties(sodium
    PROPERTIES
        C_STANDARD 99
        POSITION_INDEPENDENT_CODE ON
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

set(VERSION 1.0.20)
set(SODIUM_LIBRARY_VERSION_MAJOR 26)
set(SODIUM_LIBRARY_VERSION_MINOR 2)
set(SODIUM_LIBRARY_MINIMAL_DEF "")
if(SODIUM_MINIMAL)
    set(SODIUM_LIBRARY_MINIMAL_DEF "#define SODIUM_LIBRARY_MINIMAL 1")
endif()

set(_sodium_gen "${CMAKE_CURRENT_BINARY_DIR}/libsodium_export")
configure_file(
    "${_s}/include/sodium/version.h.in"
    "${_sodium_gen}/sodium/version.h"
    @ONLY
)

target_include_directories(sodium
    PUBLIC
        "${_sodium_gen}"
        "${_s}/include"
        "${_s}/include/sodium"
    PRIVATE
        "${_sodium_gen}/sodium"
)

target_compile_definitions(sodium
    PUBLIC
        $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:SODIUM_STATIC>
        $<$<BOOL:${SODIUM_MINIMAL}>:SODIUM_LIBRARY_MINIMAL>
    PRIVATE
        CONFIGURED
        $<$<BOOL:${BUILD_SHARED_LIBS}>:SODIUM_DLL_EXPORT>
        $<$<BOOL:${SODIUM_ENABLE_BLOCKING_RANDOM}>:USE_BLOCKING_RANDOM>
        $<$<BOOL:${SODIUM_MINIMAL}>:MINIMAL>
        $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
)

if(CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_definitions(sodium
        PRIVATE
            _CRT_SECURE_NO_WARNINGS
    )
    target_compile_options(sodium
        PUBLIC
            -mavx2
        PRIVATE
            -maes
            -mpclmul
            -mssse3
    )
endif()

unset(_s)
unset(_sodium_gen)
