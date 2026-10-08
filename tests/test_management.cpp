#include <cassert>
#include <initializer_list>
#include <cstring>
#include "components/fourvrs_portal/credentials_model.h"
#include "components/fourvrs_portal/update_model.h"
#include "components/fourvrs_portal/recovery_policy.h"
using namespace haier_management;
int main(){
 Credentials c;strcpy(c.web,"PrivateWebPassword16");strcpy(c.ota,"DifferentOtaPassword");strcpy(c.setup,"MySetupPass");c.configured=1;assert(valid(c));
 auto old=c;memset(c.web,'x',sizeof(c.web));assert(!valid(c));c=old;c.ota[5]='\n';assert(!valid(c));c=old;c.setup[0]=0;assert(!valid(c));c=old;c.configured=2;assert(!valid(c));c=old;c.magic=0;assert(!valid(c));
 assert(!password("short",6,16));assert(!password("contains a space",17,8));
 assert(newer("1.1.0","1.0.9"));assert(newer("1.10.0","1.9.9"));assert(!newer("1.1.0","1.1.0"));assert(!newer("1.0.0","1.1.0"));
 for(auto v:{"v1.2.0","1.2.0-beta","1..2","1.2.3.4","999999.1.0","-1.2.0","1.2.0 ","1.2"})assert(!newer(v,"1.0.0"));
 for (uint32_t start : {0u, 0xFFFF0000u}) {
   RecoveryPolicy policy; policy.start(start, false);
   assert(!policy.update(start+179999, false, 0, false));
   assert(!policy.update(start+180000, false, 0, true)); // Factory/ESP update must not be interrupted.
   assert(policy.update(start+180000, false, 0, false));
   policy.consume();
   assert(!policy.update(start+400000, false, 0, false));
   RecoveryPolicy rebooted; rebooted.start(start+400000, true); // Persistent one-reset budget.
   assert(!rebooted.update(start+600000, false, 0, false));
   for (uint32_t dt=0;dt<=60000;dt+=1000)
     assert(!rebooted.update(start+610000+dt, true, start+610000+dt, false));
   assert(rebooted.can_rearm(start+670000)); rebooted.rearm();
   assert(!rebooted.update(start+849999, true, start+670000, false));
   assert(rebooted.update(start+850000, true, start+670000, false));
   rebooted.consume();
   rebooted.update(start+860000,true,start+860000,false);
   rebooted.update(start+895000,true,start+860000,false); // Stale gap breaks the healthy window.
   rebooted.update(start+900000,true,start+900000,false);
   assert(!rebooted.can_rearm(start+920000));
 }
 RecoveryPolicy overflow; overflow.start(0, false);
 assert(!overflow.update(200000,true,200000,false,true));
 assert(!overflow.update(379999,true,379999,false,true));
 assert(overflow.update(380000,true,380000,false,true)); // Fresh passive status cannot hide blocked controls.
 overflow.consume();
 assert(!overflow.can_rearm(500000));
 puts("Recovery timeout, maintenance deferral, reboot budget, stable rearm and millis wrap: PASS");
 puts("Credential bounds/corruption and stable-version policy: PASS");
}
