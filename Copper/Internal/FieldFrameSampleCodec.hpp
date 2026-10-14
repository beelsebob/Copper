// The per-sample encodings of a field frame-series file's full-resolution detail, shared by
// FieldFrameSeriesWriter.cpp and FieldFrameSeriesReader.cpp so the two can never disagree. See
// docs/field_frame_series_format.md ("Full-resolution and metadata datasets") for the scheme.
#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <hdf5.h>

#include "../FieldFrameSeriesWriter.hpp"

namespace copper::field_codec {

constexpr std::array<const char*, 6> kComponentNames = {"Ex", "Ey", "Ez", "Hx", "Hy", "Hz"};

/// Bytes one stored sample occupies.
inline std::size_t sampleBytes(FieldSamplePrecision precision) {
    switch (precision) {
        case FieldSamplePrecision::Int8: return 1;
        case FieldSamplePrecision::Int12:
        case FieldSamplePrecision::Float16: return 2;
        case FieldSamplePrecision::Float32: return 4;
    }
    return 4;
}

/// The HDF5 element type a detail dataset is stored as. Int12 lives in 16-bit integers, whose
/// unused high bits Blosc2's bitshuffle compresses away; Float16 is stored as its IEEE bit pattern.
inline hid_t nativeType(FieldSamplePrecision precision, bool isSigned) {
    switch (precision) {
        case FieldSamplePrecision::Int8: return isSigned ? H5T_NATIVE_INT8 : H5T_NATIVE_UINT8;
        case FieldSamplePrecision::Int12: return isSigned ? H5T_NATIVE_INT16 : H5T_NATIVE_UINT16;
        case FieldSamplePrecision::Float16: return H5T_NATIVE_UINT16;
        case FieldSamplePrecision::Float32: return H5T_NATIVE_UINT32;
    }
    return H5T_NATIVE_UINT32;
}

/// Full-resolution dataset name for one channel: a component name, or "energy".
inline std::string datasetName(FieldSamplePrecision precision, const char* channel) {
    return std::string("/frames/") + channel + (precision == FieldSamplePrecision::Float32 ? "_xor" : "_q");
}

inline const char* precisionName(FieldSamplePrecision precision) {
    switch (precision) {
        case FieldSamplePrecision::Int8: return "int8";
        case FieldSamplePrecision::Int12: return "int12";
        case FieldSamplePrecision::Float16: return "float16";
        case FieldSamplePrecision::Float32: return "float32";
    }
    return "float32";
}

inline bool precisionFromName(const std::string& name, FieldSamplePrecision& precision) {
    for (const auto candidate : {FieldSamplePrecision::Int8, FieldSamplePrecision::Int12,
                                 FieldSamplePrecision::Float16, FieldSamplePrecision::Float32}) {
        if (name == precisionName(candidate)) {
            precision = candidate;
            return true;
        }
    }
    return false;
}

inline const char* channelsName(FieldChannels channels) {
    return channels == FieldChannels::Energy ? "energy" : "components";
}

/// Largest stored integer for a normalized magnitude of 1.
inline float integerFullScale(FieldSamplePrecision precision, bool isSigned) {
    if (precision == FieldSamplePrecision::Int8) return isSigned ? 127.0F : 255.0F;
    return isSigned ? 2047.0F : 4095.0F;
}

/// Stores `value` at `destination`. Float32 keeps the exact bit pattern XORed with `baseline` (the
/// sample's preview value), so decoding is bit exact. Every other precision stores `value / scale`
/// -- `scale` being the channel's peak magnitude in this frame -- clamped to [-1, 1] (or [0, 1] for
/// the unsigned energy channel), so a half float can't overflow or underflow on physical units.
inline void encode(FieldSamplePrecision precision, bool isSigned, float value, float baseline, float scale,
                   std::uint8_t* destination) {
    if (precision == FieldSamplePrecision::Float32) {
        const std::uint32_t bits = std::bit_cast<std::uint32_t>(value) ^ std::bit_cast<std::uint32_t>(baseline);
        std::memcpy(destination, &bits, sizeof bits);
        return;
    }
    float normalized = scale > 0.0F && std::isfinite(value) ? value / scale : 0.0F;
    normalized = std::clamp(normalized, isSigned ? -1.0F : 0.0F, 1.0F);
    if (precision == FieldSamplePrecision::Float16) {
        const std::uint16_t bits = std::bit_cast<std::uint16_t>(static_cast<_Float16>(normalized));
        std::memcpy(destination, &bits, sizeof bits);
        return;
    }
    const float stored = std::round(normalized * integerFullScale(precision, isSigned));
    if (precision == FieldSamplePrecision::Int8) {
        if (isSigned) {
            const auto quantized = static_cast<std::int8_t>(stored);
            std::memcpy(destination, &quantized, 1);
        } else {
            const auto quantized = static_cast<std::uint8_t>(stored);
            std::memcpy(destination, &quantized, 1);
        }
    } else if (isSigned) {
        const auto quantized = static_cast<std::int16_t>(stored);
        std::memcpy(destination, &quantized, 2);
    } else {
        const auto quantized = static_cast<std::uint16_t>(stored);
        std::memcpy(destination, &quantized, 2);
    }
}

/// The inverse of encode().
inline float decode(FieldSamplePrecision precision, bool isSigned, const std::uint8_t* source, float baseline,
                    float scale) {
    switch (precision) {
        case FieldSamplePrecision::Float32: {
            std::uint32_t bits = 0;
            std::memcpy(&bits, source, sizeof bits);
            return std::bit_cast<float>(bits ^ std::bit_cast<std::uint32_t>(baseline));
        }
        case FieldSamplePrecision::Float16: {
            std::uint16_t bits = 0;
            std::memcpy(&bits, source, sizeof bits);
            return static_cast<float>(std::bit_cast<_Float16>(bits)) * scale;
        }
        case FieldSamplePrecision::Int8: {
            float stored = 0.0F;
            if (isSigned) {
                std::int8_t quantized = 0;
                std::memcpy(&quantized, source, 1);
                stored = quantized;
            } else {
                std::uint8_t quantized = 0;
                std::memcpy(&quantized, source, 1);
                stored = quantized;
            }
            return stored / integerFullScale(precision, isSigned) * scale;
        }
        case FieldSamplePrecision::Int12: {
            float stored = 0.0F;
            if (isSigned) {
                std::int16_t quantized = 0;
                std::memcpy(&quantized, source, 2);
                stored = quantized;
            } else {
                std::uint16_t quantized = 0;
                std::memcpy(&quantized, source, 2);
                stored = quantized;
            }
            return stored / integerFullScale(precision, isSigned) * scale;
        }
    }
    return 0.0F;
}

/// The normalizing scale of one signed component in one frame: its peak magnitude.
inline float componentScale(float minimum, float maximum) {
    const float peak = std::max(std::abs(minimum), std::abs(maximum));
    return std::isfinite(peak) ? peak : 0.0F;
}

} // namespace copper::field_codec
