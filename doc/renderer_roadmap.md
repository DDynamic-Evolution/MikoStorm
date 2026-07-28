# Renderer Roadmap

Planned improvements to the OpenGL rendering engine, ordered by effort-to-impact.

---

## Phase 1 — Quick Wins (Low Effort, Medium Impact)

### 1.1 Replace GLH loader with glad

The current `llgl.cpp` has ~1500 lines of hand-rolled `GetProcAddress` via the legacy GLH library. `glad` auto-generates this from a GL version/profile XML, eliminating manual maintenance and making new GL features trivial to add.

- Replace `GL/glext.h` + `GL/glh_extensions.h` with `glad/gl.h`
- Remove all `extern PFN*` declarations from `llglheaders.h`
- Remove `initExtensions()` manual loading in `llgl.cpp`
- Update `llwindow` platform files to use `gladLoadGL()` / `gladLoadWGL()` etc.
- Affects: `llgl.cpp`, `llglheaders.h`, `llwindow` files, CMake

### 1.2 32-bit index buffers

`LLVertexBuffer` uses `GL_UNSIGNED_SHORT` which caps meshes at 65535 vertices. High-quality PBR/GLTF content routinely exceeds this.

- Change index type from `GL_UNSIGNED_SHORT` to `GL_UNSIGNED_INT` in `llvertexbuffer.cpp`
- Verify `GLsizei` count parameters in `drawRange()` / `drawElements()` calls
- Check buffer size calculations for the larger index size

### 1.3 GL debug callback

`glDebugMessageCallback` is already loaded but unused. A proper callback catches driver errors immediately during development.

- Install a `GL_DEBUG_SEVERITY_HIGH` callback in `LLGLManager::initExtensions()`
- Route to `LL_WARNS()` or breakpoint on error
- Toggle via existing `gDebugGL` setting

---

## Phase 2 — Core Modernization (Medium Effort, High Impact)

### 2.1 GL 4.5 Core Profile + DSA (Direct State Access)

Highest ROI change. DSA eliminates bind-before-modify — the single biggest source of redundant GL calls in the current codebase.

**New pattern (DSA):**
```cpp
// Before (bind-based):
glBindTexture(GL_TEXTURE_2D, id);
glTexStorage2D(GL_TEXTURE_2D, levels, internal, w, h);

// After (DSA):
glTextureStorage2D(id, levels, internal, w, h);
```

**Changes needed:**

| File | Change |
|------|--------|
| `llgltexture.cpp` | Replace `glBindTexture` + `glTex*` with `glCreateTextures` + `glTextureStorage*` + `glTextureSubImage*` + `glGenerateTextureMipmap` |
| `llimagegl.cpp` | Same for image-to-texture upload path |
| `llrendertarget.cpp` | Replace FBO binding with `glCreateFramebuffers`, `glNamedFramebufferTexture`, `glNamedFramebufferRenderbuffer`, `glCheckNamedFramebufferStatus` |
| `llvertexbuffer.cpp` | Replace with `glCreateVertexArrays`, `glVertexArrayVertexBuffer`, `glEnableVertexArrayAttrib`, `glVertexArrayAttribFormat`, `glVertexArrayAttribBinding` |
| `llcubemap.cpp` | `glCreateTextures(GL_TEXTURE_CUBE_MAP, ...)` + DSA uploads |
| `llrender.cpp` | Use `glBindTextureUnit(unit, id)` instead of `glActiveTexture(GL_TEXTURE0 + unit)` + `glBindTexture` |
| `llglstates.h` | Simplify state tracking; DSA reduces need for state query/restore |
| `llwindow` | Request `GL_CONTEXT_CORE_PROFILE_BIT` with version 4.5 |

**Benefits:**
- Eliminates the entire `LLTexUnit` abstraction
- No more redundant `glActiveTexture` / `glBindTexture` pairs
- Cleaner error messages (DSA functions validate at the object level)
- Tangible FPS improvement via reduced driver overhead

### 2.2 Persistent mapped buffers

Current `LLVertexBuffer::setBuffer()` uses `glMapBufferRange` with dirty-region tracking via `useBuffer()` / `unmapBuffer()`. With `GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT` (GL 4.4+), the GPU reads from the buffer while the CPU writes to a different region.

- Allocate VBOs with `GL_MAP_PERSISTENT_BIT` + `GL_MAP_COHERENT_BIT` at creation time
- Keep the mapping permanently instead of map/unmap per frame
- Use fence sync (`GLsync`) for regions still in use by GPU
- Benefits avatar-dense scenes, animated objects, and any geometry with per-frame updates

### 2.3 Compute shaders for luminance/exposure

The current luminance extraction + exposure adaptation is a multi-pass full-screen rasterization. Compute shaders are already loaded but never used.

- Write compute shader for luminance histogram calculation
- Single `glDispatchCompute` replaces multiple fullscreen quad passes
- Serves as pilot for wider compute shader adoption:
  - SSR ray marching
  - Post-processing (bloom, DoF)
  - Avatar skinning
  - Deferred light culling

---

## Phase 3 — Advanced (High Effort, Very High Impact)

### 3.1 GPU-driven indirect rendering

The CPU-side `stateSort()` + `pushBatches()` loop is the #1 bottleneck in cluttered SL scenes.

- Replace per-object draw calls with `glMultiDrawElementsIndirect` (GL 4.3+)
- Implement GPU frustum culling via compute shader
- Generate indirect draw commands on GPU
- Requires restructuring the entire draw pool system (~10k+ lines)

### 3.2 Bindless textures

`ARB_bindless_texture` (GL 4.4+) eliminates texture unit management. Each shader can reference any number of textures via 64-bit handles, bypassing the fixed texture unit limit.

- Most beneficial for PBR material rendering (many texture samplers per material)
- Adds complexity to shader code (need to declare textures in UBO/SSBO)
- Good complement to indirect rendering

### 3.3 Async texture loading via shared GL context

The viewer already creates shared GL contexts (`SDL_GL_SHARE_WITH_CURRENT_CONTEXT`). Use a dedicated thread for async texture/image upload:

- Background thread decodes images and calls `glTextureSubImage2D` on the shared context
- Frees main thread from decode + upload stalls
- Requires `GL_ARB_sync` fence objects for synchronization

---

## Non-Goals

- **Vulkan/DirectX/Metal backend** — Not practical for a solo dev; OpenGL remains the single backend
- **Mesh shaders** — Too experimental, hardware support too limited
- **Hardware ray tracing** — Requires Vulkan/D3D12, not viable on OpenGL
- **Full renderer rewrite** — Work within existing architecture

---

## Implementation Order

```
Phase 1 (quick) → Phase 2.1 (DSA) → Phase 2.2 (persistent buffers) → Phase 2.3 (compute)
→ Phase 3 (as needed based on profiling)
```

Phase 2.1 (DSA) is the recommended next step after quick wins — it touches the most code and delivers the most benefit per unit of effort.
