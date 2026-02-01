#pragma once

#include <vector>
#include <cstdint>
#include <functional>

typedef size_t lengthSizeType;
typedef std::function<void(const std::vector<uint8_t> &, std::vector<uint8_t> &)> transformFunctionType;

static constexpr size_t bytesForLengthSizeTypes = sizeof(lengthSizeType);
static constexpr int defaultPort = 9050;
static constexpr int defaultOneSocketReadSize = 512;
static constexpr int defaultOneSocketWriteSize = 512;

struct Request
{
    lengthSizeType len;
    std::vector<uint8_t> data;
};

struct Responce
{
    lengthSizeType len;
    std::vector<uint8_t> data;
};