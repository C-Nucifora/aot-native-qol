#include "aot_owned_files.h"
AOT_API void * __stdcall CreateFileW(const uint16_t *,unsigned long,unsigned long,void *,unsigned long,unsigned long,void *);
AOT_API int __stdcall ReadFile(void *,void *,unsigned long,unsigned long *,void *);
AOT_API int __stdcall CloseHandle(void *);
AOT_API unsigned long __stdcall GetModuleFileNameW(void *,uint16_t *,unsigned long);
#define INVALID_FILE ((void *)(intptr_t)-1)
static const uint32_t constants[64]={
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
typedef struct { uint32_t h[8];uint64_t bytes;uint8_t block[64];unsigned used; } Hash;
static uint32_t rotate(uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
static void compress(Hash *s){
    uint32_t w[64];
    for(unsigned i=0;i<16;i++){const uint8_t *p=s->block+4*i;w[i]=(uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
    for(unsigned i=16;i<64;i++){
        uint32_t a=w[i-15],b=w[i-2];
        w[i]=w[i-16]+(rotate(a,7)^rotate(a,18)^(a>>3))+w[i-7]+(rotate(b,17)^rotate(b,19)^(b>>10));
    }
    uint32_t a=s->h[0],b=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7];
    for(unsigned i=0;i<64;i++){
        uint32_t t=h+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+constants[i]+w[i];
        uint32_t u=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));
        h=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;
    }
    s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static void hash_init(Hash *s){
    static const uint32_t initial[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    for(unsigned i=0;i<8;i++)s->h[i]=initial[i];s->bytes=0;s->used=0;
}
static void hash_add(Hash *s,const uint8_t *p,size_t n){
    s->bytes+=n;
    for(size_t i=0;i<n;i++){s->block[s->used++]=p[i];if(s->used==64){compress(s);s->used=0;}}
}
static void hash_finish(Hash *s,uint8_t out[32]){
    uint64_t bits=s->bytes*8;s->block[s->used++]=0x80;
    if(s->used>56){while(s->used<64)s->block[s->used++]=0;compress(s);s->used=0;}
    while(s->used<56)s->block[s->used++]=0;
    for(unsigned i=0;i<8;i++)s->block[56+i]=(uint8_t)(bits>>(56-8*i));
    compress(s);
    for(unsigned i=0;i<32;i++)out[i]=(uint8_t)(s->h[i/4]>>(24-8*(i%4)));
}
void aot_sha256(const uint8_t *p,size_t n,uint8_t out[32]){Hash s;hash_init(&s);hash_add(&s,p,n);hash_finish(&s,out);}
int aot_supported_executable(void){
    uint16_t path[1024];uint8_t chunk[1024],digest[32];Hash hash;
    unsigned long length=GetModuleFileNameW(0,path,1024);
    if(!length || length>=1024)return 0;
    void *file=CreateFileW(path,0x80000000u,1,0,3,0x80,0);
    if(file==INVALID_FILE)return 0;
    hash_init(&hash);int ok=1;
    for(;;){unsigned long n=0;if(!ReadFile(file,chunk,sizeof(chunk),&n,0)){ok=0;break;}if(!n)break;hash_add(&hash,chunk,n);if(hash.bytes>13421192u){ok=0;break;}}
    if(!CloseHandle(file))ok=0;
    if(!ok || hash.bytes!=13421192u)return 0;
    hash_finish(&hash,digest);
    static const uint8_t supported[32]={0x67,0xfc,0x81,0x21,0xf6,0x5e,0xfb,0x7f,0x72,0x27,0x4f,0x7c,0xa1,0x68,0xce,0x9e,
        0x30,0xad,0x68,0xb0,0x3d,0x8e,0x9f,0x05,0xa3,0xd5,0x6c,0x3b,0x9c,0xcc,0xe9,0x17};
    for(unsigned i=0;i<32;i++)if(digest[i]!=supported[i])return 0;
    return 1;
}
AotStatus aot_file_load_exact(const uint16_t *path,size_t expected,AotOwnedFile *out){
    if(!path || !out || out->bytes || !expected || expected>16u*1024u*1024u)return AOT_BAD_RANGE;
    void *file=CreateFileW(path,0x80000000u,1,0,3,0x80,0);
    if(file==INVALID_FILE)return AOT_BAD_RANGE;
    uint8_t *bytes=VirtualAlloc(0,expected,AOT_MEM_RESERVE_COMMIT,AOT_PAGE_READWRITE);
    if(!bytes){CloseHandle(file);return AOT_BAD_RANGE;}
    size_t total=0;int ok=1;
    while(total<expected){unsigned long n=0;
        if(!ReadFile(file,bytes+total,(unsigned long)(expected-total),&n,0) || !n || n>expected-total){ok=0;break;}
        total+=n;
    }
    uint8_t extra;unsigned long n=0;
    if(ok && (!ReadFile(file,&extra,1,&n,0) || n))ok=0;
    if(!CloseHandle(file))ok=0;
    unsigned long old;
    if(!ok || !VirtualProtect(bytes,expected,0x02,&old)){VirtualFree(bytes,0,AOT_MEM_RELEASE);return AOT_BAD_RANGE;}
    out->bytes=bytes;out->size=expected;return AOT_OK;
}
AotStatus aot_file_release(AotOwnedFile *f){
    if(!f || !f->bytes)return AOT_BAD_RANGE;
    if(!VirtualFree(f->bytes,0,AOT_MEM_RELEASE))return AOT_PROTECTION_FAILED;
    f->bytes=0;f->size=0;return AOT_OK;
}
