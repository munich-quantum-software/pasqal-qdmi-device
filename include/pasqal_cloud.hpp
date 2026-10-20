/*
 * Minimal Pasqal Cloud REST client used by the Pasqal QDMI device.
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace pasqal {

/// Default Pasqal Cloud API endpoint.
inline constexpr std::string_view DEFAULT_BASE_URL =
    "https://apis.pasqal.cloud";
/// Default Pasqal Cloud token endpoint.
inline constexpr std::string_view DEFAULT_AUTH_ENDPOINT =
    "https://pasqal.eu.auth0.com/oauth/token";

/// Raised when an HTTP request fails; `status` is 0 for transport errors.
class CloudError : public std::runtime_error {
public:
  CloudError(long http_status, const std::string& message)
      : std::runtime_error(message), status(http_status) {}
  long status;
};

/// Raised when credentials are missing or rejected.
class AuthError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

/// Raised when a job wait's deadline expires.
class TimeoutError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

/// Raised when the cloud does not provide an optional capability.
class UnsupportedError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

using Deadline = std::optional<std::chrono::steady_clock::time_point>;

/// State of a Pasqal Cloud job.
enum class JobState { Pending, Running, Canceling, Done, Canceled, Error };

struct CloudJob {
  std::string id;
  std::string batch_id;
  JobState state;
  std::optional<std::size_t> runs;
  std::optional<std::string> sequence;
};

struct CloudBatch {
  std::string id;
  std::vector<std::string> job_ids;
  std::optional<std::string> sequence;
};

struct CloudConfig {
  std::string base_url{DEFAULT_BASE_URL};
  std::string auth_endpoint{DEFAULT_AUTH_ENDPOINT};
  std::string device_type;
  std::string project_id;
  std::optional<std::string> token;
  std::optional<std::string> username;
  std::optional<std::string> password;
  std::optional<std::string> client_id;
  std::optional<std::string> client_secret;
};

/// Return whether `name` is a Pasqal Cloud device type (case-insensitive).
bool is_device_type(std::string_view name);

/// Return whether `token` is usable at `now` (Unix seconds): non-empty and not
/// within 10 s of its JWT expiry. Tokens without a JWT expiry are usable.
bool is_token_usable(const std::string& token, long long now);

class CloudClient {
public:
  explicit CloudClient(CloudConfig config);

  /// The device-spec entries for the configured device, as a JSON array of
  /// {"device_type", "specs"} with the specs in Pulser's abstract
  /// representation: the one device for a QPU, the real devices' public specs
  /// for an emulator.
  std::string device_spec_entries();
  /// Selected backend's public metadata, independently of Pulser device specs.
  std::string device_metadata();
  /// Submit one closed native batch and return its ID without follow-up reads.
  std::string submit_batch(const std::vector<std::string>& sequences,
                           std::size_t runs);
  CloudBatch batch(const std::string& batch_id,
                   Deadline deadline = std::nullopt);
  CloudJob job(const std::string& job_id, Deadline deadline = std::nullopt);
  void cancel_batch(const std::string& batch_id);
  std::size_t queue_length();
  void cancel_job(const std::string& job_id);
  JobState job_state(const std::string& job_id,
                     Deadline deadline = std::nullopt);
  /// Measurement counts of the finished job, keyed by bitstring.
  std::map<std::string, std::size_t>
  job_counts(const std::string& job_id, Deadline deadline = std::nullopt);

private:
  std::string get(const std::string& path, bool authenticated = true,
                  Deadline deadline = std::nullopt);
  std::string post(const std::string& path, const std::string& body);
  std::string patch(const std::string& path);
  std::string
  bearer(Deadline deadline = std::nullopt,
         const std::optional<std::string>& rejected_token = std::nullopt);

  CloudConfig config_;
  /// Guards `token_`: jobs of one session may be checked from several threads.
  std::timed_mutex token_mutex_;
  std::string token_;
  std::optional<std::chrono::steady_clock::time_point> token_expiry_;
};

} // namespace pasqal
