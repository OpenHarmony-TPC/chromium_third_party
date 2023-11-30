// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/string_message_codec.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/containers/buffer_iterator.h"
#include "base/containers/span.h"
#include "base/functional/overloaded.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/public/mojom/array_buffer/array_buffer_contents.mojom.h"

#if defined(OHOS_MSGPORT)
#include <climits>
#endif  // defined(OHOS_MSGPORT)

namespace blink {
namespace {

// An ArrayBufferPayload impl based on std::vector.
class VectorArrayBuffer : public WebMessageArrayBufferPayload {
 public:
  VectorArrayBuffer(std::vector<uint8_t> data, size_t position, size_t length)
      : data_(std::move(data)), position_(position), length_(length) {
    size_t size = base::CheckAdd(position_, length_).ValueOrDie();
    CHECK_GE(data_.size(), size);
  }

  size_t GetLength() const override { return length_; }

  bool GetIsResizableByUserJavaScript() const override {
    // VectorArrayBuffers are not used for ArrayBuffer transfers and are
    // currently always fixed-length. Structured cloning resizables ArrayBuffers
    // is not yet supported in SMC.
    return false;
  }

  size_t GetMaxByteLength() const override { return length_; }

  absl::optional<base::span<const uint8_t>> GetAsSpanIfPossible()
      const override {
    return base::make_span(data_).subspan(position_, length_);
  }

  void CopyInto(base::span<uint8_t> dest) const override {
    CHECK_GE(dest.size(), length_);
    memcpy(dest.data(), data_.data() + position_, length_);
  }

 private:
  std::vector<uint8_t> data_;
  size_t position_;
  size_t length_;
};

// An ArrayBufferPayload impl based on mojo::BigBuffer.
class BigBufferArrayBuffer : public WebMessageArrayBufferPayload {
 public:
  explicit BigBufferArrayBuffer(mojo_base::BigBuffer data,
                                absl::optional<size_t> max_byte_length)
      : data_(std::move(data)), max_byte_length_(max_byte_length) {
    DCHECK(!max_byte_length || *max_byte_length >= GetLength());
  }

  size_t GetLength() const override { return data_.size(); }

  bool GetIsResizableByUserJavaScript() const override {
    return max_byte_length_.has_value();
  }

  size_t GetMaxByteLength() const override {
    return max_byte_length_.value_or(GetLength());
  }

  absl::optional<base::span<const uint8_t>> GetAsSpanIfPossible()
      const override {
    return base::make_span(data_);
  }

  void CopyInto(base::span<uint8_t> dest) const override {
    CHECK(dest.size() >= data_.size());
    memcpy(dest.data(), data_.data(), data_.size());
  }

 private:
  mojo_base::BigBuffer data_;
  absl::optional<size_t> max_byte_length_;
};

const uint32_t kVarIntShift = 7;
const uint32_t kVarIntMask = (1 << kVarIntShift) - 1;

const uint8_t kVersionTag = 0xFF;
const uint8_t kPaddingTag = '\0';
// serialization_tag, see v8/src/objects/value-serializer.cc
const uint8_t kOneByteStringTag = '"';
const uint8_t kTwoByteStringTag = 'c';
const uint8_t kArrayBuffer = 'B';
const uint8_t kArrayBufferTransferTag = 't';

#if defined(OHOS_MSGPORT)
const uint8_t kTrue = 'T';
const uint8_t kFalse = 'F';
const uint8_t kDouble = 'N';
const uint8_t kInt32 = 'I';
const uint8_t kUint32 = 'U';
// Error
const uint8_t kError = 'r';

// Beginning of a dense JS array. length:uint32_t
// |length| elements, followed by properties as key/value pairs
const uint8_t kBeginDenseJSArray = 'A';
// End of a dense JS array. numProperties:uint32_t length:uint32_t
const uint8_t kEndDenseJSArray = '$';

// Sub-tags only meaningful for error serialization.
enum class ErrorTag : uint8_t {
  // The error is a EvalError. No accompanying data.
  kEvalErrorPrototype = 'E',
  // The error is a RangeError. No accompanying data.
  kRangeErrorPrototype = 'R',
  // The error is a ReferenceError. No accompanying data.
  kReferenceErrorPrototype = 'F',
  // The error is a SyntaxError. No accompanying data.
  kSyntaxErrorPrototype = 'S',
  // The error is a TypeError. No accompanying data.
  kTypeErrorPrototype = 'T',
  // The error is a URIError. No accompanying data.
  kUriErrorPrototype = 'U',
  // Followed by message: string.
  kMessage = 'm',
  // Followed by a JS object: cause.
  kCause = 'c',
  // Followed by stack: string.
  kStack = 's',
  // The end of this error information.
  kEnd = '.',
};
#endif  // defined(OHOS_MSGPORT)

const uint32_t kVersion = 10;
#if defined(OHOS_MSGPORT)
const uint32_t kErrorVersion = 20;
const uint32_t kLatestVersion = 14;
#endif  // defined(OHOS_MSGPORT)

static size_t BytesNeededForUint32(uint32_t value) {
  size_t result = 0;
  do {
    result++;
    value >>= kVarIntShift;
  } while (value);
  return result;
}

void WriteUint8(uint8_t value, std::vector<uint8_t>* buffer) {
  buffer->push_back(value);
}

void WriteUint32(uint32_t value, std::vector<uint8_t>* buffer) {
  for (;;) {
    uint8_t b = (value & kVarIntMask);
    value >>= kVarIntShift;
    if (!value) {
      WriteUint8(b, buffer);
      break;
    }
    WriteUint8(b | (1 << kVarIntShift), buffer);
  }
}

void WriteBytes(const char* bytes,
                size_t num_bytes,
                std::vector<uint8_t>* buffer) {
  buffer->insert(buffer->end(), bytes, bytes + num_bytes);
}

bool ReadUint8(base::BufferIterator<const uint8_t>& iter, uint8_t* value) {
  if (const uint8_t* ptr = iter.Object<uint8_t>()) {
    *value = *ptr;
    return true;
  }
  return false;
}

bool ReadUint32(base::BufferIterator<const uint8_t>& iter, uint32_t* value) {
  *value = 0;
  uint8_t current_byte;
  int shift = 0;
  do {
    if (!ReadUint8(iter, &current_byte))
      return false;

    *value |= (static_cast<uint32_t>(current_byte & kVarIntMask) << shift);
    shift += kVarIntShift;
  } while (current_byte & (1 << kVarIntShift));
  return true;
}

bool ContainsOnlyLatin1(const std::u16string& data) {
  char16_t x = 0;
  for (char16_t c : data)
    x |= c;
  return !(x & 0xFF00);
}

#if defined(OHOS_MSGPORT)
template <typename T>
T ReadVarint(const uint8_t* position, const uint8_t* end) {
  // Reads an unsigned integer as a base-128 varint.
  // The number is written, 7 bits at a time, from the least significant to the
  // most significant 7 bits. Each byte, except the last, has the MSB set.
  // If the varint is larger than T, any more significant bits are discarded.
  // See also https://developers.google.com/protocol-buffers/docs/encoding
  static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value,
                "Only unsigned integer types can be read as varints.");
  T value = 0;
  unsigned shift = 0;
  bool has_another_byte;
  do {
    if (position > end)
      return value;
    uint8_t byte = *position;
    if ((shift < sizeof(T) * 8)) {
      value |= static_cast<T>(byte & 0x7F) << shift;
      shift += 7;
    }
    has_another_byte = byte & 0x80;
    position++;
  } while (has_another_byte);
  return value;
}

template <typename T>
void WriteVarint(T value, std::vector<uint8_t>* buffer) {
  // Writes an unsigned integer as a base-128 varint.
  // The number is written, 7 bits at a time, from the least significant to the
  // most significant 7 bits. Each byte, except the last, has the MSB set.
  // See also https://developers.google.com/protocol-buffers/docs/encoding
  static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value,
                "Only unsigned integer types can be written as varints.");
  uint8_t stack_buffer[sizeof(T) * 8 / 7 + 1];
  uint8_t* next_byte = &stack_buffer[0];
  do {
    *next_byte = (value & 0x7F) | 0x80;
    next_byte++;
    value >>= 7;
  } while (value);
  *(next_byte - 1) &= 0x7F;
  buffer->insert(buffer->end(), stack_buffer, next_byte);
}
#endif  // defined(OHOS_MSGPORT)

}  // namespace

// static
std::unique_ptr<WebMessageArrayBufferPayload>
WebMessageArrayBufferPayload::CreateFromBigBuffer(
    mojo_base::BigBuffer buffer,
    absl::optional<size_t> max_byte_length) {
  return std::make_unique<BigBufferArrayBuffer>(std::move(buffer),
                                                max_byte_length);
}

// static
std::unique_ptr<WebMessageArrayBufferPayload>
WebMessageArrayBufferPayload::CreateForTesting(std::vector<uint8_t> data) {
  auto size = data.size();
  return std::make_unique<VectorArrayBuffer>(std::move(data), 0, size);
}

TransferableMessage EncodeWebMessagePayload(const WebMessagePayload& payload) {
  TransferableMessage message;
  std::vector<uint8_t> buffer;
  WriteUint8(kVersionTag, &buffer);
  WriteUint32(kVersion, &buffer);
  absl::visit(
      base::Overloaded{
          [&](const std::u16string& str) {
            if (ContainsOnlyLatin1(str)) {
              std::string data_latin1(str.cbegin(), str.cend());
              WriteUint8(kOneByteStringTag, &buffer);
              WriteUint32(data_latin1.size(), &buffer);
              WriteBytes(data_latin1.c_str(), data_latin1.size(), &buffer);
            } else {
              size_t num_bytes = str.size() * sizeof(char16_t);
              if ((buffer.size() + 1 + BytesNeededForUint32(num_bytes)) & 1)
                WriteUint8(kPaddingTag, &buffer);
              WriteUint8(kTwoByteStringTag, &buffer);
              WriteUint32(num_bytes, &buffer);
              WriteBytes(reinterpret_cast<const char*>(str.data()), num_bytes,
                         &buffer);
            }
          },
          [&](const std::unique_ptr<WebMessageArrayBufferPayload>&
                  array_buffer) {
            WriteUint8(kArrayBufferTransferTag, &buffer);
            // Write at the first slot.
            WriteUint32(0, &buffer);

            mojo_base::BigBuffer big_buffer(array_buffer->GetLength());
            array_buffer->CopyInto(base::make_span(big_buffer));
            message.array_buffer_contents_array.push_back(
                mojom::SerializedArrayBufferContents::New(
                    std::move(big_buffer),
                    array_buffer->GetIsResizableByUserJavaScript(),
                    array_buffer->GetMaxByteLength()));
          }},
      payload);

  message.owned_encoded_message = std::move(buffer);
  message.encoded_message = message.owned_encoded_message;

  return message;
}

absl::optional<WebMessagePayload> DecodeToWebMessagePayload(
    TransferableMessage message) {
  base::BufferIterator<const uint8_t> iter(message.encoded_message);
  uint8_t tag;

  // Discard the outer envelope, including trailer info if applicable.
  if (!ReadUint8(iter, &tag))
    return absl::nullopt;
  if (tag == kVersionTag) {
    uint32_t version = 0;
    if (!ReadUint32(iter, &version))
      return absl::nullopt;
    static constexpr uint32_t kMinWireFormatVersionWithTrailer = 21;
    if (version >= kMinWireFormatVersionWithTrailer) {
      // In these versions, we expect kTrailerOffsetTag (0xFE) followed by an
      // offset and size. See details in
      // third_party/blink/renderer/core/v8/serialization/serialization_tag.h.
      auto span = iter.Span<uint8_t>(1 + sizeof(uint64_t) + sizeof(uint32_t));
      if (span.empty() || span[0] != 0xFE)
        return absl::nullopt;
    }
    if (!ReadUint8(iter, &tag))
      return absl::nullopt;
  }

  // Discard any leading version and padding tags.
  while (tag == kVersionTag || tag == kPaddingTag) {
    uint32_t version;
    if (tag == kVersionTag && !ReadUint32(iter, &version))
      return absl::nullopt;
    if (!ReadUint8(iter, &tag))
      return absl::nullopt;
  }

  switch (tag) {
    case kOneByteStringTag: {
      // Use of unsigned char rather than char here matters, so that Latin-1
      // characters are zero-extended rather than sign-extended
      uint32_t num_bytes;
      if (!ReadUint32(iter, &num_bytes))
        return absl::nullopt;
      auto span = iter.Span<unsigned char>(num_bytes / sizeof(unsigned char));
      std::u16string str(span.begin(), span.end());
      return span.size_bytes() == num_bytes
                 ? absl::make_optional(WebMessagePayload(std::move(str)))
                 : absl::nullopt;
    }
    case kTwoByteStringTag: {
      uint32_t num_bytes;
      if (!ReadUint32(iter, &num_bytes))
        return absl::nullopt;
      auto span = iter.Span<char16_t>(num_bytes / sizeof(char16_t));
      std::u16string str(span.begin(), span.end());
      return span.size_bytes() == num_bytes
                 ? absl::make_optional(WebMessagePayload(std::move(str)))
                 : absl::nullopt;
    }
    case kArrayBuffer: {
      uint32_t num_bytes;
      if (!ReadUint32(iter, &num_bytes))
        return absl::nullopt;
      size_t position = iter.position();
      return position + num_bytes == iter.total_size()
                 ? absl::make_optional(
                       WebMessagePayload(std::make_unique<VectorArrayBuffer>(
                           std::move(message.owned_encoded_message), position,
                           num_bytes)))
                 : absl::nullopt;
    }
    case kArrayBufferTransferTag: {
      uint32_t array_buffer_index;
      if (!ReadUint32(iter, &array_buffer_index))
        return absl::nullopt;
      // We only support transfer ArrayBuffer at the first index.
      if (array_buffer_index != 0)
        return absl::nullopt;
      if (message.array_buffer_contents_array.size() != 1)
        return absl::nullopt;
      auto& array_buffer_contents = message.array_buffer_contents_array[0];
      absl::optional<size_t> max_byte_length;
      if (array_buffer_contents->is_resizable_by_user_javascript) {
        max_byte_length.emplace(array_buffer_contents->max_byte_length);
      }
      return absl::make_optional(
          WebMessagePayload(std::make_unique<BigBufferArrayBuffer>(
              std::move(array_buffer_contents->contents), max_byte_length)));
    }
  }

  DLOG(WARNING) << "Unexpected tag: " << tag;
  return absl::nullopt;
}

#if defined(OHOS_MSGPORT)
void WriteString(const std::string& str, std::vector<uint8_t>* buffer) {
  WriteUint8(kOneByteStringTag, buffer);
  WriteUint32(str.length(), buffer);
  WriteBytes(str.c_str(), str.length(), buffer);
}

void WriteHeader(std::vector<uint8_t>* buffer) {
  WriteUint8(kVersionTag, buffer);      // 0xFF ---255
  WriteUint32(kErrorVersion, buffer);   // 0xFF ---20
  WriteUint8(kVersionTag, buffer);      // 0xFF ---255
  WriteUint32(kLatestVersion, buffer);  // 0xFF ---14
}

void WriteU16string(const std::u16string& str, std::vector<uint8_t>* buffer) {
  if (ContainsOnlyLatin1(str)) {
    std::string data_latin1(str.cbegin(), str.cend());
    WriteUint8(kOneByteStringTag, buffer);
    WriteUint32(data_latin1.size(), buffer);
    WriteBytes(data_latin1.c_str(), data_latin1.size(), buffer);
  } else {
    size_t num_bytes = str.size() * sizeof(char16_t);
    if ((buffer->size() + 1 + BytesNeededForUint32(num_bytes)) & 1)
      WriteUint8(kPaddingTag, buffer);
    WriteUint8(kTwoByteStringTag, buffer);
    WriteUint32(num_bytes, buffer);
    WriteBytes(reinterpret_cast<const char*>(str.data()), num_bytes, buffer);
  }
}

void WriteBool(const bool& value, std::vector<uint8_t>* buffer) {
  if (value) {
    WriteUint8(kTrue, buffer);
  } else {
    WriteUint8(kFalse, buffer);
  }
}

void WriteDouble(const double& value, std::vector<uint8_t>* buffer) {
  WriteUint8(kDouble, buffer);
  WriteBytes(reinterpret_cast<const char*>(&value), sizeof(value), buffer);
}

void WriteInt64(const int64_t& value, std::vector<uint8_t>* buffer) {
  if (INT_MIN <= value && value <= INT_MAX) {
    WriteUint8(kInt32, buffer);
    int32_t val = (int32_t)value;
    using UnsignedT = typename std::make_unsigned<int32_t>::type;
    WriteVarint(
        (static_cast<UnsignedT>(val) << 1) ^ (val >> (8 * sizeof(int32_t) - 1)),
        buffer);
  } else if (INT_MAX < value && value <= UINT_MAX) {
    WriteUint8(kUint32, buffer);
    WriteUint32(value, buffer);
  } else {
    WriteDouble(value, buffer);
  }
}

ErrorTag ConvertToErrType(const std::u16string& name) {
  std::map<std::u16string, ErrorTag> typeMap;
  typeMap.insert(std::pair<std::u16string, ErrorTag>(
      u"EvalError", ErrorTag::kEvalErrorPrototype));
  typeMap.insert(std::pair<std::u16string, ErrorTag>(
      u"RangeError", ErrorTag::kRangeErrorPrototype));
  typeMap.insert(std::pair<std::u16string, ErrorTag>(
      u"ReferenceError", ErrorTag::kReferenceErrorPrototype));
  typeMap.insert(std::pair<std::u16string, ErrorTag>(
      u"SyntaxError", ErrorTag::kSyntaxErrorPrototype));
  typeMap.insert(std::pair<std::u16string, ErrorTag>(
      u"TypeError", ErrorTag::kTypeErrorPrototype));
  typeMap.insert(std::pair<std::u16string, ErrorTag>(
      u"URIError", ErrorTag::kUriErrorPrototype));
  for (auto iter = typeMap.begin(); iter != typeMap.end(); iter++) {
    if (iter->first.compare(name) == 0) {
      return iter->second;
    }
  }
  return ErrorTag::kSyntaxErrorPrototype;
}

void WriteError(const std::u16string& name,
                const std::u16string& message,
                std::vector<uint8_t>* buffer) {
  // tag
  WriteUint8(kError, buffer);
  // F -- kReferenceErrorPrototype
  // convert name to error type
  WriteUint8(static_cast<uint8_t>(ConvertToErrType(name)), buffer);
  // m -- Followed by message: string
  WriteUint8(static_cast<uint8_t>(ErrorTag::kMessage), buffer);
  WriteU16string(message, buffer);
  // s -- Followed by stack: string
  WriteUint8(static_cast<uint8_t>(ErrorTag::kStack), buffer);
  WriteU16string(name + u": " + message, buffer);
  // . -- The end of this error information.
  WriteUint8(static_cast<uint8_t>(ErrorTag::kEnd), buffer);
}

void WriteArrayBuffer(std::vector<uint8_t>& arrbuf,
                      TransferableMessage& message,
                      std::vector<uint8_t>* buffer) {
  WriteUint8(kArrayBufferTransferTag, buffer);
  // Write at the first slot.
  WriteUint32(0, buffer);

  mojo_base::BigBuffer big_buffer(arrbuf);
  message.array_buffer_contents_array.push_back(
      mojom::SerializedArrayBufferContents::New(std::move(big_buffer), false,
                                                big_buffer.size()));
}

void WriteStringArray(std::vector<std::u16string>& arr,
                      std::vector<uint8_t>* buffer) {
  WriteUint8(kBeginDenseJSArray, buffer);
  uint32_t length = arr.size();
  WriteUint32(length, buffer);
  for (uint32_t i = 0; i < length; i++) {
    WriteU16string(arr[i], buffer);
  }
  WriteUint8(kEndDenseJSArray, buffer);
  // end of the array tag
  WriteUint32(0, buffer);
  WriteUint32(length, buffer);
}

void WriteBoolArray(std::vector<bool>& arr, std::vector<uint8_t>* buffer) {
  WriteUint8(kBeginDenseJSArray, buffer);
  uint32_t length = arr.size();
  WriteUint32(length, buffer);
  for (uint32_t i = 0; i < length; i++) {
    WriteBool(arr[i], buffer);
  }
  WriteUint8(kEndDenseJSArray, buffer);
  WriteUint32(0, buffer);
  WriteUint32(length, buffer);
}

void WriteDoubleArray(std::vector<double>& arr, std::vector<uint8_t>* buffer) {
  WriteUint8(kBeginDenseJSArray, buffer);
  uint32_t length = arr.size();
  WriteUint32(length, buffer);
  for (uint32_t i = 0; i < length; i++) {
    WriteDouble(arr[i], buffer);
  }
  WriteUint8(kEndDenseJSArray, buffer);
  WriteUint32(0, buffer);
  WriteUint32(length, buffer);
}

void WriteInt64Array(std::vector<int64_t>& arr, std::vector<uint8_t>* buffer) {
  WriteUint8(kBeginDenseJSArray, buffer);
  uint32_t length = arr.size();
  WriteUint32(length, buffer);
  for (uint32_t i = 0; i < length; i++) {
    WriteInt64(arr[i], buffer);
  }
  WriteUint8(kEndDenseJSArray, buffer);
  WriteUint32(0, buffer);
  WriteUint32(length, buffer);
}

// see src/v8/src/objects/value-serializer.cc for more details about the
// serialization.
TransferableMessage EncodeWebMessagePayload(
    struct WebMessagePort::Message& original) {
  TransferableMessage message;
  std::vector<uint8_t> buffer;
  WriteHeader(&buffer);

  switch (original.type_) {
    case WebMessagePort::Message::MessageType::STRING: {
      WriteU16string(original.data, &buffer);
      break;
    }
    case WebMessagePort::Message::MessageType::INTEGER: {
      WriteInt64(original.int64_value_, &buffer);
      break;
    }
    case WebMessagePort::Message::MessageType::BOOLEAN: {
      WriteBool(original.bool_value_, &buffer);
      break;
    }
    case WebMessagePort::Message::MessageType::DOUBLE: {
      WriteDouble(original.double_value_, &buffer);
      break;
    }
    case WebMessagePort::Message::MessageType::BINARY: {
      WriteArrayBuffer(original.array_buffer, message, &buffer);
      break;
    }
    case WebMessagePort::Message::MessageType::ERROR: {
      WriteError(original.err_name_, original.err_msg_, &buffer);
      break;
    }
    case WebMessagePort::Message::MessageType::STRINGARRAY: {
      WriteStringArray(original.string_arr_, &buffer);
      break;
    }
    case WebMessagePort::Message::MessageType::BOOLEANARRAY: {
      WriteBoolArray(original.bool_arr_, &buffer);
      break;
    }
    case WebMessagePort::Message::MessageType::DOUBLEARRAY: {
      WriteDoubleArray(original.double_arr_, &buffer);
      break;
    }
    case WebMessagePort::Message::MessageType::INT64ARRAY: {
      WriteInt64Array(original.int64_arr_, &buffer);
      break;
      case WebMessagePort::Message::MessageType::NONE:
      case WebMessagePort::Message::MessageType::DICTIONARY:
      case WebMessagePort::Message::MessageType::LIST:
        break;
      default:
        break;
    }
  }
  message.owned_encoded_message = std::move(buffer);
  message.encoded_message = message.owned_encoded_message;

  return message;
}

bool ReadOneByteString(base::BufferIterator<const uint8_t>& iter,
                       std::u16string& str) {
  uint32_t num_bytes = 0;
  if (!ReadUint32(iter, &num_bytes)) {
    LOG(ERROR) << "ReadUint32 failed";
    return false;
  }
  auto span = iter.Span<unsigned char>(num_bytes / sizeof(unsigned char));
  std::u16string result(span.begin(), span.end());
  str = result;
  return true;
}

bool ReadTwoByteString(base::BufferIterator<const uint8_t>& iter,
                       std::u16string& str) {
  uint32_t num_bytes;
  if (!ReadUint32(iter, &num_bytes)) {
    LOG(ERROR) << "ReadUint32 failed";
    return false;
  }
  auto span = iter.Span<char16_t>(num_bytes / sizeof(char16_t));
  std::u16string result(span.begin(), span.end());
  str = result;
  return true;
}

bool ReadDouble(base::BufferIterator<const uint8_t>& iter, double& result) {
  double value = 0;
  uint8_t* p = reinterpret_cast<uint8_t*>(&value);
  for (size_t i = 0; i < sizeof(double); i++) {
    if (const uint8_t* ptr = iter.Object<uint8_t>()) {
      p[i] = *ptr;
    } else {
      LOG(ERROR) << "Read memory of the double value failed";
      return false;
    }
  }
  result = value;
  return true;
}

bool ReadU16string(base::BufferIterator<const uint8_t>& iter,
                   std::u16string& str) {
  uint8_t tag = 0;
  if (!ReadUint8(iter, &tag)) {
    LOG(ERROR) << "read tag failed";
    return false;
  }
  switch (tag) {
    case kOneByteStringTag: {
      ReadOneByteString(iter, str);
      break;
    }
    case kTwoByteStringTag: {
      ReadTwoByteString(iter, str);
      break;
    }
    default:
      LOG(ERROR) << "not support tag:" << tag;
      return false;
  }
  return true;
}

// int32为可变长编码，单独一个int32可直接从iter当前读到结尾即可。
bool ReadInt32(base::BufferIterator<const uint8_t>& iter, int32_t& result) {
  auto span = iter.Span<uint8_t>(iter.total_size() - iter.position());
  if (span.empty()) {
    return false;
  }
  std::vector<uint8_t> array_buf(span.begin(), span.end());
  uint32_t value =
      ReadVarint<uint32_t>(&array_buf[0], &array_buf[array_buf.size() - 1]);
  // zigzag decode:
  // https://stackoverflow.com/questions/4533076/google-protocol-buffers-zigzag-encoding
  result =
      static_cast<int32_t>((value >> 1) ^ -static_cast<int32_t>(value & 1));
  return true;
}

// int32为可变长编码，数组中的int32无法直接使用上面方式读取，改为读取到该数组中下一个元素的标志位或者结尾。
bool ReadInt32InArray(base::BufferIterator<const uint8_t>& iter,
                      int32_t& result) {
  int32_t value = 0;
  const uint8_t* begin = iter.Object<uint8_t>();
  if (begin == nullptr || (iter.total_size() - iter.position()) < 1) {
    return false;
  }
  const uint8_t* current = begin;
  while ((iter.total_size() - iter.position()) >= 1) {
    current = iter.Object<uint8_t>();
    if (*current == kEndDenseJSArray || *current == kInt32) {
      value = ReadVarint<uint32_t>(begin, --current);
      // zigzag decode:
      // https://stackoverflow.com/questions/4533076/google-protocol-buffers-zigzag-encoding
      result =
          static_cast<int32_t>((value >> 1) ^ -static_cast<int32_t>(value & 1));
      iter.Seek(iter.position() - 1);
      return true;
    }
  }
  return false;
}

bool IsBoolElement(uint8_t element_tag) {
  return element_tag == kTrue || element_tag == kFalse;
}

bool IsStringElement(uint8_t element_tag) {
  return element_tag == kOneByteStringTag || element_tag == kTwoByteStringTag;
}

bool IsNumberElement(uint8_t element_tag) {
  return element_tag == kDouble || element_tag == kInt32 ||
         element_tag == kUint32;
}

bool ReadArray(base::BufferIterator<const uint8_t>& iter,
               struct WebMessagePort::Message& decoded_msg) {
  uint32_t len = 0;
  if (!ReadUint32(iter, &len)) {  // array length
    LOG(ERROR) << "ReadUint32 failed";
    return false;
  }
  std::vector<std::u16string> string_array;
  std::vector<bool> boolean_array;
  std::vector<double> double_array;
  std::vector<int64_t> int64_array;
  uint8_t element_tag_first = 0;
  uint8_t element_tag = 0;
  for (uint32_t i = 0; i < len; i++) {
    if (!ReadUint8(iter, &element_tag)) {
      LOG(ERROR) << "ReadUint8 failed";
      return false;
    }
    if (i == 0) {
      element_tag_first = element_tag;
    }
    bool is_ignore = false;
    if ((element_tag_first == kTrue || element_tag_first == kFalse) &&
        (element_tag == kTrue || element_tag == kFalse)) {
      is_ignore = true;
    }
    if (element_tag_first != element_tag && !is_ignore) {
      decoded_msg.data =
          u"This type not support, The elements in the array must be the same";
      decoded_msg.type_ = WebMessagePort::Message::MessageType::STRING;
      return true;
    }
    switch (element_tag) {
      case kOneByteStringTag: {
        std::u16string value;
        if (ReadOneByteString(iter, value)) {
          string_array.push_back(value);
        } else {
          LOG(ERROR) << "ReadOneByteString failed";
          return false;
        }
        break;
      }
      case kTwoByteStringTag: {
        std::u16string value;
        if (ReadTwoByteString(iter, value)) {
          string_array.push_back(value);
        } else {
          LOG(ERROR) << "ReadTwoByteString failed";
          return false;
        }
        break;
      }
      case kTrue: {
        boolean_array.push_back(true);
        break;
      }
      case kFalse: {
        boolean_array.push_back(false);
        break;
      }
      case kDouble: {
        double value = 0.0;
        if (ReadDouble(iter, value)) {
          double_array.push_back(value);
        } else {
          LOG(ERROR) << "ReadDouble failed";
          return false;
        }
        break;
      }
      case kInt32: {
        int32_t value = 0;
        if (ReadInt32InArray(iter, value)) {
          int64_array.push_back(value);
        } else {
          LOG(ERROR) << "ReadInt32InArray failed";
          return false;
        }
        break;
      }
      case kUint32: {
        uint32_t value = 0;
        if (ReadUint32(iter, &value)) {
          int64_array.push_back(value);
        } else {
          LOG(ERROR) << "ReadUint32 failed";
          return false;
        }
        break;
      }
      default: {
        decoded_msg.data =
            u"This type not support, only string/number/boolean is supported "
            u"for array elements";
        decoded_msg.type_ = WebMessagePort::Message::MessageType::STRING;
        return true;
      }
    }
  }

  while ((iter.total_size() - iter.position()) >= 1) {
    if (!ReadUint8(iter, &element_tag)) {
      LOG(ERROR) << "ReadUint8 failed";
      return false;
    }
    if (element_tag == kEndDenseJSArray) {
      LOG(INFO) << "end tag of this array";
      break;
    }
  }
  switch (element_tag_first) {
    case kOneByteStringTag:
    case kTwoByteStringTag: {
      decoded_msg.string_arr_ = std::move(string_array);
      decoded_msg.type_ = WebMessagePort::Message::MessageType::STRINGARRAY;
      break;
    }
    case kTrue:
    case kFalse: {
      decoded_msg.bool_arr_ = std::move(boolean_array);
      decoded_msg.type_ = WebMessagePort::Message::MessageType::BOOLEANARRAY;
      break;
    }
    case kDouble: {
      decoded_msg.double_arr_ = std::move(double_array);
      decoded_msg.type_ = WebMessagePort::Message::MessageType::DOUBLEARRAY;
      break;
    }
    case kInt32:
    case kUint32: {
      decoded_msg.int64_arr_ = std::move(int64_array);
      decoded_msg.type_ = WebMessagePort::Message::MessageType::INT64ARRAY;
      break;
    }
  }
  return true;
}

std::u16string GetProtoTypeName(ErrorTag tag) {
  std::u16string name = u"Error";
  switch (tag) {
    case ErrorTag::kEvalErrorPrototype: {
      name = u"EvalError";
      break;
    }
    case ErrorTag::kRangeErrorPrototype: {
      name = u"RangeError";
      break;
    }
    case ErrorTag::kReferenceErrorPrototype: {
      name = u"ReferenceError";
      break;
    }
    case ErrorTag::kSyntaxErrorPrototype: {
      name = u"SyntaxError";
      break;
    }
    case ErrorTag::kTypeErrorPrototype: {
      name = u"TypeError";
      break;
    }
    case ErrorTag::kUriErrorPrototype: {
      name = u"URIError";
      break;
    }
    default: {
      break;
    }
  }
  return name;
}

bool ReadError(base::BufferIterator<const uint8_t>& iter,
               struct WebMessagePort::Message& decoded_msg) {
  bool done = false;
  while (!done) {
    uint8_t tag = 0;
    if (!ReadUint8(iter, &tag)) {
      LOG(ERROR) << "Read tag failed, not enough byte left";
      return false;
    }
    switch (static_cast<ErrorTag>(tag)) {
      case ErrorTag::kStack: {
        std::u16string stack_err;
        ReadU16string(iter, stack_err);
        break;
      }
      case ErrorTag::kEvalErrorPrototype:
      case ErrorTag::kRangeErrorPrototype:
      case ErrorTag::kReferenceErrorPrototype:
      case ErrorTag::kSyntaxErrorPrototype:
      case ErrorTag::kTypeErrorPrototype:
      case ErrorTag::kUriErrorPrototype:
        decoded_msg.err_name_ = GetProtoTypeName(static_cast<ErrorTag>(tag));
        decoded_msg.type_ = WebMessagePort::Message::MessageType::ERROR;
        break;
      case ErrorTag::kMessage: {
        std::u16string msg_err;
        ReadU16string(iter, msg_err);
        decoded_msg.err_msg_ = std::move(msg_err);
        decoded_msg.type_ = WebMessagePort::Message::MessageType::ERROR;
        break;
      }
      case ErrorTag::kCause: {
        done = true;
        break;
      }
      case ErrorTag::kEnd: {
        done = true;
        break;
      }
      default:
        done = true;
        break;
    }
  }
  return true;
}

bool ReadHeaderTag(base::BufferIterator<const uint8_t>& iter, uint8_t& tag) {
  // Discard the outer envelope, including trailer info if applicable.
  if (!ReadUint8(iter, &tag))
    return false;
  if (tag == kVersionTag) {
    uint32_t version = 0;
    if (!ReadUint32(iter, &version))
      return false;
    static constexpr uint32_t kMinWireFormatVersionWithTrailer = 21;
    if (version >= kMinWireFormatVersionWithTrailer) {
      // In these versions, we expect kTrailerOffsetTag (0xFE) followed by an
      // offset and size. See details in
      // third_party/blink/renderer/core/v8/serialization/serialization_tag.h.
      auto span = iter.Span<uint8_t>(1 + sizeof(uint64_t) + sizeof(uint32_t));
      if (span.empty() || span[0] != 0xFE) {
        return false;
      }
    }
    if (!ReadUint8(iter, &tag)) {
      return false;
    }
  }

  // Discard any leading version and padding tags.
  while (tag == kVersionTag || tag == kPaddingTag) {
    uint32_t version;
    if (tag == kVersionTag && !ReadUint32(iter, &version)) {
      return false;
    }
    if (!ReadUint8(iter, &tag)) {
      return false;
    }
  }
  return true;
}

// see src/v8/src/objects/value-serializer.cc for more details about the
// deserialization.
bool DecodeToWebMessagePayload(const TransferableMessage& message,
                               struct WebMessagePort::Message& decoded_msg) {
  base::BufferIterator<const uint8_t> iter(message.encoded_message);
  LOG(INFO) << "decoded start, iter total_size:" << iter.total_size();

  uint8_t tag = 0;
  if (!ReadHeaderTag(iter, tag)) {
    LOG(ERROR) << "Read head tag failed, tag:" << (int)tag;
    return false;
  }
  switch (tag) {
    case kOneByteStringTag: {
      std::u16string str;
      if (ReadOneByteString(iter, str)) {
        decoded_msg.data = std::move(str);
        decoded_msg.type_ = WebMessagePort::Message::MessageType::STRING;
      } else {
        return false;
      }
      break;
    }
    case kTwoByteStringTag: {
      std::u16string str;
      if (ReadTwoByteString(iter, str)) {
        decoded_msg.data = std::move(str);
        decoded_msg.type_ = WebMessagePort::Message::MessageType::STRING;
      } else {
        return false;
      }
      break;
    }
    case kArrayBuffer: {
      uint32_t num_bytes;
      if (!ReadUint32(iter, &num_bytes))
        return false;
      auto span = iter.Span<uint8_t>(num_bytes);
      std::vector<uint8_t> array_buf(span.begin(), span.end());
      decoded_msg.array_buffer = std::move(array_buf);
      decoded_msg.type_ = WebMessagePort::Message::MessageType::BINARY;
      break;
    }
    case kTrue: {
      decoded_msg.bool_value_ = true;
      decoded_msg.type_ = WebMessagePort::Message::MessageType::BOOLEAN;
      break;
    }
    case kFalse: {
      decoded_msg.bool_value_ = false;
      decoded_msg.type_ = WebMessagePort::Message::MessageType::BOOLEAN;
      break;
    }
    case kDouble: {
      double value = 0;
      if (ReadDouble(iter, value)) {
        decoded_msg.double_value_ = value;
        decoded_msg.type_ = WebMessagePort::Message::MessageType::DOUBLE;
      } else {
        LOG(ERROR) << "ReadDouble error";
        return false;
      }
      break;
    }
    case kInt32: {
      int32_t vaule = 0;
      if (ReadInt32(iter, vaule)) {
        decoded_msg.int64_value_ = vaule;
        decoded_msg.type_ = WebMessagePort::Message::MessageType::INTEGER;
      } else {
        LOG(ERROR) << "ReadInt32 failed";
        return false;
      }
      break;
    }
    case kUint32: {
      uint32_t value = 0;
      if (ReadUint32(iter, &value)) {
        decoded_msg.int64_value_ = value;
        decoded_msg.type_ = WebMessagePort::Message::MessageType::INTEGER;
      } else {
        LOG(ERROR) << "ReadUint32 failed";
        return false;
      }
      break;
    }
    case kError: {
      if (!ReadError(iter, decoded_msg)) {
        LOG(ERROR) << "ReadError failed";
        return false;
      }
      break;
    }
    case kBeginDenseJSArray: {
      if (!ReadArray(iter, decoded_msg)) {
        LOG(ERROR) << "ReadArray failed";
        return false;
      }
      break;
    }
    default:
      decoded_msg.data =
          u"This type not support, only "
          u"string/number/boolean/arraybuffer/array/error is supported";
      decoded_msg.type_ = WebMessagePort::Message::MessageType::STRING;
      return true;
  }
  LOG(INFO) << "Decoded transfer message end";
  return true;
}
#endif  // defined(OHOS_MSGPORT)

}  // namespace blink
