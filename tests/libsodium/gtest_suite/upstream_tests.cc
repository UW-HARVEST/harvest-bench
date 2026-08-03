// GoogleTest wrappers around the upstream libsodium 1.0.23 test drivers in
// upstream/ (see ../README.md for provenance). Each upstream driver is a
// self-contained known-answer test: its main() (from cmptest.h) runs the
// driver's xmain(), captures stdout, and byte-compares it against the driver's
// frozen .exp file, returning 0 on an exact match. Every check uses only the
// library's PUBLIC C ABI, so the same suite runs against the C reference or a
// translated Rust cdylib.
//
// The glue TUs (glue/glue_<name>.c) each include one upstream driver verbatim
// with its harness main() renamed to harvest_<name>(). This file wraps each as
// one test case. Runtime working directory is set to upstream/ (by the harness
// / TEST_SRCDIR) so the .exp files are found.

#include <gtest/gtest.h>

extern "C" {
int harvest_aead_aegis128l_(void);
int harvest_aead_aegis256_(void);
int harvest_aead_aes256gcm_(void);
int harvest_aead_aes256gcm2_(void);
int harvest_aead_chacha20poly1305_(void);
int harvest_aead_chacha20poly13052_(void);
int harvest_aead_xchacha20poly1305_(void);
int harvest_auth_(void);
int harvest_auth2_(void);
int harvest_auth3_(void);
int harvest_auth5_(void);
int harvest_auth6_(void);
int harvest_auth7_(void);
int harvest_box_(void);
int harvest_box2_(void);
int harvest_box7_(void);
int harvest_box8_(void);
int harvest_box_easy_(void);
int harvest_box_easy2_(void);
int harvest_box_seal_(void);
int harvest_box_seed_(void);
int harvest_chacha20_(void);
int harvest_codecs_(void);
int harvest_codecs2_(void);
int harvest_codecs3_(void);
int harvest_core1_(void);
int harvest_core2_(void);
int harvest_core3_(void);
int harvest_core4_(void);
int harvest_core5_(void);
int harvest_core6_(void);
int harvest_core_ed25519_(void);
int harvest_core_ed25519_h2c_(void);
int harvest_core_keccak1600_(void);
int harvest_core_ristretto255_(void);
int harvest_core_ristretto255_h2c_(void);
int harvest_ed25519_convert_(void);
int harvest_generichash_(void);
int harvest_generichash2_(void);
int harvest_generichash3_(void);
int harvest_hash_(void);
int harvest_hash3_(void);
int harvest_hash_sha3256_(void);
int harvest_hash_sha3512_(void);
int harvest_ipcrypt_(void);
int harvest_kdf_(void);
int harvest_kdf_hkdf_(void);
int harvest_kdf_hkdf2_(void);
int harvest_kdf_hkdf3_(void);
int harvest_kdf_hkdf4_(void);
int harvest_kem_(void);
int harvest_kem_mlkem768_(void);
int harvest_kem_xwing_(void);
int harvest_keygen_(void);
int harvest_kx_(void);
int harvest_metamorphic_(void);
int harvest_misuse_(void);
int harvest_onetimeauth_(void);
int harvest_onetimeauth2_(void);
int harvest_onetimeauth7_(void);
int harvest_pwhash_argon2i_(void);
int harvest_pwhash_argon2id_(void);
int harvest_pwhash_argon2id2_(void);
int harvest_pwhash_scrypt_(void);
int harvest_pwhash_scrypt_ll_(void);
int harvest_randombytes_(void);
int harvest_scalarmult_(void);
int harvest_scalarmult2_(void);
int harvest_scalarmult5_(void);
int harvest_scalarmult6_(void);
int harvest_scalarmult7_(void);
int harvest_scalarmult8_(void);
int harvest_scalarmult_ed25519_(void);
int harvest_scalarmult_ristretto255_(void);
int harvest_secretbox_(void);
int harvest_secretbox2_(void);
int harvest_secretbox7_(void);
int harvest_secretbox8_(void);
int harvest_secretbox_easy_(void);
int harvest_secretbox_easy2_(void);
int harvest_secretstream_xchacha20poly1305_(void);
int harvest_shorthash_(void);
int harvest_sign_(void);
int harvest_sign2_(void);
int harvest_siphashx24_(void);
int harvest_sodium_core_(void);
int harvest_sodium_utils_(void);
int harvest_sodium_utils2_(void);
int harvest_sodium_utils3_(void);
int harvest_sodium_utils4_(void);
int harvest_sodium_version_(void);
int harvest_stream_(void);
int harvest_stream2_(void);
int harvest_stream3_(void);
int harvest_stream4_(void);
int harvest_stream5_(void);
int harvest_stream6_(void);
int harvest_verify1_(void);
int harvest_xchacha20_(void);
int harvest_xof2_(void);
int harvest_xof_shake128_(void);
int harvest_xof_shake256_(void);
int harvest_xof_turboshake128_(void);
int harvest_xof_turboshake256_(void);
}

namespace {
// libsodium drivers write a <name>.res file in CWD and read <name>.exp from
// TEST_SRCDIR (default "."). The suite CMake sets TEST_SRCDIR to the upstream
// dir; tests also chdir there via the fixture below so .res writes land in a
// temp scratch and .exp reads resolve.
TEST(Libsodium, aead_aegis128l_) { EXPECT_EQ(harvest_aead_aegis128l_(), 0); }
TEST(Libsodium, aead_aegis256_) { EXPECT_EQ(harvest_aead_aegis256_(), 0); }
TEST(Libsodium, aead_aes256gcm_) { EXPECT_EQ(harvest_aead_aes256gcm_(), 0); }
TEST(Libsodium, aead_aes256gcm2_) { EXPECT_EQ(harvest_aead_aes256gcm2_(), 0); }
TEST(Libsodium, aead_chacha20poly1305_) { EXPECT_EQ(harvest_aead_chacha20poly1305_(), 0); }
TEST(Libsodium, aead_chacha20poly13052_) { EXPECT_EQ(harvest_aead_chacha20poly13052_(), 0); }
TEST(Libsodium, aead_xchacha20poly1305_) { EXPECT_EQ(harvest_aead_xchacha20poly1305_(), 0); }
TEST(Libsodium, auth_) { EXPECT_EQ(harvest_auth_(), 0); }
TEST(Libsodium, auth2_) { EXPECT_EQ(harvest_auth2_(), 0); }
TEST(Libsodium, auth3_) { EXPECT_EQ(harvest_auth3_(), 0); }
TEST(Libsodium, auth5_) { EXPECT_EQ(harvest_auth5_(), 0); }
TEST(Libsodium, auth6_) { EXPECT_EQ(harvest_auth6_(), 0); }
TEST(Libsodium, auth7_) { EXPECT_EQ(harvest_auth7_(), 0); }
TEST(Libsodium, box_) { EXPECT_EQ(harvest_box_(), 0); }
TEST(Libsodium, box2_) { EXPECT_EQ(harvest_box2_(), 0); }
TEST(Libsodium, box7_) { EXPECT_EQ(harvest_box7_(), 0); }
TEST(Libsodium, box8_) { EXPECT_EQ(harvest_box8_(), 0); }
TEST(Libsodium, box_easy_) { EXPECT_EQ(harvest_box_easy_(), 0); }
TEST(Libsodium, box_easy2_) { EXPECT_EQ(harvest_box_easy2_(), 0); }
TEST(Libsodium, box_seal_) { EXPECT_EQ(harvest_box_seal_(), 0); }
TEST(Libsodium, box_seed_) { EXPECT_EQ(harvest_box_seed_(), 0); }
TEST(Libsodium, chacha20_) { EXPECT_EQ(harvest_chacha20_(), 0); }
TEST(Libsodium, codecs_) { EXPECT_EQ(harvest_codecs_(), 0); }
TEST(Libsodium, codecs2_) { EXPECT_EQ(harvest_codecs2_(), 0); }
TEST(Libsodium, codecs3_) { EXPECT_EQ(harvest_codecs3_(), 0); }
TEST(Libsodium, core1_) { EXPECT_EQ(harvest_core1_(), 0); }
TEST(Libsodium, core2_) { EXPECT_EQ(harvest_core2_(), 0); }
TEST(Libsodium, core3_) { EXPECT_EQ(harvest_core3_(), 0); }
TEST(Libsodium, core4_) { EXPECT_EQ(harvest_core4_(), 0); }
TEST(Libsodium, core5_) { EXPECT_EQ(harvest_core5_(), 0); }
TEST(Libsodium, core6_) { EXPECT_EQ(harvest_core6_(), 0); }
TEST(Libsodium, core_ed25519_) { EXPECT_EQ(harvest_core_ed25519_(), 0); }
TEST(Libsodium, core_ed25519_h2c_) { EXPECT_EQ(harvest_core_ed25519_h2c_(), 0); }
TEST(Libsodium, core_keccak1600_) { EXPECT_EQ(harvest_core_keccak1600_(), 0); }
TEST(Libsodium, core_ristretto255_) { EXPECT_EQ(harvest_core_ristretto255_(), 0); }
TEST(Libsodium, core_ristretto255_h2c_) { EXPECT_EQ(harvest_core_ristretto255_h2c_(), 0); }
TEST(Libsodium, ed25519_convert_) { EXPECT_EQ(harvest_ed25519_convert_(), 0); }
TEST(Libsodium, generichash_) { EXPECT_EQ(harvest_generichash_(), 0); }
TEST(Libsodium, generichash2_) { EXPECT_EQ(harvest_generichash2_(), 0); }
TEST(Libsodium, generichash3_) { EXPECT_EQ(harvest_generichash3_(), 0); }
TEST(Libsodium, hash_) { EXPECT_EQ(harvest_hash_(), 0); }
TEST(Libsodium, hash3_) { EXPECT_EQ(harvest_hash3_(), 0); }
TEST(Libsodium, hash_sha3256_) { EXPECT_EQ(harvest_hash_sha3256_(), 0); }
TEST(Libsodium, hash_sha3512_) { EXPECT_EQ(harvest_hash_sha3512_(), 0); }
TEST(Libsodium, ipcrypt_) { EXPECT_EQ(harvest_ipcrypt_(), 0); }
TEST(Libsodium, kdf_) { EXPECT_EQ(harvest_kdf_(), 0); }
TEST(Libsodium, kdf_hkdf_) { EXPECT_EQ(harvest_kdf_hkdf_(), 0); }
TEST(Libsodium, kdf_hkdf2_) { EXPECT_EQ(harvest_kdf_hkdf2_(), 0); }
TEST(Libsodium, kdf_hkdf3_) { EXPECT_EQ(harvest_kdf_hkdf3_(), 0); }
TEST(Libsodium, kdf_hkdf4_) { EXPECT_EQ(harvest_kdf_hkdf4_(), 0); }
TEST(Libsodium, kem_) { EXPECT_EQ(harvest_kem_(), 0); }
TEST(Libsodium, kem_mlkem768_) { EXPECT_EQ(harvest_kem_mlkem768_(), 0); }
TEST(Libsodium, kem_xwing_) { EXPECT_EQ(harvest_kem_xwing_(), 0); }
TEST(Libsodium, keygen_) { EXPECT_EQ(harvest_keygen_(), 0); }
TEST(Libsodium, kx_) { EXPECT_EQ(harvest_kx_(), 0); }
TEST(Libsodium, metamorphic_) { EXPECT_EQ(harvest_metamorphic_(), 0); }
TEST(Libsodium, misuse_) { EXPECT_EQ(harvest_misuse_(), 0); }
TEST(Libsodium, onetimeauth_) { EXPECT_EQ(harvest_onetimeauth_(), 0); }
TEST(Libsodium, onetimeauth2_) { EXPECT_EQ(harvest_onetimeauth2_(), 0); }
TEST(Libsodium, onetimeauth7_) { EXPECT_EQ(harvest_onetimeauth7_(), 0); }
TEST(Libsodium, pwhash_argon2i_) { EXPECT_EQ(harvest_pwhash_argon2i_(), 0); }
TEST(Libsodium, pwhash_argon2id_) { EXPECT_EQ(harvest_pwhash_argon2id_(), 0); }
TEST(Libsodium, pwhash_argon2id2_) { EXPECT_EQ(harvest_pwhash_argon2id2_(), 0); }
TEST(Libsodium, pwhash_scrypt_) { EXPECT_EQ(harvest_pwhash_scrypt_(), 0); }
TEST(Libsodium, pwhash_scrypt_ll_) { EXPECT_EQ(harvest_pwhash_scrypt_ll_(), 0); }
TEST(Libsodium, randombytes_) { EXPECT_EQ(harvest_randombytes_(), 0); }
TEST(Libsodium, scalarmult_) { EXPECT_EQ(harvest_scalarmult_(), 0); }
TEST(Libsodium, scalarmult2_) { EXPECT_EQ(harvest_scalarmult2_(), 0); }
TEST(Libsodium, scalarmult5_) { EXPECT_EQ(harvest_scalarmult5_(), 0); }
TEST(Libsodium, scalarmult6_) { EXPECT_EQ(harvest_scalarmult6_(), 0); }
TEST(Libsodium, scalarmult7_) { EXPECT_EQ(harvest_scalarmult7_(), 0); }
TEST(Libsodium, scalarmult8_) { EXPECT_EQ(harvest_scalarmult8_(), 0); }
TEST(Libsodium, scalarmult_ed25519_) { EXPECT_EQ(harvest_scalarmult_ed25519_(), 0); }
TEST(Libsodium, scalarmult_ristretto255_) { EXPECT_EQ(harvest_scalarmult_ristretto255_(), 0); }
TEST(Libsodium, secretbox_) { EXPECT_EQ(harvest_secretbox_(), 0); }
TEST(Libsodium, secretbox2_) { EXPECT_EQ(harvest_secretbox2_(), 0); }
TEST(Libsodium, secretbox7_) { EXPECT_EQ(harvest_secretbox7_(), 0); }
TEST(Libsodium, secretbox8_) { EXPECT_EQ(harvest_secretbox8_(), 0); }
TEST(Libsodium, secretbox_easy_) { EXPECT_EQ(harvest_secretbox_easy_(), 0); }
TEST(Libsodium, secretbox_easy2_) { EXPECT_EQ(harvest_secretbox_easy2_(), 0); }
TEST(Libsodium, secretstream_xchacha20poly1305_) { EXPECT_EQ(harvest_secretstream_xchacha20poly1305_(), 0); }
TEST(Libsodium, shorthash_) { EXPECT_EQ(harvest_shorthash_(), 0); }
TEST(Libsodium, sign_) { EXPECT_EQ(harvest_sign_(), 0); }
TEST(Libsodium, sign2_) { EXPECT_EQ(harvest_sign2_(), 0); }
TEST(Libsodium, siphashx24_) { EXPECT_EQ(harvest_siphashx24_(), 0); }
TEST(Libsodium, sodium_core_) { EXPECT_EQ(harvest_sodium_core_(), 0); }
TEST(Libsodium, sodium_utils_) { EXPECT_EQ(harvest_sodium_utils_(), 0); }
TEST(Libsodium, sodium_utils2_) { EXPECT_EQ(harvest_sodium_utils2_(), 0); }
TEST(Libsodium, sodium_utils3_) { EXPECT_EQ(harvest_sodium_utils3_(), 0); }
TEST(Libsodium, sodium_utils4_) { EXPECT_EQ(harvest_sodium_utils4_(), 0); }
TEST(Libsodium, sodium_version_) { EXPECT_EQ(harvest_sodium_version_(), 0); }
TEST(Libsodium, stream_) { EXPECT_EQ(harvest_stream_(), 0); }
TEST(Libsodium, stream2_) { EXPECT_EQ(harvest_stream2_(), 0); }
TEST(Libsodium, stream3_) { EXPECT_EQ(harvest_stream3_(), 0); }
TEST(Libsodium, stream4_) { EXPECT_EQ(harvest_stream4_(), 0); }
TEST(Libsodium, stream5_) { EXPECT_EQ(harvest_stream5_(), 0); }
TEST(Libsodium, stream6_) { EXPECT_EQ(harvest_stream6_(), 0); }
TEST(Libsodium, verify1_) { EXPECT_EQ(harvest_verify1_(), 0); }
TEST(Libsodium, xchacha20_) { EXPECT_EQ(harvest_xchacha20_(), 0); }
TEST(Libsodium, xof2_) { EXPECT_EQ(harvest_xof2_(), 0); }
TEST(Libsodium, xof_shake128_) { EXPECT_EQ(harvest_xof_shake128_(), 0); }
TEST(Libsodium, xof_shake256_) { EXPECT_EQ(harvest_xof_shake256_(), 0); }
TEST(Libsodium, xof_turboshake128_) { EXPECT_EQ(harvest_xof_turboshake128_(), 0); }
TEST(Libsodium, xof_turboshake256_) { EXPECT_EQ(harvest_xof_turboshake256_(), 0); }
}  // namespace
