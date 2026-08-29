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
