#pragma once

#include <cstdint>

namespace esphome {
namespace intercom_api {

// Call state (high-level FSM for display / triggers).
//   IDLE -> OUTGOING -> STREAMING (caller path)
//   IDLE -> RINGING  -> STREAMING (callee path, manual or auto answer)
enum class CallState : uint8_t {
  IDLE,        // No call in progress
  OUTGOING,    // We initiated a call, waiting for remote to RING/ANSWER
  RINGING,     // Incoming call presented to user, awaiting answer/decline
  STREAMING,   // Audio active
};

enum class CallEndReason : uint8_t {
  LOCAL_HANGUP,
  REMOTE_HANGUP,
  REMOTE_DEVICE_LOST,
  DECLINED,
  TIMEOUT,
  BUSY,
  UNREACHABLE,
  PROTOCOL_ERROR,
  BRIDGE_ERROR,
};

static constexpr const char *kReasonLocalHangup = "local_hangup";
static constexpr const char *kReasonRemoteHangup = "remote_hangup";
static constexpr const char *kReasonRemoteDeviceLost = "remote_device_lost";
static constexpr const char *kReasonDeclined = "declined";
static constexpr const char *kReasonTimeout = "timeout";
static constexpr const char *kReasonBusy = "busy";
static constexpr const char *kReasonUnreachable = "unreachable";
static constexpr const char *kReasonProtocolError = "protocol_error";
static constexpr const char *kReasonBridgeError = "bridge_error";
static constexpr const char *kReasonDnd = "DND";

inline const char *call_state_to_str(CallState state) {
  switch (state) {
    case CallState::IDLE: return "idle";
    case CallState::OUTGOING: return "outgoing";
    case CallState::RINGING: return "ringing";
    case CallState::STREAMING: return "streaming";
    default: return "unknown";
  }
}

inline const char *call_end_reason_to_str(CallEndReason reason) {
  switch (reason) {
    case CallEndReason::LOCAL_HANGUP: return kReasonLocalHangup;
    case CallEndReason::REMOTE_HANGUP: return kReasonRemoteHangup;
    case CallEndReason::REMOTE_DEVICE_LOST: return kReasonRemoteDeviceLost;
    case CallEndReason::DECLINED: return kReasonDeclined;
    case CallEndReason::TIMEOUT: return kReasonTimeout;
    case CallEndReason::BUSY: return kReasonBusy;
    case CallEndReason::UNREACHABLE: return kReasonUnreachable;
    case CallEndReason::PROTOCOL_ERROR: return kReasonProtocolError;
    case CallEndReason::BRIDGE_ERROR: return kReasonBridgeError;
    default: return "unknown";
  }
}

}  // namespace intercom_api
}  // namespace esphome
