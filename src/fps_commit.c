#include "aot_fps_session.h"
#include "aot_fps_capabilities.h"
/* UI and timer checkpoint callbacks share the verified clock owner thread.
 * Copy every value before native close can destroy the originating menu. */
static struct {
    unsigned phase,requested,retained,publish;
    uint64_t ticket;
    uint16_t path[1001];
    AotSettingsResult error;
} job;
static AotFpsMenuError deferred_error;
static unsigned error_pending;
int aot_fps_session_defer(uint64_t ticket,unsigned requested,unsigned retained,const uint16_t *path,int publish){
    if(!ticket || !aot_fps_supported(requested) || !aot_fps_supported(retained) ||
       !path || !aot_fps_session_queued(ticket))return 0;
    unsigned length=0;while(length<1000 && path[length])length++;
    if(length<3 || length==1000 ||
       !((path[1]==':' && (path[2]=='\\' || path[2]=='/')) || (path[0]=='\\' && path[1]=='\\')))return 0;
    for(unsigned i=0;i<=length;i++)job.path[i]=path[i];
    job.phase=1;job.requested=requested;job.retained=retained;job.ticket=ticket;
    job.error=AOT_SETTINGS_OK;job.publish=publish!=0;error_pending=0;return 1;
}
int aot_fps_session_defer_rollback(uint64_t ticket,unsigned requested,unsigned retained,AotSettingsResult error){
    if(!ticket || !aot_fps_supported(requested) || !aot_fps_supported(retained) ||
       !aot_fps_session_queued(ticket))return 0;
    job.phase=2;job.requested=requested;job.retained=retained;job.ticket=ticket;job.error=error;
    error_pending=0;return 1;
}
int aot_fps_session_commit_busy(void){return job.phase!=0;}
int aot_fps_session_take_error(AotFpsMenuError *out){
    if(!out || !error_pending)return 0;
    *out=deferred_error;error_pending=0;return 1;
}
void aot_fps_session_commit_poll(void){
    if(!job.phase)return;
    int result=aot_fps_session_outcome(job.ticket);
    if(!result)return;
    if(job.phase==2){
        if(result<0)aot_fps_session_fail_reason(AOT_FPS_FAIL_COMMIT_ROLLBACK);
        deferred_error=(AotFpsMenuError){job.error,job.requested,job.retained};
        error_pending=1;job.phase=0;return;
    }
    if(result>0){
        if(!job.publish){job.phase=0;return;}
        job.error=aot_settings_store(job.path,job.requested);
        if(job.error==AOT_SETTINGS_OK){job.phase=0;return;}
    }else job.error=AOT_SETTINGS_RUNTIME_ERROR;
    /* No persistence on refusal. Restore the last published target and wait
     * for its matching confirmation before exposing the native Retry error. */
    if(!aot_fps_session_select(job.retained,&job.ticket))aot_fps_session_fail_reason(AOT_FPS_FAIL_COMMIT_REQUEST);
    job.phase=2;
}
