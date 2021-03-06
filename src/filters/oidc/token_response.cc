#include "token_response.h"
#include "absl/strings/match.h"
#include "absl/time/clock.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/util/json_util.h"
#include "jwt_verify_lib/struct_utils.h"
#include "jwt_verify_lib/verify.h"
#include "spdlog/spdlog.h"

namespace authservice {
namespace filters {
namespace oidc {
namespace {
    const char *nonce_field = "nonce";
const char *token_type_field = "token_type";
const char *bearer_token_type = "bearer";
const char *id_token_field = "id_token";
const char *access_token_field = "access_token";
const char *expires_in_field = "expires_in";
}  // namespace
TokenResponse::TokenResponse(const google::jwt_verify::Jwt &id_token)
    : id_token_(id_token) {}

void TokenResponse::SetAccessToken(absl::string_view access_token) {
  access_token_ = std::string(access_token.data(), access_token.size());
}

void TokenResponse::SetExpiry(int64_t expiry) {
  expiry_ = expiry;
}

const google::jwt_verify::Jwt &TokenResponse::IDToken() const {
  return id_token_;
}

absl::optional<const std::string> TokenResponse::AccessToken() const {
  if (!access_token_.empty()) {
    return access_token_;
  }
  return absl::nullopt;
}

absl::optional<int64_t> TokenResponse::Expiry() const {
  if (expiry_) {
    return expiry_;
  }
  return absl::nullopt;
}

TokenResponseParserImpl::TokenResponseParserImpl(
    google::jwt_verify::JwksPtr keys)
    : keys_(std::move(keys)) {}

absl::optional<TokenResponse> TokenResponseParserImpl::Parse(
    const std::string &client_id, const std::string &nonce, const std::string &raw) const {
  ::google::protobuf::util::JsonParseOptions options;
  options.ignore_unknown_fields = true;
  options.case_insensitive_enum_parsing = false;
  ::google::protobuf::Struct message;
  ::google::protobuf::StringPiece str(raw.data());
  const auto status =
      ::google::protobuf::util::JsonStringToMessage(str, &message, options);
  if (!status.ok()) {
    spdlog::info("{}: JSON parsing error: {}", __func__,
                 status.message().data());
    return absl::nullopt;
  }
  auto fields = message.fields();
  // https://openid.net/specs/openid-connect-core-1_0.html#TokenResponse
  // token_type must be Bearer
  auto token_type = fields.find(token_type_field);
  if (token_type == fields.end() ||
      !(absl::EqualsIgnoreCase(
          token_type->second.string_value(),
          bearer_token_type))) {  // case insensitive comparison.
    spdlog::info("{}: missing or incorrect `token_type` in token response",
                 __func__);
    return absl::nullopt;
  }
  // There must be an id_token
  auto id_token_str = fields.find(id_token_field);
  if (id_token_str == fields.end() ||
      id_token_str->second.kind_case() !=
          google::protobuf::Value::kStringValue) {
    spdlog::info("{}: missing or invalid `id_token` in token response",
                 __func__);
    return absl::nullopt;
  }

  google::jwt_verify::Jwt id_token;
  auto jwt_status =
      id_token.parseFromString(id_token_str->second.string_value());
  if (jwt_status != google::jwt_verify::Status::Ok) {
    spdlog::info("{}: failed to parse `id_token` into a JWT: {}", __func__,
                 google::jwt_verify::getStatusString(jwt_status));
    return absl::nullopt;
  }
  // Verify our client_id is set as an entry in the token's `aud` field.
  std::vector<std::string> audiences = {client_id};
  jwt_status = google::jwt_verify::verifyJwt(id_token, *keys_, audiences);
  if (jwt_status != google::jwt_verify::Status::Ok) {
    spdlog::info("{}: `id_token` verification failed: {}", __func__, google::jwt_verify::getStatusString(jwt_status));
    return absl::nullopt;
  }
  // Verify the token contains a `nonce` claim and that it matches our expected value.
  std::string extracted_nonce;
  google::jwt_verify::StructUtils getter(id_token.payload_pb_);
  if (getter.GetString(nonce_field, &extracted_nonce) != google::jwt_verify::StructUtils::OK) {
    spdlog::info("{}: failed to retrieve `nonce` from id_token", __func__);
    return absl::nullopt;
  }
  if (nonce != extracted_nonce) {
    spdlog::info("{}: invalid `nonce` field in id_token", __func__);
    return absl::nullopt;
  }
  auto result = absl::make_optional<TokenResponse>(id_token);
  // There might be an access token too.
  auto access_token_iter = fields.find(access_token_field);
  if (access_token_iter != fields.end()) {
    result->SetAccessToken(access_token_iter->second.string_value());
  }
  // expires_in field takes precedence over JWT timeout.
  auto expiry = static_cast<int64_t>(id_token.exp_);
  auto expires_in_iter = fields.find(expires_in_field);
  if (expires_in_iter != fields.end()) {
    auto expires_in = int64_t(expires_in_iter->second.number_value());
    if (expires_in <= 0) {
      spdlog::info("{}: invalid `expired_in` token response field", __func__);
      return absl::nullopt;
    }
    // Knock 5 seconds off the expiry time to take into account the time it may have taken to retrieve the token.
    expiry = absl::ToUnixSeconds(absl::Now()) + expires_in - 5;
  }
  result->SetExpiry(expiry);
  return result;
}

}  // namespace oidc
}  // namespace filters
}  // namespace authservice
