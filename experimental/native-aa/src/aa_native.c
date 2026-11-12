/* Original freestanding deferred SMAA adapter. Upstream shader notices bundled. */
#define AOT_AA_NATIVE_BUILD
#include "aa_native.h"
#include <stdint.h>
#include <stddef.h>
#ifdef __ELF__
#define CALL __attribute__((ms_abi))
#define EXPORT __attribute__((visibility("default")))
#define IMPORT
#else
#define CALL __stdcall
#define EXPORT __declspec(dllexport)
#define IMPORT __declspec(dllimport)
#endif
IMPORT uint32_t CALL GetCurrentThreadId(void);
extern void *memcpy(void *,const void *,size_t);
extern void *memset(void *,int,size_t);
#include "../generated/compiled_shaders_c.h"
#include "../third_party/smaa/AreaTex.h"
#include "../third_party/smaa/SearchTex.h"
typedef struct {void **v;} Object;
typedef struct {uint32_t a;uint16_t b,c;uint8_t d[8];} Guid;
static const Guid texture_id={0x6f15aaf2,0xd208,0x4e89,{0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c}};
typedef struct {uint32_t width,height,mips,array,format,samples,quality,usage,bind,cpu,misc;} Texture;
typedef struct {uint32_t format,dimension,detail[4];} ShaderView;
typedef struct {uint32_t format,dimension,detail[3];} TargetView;
typedef struct {const void *data;uint32_t pitch,slice;} Initial;
typedef struct {uint32_t size,usage,bind,cpu,misc,stride;} Buffer;
typedef struct {uint32_t filter,u,v,w;float bias;uint32_t anisotropy,comparison;float border[4],minimum,maximum;} Sampler;
typedef struct {uint32_t fill,cull,front;int32_t bias;float clamp,slope;uint32_t depth_clip,scissor,multisample,line;} Raster;
typedef struct {uint32_t enabled,write,func,stencil;uint8_t read_mask,write_mask,pad[2];uint32_t front[4],back[4];} Depth;
typedef struct {float x,y,width,height,minimum,maximum;} Viewport;
_Static_assert(sizeof(Texture)==44 && sizeof(ShaderView)==24 && sizeof(TargetView)==20,"texture view ABI");
_Static_assert(sizeof(Buffer)==24 && sizeof(Sampler)==52 && sizeof(Raster)==40 && sizeof(Depth)==52,"state ABI");
static void drop(Object **p){if(*p)((uint32_t(CALL *)(Object *))(*p)->v[2])(*p);*p=0;}
static void retain(Object *p){((uint32_t(CALL *)(Object *))p->v[1])(p);}
typedef struct {Object *texture,*srv,*rtv;} Image;
static struct {
    Object *device,*recorder,*vs[4],*ps[4],*metrics,*output,*linear,*point,*raster,*depth;
    Image area,search,source,edges,weights,blend;
    Object *encoded,*decoded;
    uint32_t width,height,typeless,thread;
    int ready;
} state;
static void free_image(Image *p){drop(&p->srv);drop(&p->rtv);drop(&p->texture);}
static void free_frame(void){drop(&state.encoded);drop(&state.decoded);free_image(&state.source);free_image(&state.edges);free_image(&state.weights);free_image(&state.blend);state.width=state.height=0;}
void AotAaRelease(void){free_frame();free_image(&state.area);free_image(&state.search);
    for(unsigned i=0;i<4;i++){drop(&state.vs[i]);drop(&state.ps[i]);}
    drop(&state.metrics);drop(&state.output);drop(&state.linear);drop(&state.point);drop(&state.raster);drop(&state.depth);drop(&state.recorder);drop(&state.device);memset(&state,0,sizeof state);
}
static int texture(uint32_t width,uint32_t height,uint32_t format,uint32_t bind,Object **out,const void *data,uint32_t pitch){
    Texture desc={width,height,1,1,format,1,0,data?1u:0u,bind,0,0};Initial initial={data,pitch,0};
    return ((int32_t(CALL *)(Object *,const Texture *,const Initial *,Object **))state.device->v[5])(state.device,&desc,data?&initial:0,out)>=0;
}
static int image(uint32_t w,uint32_t h,uint32_t f,Image *out,const void *data,uint32_t pitch){
    if(!texture(w,h,f,data?8u:40u,&out->texture,data,pitch))return 0;
    if(((int32_t(CALL *)(Object *,Object *,const void *,Object **))state.device->v[7])(state.device,out->texture,0,&out->srv)<0)return 0;
    return data || ((int32_t(CALL *)(Object *,Object *,const void *,Object **))state.device->v[9])(state.device,out->texture,0,&out->rtv)>=0;
}
static int buffer(Object **out){Buffer d={16,0,4,0,0,0};return ((int32_t(CALL *)(Object *,const Buffer *,const Initial *,Object **))state.device->v[3])(state.device,&d,0,out)>=0;}
static int initialize(Object *device){
    state.device=device;retain(device);
    const unsigned char *vs[]={aa_EdgeVS,aa_WeightVS,aa_BlendVS,aa_FullscreenVS};
    const size_t vs_sizes[]={sizeof aa_EdgeVS,sizeof aa_WeightVS,sizeof aa_BlendVS,sizeof aa_FullscreenVS};
    const unsigned char *ps[]={aa_EdgePS,aa_WeightPS,aa_BlendPS,aa_CopyPS};
    const size_t ps_sizes[]={sizeof aa_EdgePS,sizeof aa_WeightPS,sizeof aa_BlendPS,sizeof aa_CopyPS};
    for(unsigned i=0;i<4;i++){
        if(((int32_t(CALL *)(Object *,const void *,size_t,Object *,Object **))device->v[12])(device,vs[i],vs_sizes[i],0,&state.vs[i])<0)return 0;
        if(((int32_t(CALL *)(Object *,const void *,size_t,Object *,Object **))device->v[15])(device,ps[i],ps_sizes[i],0,&state.ps[i])<0)return 0;
    }
    if(!buffer(&state.metrics)||!buffer(&state.output))return 0;
    Sampler s={0};s.filter=20;s.u=s.v=s.w=3;s.anisotropy=1;s.comparison=1;uint32_t max=0x7f7fffff;memcpy(&s.maximum,&max,4);
    if(((int32_t(CALL *)(Object *,const Sampler *,Object **))device->v[23])(device,&s,&state.linear)<0)return 0;
    s.filter=0;if(((int32_t(CALL *)(Object *,const Sampler *,Object **))device->v[23])(device,&s,&state.point)<0)return 0;
    Raster r={0};r.fill=3;r.cull=1;r.depth_clip=1;
    if(((int32_t(CALL *)(Object *,const Raster *,Object **))device->v[22])(device,&r,&state.raster)<0)return 0;
    Depth d={0};d.func=8;d.read_mask=d.write_mask=255;for(unsigned i=0;i<3;i++)d.front[i]=d.back[i]=1;d.front[3]=d.back[3]=8;
    if(((int32_t(CALL *)(Object *,const Depth *,Object **))device->v[21])(device,&d,&state.depth)<0)return 0;
    if(!image(AREATEX_WIDTH,AREATEX_HEIGHT,49,&state.area,areaTexBytes,AREATEX_PITCH)||!image(SEARCHTEX_WIDTH,SEARCHTEX_HEIGHT,61,&state.search,searchTexBytes,SEARCHTEX_PITCH))return 0;
    if(((int32_t(CALL *)(Object *,uint32_t,Object **))device->v[27])(device,0,&state.recorder)<0)return 0;
    state.thread=GetCurrentThreadId();state.ready=1;return 1;
}
static int prepare(uint32_t w,uint32_t h,uint32_t typeless,uint32_t unorm,uint32_t srgb){
    if(state.width==w&&state.height==h&&state.typeless==typeless)return 1;
    free_frame();if(!texture(w,h,typeless,8,&state.source.texture,0,0))goto fail;
    ShaderView v={0};v.format=unorm;v.dimension=4;v.detail[1]=1;
    if(((int32_t(CALL *)(Object *,Object *,const ShaderView *,Object **))state.device->v[7])(state.device,state.source.texture,&v,&state.encoded)<0)goto fail;
    v.format=srgb;if(((int32_t(CALL *)(Object *,Object *,const ShaderView *,Object **))state.device->v[7])(state.device,state.source.texture,&v,&state.decoded)<0)goto fail;
    if(!image(w,h,49,&state.edges,0,0)||!image(w,h,28,&state.weights,0,0)||!image(w,h,10,&state.blend,0,0))goto fail;
    state.width=w;state.height=h;state.typeless=typeless;return 1;
fail:free_frame();return 0;
}
static void pass(Object *c,unsigned index,Object *target,Object **views){
    Object *none[5]={0};((void(CALL *)(Object *,uint32_t,uint32_t,Object *const *))c->v[8])(c,0,5,none);
    ((void(CALL *)(Object *,uint32_t,Object *const *,Object *))c->v[33])(c,1,&target,0);
    ((void(CALL *)(Object *,uint32_t,uint32_t,Object *const *))c->v[8])(c,0,5,views);
    ((void(CALL *)(Object *,Object *,Object *const *,uint32_t))c->v[11])(c,state.vs[index],0,0);
    ((void(CALL *)(Object *,Object *,Object *const *,uint32_t))c->v[9])(c,state.ps[index],0,0);
    ((void(CALL *)(Object *,uint32_t,uint32_t))c->v[13])(c,3,0);
}
int AotAaRecordDeferred(void *context,void *scene_rtv,unsigned verified_srgb,unsigned confirmed_pre_ui){
    if(verified_srgb!=1 || confirmed_pre_ui!=1)return 1;
    Object *native=context,*target=scene_rtv,*device=0,*resource=0,*tex=0;int result=2;
    if(!native||!target||((uint32_t(CALL *)(Object *))native->v[112])(native)!=1)return 2;
    ((void(CALL *)(Object *,Object **))native->v[3])(native,&device);
    if(!device)return 2;
    if(state.ready && (state.device!=device || state.thread!=GetCurrentThreadId())){drop(&device);return 3;}
    if(!state.ready && !initialize(device)){AotAaRelease();drop(&device);return 4;}
    if(((int32_t(CALL *)(Object *))device->v[39])(device)<0){result=5;goto done;}
    ((void(CALL *)(Object *,Object **))target->v[7])(target,&resource);
    if(!resource || ((int32_t(CALL *)(Object *,const Guid *,Object **))resource->v[0])(resource,&texture_id,&tex)<0){result=6;goto done;}
    Object *resource_device=0;((void(CALL *)(Object *,Object **))tex->v[3])(tex,&resource_device);
    int same=resource_device==device;drop(&resource_device);if(!same){result=7;goto done;}
    Texture d={0};TargetView v={0};((void(CALL *)(Object *,Texture *))tex->v[10])(tex,&d);((void(CALL *)(Object *,TargetView *))target->v[8])(target,&v);
    if(!d.width||!d.height||d.samples!=1||d.mips!=1||d.array!=1||v.dimension!=4||v.detail[0]!=0){result=8;goto done;}
    uint32_t typeless,unorm,srgb;
    if(d.format==27||d.format==28||d.format==29){typeless=27;unorm=28;srgb=29;}
    else if(d.format==87||d.format==90||d.format==91){typeless=90;unorm=87;srgb=91;}
    else{result=9;goto done;}
    if(v.format!=unorm&&v.format!=srgb){result=10;goto done;}
    if(!prepare(d.width,d.height,typeless,unorm,srgb)){result=11;goto done;}
    Object *c=state.recorder;
    ((void(CALL *)(Object *))c->v[110])(c);
    ((void(CALL *)(Object *,Object *,Object *))c->v[47])(c,state.source.texture,tex);
    float metrics[]={1.f/d.width,1.f/d.height,(float)d.width,(float)d.height};uint32_t output[]={v.format==unorm?1u:0u,0,0,0};
    ((void(CALL *)(Object *,Object *,uint32_t,const void *,const void *,uint32_t,uint32_t))c->v[48])(c,state.metrics,0,0,metrics,0,0);
    ((void(CALL *)(Object *,Object *,uint32_t,const void *,const void *,uint32_t,uint32_t))c->v[48])(c,state.output,0,0,output,0,0);
    Viewport vp={0,0,(float)d.width,(float)d.height,0,1};((void(CALL *)(Object *,uint32_t,const Viewport *))c->v[44])(c,1,&vp);
    ((void(CALL *)(Object *,Object *))c->v[43])(c,state.raster);((void(CALL *)(Object *,Object *,uint32_t))c->v[36])(c,state.depth,0);
    ((void(CALL *)(Object *,uint32_t))c->v[24])(c,4);
    ((void(CALL *)(Object *,uint32_t,uint32_t,Object *const *))c->v[7])(c,0,1,&state.metrics);
    ((void(CALL *)(Object *,uint32_t,uint32_t,Object *const *))c->v[16])(c,0,1,&state.metrics);
    float zero[4]={0};((void(CALL *)(Object *,Object *,const float *))c->v[50])(c,state.edges.rtv,zero);
    Object *views[5]={state.encoded,0,0,0,0};
    ((void(CALL *)(Object *,uint32_t,uint32_t,Object *const *))c->v[10])(c,0,1,&state.point);pass(c,0,state.edges.rtv,views);
    views[0]=0;views[1]=state.edges.srv;views[2]=state.area.srv;views[3]=state.search.srv;
    ((void(CALL *)(Object *,uint32_t,uint32_t,Object *const *))c->v[10])(c,0,1,&state.linear);pass(c,1,state.weights.rtv,views);
    views[0]=state.decoded;views[1]=views[2]=views[3]=0;views[4]=state.weights.srv;pass(c,2,state.blend.rtv,views);
    views[0]=state.blend.srv;views[4]=0;((void(CALL *)(Object *,uint32_t,uint32_t,Object *const *))c->v[16])(c,0,1,&state.output);pass(c,3,target,views);
    Object *list=0;
    if(((int32_t(CALL *)(Object *,int,Object **))c->v[114])(c,0,&list)<0){drop(&state.recorder);AotAaRelease();result=12;goto done;}
    ((void(CALL *)(Object *,Object *,int))native->v[58])(native,list,1);drop(&list);result=0;
done:drop(&tex);drop(&resource);drop(&device);return result;
}
int AotAaRecordBoundDeferred(void *context,unsigned verified_srgb,unsigned confirmed_pre_ui){
    if(verified_srgb!=1 || confirmed_pre_ui!=1)return 1;
    Object *native=context,*targets[8]={0},*depth=0;
    if(!native || ((uint32_t(CALL *)(Object *))native->v[112])(native)!=1)return 2;
    ((void(CALL *)(Object *,uint32_t,Object **,Object **))native->v[89])(native,8,targets,&depth);
    int single=targets[0]!=0;
    for(unsigned i=1;i<8;i++)if(targets[i])single=0;
    int result=single?AotAaRecordDeferred(context,targets[0],verified_srgb,confirmed_pre_ui):13;
    for(unsigned i=0;i<8;i++)drop(&targets[i]);
    drop(&depth);return result;
}
