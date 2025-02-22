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

class HashException : public std::runtime_error
{
    private:
        wc_HashType errorAlgorithm;
        int errorCode;

    public:
        HashException(std::string errorMessage, int code, wc_HashType algorithm)
            : std::runtime_error(errorMessage), errorCode(code), errorAlgorithm(algorithm)
        {
            // Log error
            std::cerr << this->formattedMessage() << std::endl;
        }

        std::string formattedMessage()
        {
            // Return formatted message for logging
            return std::format("Error Message: {} - Returned Code: {} - Algorithm Used: {}", std::runtime_error::what(), errorCode, (int)errorAlgorithm);
        }

        int code()
        {
            // Return code
            return errorCode;
        }

        wc_HashType algorithm()
        {
            // Return algorithm
            return errorAlgorithm;
        }
};

class Hasher {
    private:
        wc_HashAlg hash;
        Blake2b blake2bhash;
        wc_HashType algorithm;
        std::vector<byte> digest;
        bool finalized;

    public:
        Hasher(wc_HashType algorithm);
        void updateWithBuffer(const byte* buffer, word32 bufferSize);
        void finalize();
        std::string getDigest();
        ~Hasher();
};

std::map<wc_HashType, std::string> calculateHashes(const std::string& filePath, const std::vector<wc_HashType>& hashesToCalculate, std::optional<std::reference_wrapper<const std::atomic<bool>>> shouldCancel = std::nullopt);

#endif // HASH_H