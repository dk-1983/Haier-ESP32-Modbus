// SPDX-License-Identifier: MIT
#include "fourvrs_portal.h"
#include "UiShell.h"
namespace esphome::fourvrs_portal {
void Portal::send_page_(const char *page){
  size_t length=strlen_P(page),start=0;
  web_.sendHeader(F("Cache-Control"),F("no-store"));web_.setContentLength(CONTENT_LENGTH_UNKNOWN);web_.send(200,"text/html; charset=utf-8","");
  const char *markers[]={"__TOKEN__","__STYLE__","__NAV__","__SSID__"};
  for(size_t i=0;i<length;++i){
    if(pgm_read_byte(page+i)!='_')continue;
    for(unsigned k=0;k<4;++k){
      size_t n=strlen(markers[k]),j=0;
      while(j<n && i+j<length && pgm_read_byte(page+i+j)==markers[k][j])++j;
      if(j!=n)continue;
      if(i>start)web_.sendContent_P(page+start,i-start);
      if(k==0)web_.sendContent(token_);
      else if(k==3)web_.sendContent(hostname_+"-setup");
      else web_.sendContent_P(k==1?UI_STYLE:UI_NAV);
      i+=n-1;start=i+1;break;
    }
  }
  if(length>start)web_.sendContent_P(page+start,length-start);web_.sendContent("");

}
}
