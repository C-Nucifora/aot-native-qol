#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "aot_settings_io.h"
#include "aot_sidecar.h"
static int exists,fail_write,fail_flush,fail_close,fail_move,temp_owned;
static unsigned long error;
static const char *read_data="version=1\nfps=60\n";
static char published[64];
void *CreateFileW(const uint16_t *p,unsigned long access,unsigned long share,void *s,unsigned long mode,unsigned long flags,void *t) {
 (void)p;(void)access;(void)share;(void)s;(void)flags;(void)t;
 if(mode==1){if(exists){error=80;return (void *)-1;}temp_owned=1;return (void *)1;}
 return (void *)2;
}
int ReadFile(void *f,void *p,unsigned long n,unsigned long *bytes,void *o) {
 (void)f;(void)o;size_t len=strlen(read_data);if(len>n)len=n;memcpy(p,read_data,len);*bytes=len;return 1;
}
int WriteFile(void *f,const void *p,unsigned long n,unsigned long *bytes,void *o) {
 (void)f;(void)o;*bytes=fail_write?n-1:n;memcpy(published,p,*bytes);published[*bytes]=0;return 1;
}
int FlushFileBuffers(void *f){(void)f;return !fail_flush;}
int CloseHandle(void *f){(void)f;return !fail_close;}
int DeleteFileW(const uint16_t *p){(void)p;assert(temp_owned);temp_owned=0;return 1;}
int MoveFileExW(const uint16_t *a,const uint16_t *b,unsigned long flags){(void)a;(void)b;assert(flags==9);if(fail_move)return 0;temp_owned=0;return 1;}
unsigned long GetLastError(void){return error;}
int main(void){
 const uint16_t path[]={'C',':','\\','a',0};unsigned fps=99;
 assert(aot_settings_load(path,&fps)==AOT_SETTINGS_OK && fps==60);
 read_data="version=1\nfps=60\ntrailing";assert(aot_settings_load(path,&fps)==AOT_SETTINGS_INVALID);
 const unsigned targets[]={60,90,120,144,165,240,0};
 for(unsigned i=0;i<7;i++){
  char expected[64];snprintf(expected,sizeof(expected),"version=1\nfps=%u\n",targets[i]);
  assert(aot_settings_store(path,targets[i])==AOT_SETTINGS_OK && !strcmp(published,expected));
  read_data=expected;fps=17;
  assert(aot_settings_load(path,&fps)==AOT_SETTINGS_OK && fps==targets[i]);
  AotFpsState selected;aot_fps_open(&selected,targets[i]);assert(selected.pending==targets[i]);
  aot_fps_defaults(&selected);assert(selected.pending==60);aot_fps_cancel(&selected);assert(selected.pending==targets[i]);
 }
 const char *invalid[]={"version=1\nfps=61\n","version=1\nfps=241\n","version=1\nfps=060\n","version=1\nfps=240\nextra","version=1\nfps=240","version=1\nfps=\n"};
 for(unsigned i=0;i<6;i++){read_data=invalid[i];fps=17;assert(aot_settings_load(path,&fps)==AOT_SETTINGS_INVALID && fps==17);}
 assert(aot_settings_store(path,241)==AOT_SETTINGS_INVALID);
 assert(aot_settings_store(path,60)==AOT_SETTINGS_OK && !strcmp(published,"version=1\nfps=60\n"));
 exists=1;assert(aot_settings_store(path,60)==AOT_SETTINGS_IO_ERROR && !temp_owned);exists=0;
 for(int i=0;i<4;i++){
 fail_write=i==0;fail_flush=i==1;fail_close=i==2;fail_move=i==3;
 assert(aot_settings_store(path,60)==AOT_SETTINGS_IO_ERROR && !temp_owned);
 }
 fail_write=fail_flush=fail_close=0;
 AotFpsState state;aot_fps_open(&state,60);aot_fps_apply_begin(&state);
 assert(aot_fps_apply_persist(&state,1,path)==AOT_SETTINGS_IO_ERROR && state.committed==60 && !state.applying);
 fail_move=0;aot_fps_apply_begin(&state);
 assert(aot_fps_apply_persist(&state,1,path)==AOT_SETTINGS_OK && !state.applying);
 exists=1;aot_fps_apply_begin(&state);assert(aot_fps_apply_persist(&state,0,path)==AOT_SETTINGS_OK);exists=0;
 for(int phase=1;phase<=5;phase++){
  AotSidecar s={0};void *widget=0;assert(aot_sidecar_allocate(&s,(void *)42));
  for(int next=2;next<=phase;next++)assert(aot_sidecar_advance(&s,(AotSidecarPhase)next));
  if(phase==5){assert(!aot_sidecar_take_cleanup(&s,&widget));aot_sidecar_main_cleared(&s);}
  unsigned action=aot_sidecar_take_cleanup(&s,&widget);assert(widget==(void *)42);
  assert(action==(phase==1?AOT_SIDECAR_FREE_RAW:phase==2?AOT_SIDECAR_DELETE:AOT_SIDECAR_CLEANUP|AOT_SIDECAR_DELETE));
  assert(!aot_sidecar_take_cleanup(&s,&widget));
 }
 return 0;
}
