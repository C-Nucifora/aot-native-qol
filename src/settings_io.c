#include "aot_mod.h"
#include "aot_settings_io.h"
#include "aot_fps_capabilities.h"
AOT_API void * __stdcall CreateFileW(const uint16_t *,unsigned long,unsigned long,void *,unsigned long,unsigned long,void *);
AOT_API int __stdcall ReadFile(void *,void *,unsigned long,unsigned long *,void *);
AOT_API int __stdcall WriteFile(void *,const void *,unsigned long,unsigned long *,void *);
AOT_API int __stdcall FlushFileBuffers(void *);
AOT_API int __stdcall CloseHandle(void *);
AOT_API int __stdcall DeleteFileW(const uint16_t *);
AOT_API int __stdcall MoveFileExW(const uint16_t *,const uint16_t *,unsigned long);
AOT_API unsigned long __stdcall GetLastError(void);
#define BAD_HANDLE ((void *)(intptr_t)-1)
static const char prefix[]="version=1\nfps=";
static unsigned format_config(char data[32],unsigned fps){
    unsigned n=0;
    for(unsigned i=0;i<sizeof(prefix)-1;i++)data[n++]=prefix[i];
    if(fps>=100)data[n++]=(char)('0'+fps/100);
    if(fps>=10)data[n++]=(char)('0'+fps/10%10);
    data[n++]=(char)('0'+fps%10);data[n++]='\n';
    return n;
}
static int absolute_path(const uint16_t *p,size_t *length) {
    size_t n=0;
    if(!p)return 0;
    while(n<1000 && p[n])++n;
    if(n<3 || n==1000)return 0;
    if(!((p[1]==':' && (p[2]=='\\' || p[2]=='/')) ||
         (p[0]=='\\' && p[1]=='\\')))return 0;
    *length=n;return 1;
}
AotSettingsResult aot_settings_load(const uint16_t *path,unsigned *fps) {
    size_t length;char data[32];unsigned long bytes=0;
    if(!fps || !absolute_path(path,&length))return AOT_SETTINGS_INVALID;
    void *file=CreateFileW(path,0x80000000u,1,0,3,0x80,0);
    if(file==BAD_HANDLE){unsigned long e=GetLastError();return e==2 || e==3?AOT_SETTINGS_MISSING:AOT_SETTINGS_IO_ERROR;}
    int ok=ReadFile(file,data,sizeof(data),&bytes,0);
    int closed=CloseHandle(file);
    if(!ok || !closed)return AOT_SETTINGS_IO_ERROR;
    /* Compare complete canonical forms. The read buffer is longer than every
     * supported form, so trailing data cannot be mistaken for EOF. */
    for(unsigned i=0;i<AOT_FPS_PRODUCTION_COUNT;i++){
        char canonical[32];unsigned n=format_config(canonical,aot_fps_targets[i]);
        if(bytes!=n)continue;
        unsigned j=0;while(j<n && data[j]==canonical[j])j++;
        if(j==n){*fps=aot_fps_targets[i];return AOT_SETTINGS_OK;}
    }
    return AOT_SETTINGS_INVALID;
}
AotSettingsResult aot_settings_store(const uint16_t *path,unsigned fps) {
    size_t length;uint16_t temporary[1005];unsigned long bytes=0;
    if(!aot_fps_supported(fps) || !absolute_path(path,&length))return AOT_SETTINGS_INVALID;
    for(size_t i=0;i<length;++i)temporary[i]=path[i];
    temporary[length]='.';temporary[length+1]='t';temporary[length+2]='m';
    temporary[length+3]='p';temporary[length+4]=0;
    /* CREATE_NEW refuses another writer's or a crash-left temporary file. */
    void *file=CreateFileW(temporary,0x40000000u,0,0,1,0x80,0);
    if(file==BAD_HANDLE)return AOT_SETTINGS_IO_ERROR;
    char config[32];unsigned size=format_config(config,fps);
    int ok=WriteFile(file,config,size,&bytes,0);
    if(!ok || bytes!=size)ok=0;
    if(ok && !FlushFileBuffers(file))ok=0;
    if(!CloseHandle(file))ok=0;
    if(ok && MoveFileExW(temporary,path,1|8))return AOT_SETTINGS_OK;
    DeleteFileW(temporary);
    return AOT_SETTINGS_IO_ERROR;
}
AotSettingsResult aot_fps_apply_persist(AotFpsState *s,int success,const uint16_t *path) {
    if(!s || !s->applying)return AOT_SETTINGS_INVALID;
    AotFpsState candidate=*s;
    if(!aot_fps_apply_finish(&candidate,success)){
        *s=candidate;return AOT_SETTINGS_OK;
    }
    AotSettingsResult result=aot_settings_store(path,candidate.committed);
    if(result==AOT_SETTINGS_OK)*s=candidate;
    else {s->pending=s->committed;s->applying=0;}
    return result;
}
