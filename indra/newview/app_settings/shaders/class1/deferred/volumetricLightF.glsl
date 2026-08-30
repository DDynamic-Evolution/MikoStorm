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

// volumetric light scattering (god rays)

// Inputs
in vec2 vary_fragcoord;

uniform vec3 sun_dir;
uniform vec3 moon_dir;
uniform vec3 sunlight_color;
uniform vec3 moonlight_color;
uniform int sun_up_factor;

uniform int vlight_steps;
uniform float vlight_multiplier;
uniform float vlight_falloff;

vec4 getPosition(vec2 pos_screen);
vec4 getNorm(vec2 pos_screen);

float sampleDirectionalShadow(vec3 pos, vec3 norm, vec2 pos_screen);

void main()
{
    vec2 pos_screen = vary_fragcoord.xy;
    vec4 pos        = getPosition(pos_screen);
    vec4 norm       = getNorm(pos_screen);

    vec3 light_dir = normalize((sun_up_factor == 1) ? sun_dir : moon_dir);
    vec3 light_col = (sun_up_factor == 1) ? sunlight_color : moonlight_color;

    // view ray from the camera toward this pixel's surface point (eye space, camera at origin)
    float dist     = max(length(pos.xyz), 0.01);
    vec3 view_dir  = pos.xyz / dist;

    // Henyey-Greenstein style phase function (forward scattering, peaks toward the sun)
    float cos_theta = clamp(dot(view_dir, light_dir), -1.0, 1.0);
    float phase     = 0.028648 / pow(1.64 - 1.6 * cos_theta, 1.5);

    float max_steps = min(max(vlight_steps, 1), 64);
    float step_size = dist / max_steps;

    vec3 scattering = vec3(0.0);

    for (int i = 0; i < 64; i++)
    {
        if (i >= vlight_steps)
        {
            break;
        }

        // sample the air between the camera and the surface
        float t = step_size * (float(i) + 1.0);
        vec3 sample_pos = view_dir * t;

        // weight samples near the surface (where silhouettes cast light shafts) most
        float w = 1.0 - (float(i) / max_steps);
        float falloff = pow(max(w, 0.0), vlight_falloff);

        // solar occlusion of the air volume at this sample
        float shadow = sampleDirectionalShadow(sample_pos, norm.xyz, pos_screen);

        scattering += light_col * (shadow * phase * falloff);
    }

    scattering *= (vlight_multiplier / max_steps);

    frag_color = vec4(max(scattering, vec3(0.0)), 0.0);
}