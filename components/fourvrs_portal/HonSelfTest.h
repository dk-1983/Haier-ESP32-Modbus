#pragma once
#include "esphome/components/haier/hon_climate.h"
// Isolated, unregistered object. No UART parent, setup(), loop(), or writes.
namespace esphome { namespace fourvrs_portal {
class HonProbe : public haier::HonClimate {
 public:
  bool check() {
    set_control_method(haier::HonControlMethod::MONITOR_ONLY);
    for (unsigned i=0; i<6; ++i) {
      action_request_ = PendingAction({static_cast<haier::ActionRequest>(i), {}});
      if (prepare_pending_action() || action_request_.has_value()) return false;
    }
    if (prepare_pending_action()) return false;
    set_control_method(haier::HonControlMethod::SET_GROUP_PARAMETERS);
    send_power_off_command();
    if (!prepare_pending_action() || !action_request_->message.has_value()) return false;
    action_request_.reset();
    uint8_t truncated[33]{};
    for (uint8_t size : {uint8_t(0), uint8_t(1), uint8_t(33)})
      if (process_status_message_(truncated, size) != haier_protocol::HandlerError::WRONG_MESSAGE_STRUCTURE) return false;
    return true;
  }
};
inline bool hon_self_test() { auto probe = std::unique_ptr<HonProbe>(new HonProbe()); return probe->check(); }
}}
