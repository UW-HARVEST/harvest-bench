/* Deterministic delegating VFS. Every native callback receives its own VFS. */
#include "sqlite3.h"
#include <stdint.h>
#include <string.h>
static sqlite3_vfs wrapper;
static sqlite3_vfs *native;
static uint64_t state;
static int open_file(sqlite3_vfs*v,sqlite3_filename n,sqlite3_file*f,int flags,int*out){return native->xOpen(native,n,f,flags,out);}
static int delete_file(sqlite3_vfs*v,const char*n,int sync){return native->xDelete(native,n,sync);}
static int access_file(sqlite3_vfs*v,const char*n,int flags,int*out){return native->xAccess(native,n,flags,out);}
static int full_path(sqlite3_vfs*v,const char*n,int size,char*out){return native->xFullPathname(native,n,size,out);}
static void *dl_open(sqlite3_vfs*v,const char*n){return native->xDlOpen(native,n);}
static void dl_error(sqlite3_vfs*v,int n,char*out){native->xDlError(native,n,out);}
static void (*dl_sym(sqlite3_vfs*v,void*h,const char*n))(void){return native->xDlSym(native,h,n);}
static void dl_close(sqlite3_vfs*v,void*h){native->xDlClose(native,h);}
static int random_bytes(sqlite3_vfs*v,int n,char*out){
  for(int i=0;i<n;i++){
    /* xorshift64, low byte per step; seed is fixed per isolated scenario. */
    state ^= state << 13; state ^= state >> 7; state ^= state << 17;
    out[i]=(char)(state & 255);
  }
  return n;
}
static int sleep_us(sqlite3_vfs*v,int n){return native->xSleep(native,n);}
static int current_time(sqlite3_vfs*v,double*out){*out=2451544.5;return SQLITE_OK;}
static int last_error(sqlite3_vfs*v,int n,char*out){return native->xGetLastError(native,n,out);}
static int current_time64(sqlite3_vfs*v,sqlite3_int64*out){*out=INT64_C(211813444800000);return SQLITE_OK;}
static int set_call(sqlite3_vfs*v,const char*n,sqlite3_syscall_ptr p){return native->xSetSystemCall(native,n,p);}
static sqlite3_syscall_ptr get_call(sqlite3_vfs*v,const char*n){return native->xGetSystemCall(native,n);}
static const char *next_call(sqlite3_vfs*v,const char*n){return native->xNextSystemCall(native,n);}
int harvest_environment(void){
  native=sqlite3_vfs_find(0);
  if(!native || native==&wrapper) return SQLITE_ERROR;
  memset(&wrapper,0,sizeof(wrapper));
  wrapper.iVersion=native->iVersion>3?3:native->iVersion;
  wrapper.szOsFile=native->szOsFile;wrapper.mxPathname=native->mxPathname;
  wrapper.zName="harvest_deterministic";
  wrapper.xOpen=open_file;wrapper.xDelete=delete_file;wrapper.xAccess=access_file;
  wrapper.xFullPathname=full_path;
  if(native->xDlOpen)wrapper.xDlOpen=dl_open;
  if(native->xDlError)wrapper.xDlError=dl_error;
  if(native->xDlSym)wrapper.xDlSym=dl_sym;
  if(native->xDlClose)wrapper.xDlClose=dl_close;
  wrapper.xRandomness=random_bytes;wrapper.xSleep=sleep_us;
  wrapper.xCurrentTime=current_time;
  if(native->xGetLastError)wrapper.xGetLastError=last_error;
  if(wrapper.iVersion>=2)wrapper.xCurrentTimeInt64=current_time64;
  if(wrapper.iVersion>=3){
    if(native->xSetSystemCall)wrapper.xSetSystemCall=set_call;
    if(native->xGetSystemCall)wrapper.xGetSystemCall=get_call;
    if(native->xNextSystemCall)wrapper.xNextSystemCall=next_call;
  }
  state=UINT64_C(0x4d595df4d0f33173);
  int rc=sqlite3_vfs_register(&wrapper,1);
  sqlite3_randomness(0,0);
  return rc;
}
void harvest_environment_end(void){sqlite3_vfs_unregister(&wrapper);}
