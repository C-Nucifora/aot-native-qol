#include "aot_fps_menu.h"
#ifdef AOT_FPS_TRIAL
#include "aot_fps_trial.h"
#endif
#ifdef AOT_FPS_SESSION
#include "aot_fps_session.h"
#endif
#include "aot_fps_menu_guards.h"
#include "aot_fps_capabilities.h"
_Static_assert(AOT_FPS_PRODUCTION_COUNT<=16,"Native value widget has16 rows");
/* Staged offline candidate. UI-thread-only, no patch installation in this unit. */
#define F(o,t,n) (*(t *)((uint8_t *)(o)+(n)))
#define VP(o,n,t) ((t)F(F(o,void *,0),uintptr_t,n))
#define FN(r,t) ((t)(binding.image+(r)))
typedef int (*Init)(void *);
typedef void (*Void)(void *);
typedef void *(*Delete)(void *,unsigned);
typedef int (*Update)(void *,float);
typedef void (*Render)(void *,unsigned);
typedef void *(*Row)(void *,unsigned);
typedef int (*Attach)(void *,void *);
typedef struct {
    void *menu,*widget;
    uintptr_t original[26],vtable_storage[27],widget_vtable_storage[20];
    uintptr_t *vtable,*widget_vtable;
    AotNativeFpsSession session;
    unsigned screen,published,error_pending,error_active,retry_pending;
    AotFpsMenuError error;
#ifdef AOT_FPS_SESSION
    unsigned apply_phase,apply_requested,apply_retry;
    uint64_t apply_ticket;
    AotSettingsResult apply_error;
#endif
#ifdef AOT_FPS_TRIAL
    unsigned apply_queued;
#endif
} Instance;
static Instance instances[4];
static AotFpsMenuBinding binding;
/*43c950 formats row+f8 with narrow "%s"; ASCII needs no locale conversion.
 * Stock row cleanup resets this borrowed pointer and never frees the string. */
static const char fps_heading[]="FPS";
static Instance *find(void *menu){
    for(unsigned i=0;i<4;i++)if(instances[i].menu==menu)return &instances[i];
    return 0;
}
static void copy_words(uintptr_t *d,const uintptr_t *s,unsigned n){
    for(unsigned i=0;i<n;i++)d[i]=s[i];
}
static void zero(void *v,size_t n){
    uint8_t *p=v;for(size_t i=0;i<n;i++)p[i]=0;
}
static void *row(void *o,unsigned i){return VP(o,0x88,Row)(o,i);}
static void dispose(Instance *s){
    if(!s->widget)return;
    void *w=s->widget;s->widget=0;
    VP(w,0x10,Void)(w);
    VP(w,0,Delete)(w,1);
}
/* Stock3d72b0 dereferences a failed row allocation at3d72f2.
 * This per-widget replacement adds checked allocation and leaves null slots
 * for stock3d7340 cleanup. Global vtables and stock widgets stay untouched. */
static int safe_value_rows(void *w){
    F(w,int32_t,0x80)=0;
    for(unsigned i=0;i<16;i++){
        void *r=VP(w,0x90,void *(*)(void *))(w);
        F(w,void *,0x88+8*i)=r;
        if(!r || !VP(r,8,Init)(r))return 0;
        if(!FN(0x3da080,Attach)(w,r))return 0;
        F(r,void *,0x20)=w;
    }
    F(w,uint64_t,0x108)=0;
    return 1;
}
/* Stock widgets render all enabled choices at distinct horizontal positions.
 * Our carousel shares one rectangle. Replace the OUTER draw entry so neither
 *3d7490 nor3d9ed0's attached-child traversal emits the other six labels. The
 * selected row's own native+20 wrapper retains ancestor/visibility checks. */
static void draw_selected_value(void *w,unsigned pass){
    unsigned flags=F(w,unsigned,0x38),index=F(w,unsigned,0x80);
    if(!(flags&2) || (flags&0x20) || index>=binding.value_count)return;
    void *value=row(w,index);
    if(value)VP(value,0x20,Render)(value,pass);
}
static int add_row(Instance *s){
    void *m=s->menu,*c=(uint8_t *)m+0x70;
    /* No stock mutation before every fallible preparation step succeeds. */
    void *r3=row(c,3),*r4=row(c,4),*r5=row(c,5);
    if(!r3||!r4||!r5|| (F(r5,unsigned,0x38)&2) || F(r5,void *,0x68))return 0;
    struct {uint32_t tag,pad;void *file;} info={0x478,0,0};
    void *allocator=binding.image+0xbe7800;
    void *w=VP(allocator,0x28,void *(*)(void *,size_t,void *))(allocator,0x110,&info);
    if(!w)return 0;
    /*3d7070 has no allocations or exceptional calls: only zero helper812e60. */
    FN(0x3d7070,void *(*)(void *))(w);s->widget=w;
    s->widget_vtable=s->widget_vtable_storage+1;
    copy_words(s->widget_vtable_storage,F(w,uintptr_t *,0)-1,20);
    s->widget_vtable[0x30/8]=(uintptr_t)safe_value_rows;
    s->widget_vtable[0x20/8]=(uintptr_t)draw_selected_value;
    F(w,uintptr_t *,0)=s->widget_vtable;
    if(!VP(w,8,Init)(w)){dispose(s);return 0;}
    F(w,unsigned,0x6c)=0x400000;F(w,unsigned,0x70)=0x1000000;
    for(unsigned i=0;i<binding.value_count;i++){
        FN(0x3d7170,int (*)(void *,unsigned,const void *))(w,i,binding.value_sprites+0x78*i);
        void *value=row(w,i);
        F(value,int32_t,0x10)=binding.value_x;F(value,int32_t,0x14)=0;
        F(value,int32_t,0x18)=binding.value_width;F(value,int32_t,0x1c)=60;
    }
    F(w,unsigned,0x80)=aot_fps_index(s->session.fps.pending);
    if(!FN(0x3da080,Attach)(r5,w)){dispose(s);return 0;}
    F(w,void *,0x20)=r5;
    /* Commit the bijection only after successful attachment. */
    F(c,void *,0x88+3*8)=r5;F(c,void *,0x88+4*8)=r3;F(c,void *,0x88+5*8)=r4;
    F(r5,void *,0x68)=w;
    F(r5,int32_t,0x10)=36;F(r5,int32_t,0x14)=288;
    F(r5,int32_t,0x18)=1428;
    F(r5,int32_t,0x1c)=F(row(c,2),int32_t,0x1c);
    /*>102 deliberately skips cached MenuText and uses original font fallback. */
    F(r5,uint32_t,0xf0)=UINT32_MAX;F(r5,const char *,0xf8)=fps_heading;
    F(r3,int32_t,0x14)=360;F(r4,int32_t,0x14)=432;
    F(c,unsigned,0x110)=4;F(r5,unsigned,0x38)|=2;
    s->published=1;return 1;
}
/* Native3d75b0 already navigates enabled child rows, skips disabled6..15,
 * and propagates focus through main row+68. Keep that stock input path. */
static void sync_widget(Instance *s){
    if(s->published && s->widget)F(s->widget,unsigned,0x80)=aot_fps_index(s->session.fps.pending);
}
static void read_widget(Instance *s){
    if(!s->published || !s->widget)return;
    unsigned index=F(s->widget,unsigned,0x80);
    if(index<binding.value_count)s->session.fps.pending=aot_fps_targets[index];
    else sync_widget(s);
}
#ifdef AOT_FPS_SESSION
static void advance_apply(Instance *s);
#endif
static int initialize(void *m){
    Instance *s=find(m);
    int result=((Init)s->original[0x30/8])(m);
    if(!result)return result;
    s->screen=1;
    /* Add-on failure leaves a complete stock screen and original dispatch. */
    (void)add_row(s);
#ifdef AOT_FPS_SESSION
    if(aot_fps_session_take_error(&s->error))s->error_pending=1;
#endif
    return result;
}
static void teardown(void *m){
    Instance *s=find(m);if(!s->screen)return;
#ifdef AOT_FPS_SESSION
    if(s->apply_phase){
        uint64_t ticket;
        if(!aot_fps_session_select(s->session.fps.committed,&ticket))aot_fps_session_fail_reason(AOT_FPS_FAIL_TEARDOWN_REQUEST);
        if(s->apply_phase==1)aot_fps_session_display_finished();
        s->apply_phase=0;
    }
#endif
    ((Void)s->original[0x38/8])(m);
    s->screen=0;s->published=0;
    s->error_pending=s->error_active=s->retry_pending=0;
#ifdef AOT_FPS_TRIAL
    s->apply_queued=0;
#endif
    /* Main cleanup has cleared the borrowed reference and detached the widget. */
    dispose(s);
}
static void *destroy(void *m,unsigned flags){
    Instance *s=find(m);
    Delete original=(Delete)s->original[0];
    if(s->screen)teardown(m);
    dispose(s);
    F(m,uintptr_t *,0)=(uintptr_t *)(binding.image+0x9fa2d8);
    zero(s,sizeof(*s));
#ifdef AOT_FPS_CANDIDATE
    extern void aot_fps_stage_instance_released(void);
    aot_fps_stage_instance_released();
#endif
    return original(m,flags);
}
static void load(void *m){Instance *s=find(m);aot_native_fps_load(&s->session,(Void)s->original[19]);sync_widget(s);}
static void capture(void *m){Instance *s=find(m);aot_native_fps_capture(&s->session,(Void)s->original[20]);}
static void restore(void *m){Instance *s=find(m);aot_native_fps_restore(&s->session,(Void)s->original[21]);sync_widget(s);}
static void defaults(void *m){Instance *s=find(m);aot_native_fps_defaults(&s->session,(Void)s->original[22]);sync_widget(s);}
static void commit(void *m){Instance *s=find(m);aot_native_fps_commit(&s->session,(Void)s->original[23]);}
static int unchanged(void *m){Instance *s=find(m);return aot_native_fps_unchanged(&s->session,(int (*)(void *))s->original[24]);}
static void help(void *m){
    Instance *s=find(m);
    if(!s->published){((Void)s->original[17])(m);return;}
    int index=F(m,int32_t,0xf0);if(index<0||index>=6)return;
    void *h=F(binding.image,void *,0xc209e8);
    FN(0x3c13d0,void (*)(void *,unsigned,unsigned))(h,index<4?28:27,0);
    F(h,unsigned,0x70)=0;F(h,unsigned,0x6c)=0x43;
}
static int available(void *m){
    unsigned flags=F(m,unsigned,0x38);
    if((flags&0xc7)!=0xc7 || (flags&0x20))return 0;
    void *parent=F(m,void *,0x20);
    return !parent || FN(0x2dc0f0,int (*)(void *))(parent);
}
#ifdef AOT_FPS_SESSION
static void apply_error(Instance *s){
    /* Native graphics Apply completed even if FPS/disk publication failed. */
    if(!s->session.display_restored && !s->apply_retry)
        aot_native_fps_capture(&s->session,(Void)s->original[20]);
    s->error=(AotFpsMenuError){s->apply_error,s->apply_requested,s->session.fps.committed};
    s->session.last_error=s->apply_error;
    s->session.fps.pending=s->session.fps.committed;s->session.fps.original=s->session.fps.committed;
    s->session.fps.applying=0;
    sync_widget(s);s->apply_phase=0;s->error_pending=1;binding.on_error(s->error);
}
static void rollback_apply(Instance *s,AotSettingsResult error){
    s->apply_error=error;
    if(!aot_fps_session_select(s->session.fps.committed,&s->apply_ticket))aot_fps_session_fail_reason(AOT_FPS_FAIL_MENU_REQUEST);
    s->apply_phase=3;
}
static void advance_apply(Instance *s){
    if(s->apply_phase==1){
        if(!aot_fps_session_display_ready())return;
        commit(s->menu);
        /* Original display Apply/recovery runs ONCE, before async FPS commit. */
        aot_native_fps_finish_apply(&s->session,FN(0x37cf30,uintptr_t (*)(void *)));
        unsigned target=s->session.display_restored?s->session.fps.committed:s->apply_requested;
        int accepted=aot_fps_session_select(target,&s->apply_ticket);
        aot_fps_session_display_finished();
        if(!accepted){rollback_apply(s,AOT_SETTINGS_RUNTIME_ERROR);return;}
        s->apply_phase=2;
    }
    if(s->apply_phase!=2 && s->apply_phase!=3)return;
    unsigned target=s->session.display_restored?s->session.fps.committed:s->apply_requested;
    int handed=s->apply_phase==2?
        aot_fps_session_defer(s->apply_ticket,target,s->session.fps.committed,s->session.config_path,!s->session.display_restored):
        aot_fps_session_defer_rollback(s->apply_ticket,s->apply_requested,s->session.fps.committed,s->apply_error);
    if(handed){
        /* Settings forces pacing1 and skips the owner Wait. Close normally,
         * then the owned coordinator confirms/persists after leaving Options.
         * Do NOT call this confirmation or publish the pending scalar here. */
        s->apply_phase=0;s->session.fps.applying=0;
        VP(s->menu,0x68,Void)(s->menu);F(s->menu,unsigned,0x68)=1;return;
    }
    int result=aot_fps_session_outcome(s->apply_ticket);
    if(!result)return;
    if(s->apply_phase==3){
        if(result<0)aot_fps_session_fail_reason(AOT_FPS_FAIL_MENU_ROLLBACK);
        apply_error(s);return;
    }
    if(result<0){rollback_apply(s,AOT_SETTINGS_RUNTIME_ERROR);return;}
    /* Display recovery restores the retained target. A stock confirmation
     * may be available without changing the suspended menu pacing. */
    s->session.fps.pending=s->session.fps.committed;
    (void)aot_fps_apply_finish(&s->session.fps,0);
    s->apply_phase=0;sync_widget(s);
    VP(s->menu,0x68,Void)(s->menu);F(s->menu,unsigned,0x68)=1;
}
static void begin_apply(Instance *s,int retry){
    if(s->apply_phase)return;
    s->apply_requested=retry?s->error.requested_target:s->session.fps.pending;
    s->apply_retry=(unsigned)retry;s->apply_error=AOT_SETTINGS_OK;
    if(retry){
        s->session.fps.pending=s->apply_requested;s->session.display_restored=0;
        aot_fps_apply_begin(&s->session.fps);
        if(!aot_fps_session_select(s->apply_requested,&s->apply_ticket)){
            rollback_apply(s,AOT_SETTINGS_RUNTIME_ERROR);return;
        }
        s->apply_phase=2;
    }else s->apply_phase=1;
    advance_apply(s);
}
#else
static int finish_apply_and_report(Instance *s){
    unsigned requested=s->session.fps.pending;
    aot_native_fps_finish_apply(&s->session,FN(0x37cf30,uintptr_t (*)(void *)));
    if(s->session.last_error!=AOT_SETTINGS_OK){
        /* Stock display/registry Apply already succeeded. Make that native
         * snapshot current so later Cancel cannot discard applied graphics
         * merely because the separate mod config publication failed. */
        if(!s->session.display_restored)
            aot_native_fps_capture(&s->session,(Void)s->original[20]);
        AotFpsMenuError error={s->session.last_error,requested,s->session.fps.committed};
        /* Copy status before close can destroy the session or its native menu. */
        s->error=error;s->error_pending=1;
        binding.on_error(error);return 0;
    }
    return 1;
}
#endif
static int update_body(void *m,float delta){
    Instance *s=find(m);
    /* Config errors own this embedded dialog. Never feed Retry into the
     * stock discard handler3762c0, which would restore/close the screen. */
    if(s->error_active){
        void *dialog=(uint8_t *)m+0x188;
        VP(dialog,8,Update)(dialog,delta);
        if(F(dialog,unsigned,0x28)==5){
            s->retry_pending=F(dialog,unsigned,0x458)==0;
            s->error_active=0;VP(dialog,0x18,Void)(dialog);
        }
        return 0;
    }
    if(s->error_pending)return 0;
#ifdef AOT_FPS_SESSION
    if(s->apply_phase){advance_apply(s);return 0;}
#endif
#ifdef AOT_FPS_TRIAL
    if(s->apply_queued){
        if(!aot_fps_trial_display_ready())return 0;
        s->apply_queued=0;commit(m);
        if(finish_apply_and_report(s)){VP(m,0x68,Void)(m);F(m,unsigned,0x68)=1;}
        return 0;
    }
#endif
    /* Keep wrapped Apply completion even if extra-row preparation failed. */
    int defaults_index=s->published?4:3,apply_index=s->published?5:4;
    if(FN(0x3762c0,Update)(m,delta))return 0;
    int result=FN(0x3761c0,Update)(m,delta);
    read_widget(s);
    if(available(m)){
        FN(0x37cd80,Void)(m);
        unsigned input=F(binding.image,unsigned,0xc9d7f8);
        unsigned confirm=F(binding.image,unsigned,0xc20ba0),sound=0;
        int play=0,index=F(m,int32_t,0xf0);
        if(input&confirm){
            if(index==defaults_index){defaults(m);play=1;}
            else if(index==apply_index){
#ifdef AOT_FPS_TRIAL
                if(!aot_fps_trial_display_ready()){s->apply_queued=1;return result;}
#endif
#ifdef AOT_FPS_SESSION
                begin_apply(s,0);
#else
                commit(m);
                if(finish_apply_and_report(s)){
                    VP(m,0x68,Void)(m);F(m,unsigned,0x68)=1;
                }
#endif
                play=1;
            }
            if(play)sound=F(binding.image,unsigned,0xc9b368);
        }else if(input&(confirm==8?4:8)){
            if(unchanged(m)){VP(m,0x68,Void)(m);F(m,unsigned,0x68)=2;}
            else FN(0x376380,Void)(m);
            sound=F(binding.image,unsigned,0xc9b370);play=1;
        }
        if(play)FN(0x26ff70,void (*)(void *,unsigned,unsigned))(binding.image+0xc86570,sound,1);
    }
    void *arrow=(uint8_t *)m+0x938;
    VP(arrow,0x18,Update)(arrow,delta);
    return result;
}
static void after_update(Instance *s){
    if(!s || !s->screen)return;
    if(s->retry_pending){
        s->retry_pending=0;
#ifdef AOT_FPS_SESSION
        begin_apply(s,1);
#else
        s->session.fps.pending=s->error.requested_target;
        aot_fps_apply_begin(&s->session.fps);
        /* Retry ONLY the failed mod persistence; stock display already applied.
         * Higher targets must add effective-runtime rollback before this path. */
        s->session.last_error=aot_fps_apply_persist(&s->session.fps,1,s->session.config_path);
        if(s->session.last_error!=AOT_SETTINGS_OK){
            s->error.error=s->session.last_error;s->error_pending=1;
            binding.on_error(s->error);
        }
#endif
    }
    if(s->error_pending && !s->error_active){
        void *dialog=(uint8_t *)s->menu+0x188;
        if(F(dialog,unsigned,0x28)!=6)return;
        static const char message[]="Could not save FPS settings. Retry?";
        _Static_assert(sizeof(message)<1023,"Native copied-message limit");
        s->error_pending=0;s->error_active=1;
        F(dialog,unsigned,0x458)=1;
        VP(dialog,0x18,Void)(dialog);
        VP(dialog,0x38,void (*)(void *,const char *))(dialog,
            s->error.error==AOT_SETTINGS_RUNTIME_ERROR?"Could not apply FPS setting. Retry?":message);
        VP(dialog,0x20,Void)(dialog);
    }
}
static int update(void *m,float delta){
    int result=update_body(m,delta);
    /* Find association again: no borrowed menu pointer survives close. */
    after_update(find(m));
    Instance *s=find(m);
    /* Native parent2e1fe5 accepts completed Apply only for stock row<=4.
     * Keep row5 during visible navigation; translate only its final closed
     * result, after3761c0 has completed the original close animation. */
    if(s && s->published && result==1 && (F(m,unsigned,0x38)&0x20) &&
       !(F(m,unsigned,0x38)&0x10) && F(m,unsigned,0x68)==1 &&
       F(m,unsigned,0xf0)==5)F(m,unsigned,0xf0)=4;
    return result;
}
/* Native draw already traverses enabled main rows and their borrowed values.
 * Keep original draw including specialized resolution arrows and dialogs. */
static void render(void *m,unsigned pass){Instance *s=find(m);((Render)s->original[9])(m,pass);}
int aot_fps_menu_bind(const AotFpsMenuBinding *b){
    if(!b||!b->image||!b->config_path||!b->value_sprites || b->value_count!=AOT_FPS_PRODUCTION_COUNT||!b->on_error||b->value_width<=0)return 0;
    if(aot_fps_menu_instances()||b->image_size<0xc9d7fc)return 0;
    for(unsigned i=0;i<sizeof(guards)/sizeof(guards[0]);i++){
        if(guards[i].rva>b->image_size-16)return 0;
        for(unsigned j=0;j<16;j++)if(b->image[guards[i].rva+j]!=guards[i].bytes[j])return 0;
    }
    for(unsigned i=0;i<26;i++)
        if(F(b->image,uintptr_t,0x9fa2d8+8*i)!=(uintptr_t)b->image+menu_targets[i])return 0;
    for(unsigned i=0;i<10;i++)
        if(F(b->image,uintptr_t,0x9fe898+8*i)!=(uintptr_t)b->image+dialog_targets[i])return 0;
    binding=*b;return 1;
}
void *aot_fps_menu_constructor(void *m,unsigned device){
    void *result=FN(0x37bef0,void *(*)(void *,unsigned))(m,device);
    if(!result)return result;
    Instance *s=0;
    for(unsigned i=0;i<4;i++)if(!instances[i].menu){s=&instances[i];break;}
    if(!s)return result;
    s->menu=result;s->session.menu=result;s->session.config_path=binding.config_path;
    copy_words(s->original,F(result,uintptr_t *,0),26);
    s->vtable=s->vtable_storage+1;
    copy_words(s->vtable_storage,F(result,uintptr_t *,0)-1,27);
    s->vtable[0]=(uintptr_t)destroy;s->vtable[6]=(uintptr_t)initialize;
    s->vtable[7]=(uintptr_t)teardown;s->vtable[8]=(uintptr_t)update;s->vtable[9]=(uintptr_t)render;
    s->vtable[17]=(uintptr_t)help;s->vtable[19]=(uintptr_t)load;s->vtable[20]=(uintptr_t)capture;
    s->vtable[21]=(uintptr_t)restore;s->vtable[22]=(uintptr_t)defaults;
    s->vtable[23]=(uintptr_t)commit;s->vtable[24]=(uintptr_t)unchanged;
    F(result,uintptr_t *,0)=s->vtable;
    return result;
}
unsigned aot_fps_menu_instances(void){
    unsigned count=0;for(unsigned i=0;i<4;i++)count+=instances[i].menu!=0;return count;
}

AotSettingsResult aot_fps_menu_last_error(void *m){
    Instance *s=find(m);return s?s->session.last_error:AOT_SETTINGS_INVALID;
}
