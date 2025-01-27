#define WOLFSSL_NO_OPTIONS_H
#define WC_RSA_BLINDING
#define WOLFSSL_SHA512
#define WOLFSSL_SHA3

#include <wolfssl/wolfcrypt/hash.h>
#include "hash.h"
#include <string>
#include <stdexcept>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <future>

constexpr size_t BUFFER_SIZE = 1024 * 1024; // 1 MB

Hasher::Hasher(wc_HashType algorithm) : algorithm(algorithm), finalized(false)
{
    this->algorithm = algorithm;

    int ret = wc_HashInit(&hash, algorithm);
    if (ret != 0)
    {
        throw HashException("Failed to initalize hash!", ret, algorithm);
    }
}

void Hasher::updateWithBuffer(const byte* buffer, word32 bufferSize)
{
    if (this->finalized)
    {
        throw std::logic_error("You cannot update a hash after it has been finalized!");
    }

    int ret = wc_HashUpdate(&this->hash, this->algorithm, buffer, bufferSize);
    if (ret != 0)
    {
        throw HashException("Failed to update hash!", ret, this->algorithm);
    }
}

void Hasher::finalize()
{
    if (this->finalized)
    {
        throw std::logic_error("You cannot finalize a hash twice!");
    }
    this->finalized = true;

    int digestSize = wc_HashGetDigestSize(algorithm);
    if (digestSize <= 0)
    {
        throw HashException("Got invalid digest size!", digestSize, this->algorithm);
    }

    this->digest.resize(digestSize);

    int ret = wc_HashFinal(&this->hash, this->algorithm, this->digest.data());
    if (ret != 0)
    {
        throw HashException("Failed to store digest!", ret, this->algorithm);
    }
}

std::string Hasher::getDigest()
{
    if (!this->finalized)
    {
        throw std::logic_error("You must finalize a hash to get the digest!");
    }

    // Convert digest to hex string
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < this->digest.size(); ++i)
    {
        ss << std::setw(2) << static_cast<int>(this->digest[i]);
    }

    return ss.str();
}

Hasher::~Hasher()
{
    int ret = wc_HashFree(&this->hash, this->algorithm);
    if (ret != 0)
    {
        std::cerr << std::format("Failed to free hash! - Error Code: {}", ret) << std::endl;
    }
}

std::map<wc_HashType, std::string> calculateHashes(const std::string& filePath, const std::vector<wc_HashType>& hashesToCalculate, std::optional<std::reference_wrapper<const std::atomic<bool>>> shouldCancel)
{
    // Helper to check cancellation
    auto isCancelled = [&]() -> bool {
        return shouldCancel && shouldCancel->get().load();
    };

    std::map<wc_HashType, std::unique_ptr<Hasher>> hashes;
    if (isCancelled())
    {
        return {};
    }

    for (wc_HashType algorithm : hashesToCalculate)
    {
        hashes[algorithm] = std::make_unique<Hasher>(algorithm);
    }
    if (isCancelled())
    {
        return {};
    }

    // Read file
    std::ifstream file(filePath, std::ios::binary);
    std::vector<byte> buffer(BUFFER_SIZE);
    while (file)
    {
        if (isCancelled())
        {
            return {};
        }

        file.read((char*)buffer.data(), BUFFER_SIZE);
        std::streamsize bytesRead = file.gcount();

        for (const auto& [_, hasher] : hashes)
        {
            hasher->updateWithBuffer(buffer.data(), (word32)bytesRead);
        }
    }
    file.close();

    for (const auto& [_, hasher] : hashes)
    {
        hasher->finalize();
    }
    if (isCancelled())
    {
        return {};
    }

    std::map<wc_HashType, std::string> calculateHashes;
    for (const auto& [algorithm, hasher] : hashes)
    {
        calculateHashes[algorithm] = hasher->getDigest();
    }
    if (isCancelled())
    {
        return {};
    }

    return calculateHashes;
}