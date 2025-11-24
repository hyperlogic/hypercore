/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/binaryattribute.h"

static uint32_t property_type_size_arr[
    static_cast<size_t>(BinaryAttribute::Type::NumTypes)] = {
    0,                  // Unknown
    sizeof(int8_t),     // Char
    sizeof(uint8_t),    // UChar
    sizeof(int16_t),    // Short
    sizeof(uint16_t),   // UShort
    sizeof(int32_t),    // Int
    sizeof(uint32_t),   // UInt
    sizeof(float),      // Float
    sizeof(double)      // Double
};

BinaryAttribute::BinaryAttribute(Type type_in, size_t offset_in)
    : type(type_in),
      size(property_type_size_arr[static_cast<uint32_t>(type_in)]),
      offset(offset_in) {
}
