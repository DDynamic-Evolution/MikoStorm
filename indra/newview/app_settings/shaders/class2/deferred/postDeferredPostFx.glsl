/**
 * @file postDeferredPostFx.glsl
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2026, MikoStorm contributors
 * $/LicenseInfo$
 *
 * Black Dragon style post-processing composite pass, reimplemented for MikoStorm.
 * Applies (each independently gated by its uniform, all off at default values so
 * the default image is unchanged):
 *   - color grading: saturation / contrast / brightness
 *   - vignette
 *   - optional film grain (HAS_NOISE)
 *
 * This runs as the final composite step right before the present to screen.
 * Color space: input is already tonemapped/display-space; we never re-gamma here.
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

uniform sampler2D diffuseRect;
uniform sampler2D depthMap;

uniform vec2 screen_res;
uniform float saturation;
uniform float contrast;
uniform float brightness;
uniform float postfx_strength;
uniform float vignette_amount;
uniform float film_grain;

// AYAR cinematic effects (0 = off; gated upsteam by the *_InCinematicEnabled toggle)
uniform float aya_temp;        // AYAR17 color temperature [-1..1], warm(+) / cool(-)
uniform float aya_ap_strength; // AYAR16 aerial perspective [0..1] depth haze
uniform float aya_cloud;       // AYAR18 cloud volumetric [0..1]
uniform float aya_atmo;        // AYAR14 atmosphere/volume [0..1]

in vec2 vary_fragcoord;

//=================================
// borrowed noise from:
//  <https://www.shadertoy.com/view/4dS3Wd>
//  By Morgan McGuire @morgan3d, http://graphicscodex.com
//
float hash(float n) { return fract(sin(n) * 1e4); }
float hash(vec2 p) { return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); }

float noise(vec2 x) {
    vec2 i = floor(x);
    vec2 f = fract(x);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}
//=============================

vec3 clampHDRRange(vec3 color);


void main()
{
    vec4 diff = texture(diffuseRect, vary_fragcoord.xy);

    // Color grading (gated by postfx_strength > 0)
    if (postfx_strength > 0.0)
    {
        // Saturation (luminance-preserving)
        float lum = dot(diff.rgb, vec3(0.2126, 0.7152, 0.0722));
        vec3 graded = mix(vec3(lum), diff.rgb, saturation);
        // Contrast (around midpoint 0.5)
        graded = (graded - 0.5) * contrast + 0.5;
        // Brightness offset
        graded += brightness;
        diff.rgb = mix(diff.rgb, graded, postfx_strength);
    }

    // Vignette (gated by vignette_amount > 0)
    if (vignette_amount > 0.0)
    {
        vec2 ndc = vary_fragcoord.xy * 2.0 - 1.0;
        float dist = length(ndc);
        diff.rgb *= 1.0 - vignette_amount * smoothstep(0.0, 1.5, dist * 0.9);
    }

    // Depth-derived haze factor for the cinematic atmosphere/aerial effects.
    // depthMap holds clamped non-linear depth; invert it to a 0..1 distance hint
    // (1 = far). Gated so it costs nothing when all AYAR depth effects are off.
    float haze = 0.0;
    if (aya_ap_strength > 0.0 || aya_atmo > 0.0)
    {
        float zd = clamp(texture(depthMap, vary_fragcoord.xy).r, 0.0, 1.0);
        // Non-linear near->far remap: far values push toward 1.
        haze = clamp(1.0 - zd, 0.0, 1.0);
    }

    // AYAR17 Color temperature (warm orange / cool blue tint)
    if (aya_temp != 0.0)
    {
        float t = clamp(aya_temp, -1.0, 1.0);
        vec3 warm = vec3(1.0, 0.9, 0.75);
        vec3 cool = vec3(0.8, 0.9, 1.05);
        vec3 tint = mix(cool, warm, (t + 1.0) * 0.5);
        diff.rgb *= mix(vec3(1.0), tint, abs(t));
    }

    // AYAR16 Aerial perspective: desaturate + push toward a hazy sky tint with distance
    if (aya_ap_strength > 0.0)
    {
        float a = clamp(aya_ap_strength, 0.0, 1.0) * haze;
        float lum = dot(diff.rgb, vec3(0.2126, 0.7152, 0.0722));
        vec3 desat = vec3(lum);
        vec3 sky = vec3(0.65, 0.75, 0.95);
        diff.rgb = mix(diff.rgb, mix(desat, sky, 0.5), a);
    }

    // AYAR14 Atmosphere/volume: warm golden haze that strengthens with distance
    if (aya_atmo > 0.0)
    {
        float a = clamp(aya_atmo, 0.0, 1.0) * haze;
        diff.rgb = mix(diff.rgb, vec3(0.95, 0.8, 0.55), a * 0.35);
    }

    // AYAR18 Cloud volumetric: subtle brightening variation from the film grain noise
#ifdef HAS_NOISE
    if (aya_cloud > 0.0)
    {
        float c = clamp(aya_cloud, 0.0, 1.0);
        float n = noise(vary_fragcoord.xy * 8.0);
        diff.rgb += vec3(c * (n - 0.5) * 0.15);
    }
#endif

#ifdef HAS_NOISE
    if (film_grain > 0.0)
    {
        vec2 tc = vary_fragcoord.xy * screen_res * 4.0;
        vec3 seed = (diff.rgb + vec3(1.0)) * vec3(tc.xy, tc.x + tc.y);
        vec3 nz = vec3(noise(seed.rg), noise(seed.gb), noise(seed.rb));
        diff.rgb += nz * film_grain;
    }
#endif

    diff.rgb = clampHDRRange(diff.rgb);
    frag_color = diff;

    gl_FragDepth = texture(depthMap, vary_fragcoord.xy).r;
}
