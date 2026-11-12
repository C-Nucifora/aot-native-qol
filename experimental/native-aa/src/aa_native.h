#ifndef AOT_AA_NATIVE_H
#define AOT_AA_NATIVE_H
#if defined(AOT_AA_NATIVE_BUILD) && !defined(__ELF__)
#define AOT_AA_NATIVE_API __declspec(dllexport)
#else
#define AOT_AA_NATIVE_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* Return0 only when a self-contained SMAA list is merged into the supplied
 * DEFERRED context. Both proof gates must be1. No native-context ClearState,
 * FinishCommandList, timing patch or UI draw occurs. CAS is deliberately off. */
AOT_AA_NATIVE_API int AotAaRecordDeferred(void *context,void *scene_rtv,unsigned verified_srgb,unsigned confirmed_pre_ui);
/* All calls, including release, must be externally serialized on one rendering
 * worker. Rejects multiple bound targets; acquires/releases getter references. */
AOT_AA_NATIVE_API int AotAaRecordBoundDeferred(void *context,unsigned verified_srgb,unsigned confirmed_pre_ui);
/* Release only after callers are quiescent; no game target is retained. */
AOT_AA_NATIVE_API void AotAaRelease(void);
#ifdef __cplusplus
}
#endif
#endif
