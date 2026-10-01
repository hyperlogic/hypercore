/*
    Copyright (c) 2025 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/npyimport.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "src/import.h"
#include "src/log.h"
#include "src/node.h"
#include "src/util.h"

namespace hyper {

namespace {

enum PickleType {
  kPickleNone,
  kPickleInt,
  kPickleFloat,
  kPickleBool,
  kPickleString,
  kPickleBytes,
  kPickleList,
  kPickleDict,
  kPickleTuple,
  kPickleNumpyArray,
};

struct PickleValue {
  PickleType type = kPickleNone;
  int64_t int_val = 0;
  double float_val = 0.0;
  bool bool_val = false;
  std::string str_val;
  std::vector<uint8_t> bytes_val;
  std::vector<PickleValue> list_val;  // also used for tuples
  std::vector<std::pair<std::string, PickleValue>> dict_val;
  // numpy array reconstruction info
  std::string np_dtype;
  std::vector<int64_t> np_shape;
  std::vector<uint8_t> np_data;

  const PickleValue* DictGet(const std::string& key) const {
    for (const auto& [k, v] : dict_val) {
      if (k == key) return &v;
    }
    return nullptr;
  }
};

// Minimal pickle (protocol 2-5) virtual machine, sufficient to decode the
// numpy object arrays / OrderedDicts that poselib writes.
struct PickleParser {
  const uint8_t* data = nullptr;
  size_t size = 0;
  size_t pos = 0;
  std::vector<PickleValue> stack;
  std::vector<size_t> marks;
  std::vector<PickleValue> memo;

  bool Avail(size_t n) const { return pos + n <= size; }

  uint8_t ReadByte() { return data[pos++]; }
  uint16_t ReadU16() {
    uint16_t v = static_cast<uint16_t>(data[pos] | (data[pos + 1] << 8));
    pos += 2;
    return v;
  }
  uint32_t ReadU32() {
    uint32_t v = static_cast<uint32_t>(data[pos]) |
                 (static_cast<uint32_t>(data[pos + 1]) << 8) |
                 (static_cast<uint32_t>(data[pos + 2]) << 16) |
                 (static_cast<uint32_t>(data[pos + 3]) << 24);
    pos += 4;
    return v;
  }
  int32_t ReadI32() { return static_cast<int32_t>(ReadU32()); }
  int64_t ReadI64() {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= static_cast<uint64_t>(data[pos++]) << (i * 8);
    return static_cast<int64_t>(v);
  }
  double ReadF64() {
    // big-endian double (pickle BINFLOAT)
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v |= static_cast<uint64_t>(data[pos++]) << (i * 8);
    double d;
    memcpy(&d, &v, 8);
    return d;
  }
  std::string ReadString(size_t len) {
    std::string s(reinterpret_cast<const char*>(data + pos), len);
    pos += len;
    return s;
  }

  bool PopMark(size_t* mark) {
    if (marks.empty()) return false;
    *mark = marks.back();
    marks.pop_back();
    return *mark <= stack.size();
  }

  bool Parse() {
    while (pos < size) {
      uint8_t op = ReadByte();
      switch (op) {
        case 0x80: {  // PROTO
          pos++;  // skip protocol version
          break;
        }
        case 0x95: {  // FRAME
          pos += 8;  // skip frame length
          break;
        }
        case '.': {  // STOP
          return true;
        }
        case '(':  // MARK
          marks.push_back(stack.size());
          break;
        case ')': {  // EMPTY_TUPLE
          PickleValue v;
          v.type = kPickleTuple;
          stack.push_back(v);
          break;
        }
        case 0x85: {  // TUPLE1
          if (stack.size() < 1) return false;
          PickleValue v;
          v.type = kPickleTuple;
          v.list_val.push_back(stack.back());
          stack.pop_back();
          stack.push_back(v);
          break;
        }
        case 0x86: {  // TUPLE2
          if (stack.size() < 2) return false;
          PickleValue v;
          v.type = kPickleTuple;
          v.list_val.resize(2);
          v.list_val[1] = stack.back();
          stack.pop_back();
          v.list_val[0] = stack.back();
          stack.pop_back();
          stack.push_back(v);
          break;
        }
        case 0x87: {  // TUPLE3
          if (stack.size() < 3) return false;
          PickleValue v;
          v.type = kPickleTuple;
          v.list_val.resize(3);
          v.list_val[2] = stack.back();
          stack.pop_back();
          v.list_val[1] = stack.back();
          stack.pop_back();
          v.list_val[0] = stack.back();
          stack.pop_back();
          stack.push_back(v);
          break;
        }
        case 't': {  // TUPLE (from mark)
          size_t mark;
          if (!PopMark(&mark)) return false;
          PickleValue v;
          v.type = kPickleTuple;
          v.list_val.assign(stack.begin() + mark, stack.end());
          stack.resize(mark);
          stack.push_back(v);
          break;
        }
        case ']': {  // EMPTY_LIST
          PickleValue v;
          v.type = kPickleList;
          stack.push_back(v);
          break;
        }
        case 'l': {  // LIST (from mark)
          size_t mark;
          if (!PopMark(&mark)) return false;
          PickleValue v;
          v.type = kPickleList;
          v.list_val.assign(stack.begin() + mark, stack.end());
          stack.resize(mark);
          stack.push_back(v);
          break;
        }
        case 'e': {  // APPENDS
          size_t mark;
          if (!PopMark(&mark) || mark == 0) return false;
          auto& list = stack[mark - 1];
          for (size_t i = mark; i < stack.size(); i++) {
            list.list_val.push_back(stack[i]);
          }
          stack.resize(mark);
          break;
        }
        case 'a': {  // APPEND
          if (stack.size() < 2) return false;
          PickleValue item = stack.back();
          stack.pop_back();
          stack.back().list_val.push_back(item);
          break;
        }
        case '}': {  // EMPTY_DICT
          PickleValue v;
          v.type = kPickleDict;
          stack.push_back(v);
          break;
        }
        case 'u': {  // SETITEMS
          size_t mark;
          if (!PopMark(&mark) || mark == 0) return false;
          auto& dict = stack[mark - 1];
          for (size_t i = mark; i + 1 < stack.size(); i += 2) {
            dict.dict_val.push_back({stack[i].str_val, stack[i + 1]});
          }
          stack.resize(mark);
          break;
        }
        case 's': {  // SETITEM
          if (stack.size() < 3) return false;
          PickleValue val = stack.back();
          stack.pop_back();
          PickleValue key = stack.back();
          stack.pop_back();
          stack.back().dict_val.push_back({key.str_val, val});
          break;
        }
        case 0x8c: {  // SHORT_BINUNICODE
          if (!Avail(1)) return false;
          uint8_t len = ReadByte();
          if (!Avail(len)) return false;
          PickleValue v;
          v.type = kPickleString;
          v.str_val = ReadString(len);
          stack.push_back(v);
          break;
        }
        case 0x8d: {  // BINUNICODE8
          if (!Avail(8)) return false;
          int64_t len = ReadI64();
          if (len < 0 || !Avail(static_cast<size_t>(len))) return false;
          PickleValue v;
          v.type = kPickleString;
          v.str_val = ReadString(static_cast<size_t>(len));
          stack.push_back(v);
          break;
        }
        case 'X': {  // BINUNICODE
          if (!Avail(4)) return false;
          uint32_t len = ReadU32();
          if (!Avail(len)) return false;
          PickleValue v;
          v.type = kPickleString;
          v.str_val = ReadString(len);
          stack.push_back(v);
          break;
        }
        case 'C': {  // SHORT_BINBYTES
          if (!Avail(1)) return false;
          uint8_t len = ReadByte();
          if (!Avail(len)) return false;
          PickleValue v;
          v.type = kPickleBytes;
          v.bytes_val.assign(data + pos, data + pos + len);
          pos += len;
          stack.push_back(v);
          break;
        }
        case 'B': {  // BINBYTES
          if (!Avail(4)) return false;
          uint32_t len = ReadU32();
          if (!Avail(len)) return false;
          PickleValue v;
          v.type = kPickleBytes;
          v.bytes_val.assign(data + pos, data + pos + len);
          pos += len;
          stack.push_back(v);
          break;
        }
        case 0x8e: {  // BINBYTES8
          if (!Avail(8)) return false;
          int64_t len = ReadI64();
          if (len < 0 || !Avail(static_cast<size_t>(len))) return false;
          PickleValue v;
          v.type = kPickleBytes;
          v.bytes_val.assign(data + pos, data + pos + static_cast<size_t>(len));
          pos += static_cast<size_t>(len);
          stack.push_back(v);
          break;
        }
        case 'K': {  // BININT1
          if (!Avail(1)) return false;
          PickleValue v;
          v.type = kPickleInt;
          v.int_val = ReadByte();
          stack.push_back(v);
          break;
        }
        case 'M': {  // BININT2
          if (!Avail(2)) return false;
          PickleValue v;
          v.type = kPickleInt;
          v.int_val = ReadU16();
          stack.push_back(v);
          break;
        }
        case 'J': {  // BININT
          if (!Avail(4)) return false;
          PickleValue v;
          v.type = kPickleInt;
          v.int_val = ReadI32();
          stack.push_back(v);
          break;
        }
        case 'G': {  // BINFLOAT
          if (!Avail(8)) return false;
          PickleValue v;
          v.type = kPickleFloat;
          v.float_val = ReadF64();
          stack.push_back(v);
          break;
        }
        case 0x88: {  // NEWTRUE
          PickleValue v;
          v.type = kPickleBool;
          v.bool_val = true;
          stack.push_back(v);
          break;
        }
        case 0x89: {  // NEWFALSE
          PickleValue v;
          v.type = kPickleBool;
          v.bool_val = false;
          stack.push_back(v);
          break;
        }
        case 'N': {  // NONE
          PickleValue v;
          v.type = kPickleNone;
          stack.push_back(v);
          break;
        }
        case 'c': {  // GLOBAL
          std::string module, name;
          while (pos < size && data[pos] != '\n') module += static_cast<char>(data[pos++]);
          pos++;  // skip \n
          while (pos < size && data[pos] != '\n') name += static_cast<char>(data[pos++]);
          pos++;  // skip \n
          PickleValue v;
          v.type = kPickleString;
          v.str_val = module + "." + name;
          stack.push_back(v);
          break;
        }
        case 0x93: {  // STACK_GLOBAL
          if (stack.size() < 2) return false;
          PickleValue name_val = stack.back();
          stack.pop_back();
          PickleValue mod_val = stack.back();
          stack.pop_back();
          PickleValue v;
          v.type = kPickleString;
          v.str_val = mod_val.str_val + "." + name_val.str_val;
          stack.push_back(v);
          break;
        }
        case 'R': {  // REDUCE
          if (stack.size() < 2) return false;
          PickleValue args = stack.back();
          stack.pop_back();
          PickleValue callable = stack.back();
          stack.pop_back();
          if (callable.str_val.find("_reconstruct") != std::string::npos ||
              callable.str_val.find("scalar") != std::string::npos) {
            // numpy._core.multiarray._reconstruct or numpy.core.multiarray._reconstruct
            PickleValue v;
            v.type = kPickleNumpyArray;
            stack.push_back(v);
          } else if (callable.str_val.find("OrderedDict") != std::string::npos) {
            // OrderedDict() - treat as dict
            PickleValue v;
            v.type = kPickleDict;
            if (args.type == kPickleTuple && !args.list_val.empty() &&
                args.list_val[0].type == kPickleList) {
              // OrderedDict([(k,v), ...])
              for (const auto& pair : args.list_val[0].list_val) {
                if (pair.type == kPickleTuple && pair.list_val.size() == 2) {
                  v.dict_val.push_back({pair.list_val[0].str_val, pair.list_val[1]});
                }
              }
            }
            stack.push_back(v);
          } else if (callable.str_val.find("dtype") != std::string::npos) {
            // numpy.dtype('f4') etc
            PickleValue v;
            v.type = kPickleString;
            if (args.type == kPickleTuple && !args.list_val.empty()) {
              v.str_val = args.list_val[0].str_val;
            }
            stack.push_back(v);
          } else {
            // generic reduce - push None
            PickleValue v;
            v.type = kPickleNone;
            stack.push_back(v);
          }
          break;
        }
        case 'b': {  // BUILD
          if (stack.size() < 2) return false;
          PickleValue state = stack.back();
          stack.pop_back();
          auto& obj = stack.back();
          if (obj.type == kPickleNumpyArray && state.type == kPickleTuple) {
            // numpy array BUILD state is (version, shape, dtype, fortran, data)
            if (state.list_val.size() >= 5) {
              if (state.list_val[1].type == kPickleTuple) {
                for (const auto& dim : state.list_val[1].list_val) {
                  obj.np_shape.push_back(dim.int_val);
                }
              }
              if (state.list_val[2].type == kPickleString) {
                obj.np_dtype = state.list_val[2].str_val;
              }
              // data (index 4): raw bytes for numeric arrays, list for object arrays
              if (state.list_val[4].type == kPickleBytes) {
                obj.np_data = state.list_val[4].bytes_val;
              } else if (state.list_val[4].type == kPickleList) {
                // Object dtype array (e.g. 0-d array wrapping an OrderedDict)
                // Replace this numpy array with the unwrapped object
                if (state.list_val[4].list_val.size() == 1) {
                  obj = state.list_val[4].list_val[0];
                }
              }
            }
          }
          break;
        }
        case 0x81: {  // NEWOBJ
          if (stack.size() < 2) return false;
          PickleValue args = stack.back();
          stack.pop_back();
          PickleValue cls = stack.back();
          stack.pop_back();
          if (cls.str_val.find("ndarray") != std::string::npos) {
            PickleValue v;
            v.type = kPickleNumpyArray;
            stack.push_back(v);
          } else if (cls.str_val.find("dtype") != std::string::npos) {
            PickleValue v;
            v.type = kPickleString;
            if (args.type == kPickleTuple && !args.list_val.empty()) {
              v.str_val = args.list_val[0].str_val;
            }
            stack.push_back(v);
          } else {
            PickleValue v;
            v.type = kPickleNone;
            stack.push_back(v);
          }
          break;
        }
        case 'q': {  // BINPUT
          if (!Avail(1) || stack.empty()) return false;
          uint8_t idx = ReadByte();
          if (memo.size() <= idx) memo.resize(idx + 1);
          memo[idx] = stack.back();
          break;
        }
        case 'r': {  // LONG_BINPUT
          if (!Avail(4) || stack.empty()) return false;
          uint32_t idx = ReadU32();
          if (memo.size() <= idx) memo.resize(idx + 1);
          memo[idx] = stack.back();
          break;
        }
        case 'h': {  // BINGET
          if (!Avail(1)) return false;
          uint8_t idx = ReadByte();
          if (idx >= memo.size()) return false;
          stack.push_back(memo[idx]);
          break;
        }
        case 'j': {  // LONG_BINGET
          if (!Avail(4)) return false;
          uint32_t idx = ReadU32();
          if (idx >= memo.size()) return false;
          stack.push_back(memo[idx]);
          break;
        }
        case 0x94: {  // MEMOIZE (protocol 4)
          if (stack.empty()) return false;
          memo.push_back(stack.back());
          break;
        }
        case '0': {  // POP
          if (!stack.empty()) stack.pop_back();
          break;
        }
        case '2': {  // DUP
          if (stack.empty()) return false;
          stack.push_back(stack.back());
          break;
        }
        default:
          Log::E("npyimport: unknown pickle opcode 0x%02x at pos %zu\n", op, pos - 1);
          return false;
      }
    }
    return true;
  }
};

bool ParseNpyHeader(const uint8_t* data, size_t size, size_t* data_offset) {
  // NPY format: \x93NUMPY + major + minor + header_len + header_string
  if (size < 12) return false;
  if (data[0] != 0x93 || data[1] != 'N' || data[2] != 'U' ||
      data[3] != 'M' || data[4] != 'P' || data[5] != 'Y') {
    return false;
  }
  uint8_t major = data[6];
  size_t header_len;
  size_t prefix_size;
  if (major == 1) {
    header_len = data[8] | (data[9] << 8);
    prefix_size = 10;
  } else {
    header_len = data[8] | (data[9] << 8) | (data[10] << 16) | (data[11] << 24);
    prefix_size = 12;
  }
  *data_offset = prefix_size + header_len;
  return *data_offset < size;
}

// Fetches od[key]["arr"] and validates that it is a numpy array with the given shape.
// A dim of -1 in expected_shape matches any size.
const PickleValue* GetTensorArr(const PickleValue& dict, const char* key,
                                const std::vector<int64_t>& expected_shape,
                                size_t element_size) {
  const PickleValue* tensor = dict.DictGet(key);
  if (!tensor || tensor->type != kPickleDict) {
    Log::E("npyimport: missing '%s'\n", key);
    return nullptr;
  }
  const PickleValue* arr = tensor->DictGet("arr");
  if (!arr || arr->type != kPickleNumpyArray) {
    Log::E("npyimport: missing '%s' arr\n", key);
    return nullptr;
  }
  if (arr->np_shape.size() != expected_shape.size()) {
    Log::E("npyimport: '%s' has %zu dims, expected %zu (only single frame SkeletonStates are supported)\n",
           key, arr->np_shape.size(), expected_shape.size());
    return nullptr;
  }
  size_t num_elements = 1;
  for (size_t i = 0; i < expected_shape.size(); i++) {
    if (expected_shape[i] >= 0 && arr->np_shape[i] != expected_shape[i]) {
      Log::E("npyimport: '%s' has unexpected shape\n", key);
      return nullptr;
    }
    if (arr->np_shape[i] < 0) return nullptr;
    num_elements *= static_cast<size_t>(arr->np_shape[i]);
  }
  if (arr->np_data.size() < num_elements * element_size) {
    Log::E("npyimport: '%s' data is truncated\n", key);
    return nullptr;
  }
  return arr;
}

}  // namespace

std::shared_ptr<Asset> AssetImportNpyAbs(const std::string& filename) {
  std::shared_ptr<Asset> asset = std::make_shared<Asset>();

  std::vector<uint8_t> file_data;
  if (!LoadBinaryFile(filename, file_data)) {
    Log::E("npyimport: could not load \"%s\"\n", filename.c_str());
    return asset;
  }

  const uint8_t* raw = file_data.data();
  size_t data_offset = 0;
  if (!ParseNpyHeader(raw, file_data.size(), &data_offset)) {
    Log::E("npyimport: invalid npy header in \"%s\"\n", filename.c_str());
    return asset;
  }

  PickleParser parser;
  parser.data = raw + data_offset;
  parser.size = file_data.size() - data_offset;
  if (!parser.Parse() || parser.stack.empty()) {
    Log::E("npyimport: pickle parse failed for \"%s\"\n", filename.c_str());
    return asset;
  }

  const PickleValue& od = parser.stack.back();
  if (od.type != kPickleDict) {
    Log::E("npyimport: expected dict at top level\n");
    return asset;
  }

  // rotation: float32 [num_joints, 4] quaternions (x, y, z, w)
  const PickleValue* rotation_arr = GetTensorArr(od, "rotation", {-1, 4}, sizeof(float));
  if (!rotation_arr) return asset;
  const size_t num_joints = static_cast<size_t>(rotation_arr->np_shape[0]);
  const int64_t n = static_cast<int64_t>(num_joints);
  const float* rot_data = reinterpret_cast<const float*>(rotation_arr->np_data.data());

  // root_translation: float32 [3]
  const PickleValue* root_trans_arr = GetTensorArr(od, "root_translation", {3}, sizeof(float));
  if (!root_trans_arr) return asset;
  const float* root_trans_data = reinterpret_cast<const float*>(root_trans_arr->np_data.data());

  const PickleValue* skel_tree = od.DictGet("skeleton_tree");
  if (!skel_tree || skel_tree->type != kPickleDict) {
    Log::E("npyimport: missing 'skeleton_tree'\n");
    return asset;
  }

  // node_names: list of strings
  const PickleValue* node_names = skel_tree->DictGet("node_names");
  if (!node_names || node_names->type != kPickleList) {
    Log::E("npyimport: missing 'node_names'\n");
    return asset;
  }
  if (node_names->list_val.size() != num_joints) {
    Log::E("npyimport: node_names count (%zu) != num_joints (%zu)\n",
           node_names->list_val.size(), num_joints);
    return asset;
  }

  // parent_indices: int64 or int32 [num_joints]
  const PickleValue* parent_probe = GetTensorArr(*skel_tree, "parent_indices", {n}, 1);
  if (!parent_probe) return asset;
  const bool parent_is_64 = parent_probe->np_dtype.find('8') != std::string::npos;
  const PickleValue* parent_arr =
      GetTensorArr(*skel_tree, "parent_indices", {n}, parent_is_64 ? sizeof(int64_t) : sizeof(int32_t));
  if (!parent_arr) return asset;
  std::vector<int64_t> parent_indices(num_joints);
  if (parent_is_64) {
    const int64_t* p64 = reinterpret_cast<const int64_t*>(parent_arr->np_data.data());
    for (size_t i = 0; i < num_joints; i++) parent_indices[i] = p64[i];
  } else {
    const int32_t* p32 = reinterpret_cast<const int32_t*>(parent_arr->np_data.data());
    for (size_t i = 0; i < num_joints; i++) parent_indices[i] = p32[i];
  }
  for (size_t i = 0; i < num_joints; i++) {
    if (parent_indices[i] >= n || (parent_indices[i] >= 0 && static_cast<size_t>(parent_indices[i]) == i)) {
      Log::E("npyimport: invalid parent index %lld for joint %zu\n",
             static_cast<long long>(parent_indices[i]), i);  // NOLINT(runtime/int)
      return asset;
    }
  }

  // local_translation: float32 [num_joints, 3]
  const PickleValue* trans_arr = GetTensorArr(*skel_tree, "local_translation", {n, 3}, sizeof(float));
  if (!trans_arr) return asset;
  const float* trans_data = reinterpret_cast<const float*>(trans_arr->np_data.data());

  const PickleValue* is_local_val = od.DictGet("is_local");
  const bool is_local = is_local_val && is_local_val->type == kPickleBool && is_local_val->bool_val;
  Log::I("npyimport: \"%s\" is_local=%s, num_joints=%zu\n",
         filename.c_str(), is_local ? "true" : "false", num_joints);

  // Build per-joint translations & rotations.  Translations are always local, except
  // joint 0 which comes from root_translation (matches poselib's SkeletonState).
  std::vector<glm::vec3> translations(num_joints);
  std::vector<glm::quat> rotations(num_joints);
  for (size_t i = 0; i < num_joints; i++) {
    if (i == 0) {
      translations[i] = glm::vec3(root_trans_data[0], root_trans_data[1], root_trans_data[2]);
    } else {
      translations[i] = glm::vec3(trans_data[i * 3], trans_data[i * 3 + 1], trans_data[i * 3 + 2]);
    }
    // npy quats are (x, y, z, w); glm::quat ctor is (w, x, y, z)
    rotations[i] = glm::quat(rot_data[i * 4 + 3], rot_data[i * 4],
                             rot_data[i * 4 + 1], rot_data[i * 4 + 2]);
  }

  // Determine an order where every parent precedes its children.
  std::vector<size_t> order;
  order.reserve(num_joints);
  {
    std::vector<bool> placed(num_joints, false);
    std::vector<std::vector<size_t>> children(num_joints);
    for (size_t i = 0; i < num_joints; i++) {
      if (parent_indices[i] < 0) {
        order.push_back(i);
        placed[i] = true;
      } else {
        children[static_cast<size_t>(parent_indices[i])].push_back(i);
      }
    }
    for (size_t k = 0; k < order.size(); k++) {
      for (size_t c : children[order[k]]) {
        if (!placed[c]) {
          order.push_back(c);
          placed[c] = true;
        }
      }
    }
    if (order.size() != num_joints) {
      Log::E("npyimport: skeleton_tree contains a cycle or unreachable joints\n");
      return asset;
    }
  }

  std::vector<glm::mat4> local_xforms(num_joints);
  if (is_local) {
    for (size_t i = 0; i < num_joints; i++) {
      local_xforms[i] = MakeMat4(rotations[i], translations[i]);
    }
  } else {
    // Rotations are global; convert to local the same way poselib does:
    // local = inv(parent_global) * child_global, then decompose.
    std::vector<glm::mat4> global_xforms(num_joints);
    for (size_t i = 0; i < num_joints; i++) {
      global_xforms[i] = MakeMat4(rotations[i], translations[i]);
    }
    for (size_t i = 0; i < num_joints; i++) {
      glm::mat4 local_xform;
      if (parent_indices[i] >= 0) {
        local_xform = glm::inverse(global_xforms[static_cast<size_t>(parent_indices[i])]) * global_xforms[i];
      } else {
        local_xform = global_xforms[i];
      }
      glm::vec3 s, pos;
      glm::quat rot;
      Decompose(local_xform, &s, &rot, &pos);
      local_xforms[i] = MakeMat4(rot, pos);
    }
  }

  // Build the node hierarchy.  node_vec is in parent-before-child order, with
  // Node::idx() equal to each node's position in node_vec.
  std::vector<std::shared_ptr<Node>> joint_to_node(num_joints);
  asset->node_vec.reserve(num_joints);
  for (size_t joint : order) {
    const std::string& name = node_names->list_val[joint].str_val;
    std::shared_ptr<Node> parent;
    if (parent_indices[joint] >= 0) {
      parent = joint_to_node[static_cast<size_t>(parent_indices[joint])];
    }
    auto node = std::make_shared<Node>(asset->node_vec.size(), name, parent, local_xforms[joint]);
    if (parent) {
      parent->child_vec().push_back(node);
    } else if (!asset->root_node) {
      asset->root_node = node;
    } else {
      Log::W("npyimport: multiple root joints, \"%s\" will not be attached to the root\n", name.c_str());
    }
    if (asset->string_to_node_map.count(name)) {
      Log::W("duplicate node name \"%s\" detected.\n", name.c_str());
    }
    asset->string_to_node_map[name] = node;
    asset->node_vec.push_back(node);
    joint_to_node[joint] = node;
  }

  if (asset->root_node) {
    asset->root_node->Update();  // update all the abs xforms in the tree.
  }
  Log::D("num nodes = %zu\n", asset->node_vec.size());

  return asset;
}

}  // namespace hyper
