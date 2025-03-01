#define WOLFSSL_NO_OPTIONS_H
#define WC_RSA_BLINDING
#define WOLFSSL_SHA512
#define WOLFSSL_SHA3
#define HAVE_BLAKE2
#define HAVE_BLAKE2B

#include "hash.h"

#include <wolfssl/wolfcrypt/blake2.h>
#include <wolfssl/wolfcrypt/hash.h>

#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

constexpr size_t BUFFER_SIZE = 1024 * 1024; // 1 MB

Hasher::Hasher(const wc_HashType algorithm) : algorithm(algorithm), finalized(false)
{
    // Get digest size
    int digestSize = 64;
    if (algorithm != WC_HASH_TYPE_BLAKE2B)
    {
        digestSize = wc_HashGetDigestSize(algorithm);
        if (digestSize <= 0)
        {
            throw HashException("Got invalid digest size!", digestSize, this->algorithm);
        }
    }

    this->digest.resize(digestSize);

    // Blake2b specific, Unified hash algorithm doesn't support it
    if (algorithm == WC_HASH_TYPE_BLAKE2B)
    {
        if (const int ret = wc_InitBlake2b(&blake2bhash, digestSize); ret != 0)
        {
            throw HashException("Failed to initialize hash!", ret, algorithm);
        }
        return;
    }

    if (const int ret = wc_HashInit(&hash, algorithm); ret != 0)
    {
        throw HashException("Failed to initialize hash!", ret, algorithm);
    }
}

void Hasher::updateWithBuffer(const byte *buffer, word32 bufferSize)
{
    if (this->finalized)
    {
        throw std::logic_error("You cannot update a hash after it has been finalized!");
    }

    // Blake2b specific, Unified hash algorithm doesn't support it
    if (this->algorithm == WC_HASH_TYPE_BLAKE2B)
    {
        if (const int ret = wc_Blake2bUpdate(&this->blake2bhash, buffer, bufferSize); ret != 0)
        {
            throw HashException("Failed to update hash!", ret, this->algorithm);
        }
        return;
    }

    if (const int ret = wc_HashUpdate(&this->hash, this->algorithm, buffer, bufferSize); ret != 0)
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

    // Blake2b specific, Unified hash algorithm doesn't support it
    if (this->algorithm == WC_HASH_TYPE_BLAKE2B)
    {
        if (const int ret =
                wc_Blake2bFinal(&this->blake2bhash, this->digest.data(), static_cast<word32>(this->digest.size()));
            ret != 0)
        {
            throw HashException("Failed to store digest!", ret, this->algorithm);
        }
        return;
    }

    if (const int ret = wc_HashFinal(&this->hash, this->algorithm, this->digest.data()); ret != 0)
    {
        throw HashException("Failed to store digest!", ret, this->algorithm);
    }
}

std::string Hasher::getDigest() const
{
    if (!this->finalized)
    {
        throw std::logic_error("You must finalize a hash to get the digest!");
    }

    // Convert digest to hex string
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (const unsigned char i : this->digest)
    {
        ss << std::setw(2) << static_cast<int>(i);
    }

    return ss.str();
}

Hasher::~Hasher()
{
    // Blake2b specific, Unified hash algorithm doesn't support it
    if (this->algorithm == WC_HASH_TYPE_BLAKE2B)
    {
        return;
    }

    if (int ret = wc_HashFree(&this->hash, this->algorithm); ret != 0)
    {
        std::cerr << std::format("Failed to free hash! - Error Code: {}", ret) << std::endl;
    }
}

std::map<wc_HashType, std::string> calculateHashes(
    const std::string &filePath, const std::vector<wc_HashType> &hashesToCalculate,
    const std::optional<std::reference_wrapper<const std::atomic<bool>>> shouldCancel)
{
    // Helper to check cancellation
    auto isCancelled = [&]() -> bool { return shouldCancel && shouldCancel->get().load(); };

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

        file.read(reinterpret_cast<char *>(buffer.data()), BUFFER_SIZE);
        const std::streamsize bytesRead = file.gcount();

        for (const auto &hasher : hashes | std::views::values)
        {
            hasher->updateWithBuffer(buffer.data(), static_cast<word32>(bytesRead));
        }
    }
    file.close();

    for (const auto &hasher : hashes | std::views::values)
    {
        hasher->finalize();
    }
    if (isCancelled())
    {
        return {};
    }

    std::map<wc_HashType, std::string> calculateHashes;
    for (const auto &[algorithm, hasher] : hashes)
    {
        calculateHashes[algorithm] = hasher->getDigest();
    }
    if (isCancelled())
    {
        return {};
    }

    return calculateHashes;
}