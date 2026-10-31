/* Validation-only interposer. Never linked into the test suite or oracle. */
#define _GNU_SOURCE
#include "sqlite3.h"
#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static const char *mode(void){
  const char *m=getenv("HARVEST_FAULT_MODE");
  if(!m)return "";
  FILE *f=fopen("/proc/self/cmdline","rb");
  if(!f)return "";
  char buf[8192];size_t n=fread(buf,1,sizeof(buf)-1,f);fclose(f);
  for(size_t i=0;i<n;i++)if(!buf[i])buf[i]=' ';
  buf[n]=0;
  return strstr(buf,"--gtest_filter=SqliteTractor.Replay/cast_cast_10_2_L521")?m:"";
}
int sqlite3_open_v2(const char *path,sqlite3 **out,int flags,const char *vfs){
  int (*real)(const char*,sqlite3**,int,const char*)=dlsym(RTLD_NEXT,"sqlite3_open_v2");
  if(!strcmp(mode(),"setup"))return real("/proc/harvest-invalid-directory/test.db",out,flags,vfs);
  return real(path,out,flags,vfs);
}
int sqlite3_prepare_v3(sqlite3 *db,const char *sql,int n,unsigned flags,sqlite3_stmt **out,const char **tail){
  int (*real)(sqlite3*,const char*,int,unsigned,sqlite3_stmt**,const char**)=dlsym(RTLD_NEXT,"sqlite3_prepare_v3");
  const char *m=mode();
  if(strstr(sql,"CAST(44 AS REAL) AS 'm'")){
    if(!strcmp(m,"assertion")){
      const char *unused;int rc=real(db,"SELECT 999;",-1,flags,out,&unused);
      if(tail)*tail=sql+strlen(sql);return rc;
    }
    if(!strcmp(m,"crash"))raise(SIGSEGV);
    if(!strcmp(m,"timeout"))for(;;)pause();
  }
  return real(db,sql,n,flags,out,tail);
}
