// SPDX-License-Identifier: MIT
#include "fourvrs_portal.h"
#include "SettingsPage.h"
#include <Preferences.h>
namespace esphome::fourvrs_portal {
bool Portal::credentials_setup_() {
  ::Preferences p;if(!p.begin("haier-keys",false))return false;
  if(p.isKey("config")) {
    if(p.getBytesLength("config")!=sizeof(credentials_)||p.getBytes("config",&credentials_,sizeof(credentials_))!=sizeof(credentials_)||!haier_management::valid(credentials_))return false;
  } else {
    if(public_release_) {
      // No shared admin/OTA password: random values remain inaccessible until provisioning.
      for(unsigned i=0;i<8;++i){snprintf(credentials_.web+i*8,9,"%08lx",(unsigned long)esp_random());snprintf(credentials_.ota+i*8,9,"%08lx",(unsigned long)esp_random());}
      strlcpy(credentials_.setup,"Haier-Setup",sizeof(credentials_.setup));credentials_.configured=0;
    } else {
      // One-time migration: keep the installed private build's credentials through future public OTA.
      strlcpy(credentials_.web,ota_password_.c_str(),sizeof(credentials_.web));
      strlcpy(credentials_.ota,ota_password_.c_str(),sizeof(credentials_.ota));
      strlcpy(credentials_.setup,setup_password_.c_str(),sizeof(credentials_.setup));credentials_.configured=1;
    }
    if(!haier_management::valid(credentials_)||p.putBytes("config",&credentials_,sizeof(credentials_))!=sizeof(credentials_))return false;
  }
  setup_password_=credentials_.setup;ota_password_=credentials_.ota;return credentials_ready_=true;
}
void Portal::credentials_web_() {
  auto access=[this](){return credentials_ready_&&(credentials_.configured?test_auth_():portal_request_());};
  web_.on("/settings",HTTP_GET,[this,access](){if(access())send_page_(SETTINGS_PAGE);});
  web_.on("/settings/status",HTTP_GET,[this,access](){if(!access())return;web_.sendHeader("Cache-Control","no-store");web_.send(200,"application/json",credentials_.configured?"{\"configured\":true}":"{\"configured\":false}");});
  web_.on("/settings/passwords",HTTP_POST,[this,access](){
    if(!access())return;
    if(web_.arg("token")!=token_){web_.send(403,"text/plain","Invalid token");return;}
    if(updates_busy_()||test_pending_||pending_||scanning_||wifi_reset_pending_||credentials_restart_){web_.send(409,"text/plain","Operation pending");return;}
    for(unsigned i=0;i<web_.args();++i){String k=web_.argName(i);if(k!="token"&&k!="web_password"&&k!="ota_password"&&k!="setup_password"){web_.send(400,"text/plain","Unknown field");return;}}
    auto next=credentials_;bool changed=false;
    const char *keys[]={"web_password","ota_password","setup_password"};char *dest[]={next.web,next.ota,next.setup};size_t caps[]={sizeof(next.web),sizeof(next.ota),sizeof(next.setup)};
    for(unsigned i=0;i<3;++i){String value=web_.arg(keys[i]);if(!value.length()){if(!credentials_.configured){web_.send(400,"text/plain","Set all three passwords");return;}continue;}
      if(value.length()>=caps[i]||strlen(value.c_str())!=value.length()||!haier_management::password(value.c_str(),value.length()+1,i==2?8:16)){web_.send(400,"text/plain","Invalid password length or characters");return;}
      strlcpy(dest[i],value.c_str(),caps[i]);changed=true;
    }
    if(!changed){web_.send(400,"text/plain","No password supplied");return;}
    next.configured=1;::Preferences p;
    if(!p.begin("haier-keys",false)||p.putBytes("config",&next,sizeof(next))!=sizeof(next)){web_.send(503,"text/plain","Could not save passwords");return;}
    credentials_=next;ota_password_=next.ota;setup_password_=next.setup;
    credentials_restart_=true;credentials_restart_at_=millis();
    web_.sendHeader("Cache-Control","no-store");web_.send(202,"application/json","{\"saved\":true,\"restart\":true}");
  });
}
}
