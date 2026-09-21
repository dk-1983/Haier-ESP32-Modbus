// SPDX-License-Identifier: MIT
#pragma once
#include <cstdio>
namespace haier_management {
inline bool newer(const char *candidate,const char *current) {
  auto valid=[](const char *s){unsigned dots=0,digits=0;for(;*s;++s){if(*s=='.'){if(!digits)return false;++dots;digits=0;}else if(*s>='0'&&*s<='9'){if(++digits>5)return false;}else return false;}return dots==2&&digits>0;};
  if(!valid(candidate)||!valid(current))return false;
  unsigned a,b,c,x,y,z;int n=0,m=0;
  if(sscanf(candidate,"%u.%u.%u%n",&a,&b,&c,&n)!=3||candidate[n])return false;
  if(sscanf(current,"%u.%u.%u%n",&x,&y,&z,&m)!=3||current[m])return false;
  return a!=x?a>x:b!=y?b>y:c>z;
}
}
