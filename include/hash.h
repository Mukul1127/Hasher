#ifndef HASH_H
#define HASH_H

#define WOLFSSL_NO_OPTIONS_H
#define WC_RSA_BLINDING
#define WOLFSSL_SHA512
#define WOLFSSL_SHA3
#define HAVE_BLAKE2
#define HAVE_BLAKE2B

#include <wolfssl/wolfcrypt/blake2.h>
#include <wolfssl/wolfcrypt/hash.h>
#include <stdexcept>
#include <string>
#include <iostream>
#include <format>
#include <vector>
#include <map>
#include <future>

class HashException;

class Hasher {
    wc_HashAlg hash{};
    Blake2b blake2bhash{};
    wc_HashType algorithm;
    std::vector<byte> digest;
    bool finalized;

    public:
        explicit Hasher(wc_HashType algorithm);
        void updateWithBuffer(const byte* buffer, word32 bufferSize);
        void finalize();
        [[nodiscard]] std::string getDigest() const;
        ~Hasher();
};

std::map<wc_HashType, std::string> calculateHashes(const std::string& filePath, const std::vector<wc_HashType>& hashesToCalculate, std::optional<std::reference_wrapper<const std::atomic<bool>>> shouldCancel = std::nullopt);

#endif // HASH_H