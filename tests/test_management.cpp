#include <cassert>
#include <initializer_list>
#include <cstring>
#include "components/fourvrs_portal/credentials_model.h"
#include "components/fourvrs_portal/update_model.h"
using namespace haier_management;
int main(){
 Credentials c;strcpy(c.web,"PrivateWebPassword16");strcpy(c.ota,"DifferentOtaPassword");strcpy(c.setup,"MySetupPass");c.configured=1;assert(valid(c));
 auto old=c;memset(c.web,'x',sizeof(c.web));assert(!valid(c));c=old;c.ota[5]='\n';assert(!valid(c));c=old;c.setup[0]=0;assert(!valid(c));c=old;c.configured=2;assert(!valid(c));c=old;c.magic=0;assert(!valid(c));
 assert(!password("short",6,16));assert(!password("contains a space",17,8));
 assert(newer("1.1.0","1.0.9"));assert(newer("1.10.0","1.9.9"));assert(!newer("1.1.0","1.1.0"));assert(!newer("1.0.0","1.1.0"));
 for(auto v:{"v1.2.0","1.2.0-beta","1..2","1.2.3.4","999999.1.0","-1.2.0","1.2.0 ","1.2"})assert(!newer(v,"1.0.0"));
 puts("Credential bounds/corruption and stable-version policy: PASS");
}
