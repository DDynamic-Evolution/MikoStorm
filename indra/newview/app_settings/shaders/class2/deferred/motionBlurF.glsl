/**
 * @file motionBlurF.glsl
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2026, MikoStorm contributors
 * $/LicenseInfo$
 *
 * Camera-motion motion blur, reimplemented for MikoStorm (never copied from
 * closed-source viewers). Uses "depth rearrangement": reconstruct each pixel's
 * current-frame view-space position from the depth buffer, transform it into the
 * previous frame's camera space with the camera delta matrix, reproject with the
 * previous frame's projection to find where that pixel appeared last frame, and
 * average the already-lit image along the resulting screen-space velocity.
 *
 * Runs as a screen-space post pass on the lit image. Depth-aware rejection keeps
 * the blur from smearing across object edges.
 *
 * Helper functions are prefixed with "mb" and declared locally so they never
 * collide with the auto-injected deferredUtil.glsl helpers.
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec2 vary_fragcoord;

uniform sampler2D diffuseRect;
uniform sampler2D depthMap;

uniform float blur_strength;        // RenderMotionBlurStrength (0 disables the effect)
uniform mat4  inv_proj;             // current inverse projection (auto-bound)
uniform mat4  inv_modelview_delta;  // current view-space -> previous frame view-space
uniform mat4  last_proj;            // previous frame projection (gGLLastProjection)

float mbDepth(vec2 p)
{
    return texture(depthMap, p).r;
}

// current-frame camera-space position of the given screen pixel
vec3 mbViewPos(vec2 p)
{
    float depth = mbDepth(p);
    vec2 ndc = p * 2.0 - 1.0;
    vec4 out_pos = inv_proj * vec4(ndc.x, ndc.y, 2.0 * depth - 1.0, 1.0);
    out_pos /= out_pos.w;
    return out_pos.xyz;
}

// clip space -> UV [0..1]
vec2 mbScreenFromClip(vec4 clip)
{
    vec4 ndc = clip;
    ndc.xyz /= clip.w;
    return ndc.xy * 0.5 + 0.5;
}

void main()
{
    vec2 uv = clamp(vary_fragcoord.xy, vec2(0.0), vec2(1.0));
    vec4 src = texture(diffuseRect, uv);

    // current-frame view-space position (camera at origin)
    vec3 view_pos = mbViewPos(uv);

    // where that point was in the PREVIOUS frame's camera space
    vec3 prev_view_pos = (inv_modelview_delta * vec4(view_pos, 1.0)).xyz;

    // screen (UV) position of that point in the previous frame
    vec2 vel = vec2(0.0);
    vec4 last_clip = last_proj * vec4(prev_view_pos, 1.0);
    if (abs(last_clip.w) > 1e-5)
    {
        vec2 last_uv = mbScreenFromClip(last_clip);
        vel = uv - last_uv; // uv-space motion this pixel underwent
    }

    // scale by strength and guard against pathological smears
    vec2 vel_scaled = clamp(vel, vec2(-0.5), vec2(0.5)) * clamp(blur_strength, 0.0, 10.0) * 0.15;

    if (dot(vel_scaled, vel_scaled) < 1e-8)
    {
        frag_color = src;
        return;
    }

    const int N = 9;
    vec3 acc = vec3(0.0);
    float wsum = 0.0;
    float cdepth = mbDepth(uv);

    for (int i = 0; i < N; i++)
    {
        float t = float(i) / float(N - 1); // 0..1 across the motion trail
        vec2 tc = clamp(uv - vel_scaled * t, vec2(0.0), vec2(1.0));

        // reject samples that cross a depth discontinuity
        float d = mbDepth(tc);
        float depth_w = 1.0 - smoothstep(0.15, 0.5, abs(d - cdepth));

        acc += texture(diffuseRect, tc).rgb * depth_w;
        wsum += depth_w;
    }

    acc = (wsum > 1e-4) ? (acc / wsum) : src.rgb;

    frag_color = vec4(acc, src.a);
}
