/**
 * @file volumetricLightF.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2007, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

// volumetric light scattering (god rays) - screen-space pass
// Reads ONLY the already-lit source image and additively composites radial
// light-shafts streaming toward the sun's screen position. It has no dependency
// on the G-buffer or shadow maps, so it can run safely as a post-process step.

// Inputs
in vec2 vary_fragcoord;

uniform sampler2D diffuseRect;

uniform vec2  sun_screen_pos;   // projected sun position in UV space [0..1]
uniform float sun_visible;      // 1.0 when the sun is in front of the camera

uniform int   vlight_steps;
uniform float vlight_multiplier;
uniform float vlight_falloff;

void main()
{
    vec2 uv = clamp(vary_fragcoord.xy, vec2(0.0), vec2(1.0));

    vec3 base = texture(diffuseRect, uv).rgb;

    vec3 rays = vec3(0.0);
    if (sun_visible > 0.5)
    {
        vec2 to_sun = sun_screen_pos - uv;
        float dist_to_sun = length(to_sun);

        if (dist_to_sun > 1e-4)
        {
            vec2 dir = to_sun / dist_to_sun;

            float max_steps = min(max(float(vlight_steps), 1.0), 64.0);
            float step = dist_to_sun / max_steps;

            vec3 acc = vec3(0.0);
            float w_sum = 0.0;

            for (int i = 1; i <= 64; i++)
            {
                if (float(i) > max_steps)
                {
                    break;
                }

                vec2 suv = uv + dir * (step * float(i));

                // stop marching once we leave the viewport
                if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0)
                {
                    break;
                }

                // sample the already-lit scene along the shaft toward the sun
                vec3 sc = texture(diffuseRect, suv).rgb;

                // weight samples closest to the sun (and thus to the streak) most
                float w = 1.0 - (float(i) / max_steps);
                float fw = pow(max(w, 0.0), vlight_falloff);

                acc += sc * fw;
                w_sum += fw;
            }

            if (w_sum > 0.0)
            {
                acc /= w_sum;
            }

            rays = acc * vlight_multiplier;
            rays = clamp(max(rays, vec3(0.0)), vec3(0.0), vec3(vlight_multiplier));
        }
    }

    frag_color = vec4(base + rays, 1.0);
}
