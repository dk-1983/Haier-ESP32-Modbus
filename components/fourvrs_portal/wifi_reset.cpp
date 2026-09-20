// SPDX-License-Identifier: MIT
#include "fourvrs_portal.h"
#include "WifiResetPage.h"
namespace esphome::fourvrs_portal {
void Portal::wifi_reset_web_() {
  web_.on("/wifi/reset",HTTP_GET,[this](){
    if(!test_auth_())return;
    send_page_(WIFI_RESET_PAGE);
  });
  web_.on("/wifi/reset",HTTP_POST,[this](){
    if(!test_auth_())return;
    if(web_.arg("token")!=token_ || web_.arg("confirm")!="RESET_WIFI"){web_.send(403,"text/plain","Confirmation required");return;}
    if(test_pending_ || pending_ || scanning_ || wifi_reset_pending_){web_.send(409,"text/plain","Operation pending; retry after it completes");return;}
    // Clear both committed and candidate networks, so a reboot cannot restore the candidate.
    NetworkConfig old_saved{},old_candidate{},empty{};
    saved_pref_.load(&old_saved);candidate_pref_.load(&old_candidate);
    bool ok=saved_pref_.save(&empty) && candidate_pref_.save(&empty) && global_preferences->sync();
    if(!ok){
      // Best-effort rollback if persistence failed. Keep the current live link intact.
      saved_pref_.save(&old_saved);candidate_pref_.save(&old_candidate);global_preferences->sync();
      storage_ok_=false;web_.send(503,"text/plain","Could not save Wi-Fi reset; current connection retained");return;
    }
    saved_=active_=empty;candidate_=pending_=false;storage_ok_=true;
    wifi_drop_pending_=false;wifi_reset_pending_=true;wifi_reset_at_=millis();
    web_.sendHeader("Cache-Control","no-store");
    web_.send(202,"application/json",String("{\"reset\":true,\"setup_ssid\":")+json_string_(hostname_+"-setup")+",\"setup_ip\":\"192.168.4.1\"}");
  });
}
void Portal::wifi_reset_apply_() {
  // Allow the HTTP response to leave before deliberately dropping the STA connection.
  if(uint32_t(millis()-wifi_reset_at_)<750)return;
  wifi_reset_pending_=false;
  if(ota_active_){ArduinoOTA.end();ota_active_=false;}
  WiFi.disconnect(false,false);connected_before_=false;
  outage_since_=last_attempt_=millis();start_portal_();
}
}
