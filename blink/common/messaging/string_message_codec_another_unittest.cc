/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace blink {
namespace {

TEST(StringMessageCodecTest_, WriteInt64_001) {
  struct WebMessagePort::Message original;
  original.type_ = WebMessagePort::Message::MessageType::INTEGER;
  original.int64_value_ = 64;
  TransferableMessage message = EncodeWebMessagePayload(original);
  EXPECT_EQ(message.owned_encoded_message[4], 'I');
}

TEST(StringMessageCodecTest_, WriteInt64_002) {
  struct WebMessagePort::Message original;
  original.type_ = WebMessagePort::Message::MessageType::INTEGER;
  original.int64_value_ = -2147483649;
  TransferableMessage message = EncodeWebMessagePayload(original);
  EXPECT_EQ(message.owned_encoded_message[4], 'N');
}

TEST(StringMessageCodecTest_, WriteInt64_003) {
  struct WebMessagePort::Message original;
  original.type_ = WebMessagePort::Message::MessageType::INTEGER;
  original.int64_value_ = 2147483648;
  TransferableMessage message = EncodeWebMessagePayload(original);
  EXPECT_EQ(message.owned_encoded_message[4], 'U');
}

TEST(StringMessageCodecTest_, WriteInt64_004) {
  struct WebMessagePort::Message original;
  original.type_ = WebMessagePort::Message::MessageType::INTEGER;
  original.int64_value_ = 4294967297;
  TransferableMessage message = EncodeWebMessagePayload(original);
  EXPECT_EQ(message.owned_encoded_message[4], 'N');
}

TEST(StringMessageCodecTest_, WriteBool_001) {
  struct WebMessagePort::Message original;
  original.type_ = WebMessagePort::Message::MessageType::BOOLEAN;
  original.bool_value_ = true;
  TransferableMessage message = EncodeWebMessagePayload(original);
  EXPECT_EQ(message.owned_encoded_message[4], 'T');
}

TEST(StringMessageCodecTest_, WriteBool_002) {
  struct WebMessagePort::Message original;
  original.type_ = WebMessagePort::Message::MessageType::BOOLEAN;
  original.bool_value_ = false;
  TransferableMessage message = EncodeWebMessagePayload(original);
  EXPECT_EQ(message.owned_encoded_message[4], 'F');
}

TEST(StringMessageCodecTest_, WriteU16string_001) {
  struct WebMessagePort::Message original;
  original.type_ = WebMessagePort::Message::MessageType::STRING;
  original.data = u"Hello";
  TransferableMessage message = EncodeWebMessagePayload(original);
  EXPECT_EQ(message.owned_encoded_message[4], '"');
}

TEST(StringMessageCodecTest_, WriteU16string_002) {
  struct WebMessagePort::Message original;
  original.type_ = WebMessagePort::Message::MessageType::STRING;
  original.data = u"Hello世界";
  TransferableMessage message = EncodeWebMessagePayload(original);
  EXPECT_EQ(message.owned_encoded_message[4], 'c');
}

TEST(StringMessageCodecTest_, WriteStringArray) {
  WebMessagePort::Message original;
  original.type_ = WebMessagePort::Message::MessageType::STRINGARRAY;
  original.string_arr_ = {u"hello", u"world"};
  TransferableMessage result = EncodeWebMessagePayload(original);
  EXPECT_EQ(result.encoded_message[20], '$');
  EXPECT_EQ(result.encoded_message.size(), 23);
}

TEST(StringMessageCodecTest_, WriteBoolArray) {
  WebMessagePort::Message original;
  original.type_ = WebMessagePort::Message::MessageType::BOOLEANARRAY;
  original.bool_arr_ = {true, false, true, false, true};
  TransferableMessage result = EncodeWebMessagePayload(original);
  EXPECT_EQ(result.encoded_message[11], '$');
  EXPECT_EQ(result.encoded_message.size(), 14);
}

TEST(StringMessageCodecTest_, WriteDoubleArray) {
  WebMessagePort::Message original;
  original.type_ = WebMessagePort::Message::MessageType::DOUBLEARRAY;
  original.double_arr_ = {1.1, 2.2, 3.3, 4.4, 5.5};
  TransferableMessage result = EncodeWebMessagePayload(original);
  EXPECT_EQ(result.encoded_message[51], '$');
  EXPECT_EQ(result.encoded_message.size(), 54);
}

TEST(StringMessageCodecTest_, WriteInt64Array) {
  WebMessagePort::Message original;
  original.type_ = WebMessagePort::Message::MessageType::INT64ARRAY;
  original.int64_arr_ = {1, 2, 3, 4, 5};
  TransferableMessage result = EncodeWebMessagePayload(original);
  EXPECT_EQ(result.encoded_message[16], '$');
  EXPECT_EQ(result.encoded_message.size(), 19);
}

TEST(StringMessageCodecTest_, ConvertToErrType_001) {
  WebMessagePort::Message original;
  original.type_ = WebMessagePort::Message::MessageType::ERROR;
  original.err_name_ = u"ReferenceError";
  original.err_msg_ = u"Invalid reference";
  TransferableMessage result = EncodeWebMessagePayload(original);
  EXPECT_EQ(result.encoded_message[5], 'F');
}

TEST(StringMessageCodecTest_, ConvertToErrType_002) {
  WebMessagePort::Message original;
  original.type_ = WebMessagePort::Message::MessageType::ERROR;
  original.err_name_ = u"UnknownError";
  original.err_msg_ = u"Invalid reference";
  TransferableMessage result = EncodeWebMessagePayload(original);
  EXPECT_EQ(result.encoded_message[5], 'S');
}

TEST(StringMessageCodecTest_, EncodeWebMessagePayload_001) {
  WebMessagePort::Message original;
  original.type_ = WebMessagePort::Message::MessageType::BINARY;
  original.array_buffer = {0x01, 0x02, 0x03};
  TransferableMessage result = EncodeWebMessagePayload(original);
  EXPECT_EQ(result.encoded_message[4], 't');
  EXPECT_EQ(result.encoded_message.size(), 6);
}

TEST(StringMessageCodecTest_, EncodeWebMessagePayload_002) {
  WebMessagePort::Message original;
  original.type_ = WebMessagePort::Message::MessageType::DICTIONARY;
  TransferableMessage result = EncodeWebMessagePayload(original);
  EXPECT_EQ(result.encoded_message.size(), 4);
}

TEST(StringMessageCodecTest_, EncodeWebMessagePayload_003) {
  WebMessagePort::Message original;
  original.type_ = WebMessagePort::Message::MessageType::LIST;
  TransferableMessage result = EncodeWebMessagePayload(original);
  EXPECT_EQ(result.encoded_message.size(), 4);
}

TEST(StringMessageCodecTest, EncodeWebMessagePayload_004) {
  WebMessagePort::Message original;
  original.type_ = WebMessagePort::Message::MessageType::DOUBLE;
  original.double_value_ = 1.1;
  TransferableMessage result = EncodeWebMessagePayload(original);
  EXPECT_EQ(result.encoded_message[4], 'N');
}

TEST(StringMessageCodecTest_, DecodeToWebMessagePayload_001) {
  TransferableMessage message;
  const uint8_t data[] = {0x54, 0x02, 0x03, 0x04, 0x05};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), true);
  EXPECT_EQ(decoded_msg.bool_value_, true);
}

TEST(StringMessageCodecTest_, DecodeToWebMessagePayload_002) {
  TransferableMessage message;
  const uint8_t data[] = {0x22, 0x02, 0x03, 0x04, 0x05};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), true);
  EXPECT_EQ(decoded_msg.type_, WebMessagePort::Message::MessageType::STRING);
}

TEST(StringMessageCodecTest_, DecodeToWebMessagePayload_003) {
  TransferableMessage message;
  const uint8_t data[] = {0x63, 0x02, 0x03, 0x04, 0x05};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), true);
  EXPECT_EQ(decoded_msg.type_, WebMessagePort::Message::MessageType::STRING);
}

TEST(StringMessageCodecTest_, DecodeToWebMessagePayload_004) {
  TransferableMessage message;
  const uint8_t data[] = {0x46, 0x02, 0x03, 0x04, 0x05};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), true);
  EXPECT_EQ(decoded_msg.bool_value_, false);
}

TEST(StringMessageCodecTest_, DecodeToWebMessagePayload_005) {
  TransferableMessage message;
  const uint8_t data[] = {0x4e, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), true);
  EXPECT_EQ(decoded_msg.type_, WebMessagePort::Message::MessageType::DOUBLE);
}

TEST(StringMessageCodecTest_, DecodeToWebMessagePayload_006) {
  TransferableMessage message;
  const uint8_t data[] = {0x42, 0x02, 0x03, 0x04, 0x05};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), true);
  EXPECT_EQ(decoded_msg.type_, WebMessagePort::Message::MessageType::BINARY);
}

TEST(StringMessageCodecTest_, DecodeToWebMessagePayload_007) {
  TransferableMessage message;
  const uint8_t data[] = {0x49, 0x02, 0x03, 0x04, 0x05};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), true);
  EXPECT_EQ(decoded_msg.type_, WebMessagePort::Message::MessageType::INTEGER);
}

TEST(StringMessageCodecTest_, DecodeToWebMessagePayload_008) {
  TransferableMessage message;
  const uint8_t data[] = {0x55, 0x02, 0x03, 0x04, 0x05};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), true);
  EXPECT_EQ(decoded_msg.type_, WebMessagePort::Message::MessageType::INTEGER);
}

TEST(StringMessageCodecTest_, DecodeToWebMessagePayload_009) {
  TransferableMessage message;
  const uint8_t data[] = {0x72, 0x02, 0x03, 0x04, 0x05};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), true);
}
TEST(StringMessageCodecTest_, DecodeToWebMessagePayload_010) {
  TransferableMessage message;
  const uint8_t data[] = {0x11, 0x02, 0x03, 0x04, 0x05};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), true);
  EXPECT_EQ(decoded_msg.type_, WebMessagePort::Message::MessageType::STRING);
}

TEST(StringMessageCodecTest_, DecodeToWebMessagePayload_011) {
  TransferableMessage message;
  const uint8_t data[] = {0x22};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), false);
}

TEST(StringMessageCodecTest_, DecodeToWebMessagePayload_012) {
  TransferableMessage message;
  const uint8_t data[] = {0x63};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), false);
}

TEST(StringMessageCodecTest_, DecodeToWebMessagePayload_013) {
  TransferableMessage message;
  const uint8_t data[] = {0x42};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), false);
}

TEST(StringMessageCodecTest_, DecodeToWebMessagePayload_014) {
  TransferableMessage message;
  const uint8_t data[] = {0x4e};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), false);
}

TEST(StringMessageCodecTest_, DecodeToWebMessagePayload_015) {
  TransferableMessage message;
  const uint8_t data[] = {0x49};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), false);
}

TEST(StringMessageCodecTest_, DecodeToWebMessagePayload_016) {
  TransferableMessage message;
  const uint8_t data[] = {0x55};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), false);
}

TEST(StringMessageCodecTest_, DecodeToWebMessagePayload_017) {
  TransferableMessage message;
  const uint8_t data[] = {0x72};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), false);
}

TEST(StringMessageCodecTest_, ReadArray_001) {
  TransferableMessage message;
  const uint8_t data[] = {0x41};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), false);
}

TEST(StringMessageCodecTest_, ReadArray_002) {
  TransferableMessage message;
  const uint8_t data[] = {0x41, 0x02};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), false);
}

TEST(StringMessageCodecTest_, ReadArray_003) {
  TransferableMessage message;
  const uint8_t data[] = {0x41, 0x02, 0x03, 0x22, 0x05};
  message.encoded_message = data;
  WebMessagePort::Message decoded_msg;
  EXPECT_EQ(DecodeToWebMessagePayload(message, decoded_msg), true);
}
}  // namespace
}  // namespace blink