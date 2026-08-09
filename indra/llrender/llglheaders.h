#ifndef LL_LLGLHEADERS_H
#define LL_LLGLHEADERS_H

#if LL_WINDOWS
#include "llwin32headers.h"
#endif

#include "glad/glad.h"

#include <GL/glu.h>

#if LL_WINDOWS

#include <GL/wglext.h>

extern PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
extern PFNWGLGETGPUIDSAMDPROC            wglGetGPUIDsAMD;
extern PFNWGLGETGPUINFOAMDPROC           wglGetGPUInfoAMD;
extern PFNWGLGETCONTEXTGPUIDAMDPROC      wglGetContextGPUIDAMD;
extern PFNWGLCREATEASSOCIATEDCONTEXTAMDPROC  wglCreateAssociatedContextAMD;
extern PFNWGLCREATEASSOCIATEDCONTEXTATTRIBSAMDPROC wglCreateAssociatedContextAttribsAMD;
extern PFNWGLDELETEASSOCIATEDCONTEXTAMDPROC  wglDeleteAssociatedContextAMD;
extern PFNWGLMAKEASSOCIATEDCONTEXTCURRENTAMDPROC wglMakeAssociatedContextCurrentAMD;
extern PFNWGLGETCURRENTASSOCIATEDCONTEXTAMDPROC wglGetCurrentAssociatedContextAMD;
extern PFNWGLBLITCONTEXTFRAMEBUFFERAMDPROC wglBlitContextFramebufferAMD;
extern PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
extern PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB;
extern PFNWGLSWAPINTERVALEXTPROC    wglSwapIntervalEXT;
extern PFNWGLGETSWAPINTERVALEXTPROC wglGetSwapIntervalEXT;

#endif

#ifndef GL_DEPTH_CLAMP
#define GL_DEPTH_CLAMP 0x864F
#endif

#ifndef GL_NVX_gpu_memory_info
#define GL_NVX_gpu_memory_info
#define GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX          0x9047
#define GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX    0x9048
#define GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX  0x9049
#define GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX            0x904A
#define GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX            0x904B
#endif

#ifndef GL_ATI_meminfo
#define GL_ATI_meminfo
#define GL_VBO_FREE_MEMORY_ATI                     0x87FB
#define GL_TEXTURE_FREE_MEMORY_ATI                 0x87FC
#define GL_RENDERBUFFER_FREE_MEMORY_ATI            0x87FD
#endif

#if defined(TRACY_ENABLE) && LL_PROFILER_ENABLE_TRACY_OPENGL
    #include <tracy/TracyOpenGL.hpp>
#endif

#endif
