/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of openfx-misc <https://github.com/devernay/openfx-misc>,
 * Copyright (C) 2016 INRIA
 *
 * openfx-misc is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * openfx-misc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with openfx-misc.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
 * ***** END LICENSE BLOCK ***** */

/*
 * OFX Shadertoy plugin.
 *
 * References:
 * https://www.shadertoy.com (v0.8.3 as of march 22, 2016)
 * http://www.iquilezles.org/apps/shadertoy/index2.html (original Shader Toy v0.4)
 *
 * TODO:
 * - add multipass support (using tabs for UI as in shadertoys)
 */

#if defined(OFX_SUPPORTS_OPENGLRENDER) || defined(HAVE_OSMESA) // at least one is required for this plugin

#include "Shadertoy.h"

#include <cfloat> // DBL_MAX
#include <cstddef>
#include <climits>
#include <string>
#include <fstream>
#include <streambuf>
#ifdef DEBUG
#include <iostream>
#endif

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#include <windows.h>
#endif

#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include "ofxOpenGLRender.h"
#include "ofxsCoords.h"
#include "ofxsFormatResolution.h"

using namespace OFX;

//OFXS_NAMESPACE_ANONYMOUS_ENTER // defines external classes
#define NBINPUTS SHADERTOY_NBINPUTS
#define NBUNIFORMS SHADERTOY_NBUNIFORMS

#define kPluginName "Shadertoy"
#define kPluginGrouping "Filter"
#define kPluginDescription \
    "Apply a Shadertoy fragment shader (multipass shaders and sound are not supported). See http://www.shadertoy.com\n" \
    "\n" \
    "This help only covers the parts of GLSL ES that are relevant for Shadertoy. " \
    "For the complete specification please have a look at GLSL ES specification " \
    "http://www.khronos.org/registry/gles/specs/2.0/GLSL_ES_Specification_1.0.17.pdf " \
    "or pages 3 and 4 of the OpenGL ES 2.0 quick reference card " \
    "https://www.khronos.org/opengles/sdk/docs/reference_cards/OpenGL-ES-2_0-Reference-card.pdf\n" \
    "A Shadertoy/GLSL tutorial can be found at https://www.shadertoy.com/view/Md23DV\n" \
    "\n" \
    "Language:\n" \
    "\n" \
    "    Preprocessor: # #define #undef #if #ifdef #ifndef #else #elif #endif #error #pragma #extension #version #line\n" \
/*"    Operators: () + - ! * / % < > <= >= == != && ||\n"*/ \
    "    Operators: usual GLSL/C/C++/Java operators\n" \
    "    Comments: // /* */\n" \
    "    Types: void bool int float vec2 vec3 vec4 bvec2 bvec3 bvec4 ivec2 ivec3 ivec4 mat2 mat3 mat4 sampler2D\n" \
    "    Function Parameter Qualifiers: [none], in, out, inout\n" \
    "    Global Variable Qualifiers: const\n" \
    "    Vector Components: .xyzw .rgba .stpq\n" \
    "    Flow Control: if else for return break continue\n" \
    "    Output: vec4 fragColor\n" \
    "    Input: vec2 fragCoord\n" \
    "\n" \
    "\n" \
    "Built-in Functions:\n" \
    "\n" \
    "    type radians (type degrees)\n" \
    "    type degrees (type radians)\n" \
    "    type sin (type angle)\n" \
    "    type cos (type angle)\n" \
    "    type tan (type angle)\n" \
    "    type asin (type x)\n" \
    "    type acos (type x)\n" \
    "    type atan (type y, type x)\n" \
    "    type atan (type y_over_x)\n" \
    "\n" \
    "\n" \
    "\n" \
    "    type pow (type x, type y)\n" \
    "    type exp (type x)\n" \
    "    type log (type x)\n" \
    "    type exp2 (type x)\n" \
    "    type log2 (type x)\n" \
    "    type sqrt (type x)\n" \
    "    type inversesqrt (type x)\n" \
    "\n" \
    "    type abs (type x)\n" \
    "    type sign (type x)\n" \
    "    type floor (type x)\n" \
    "    type ceil (type x)\n" \
    "    type fract (type x)\n" \
    "    type mod (type x, float y)\n" \
    "    type mod (type x, type y)\n" \
    "    type min (type x, type y)\n" \
    "    type min (type x, float y)\n" \
    "    type max (type x, type y)\n" \
    "    type max (type x, float y)\n" \
    "    type clamp (type x, type minV, type maxV)\n" \
    "    type clamp (type x, float minV, float maxV)\n" \
    "    type mix (type x, type y, type a)\n" \
    "    type mix (type x, type y, float a)\n" \
    "    type step (type edge, type x)\n" \
    "    type step (float edge, type x)\n" \
    "    type smoothstep (type a, type b, type x)\n" \
    "    type smoothstep (float a, float b, type x)\n" \
    "    mat matrixCompMult (mat x, mat y)\n" \
    "\n" \
    "	\n" \
    "\n" \
    "    float length (type x)\n" \
    "    float distance (type p0, type p1)\n" \
    "    float dot (type x, type y)\n" \
    "    vec3 cross (vec3 x, vec3 y)\n" \
    "    type normalize (type x)\n" \
    "    type faceforward (type N, type I, type Nref)\n" \
    "    type reflect (type I, type N)\n" \
    "    type refract (type I, type N,float eta)\n" \
    "\n" \
    "    bvec lessThan(vec x, vec y)\n" \
    "    bvec lessThan(ivec x, ivec y)\n" \
    "    bvec lessThanEqual(vec x, vec y)\n" \
    "    bvec lessThanEqual(ivec x, ivec y)\n" \
    "    bvec greaterThan(vec x, vec y)\n" \
    "    bvec greaterThan(ivec x, ivec y)\n" \
    "    bvec greaterThanEqual(vec x, vec y)\n" \
    "    bvec greaterThanEqual(ivec x, ivec y)\n" \
    "    bvec equal(vec x, vec y)\n" \
    "    bvec equal(ivec x, ivec y)\n" \
    "    bvec equal(bvec x, bvec y)\n" \
    "    bvec notEqual(vec x, vec y)\n" \
    "    bvec notEqual(ivec x, ivec y)\n" \
    "    bvec notEqual(bvec x, bvec y)\n" \
    "    bool any(bvec x)\n" \
    "    bool all(bvec x)\n" \
    "    bvec not(bvec x)\n" \
    "\n" \
    "	\n" \
    "\n" \
    "    vec4 texture2D(sampler2D sampler, vec2 coord )\n" \
    "    vec4 texture2D(sampler2D sampler, vec2 coord, float bias)\n" \
    "    vec4 textureCube(samplerCube sampler, vec3 coord)\n" \
    "    vec4 texture2DProj(sampler2D sampler, vec3 coord )\n" \
    "    vec4 texture2DProj(sampler2D sampler, vec3 coord, float bias)\n" \
    "    vec4 texture2DProj(sampler2D sampler, vec4 coord)\n" \
    "    vec4 texture2DProj(sampler2D sampler, vec4 coord, float bias)\n" \
    "    vec4 texture2DLodEXT(sampler2D sampler, vec2 coord, float lod)\n" \
    "    vec4 texture2DProjLodEXT(sampler2D sampler, vec3 coord, float lod)\n" \
    "    vec4 texture2DProjLodEXT(sampler2D sampler, vec4 coord, float lod)\n" \
    "    vec4 textureCubeLodEXT(samplerCube sampler, vec3 coord, float lod)\n" \
    "    vec4 texture2DGradEXT(sampler2D sampler, vec2 P, vec2 dPdx, vec2 dPdy)\n" \
    "    vec4 texture2DProjGradEXT(sampler2D sampler, vec3 P, vec2 dPdx, vec2 dPdy)\n" \
    "    vec4 texture2DProjGradEXT(sampler2D sampler, vec4 P, vec2 dPdx, vec2 dPdy)\n" \
    "    vec4 textureCubeGradEXT(samplerCube sampler, vec3 P, vec3 dPdx, vec3 dPdy)\n" \
    "\n" \
    "    type dFdx( type x ), dFdy( type x )\n" \
    "    type fwidth( type p )\n" \
    "\n" \
    "\n" \
    "How-to\n" \
    "\n" \
    "    Use structs: struct myDataType { float occlusion; vec3 color; }; myDataType myData = myDataType(0.7, vec3(1.0, 2.0, 3.0));\n" \
    "    Initialize arrays: arrays cannot be initialized in WebGL.\n" \
    "    Do conversions: int a = 3; float b = float(a);\n" \
    "    Do component swizzling: vec4 a = vec4(1.0,2.0,3.0,4.0); vec4 b = a.zyyw;\n" \
    "    Access matrix components: mat4 m; m[1] = vec4(2.0); m[0][0] = 1.0; m[2][3] = 2.0;\n" \
    "\n" \
    "\n" \
    "Be careful!\n" \
    "\n" \
    "    the f suffix for floating pont numbers: 1.0f is illegal in GLSL. You must use 1.0\n" \
    "    saturate(): saturate(x) doesn't exist in GLSL. Use clamp(x,0.0,1.0) instead\n" \
    "    pow/sqrt: please don't feed sqrt() and pow() with negative numbers. Add an abs() or max(0.0,) to the argument\n" \
    "    mod: please don't do mod(x,0.0). This is undefined in some platforms\n" \
    "    variables: initialize your variables! Don't assume they'll be set to zero by default\n" \
    "    functions: don't call your functions the same as some of your variables\n" \
    "\n" \
    "\n" \
    "Shadertoy Inputs\n" \
    "vec3	iResolution	image	The viewport resolution (z is pixel aspect ratio, usually 1.0)\n" \
    "float	iGlobalTime	image/sound	Current time in seconds\n" \
    "float	iTimeDelta	image	Time it takes to render a frame, in seconds\n" \
    "int	iFrame	image	Current frame\n" \
    "float	iFrameRate	image	Number of frames rendered per second\n" \
    "float	iChannelTime["STRINGISE(NBINPUTS)"]	image	Time for channel (if video or sound), in seconds\n" \
    "vec3	iChannelResolution["STRINGISE(NBINPUTS)"]	image/sound	Input texture resolution for each channel\n" \
    "vec4	iMouse	image	xy = current pixel coords (if LMB is down). zw = click pixel\n" \
    "sampler2D	iChannel{i}	image/sound	Sampler for input textures i\n" \
    "vec4	iDate	image/sound	Year, month, day, time in seconds in .xyzw\n" \
    "float	iSampleRate	image/sound	The sound sample rate (typically 44100)\n" \
    "vec2	iRenderScale	image	The OpenFX render scale (e.g. 0.5,0.5 when rendering half-size) [OFX plugin only]\n" \
    "\n" \
    "Shadertoy Outputs\n" \
    "For image shaders, fragColor is used as output channel. It is not, for now, mandatory but recommended to leave the alpha channel to 1.0.\n" \
    "\n" \
    "For sound shaders, the mainSound() function returns a vec2 containing the left and right (stereo) sound channel wave data.\n" \
    "\n" \
    "OpenFX extensions to Shadertoy\n" \
    "\n" \
    "* The pre-defined `iRenderScale` uniform contains the current render scale. Basically all pixel sizes must be multiplied by the renderscale to get a scale-independent effect. For compatibility with Shadertoy, the first line that starts with `const vec2 iRenderScale` is ignored (the full line should be `const vec2 iRenderScale = vec2(1.,1.);`).\n" \
    "* The shader may define additional uniforms, which should have a default value, as in `uniform vec2 blurSize = (5., 5.);`.\n" \
    "  These uniforms can be made available as OpenFX parameters using settings in the 'Extra parameters' group, which can be set automatically using the 'Auto. Params' button (in this case, parameters are updated when the image is rendered).\n" \
    "  A parameter label and help string can be given in the comment on the same line. The help string must be in parenthesis.\n" \
    "  `uniform vec2 blurSize = (5., 5.); // Blur Size (The blur size in pixels.)`\n" \
    "  min/max values can also be given after a comma. The strings must be exactly `min=` and `max=`, without additional spaces, separated by a comma, and the values must have the same dimension as the uniform:\n" \
    "  `uniform vec2 blurSize = (5., 5.); // Blur Size (The blur size in pixels.), min=(0.,0.), max=(1000.,1000.)`\n" \
    "* The following comment line placed in the shader gives a label and help string to input 1 (the comment must be the only thing on the line):\n" \
    "  `// iChannel1: Noise (A noise texture to be used for random number calculations. The texture should not be frame-varying.)`\n" \
    "* This one also sets the filter and wrap parameters:\n" \
    "  `// iChannel0: Source (Source image.), filter=linear, wrap=clamp`\n" \
    "* And this one sets the output bouding box (possible values are Default, Union, Interection, and iChannel0 to iChannel3):\n" \
    "  `// BBox: iChannel0`"


#define kPluginDescriptionMarkdown \
    "Apply a [Shadertoy](http://www.shadertoy.com) fragment shader (multipass shaders and sound are not supported).\n" \
    "\n" \
    "This help only covers the parts of GLSL ES that are relevant for Shadertoy. For the complete specification please have a look at [GLSL ES specification](http://www.khronos.org/registry/gles/specs/2.0/GLSL_ES_Specification_1.0.17.pdf)  or pages 3 and 4 of the [OpenGL ES 2.0 quick reference card](https://www.khronos.org/opengles/sdk/docs/reference_cards/OpenGL-ES-2_0-Reference-card.pdf).\n" \
    "See also the [Shadertoy/GLSL tutorial](https://www.shadertoy.com/view/Md23DV).\n" \
    "\n" \
    "### Language:\n" \
    "\n" \
    "* __Preprocessor:__ `#` `#define` `#undef` `#if` `#ifdef` `#ifndef` `#else` `#elif` `#endif` `#error` `#pragma` `#extension` `#version` `#line`\n" \
    "* __Operators:__ `()` `+` `-` `!` `*` `/` `%` `<` `>` `<=` `>=` `==` `!=` `&&` `||`\n" \
    "* __Comments:__ `//` `/*` `*/`\n" \
    "* __Types:__ void bool int float vec2 vec3 vec4 bvec2 bvec3 bvec4 ivec2 ivec3 ivec4 mat2 mat3 mat4 sampler2D\n" \
    "* __Function Parameter Qualifiers:__ ~~none~~, in, out, inout\n" \
    "* __Global Variable Qualifiers:__ const\n" \
    "* __Vector Components:__ .xyzw .rgba .stpq\n" \
    "* __Flow Control:__ if else for return break continue\n" \
    "* __Output:__ vec4 fragColor\n" \
    "* __Input:__ vec2 fragCoord\n" \
    "\n" \
    "\n" \
    "### Built-in Functions:\n" \
    "\n" \
    "* type radians (type degrees)\n" \
    "* type degrees (type radians)\n" \
    "* type sin (type angle)\n" \
    "* type cos (type angle)\n" \
    "* type tan (type angle)\n" \
    "* type asin (type x)\n" \
    "* type acos (type x)\n" \
    "* type atan (type y, type x)\n" \
    "* type atan (type y_over_x)\n" \
    "\n" \
    "	\n" \
    "\n" \
    "* type pow (type x, type y)\n" \
    "* type exp (type x)\n" \
    "* type log (type x)\n" \
    "* type exp2 (type x)\n" \
    "* type log2 (type x)\n" \
    "* type sqrt (type x)\n" \
    "* type inversesqrt (type x)\n" \
    "\n" \
    "* type abs (type x)\n" \
    "* type sign (type x)\n" \
    "* type floor (type x)\n" \
    "* type ceil (type x)\n" \
    "* type fract (type x)\n" \
    "* type mod (type x, float y)\n" \
    "* type mod (type x, type y)\n" \
    "* type min (type x, type y)\n" \
    "* type min (type x, float y)\n" \
    "* type max (type x, type y)\n" \
    "* type max (type x, float y)\n" \
    "* type clamp (type x, type minV, type maxV)\n" \
    "* type clamp (type x, float minV, float maxV)\n" \
    "* type mix (type x, type y, type a)\n" \
    "* type mix (type x, type y, float a)\n" \
    "* type step (type edge, type x)\n" \
    "* type step (float edge, type x)\n" \
    "* type smoothstep (type a, type b, type x)\n" \
    "* type smoothstep (float a, float b, type x)\n" \
    "* mat matrixCompMult (mat x, mat y)\n" \
    "\n" \
    "	\n" \
    "\n" \
    "* float length (type x)\n" \
    "* float distance (type p0, type p1)\n" \
    "* float dot (type x, type y)\n" \
    "* vec3 cross (vec3 x, vec3 y)\n" \
    "* type normalize (type x)\n" \
    "* type faceforward (type N, type I, type Nref)\n" \
    "* type reflect (type I, type N)\n" \
    "* type refract (type I, type N,float eta)\n" \
    "\n" \
    "* bvec lessThan(vec x, vec y)\n" \
    "* bvec lessThan(ivec x, ivec y)\n" \
    "* bvec lessThanEqual(vec x, vec y)\n" \
    "* bvec lessThanEqual(ivec x, ivec y)\n" \
    "* bvec greaterThan(vec x, vec y)\n" \
    "* bvec greaterThan(ivec x, ivec y)\n" \
    "* bvec greaterThanEqual(vec x, vec y)\n" \
    "* bvec greaterThanEqual(ivec x, ivec y)\n" \
    "* bvec equal(vec x, vec y)\n" \
    "* bvec equal(ivec x, ivec y)\n" \
    "* bvec equal(bvec x, bvec y)\n" \
    "* bvec notEqual(vec x, vec y)\n" \
    "* bvec notEqual(ivec x, ivec y)\n" \
    "* bvec notEqual(bvec x, bvec y)\n" \
    "* bool any(bvec x)\n" \
    "* bool all(bvec x)\n" \
    "* bvec not(bvec x)\n" \
    "\n" \
    "	\n" \
    "\n" \
    "* vec4 texture2D(sampler2D sampler, vec2 coord )\n" \
    "* vec4 texture2D(sampler2D sampler, vec2 coord, float bias)\n" \
    "* vec4 textureCube(samplerCube sampler, vec3 coord)\n" \
    "* vec4 texture2DProj(sampler2D sampler, vec3 coord )\n" \
    "* vec4 texture2DProj(sampler2D sampler, vec3 coord, float bias)\n" \
    "* vec4 texture2DProj(sampler2D sampler, vec4 coord)\n" \
    "* vec4 texture2DProj(sampler2D sampler, vec4 coord, float bias)\n" \
    "* vec4 texture2DLodEXT(sampler2D sampler, vec2 coord, float lod)\n" \
    "* vec4 texture2DProjLodEXT(sampler2D sampler, vec3 coord, float lod)\n" \
    "* vec4 texture2DProjLodEXT(sampler2D sampler, vec4 coord, float lod)\n" \
    "* vec4 textureCubeLodEXT(samplerCube sampler, vec3 coord, float lod)\n" \
    "* vec4 texture2DGradEXT(sampler2D sampler, vec2 P, vec2 dPdx, vec2 dPdy)\n" \
    "* vec4 texture2DProjGradEXT(sampler2D sampler, vec3 P, vec2 dPdx, vec2 dPdy)\n" \
    "* vec4 texture2DProjGradEXT(sampler2D sampler, vec4 P, vec2 dPdx, vec2 dPdy)\n" \
    "* vec4 textureCubeGradEXT(samplerCube sampler, vec3 P, vec3 dPdx, vec3 dPdy)\n" \
    "\n" \
    "* type dFdx( type x ), dFdy( type x )\n" \
    "* type fwidth( type p )\n" \
    "\n" \
    "\n" \
    "### How-to\n" \
    "\n" \
    "* __Use structs:__ `struct myDataType { float occlusion; vec3 color; }; myDataType myData = myDataType(0.7, vec3(1.0, 2.0, 3.0));`\n" \
    "* __Initialize arrays:__ arrays cannot be initialized in WebGL.\n" \
    "* __Do conversions:__ `int a = 3; float b = float(a);`\n" \
    "* __Do component swizzling:__ `vec4 a = vec4(1.0,2.0,3.0,4.0); vec4 b = a.zyyw;`\n" \
    "* __Access matrix components:__ `mat4 m; m[1] = vec4(2.0); m[0][0] = 1.0; m[2][3] = 2.0;`\n" \
    "\n" \
    "\n" \
    "### Be careful!\n" \
    "\n" \
    "* __the f suffix for floating point numbers:__ 1.0f is illegal in GLSL. You must use 1.0\n" \
    "* __saturate():__ saturate(x) doesn't exist in GLSL. Use clamp(x,0.0,1.0) instead\n" \
    "* __pow/sqrt:__ please don't feed sqrt() and pow() with negative numbers. Add an abs() or max(0.0,) to the argument\n" \
    "* __mod:__ please don't do mod(x,0.0). This is undefined in some platforms\n" \
    "* __variables:__ initialize your variables! Don't assume they'll be set to zero by default\n" \
    "* __functions:__ don't call your functions the same as some of your variables\n" \
    "\n" \
    "\n" \
    "### Shadertoy Inputs\n\n" \
    "Type | Name | Function | Description\n" \
    "--- | --- | --- | ---\n" \
    "vec3 | iResolution | image | The viewport resolution (z is pixel aspect ratio, usually 1.0)\n" \
    "float | iGlobalTime | image/sound | Current time in seconds\n" \
    "float | iTimeDelta | image | Time it takes to render a frame, in seconds\n" \
    "int | iFrame | image | Current frame\n" \
    "float | iFrameRate | image | Number of frames rendered per second\n" \
    "float | iChannelTime["STRINGISE(NBINPUTS)"] | image | Time for channel (if video or sound), in seconds\n" \
    "vec3 | iChannelResolution["STRINGISE(NBINPUTS)"] | image/sound | Input texture resolution for each channel\n" \
    "vec4 | iMouse | image | xy = current pixel coords (if LMB is down). zw = click pixel\n" \
    "sampler2D | iChannel{i} | image/sound | Sampler for input textures i\n" \
    "vec4 | iDate | image/sound | Year, month, day, time in seconds in .xyzw\n" \
    "float | iSampleRate | image/sound | The sound sample rate (typically 44100)\n" \
    "vec2 | iRenderScale | image | The OpenFX render scale (e.g. 0.5,0.5 when rendering half-size) [OFX plugin only]\n" \
    "\n" \
    "### Shadertoy Outputs\n" \
    "For image shaders, fragColor is used as output channel. It is not, for now, mandatory but recommended to leave the alpha channel to 1.0.\n" \
    "\n" \
    "For sound shaders, the mainSound() function returns a vec2 containing the left and right (stereo) sound channel wave data.\n" \
    "\n" \
    "### OpenFX extensions to Shadertoy\n" \
    "\n" \
    "* The pre-defined `iRenderScale` uniform contains the current render scale. Basically all pixel sizes must be multiplied by the renderscale to get a scale-independent effect. For compatibility with Shadertoy, the first line that starts with `const vec2 iRenderScale` is ignored (the full line should be `const vec2 iRenderScale = vec2(1.,1.);`).\n" \
    "* The shader may define additional uniforms, which should have a default value, as in `uniform vec2 blurSize = (5., 5.);`.\n" \
    "  These uniforms can be made available as OpenFX parameters using settings in the 'Extra parameters' group, which can be set automatically using the 'Auto. Params' button (in this case, parameters are updated when the image is rendered).\n" \
    "  A parameter label and help string can be given in the comment on the same line. The help string must be in parenthesis.\n" \
    "  `uniform vec2 blurSize = (5., 5.); // Blur Size (The blur size in pixels.)`\n" \
    "  min/max values can also be given after a comma. The strings must be exactly `min=` and `max=`, without additional spaces, separated by a comma, and the values must have the same dimension as the uniform:\n" \
    "  `uniform vec2 blurSize = (5., 5.); // Blur Size (The blur size in pixels.), min=(0.,0.), max=(1000.,1000.)`\n" \
    "* The following comment line placed in the shader gives a label and help string to input 1 (the comment must be the only thing on the line):\n" \
    "  `// iChannel1: Noise (A noise texture to be used for random number calculations. The texture should not be frame-varying.)`\n" \
    "* This one also sets the filter and wrap parameters:\n" \
    "  `// iChannel0: Source (Source image.), filter=linear, wrap=clamp`\n" \
    "* And this one sets the output bouding box (possible values are Default, Union, Interection, and iChannel0 to iChannel3):\n" \
    "  `// BBox: iChannel0`"

#define kPluginIdentifier "net.sf.openfx.Shadertoy"
#define kPluginVersionMajor 1 // Incrementing this number means that you have broken backwards compatibility of the plug-in.
#define kPluginVersionMinor 0 // Increment this when you have fixed a bug or made it faster.

#define kSupportsTiles 1
#define kSupportsMultiResolution 1
#define kSupportsRenderScale 1
#define kSupportsMultipleClipPARs false
#define kSupportsMultipleClipDepths false


#define kShaderInputsHint \
    "Shader Inputs:\n" \
    "uniform vec3      iResolution;           // viewport resolution (in pixels)\n" \
    "uniform float     iGlobalTime;           // shader playback time (in seconds)\n" \
    "uniform float     iTimeDelta;            // render time (in seconds)\n" \
    "uniform int       iFrame;                // shader playback frame\n" \
    "uniform float     iChannelTime["STRINGISE (NBINPUTS)"];       // channel playback time (in seconds)\n" \
    "uniform vec3      iChannelResolution["STRINGISE (NBINPUTS)"]; // channel resolution (in pixels)\n" \
    "uniform vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click\n" \
    "uniform samplerXX iChannel0..3;          // input channel. XX = 2D/Cube\n" \
    "uniform vec4      iDate;                 // (year, month, day, time in seconds)\n" \
    "uniform float     iSampleRate;           // sound sample rate (i.e., 44100)\n" \
    ""
#define kParamBBox "bbox"
#define kParamBBoxLabel "Output Bounding Box"
#define kParamBBoxHint "What to use to produce the output image's bounding box. If no selected input is connected, use the project size."
#define kParamBBoxOptionDefault "Default"
#define kParamBBoxOptionDefaultHint "Default bounding box (project size)."
#define kParamBBoxOptionFormat "Format"
#define kParamBBoxOptionFormatHint "Use a pre-defined image format."
//#define kParamBBoxOptionSize "Size"
//#define kParamBBoxOptionSizeHint "Use a specific extent (size and offset)."
#define kParamBBoxOptionUnion "Union"
#define kParamBBoxOptionUnionHint "Union of all connected inputs."
#define kParamBBoxOptionIntersection "Intersect"
#define kParamBBoxOptionIntersectionHint "Intersection of all connected inputs."
#define kParamBBoxOptionIChannel "iChannel"
#define kParamBBoxOptionIChannelHint "Bounding box of iChannel"

#define kParamFormat kNatronParamFormatChoice
#define kParamFormatLabel "Format"
#define kParamFormatHint "The output format."

#define kParamFormatSize kNatronParamFormatSize
#define kParamFormatSizeLabel "Size"
#define kParamFormatSizeHint "The output dimensions of the image in pixels."

#define kParamFormatPAR kNatronParamFormatPar
#define kParamFormatPARLabel "Pixel Aspect Ratio"
#define kParamFormatPARHint "Output pixel aspect ratio."

#define kGroupImageShader "imageShaderGroup"
#define kGroupImageShaderLabel "Image Shader"

#define kParamImageShaderFileName "imageShaderFileName"
#define kParamImageShaderFileNameLabel "Load from File"
#define kParamImageShaderFileNameHint "Load the source from the given file. The file contents is only loaded once. Press the \"Reload\" button to load again the same file."

#define kParamImageShaderReload "imageShaderReload"
#define kParamImageShaderReloadLabel "Reload"
#define kParamImageShaderReloadHint "Reload the source from the given file."

#define kParamImageShaderSource "imageShaderSource"
#define kParamImageShaderSourceLabel "Source"
#define kParamImageShaderSourceHint "Image shader.\n\n"kShaderInputsHint

#define kParamImageShaderCompile "imageShaderCompile"
#define kParamImageShaderCompileLabel "Compile"
#define kParamImageShaderCompileHint "Compile the image shader."

// parameter to trigger a new render and make sure the shader is compiled
#define kParamImageShaderTriggerRender "imageShaderTriggerRender"

// parameter used to trigger an InstanceChanged once the Shader was compiled in the render function and parameters were updated
#define kParamImageShaderParamsUpdated "imageShaderParamsUpdated"

#define kParamAuto "autoParams"
#define kParamAutoLabel "Auto. Params"
#define kParamAutoHint "Automatically set the parameters from the shader source next time image is rendered. May require clicking twice, depending on the OpenFX host."

#define kParamResetParams "resetParams"
#define kParamResetParamsLabel "Reset Params Values"
#define kParamResetParamsHint "Set the extra parameters to their default values, as set automatically by the \"Auto. Params\", or in the \"Extra Parameters\" group."


#define kParamImageShaderDefault                            \
    "void mainImage( out vec4 fragColor, in vec2 fragCoord )\n" \
    "{\n"                                                       \
    "    vec2 uv = fragCoord.xy / iResolution.xy;\n"            \
    "    fragColor = vec4(uv,0.5+0.5*sin(iGlobalTime),1.0);\n"  \
    "}"

// mouse parameters, see https://www.shadertoy.com/view/Mss3zH
#define kParamMouseParams "mouseParams"
#define kParamMouseParamsLabel "Mouse Params."
#define kParamMouseParamsHint "Enable mouse parameters."

#define kParamMousePosition "mousePosition"
#define kParamMousePositionLabel "Mouse Pos."
#define kParamMousePositionHint "Mouse position, in pixels. Gets mapped to the xy components of the iMouse input."

#define kParamMouseClick "mouseClick"
#define kParamMouseClickLabel "Click Pos."
#define kParamMouseClickHint "Mouse click position, in pixels. The zw components of the iMouse input contain mouseClick if mousePressed is checked, else -mouseClick."

#define kParamMousePressed "mousePressed"
#define kParamMousePressedLabel "Mouse Pressed"
#define kParamMousePressedHint "When checked, the zw components of the iMouse input contain mouseClick, else they contain -mouseClick. If the host does not support animating this parameter, use negative values for mouseClick to emulate a released mouse button."

#define kGroupExtraParameters "extraParametersGroup"
#define kGroupExtraParametersLabel "Extra Parameters"
#define kGroupExtraParametersHint "Description of extra parameters (a.k.a. uniforms) used by the shader source. Note that these parameters must be explicitely declared as uniforms in the shader (to keep compatibility with shadertoy, they may also have a default value set in the shader source)."

#define kGroupParameter "extraParameterGroup"
#define kGroupParameterLabel "Param "

#define kParamCount "paramCount"
#define kParamCountLabel "No. of Params"
#define kParamCountHint "Number of extra parameters."

#define kParamType "paramType" // followed by param number
#define kParamTypeLabel "Type"
#define kParamTypeHint "Type of the parameter."
#define kParamTypeOptionNone "none"
#define kParamTypeOptionBool "bool"
#define kParamTypeOptionInt "int"
#define kParamTypeOptionFloat "float"
#define kParamTypeOptionVec2 "vec2"
#define kParamTypeOptionVec3 "vec3"
#define kParamTypeOptionVec4 "vec4"

#define kParamName "paramName" // followed by param number
#define kParamNameLabel "Name"
#define kParamNameHint "Name of the parameter, as used in the shader."

#define kParamLabel "paramLabel" // followed by param number
#define kParamLabelLabel "Label"
#define kParamLabelHint "Label of the parameter, as displayed in the user interface."

#define kParamHint "paramHint" // followed by param number
#define kParamHintLabel "Hint"
#define kParamHintHint "Help for the parameter."

#define kParamValue "paramValue"
#define kParamValueBool kParamValue"Bool" // followed by param number
#define kParamValueInt kParamValue"Int" // followed by param number
#define kParamValueFloat kParamValue"Float" // followed by param number
#define kParamValueVec2 kParamValue"Vec2" // followed by param number
#define kParamValueVec3 kParamValue"Vec3" // followed by param number
#define kParamValueVec4 kParamValue"Vec4" // followed by param number
#define kParamValueLabel "Value" // followed by param number
#define kParamValueHint "Value of the parameter."

#define kParamDefault "paramDefault"
#define kParamDefaultBool kParamDefault"Bool" // followed by param number
#define kParamDefaultInt kParamDefault"Int" // followed by param number
#define kParamDefaultFloat kParamDefault"Float" // followed by param number
#define kParamDefaultVec2 kParamDefault"Vec2" // followed by param number
#define kParamDefaultVec3 kParamDefault"Vec3" // followed by param number
#define kParamDefaultVec4 kParamDefault"Vec4" // followed by param number
#define kParamDefaultLabel "Default" // followed by param number
#define kParamDefaultHint "Default value of the parameter."

#define kParamMin "paramMin"
#define kParamMinInt kParamMin"Int" // followed by param number
#define kParamMinFloat kParamMin"Float" // followed by param number
#define kParamMinVec2 kParamMin"Vec2" // followed by param number
#define kParamMinLabel "Min" // followed by param number
#define kParamMinHint "Min value of the parameter."

#define kParamMax "paramMax"
#define kParamMaxInt kParamMax"Int" // followed by param number
#define kParamMaxFloat kParamMax"Float" // followed by param number
#define kParamMaxVec2 kParamMax"Vec2" // followed by param number
#define kParamMaxLabel "Max" // followed by param number
#define kParamMaxHint "Max value of the parameter."

#define kParamInputFilter "mipmap"
#define kParamInputFilterLabel "Filter"
#define kParamInputFilterHint "Texture filter for this input."
#define kParamInputFilterOptionNearest "Nearest"
#define kParamInputFilterOptionNearestHint "MIN/MAG = GL_NEAREST/GL_NEAREST"
#define kParamInputFilterOptionLinear "Linear"
#define kParamInputFilterOptionLinearHint "MIN/MAG = GL_LINEAR/GL_LINEAR"
#define kParamInputFilterOptionMipmap "Mipmap"
#define kParamInputFilterOptionMipmapHint "MIN/MAG = GL_LINEAR_MIPMAP_LINEAR/GL_LINEAR"
#define kParamInputFilterOptionAnisotropic "Anisotropic"
#define kParamInputFilterOptionAnisotropicHint "Mipmap with anisotropic filtering. Available with GPU if supported (check for the presence of the GL_EXT_texture_filter_anisotropic extension in the Renderer Info) and with \"softpipe\" CPU driver."

#define kParamInputWrap "wrap"
#define kParamInputWrapLabel "Wrap"
#define kParamInputWrapHint "Texture wrap parameter for this input."
#define kParamInputWrapOptionRepeat "Repeat"
#define kParamInputWrapOptionRepeatHint "WRAP_S/T = GL_REPEAT"
#define kParamInputWrapOptionClamp "Clamp"
#define kParamInputWrapOptionClampHint "WRAP_S/T = GL_CLAMP_TO_EDGE"
#define kParamInputWrapOptionMirror "Mirror"
#define kParamInputWrapOptionMirrorHint "WRAP_S/T = GL_MIRRORED_REPEAT"

#define kParamInputName "inputName" // name for the label for each input

#define kParamInputEnable "inputEnable"
#define kParamInputEnableLabel "Enable"
#define kParamInputEnableHint "Enable this input."

#define kParamInputLabel "inputLabel"
#define kParamInputLabelLabel "Label"
#define kParamInputLabelHint "Label for this input in the user interface."

#define kParamInputHint "inputHint"
#define kParamInputHintLabel "Hint"
#define kParamInputHintHint "Help for this input."

#if defined(OFX_SUPPORTS_OPENGLRENDER) && defined(HAVE_OSMESA)
#define kParamEnableGPU "enableGPU"
#define kParamEnableGPULabel "Enable GPU Render"
#define kParamEnableGPUHint \
    "Enable GPU-based OpenGL render.\n" \
    "If the checkbox is checked but is not enabled (i.e. it cannot be unchecked), GPU render can not be enabled or disabled from the plugin and is probably part of the host options.\n" \
    "If the checkbox is not checked and is not enabled (i.e. it cannot be checked), GPU render is not available on this host.\n"
#endif

#ifdef HAVE_OSMESA
#define kParamCPUDriver "cpuDriver"
#define kParamCPUDriverLabel "CPU Driver"
#define kParamCPUDriverHint "Driver for CPU rendering. May be \"softpipe\" (slower, has GL_EXT_texture_filter_anisotropic GL_ARB_texture_query_lod GL_ARB_pipeline_statistics_query) or \"llvmpipe\" (faster, has GL_ARB_buffer_storage GL_EXT_polygon_offset_clamp)."
#define kParamCPUDriverOptionSoftPipe "softpipe"
#define kParamCPUDriverOptionLLVMPipe "llvmpipe"
#define kParamCPUDriverDefault ShadertoyPlugin::eCPUDriverLLVMPipe
#endif

#define kParamRendererInfo "rendererInfo"
#define kParamRendererInfoLabel "Renderer Info..."
#define kParamRendererInfoHint "Retrieve information about the current OpenGL renderer."

#define kClipChannel "iChannel"

static
std::string
unsignedToString(unsigned i)
{
    if (i == 0) {
        return "0";
    }
    std::string nb;
    for (unsigned j = i; j != 0; j /= 10) {
        nb += ( '0' + (j % 10) );
    }

    return nb;
}

////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */

ShadertoyPlugin::ShadertoyPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
    , _dstClip(0)
    , _srcClips(NBINPUTS, (OFX::Clip*)        NULL)
    , _inputEnable  (NBINPUTS, (OFX::BooleanParam*) NULL)
    , _inputLabel   (NBINPUTS, (OFX::StringParam*) NULL)
    , _inputHint    (NBINPUTS, (OFX::StringParam*) NULL)
    , _inputFilter  (NBINPUTS, (OFX::ChoiceParam*) NULL)
    , _inputWrap    (NBINPUTS, (OFX::ChoiceParam*) NULL)
    , _bbox(0)
    , _format(0)
    , _formatSize(0)
    , _formatPar(0)
    , _imageShaderFileName(0)
    , _imageShaderSource(0)
    , _imageShaderCompile(0)
    , _imageShaderTriggerRender(0)
    , _imageShaderParamsUpdated(0)
    , _mouseParams(0)
    , _mousePosition(0)
    , _mouseClick(0)
    , _mousePressed(0)
    , _groupExtra(0)
    , _paramCount(0)
    , _paramGroup     (NBUNIFORMS, (OFX::GroupParam*)   NULL)
    , _paramType      (NBUNIFORMS, (OFX::ChoiceParam*)  NULL)
    , _paramName      (NBUNIFORMS, (OFX::StringParam*)  NULL)
    , _paramLabel     (NBUNIFORMS, (OFX::StringParam*)  NULL)
    , _paramHint      (NBUNIFORMS, (OFX::StringParam*)  NULL)
    , _paramValueBool (NBUNIFORMS, (OFX::BooleanParam*) NULL)
    , _paramValueInt  (NBUNIFORMS, (OFX::IntParam*)     NULL)
    , _paramValueFloat(NBUNIFORMS, (OFX::DoubleParam*)  NULL)
    , _paramValueVec2 (NBUNIFORMS, (Double2DParam*)     NULL)
    , _paramValueVec3 (NBUNIFORMS, (OFX::Double3DParam*)NULL)
    , _paramValueVec4 (NBUNIFORMS, (OFX::RGBAParam*)    NULL)
    , _paramDefaultBool (NBUNIFORMS, (OFX::BooleanParam*) NULL)
    , _paramDefaultInt  (NBUNIFORMS, (OFX::IntParam*)     NULL)
    , _paramDefaultFloat(NBUNIFORMS, (OFX::DoubleParam*)  NULL)
    , _paramDefaultVec2 (NBUNIFORMS, (Double2DParam*)     NULL)
    , _paramDefaultVec3 (NBUNIFORMS, (OFX::Double3DParam*)NULL)
    , _paramDefaultVec4 (NBUNIFORMS, (OFX::RGBAParam*)    NULL)
    , _paramMinInt  (NBUNIFORMS, (OFX::IntParam*)     NULL)
    , _paramMinFloat(NBUNIFORMS, (OFX::DoubleParam*)  NULL)
    , _paramMinVec2 (NBUNIFORMS, (Double2DParam*)     NULL)
    , _paramMaxInt  (NBUNIFORMS, (OFX::IntParam*)     NULL)
    , _paramMaxFloat(NBUNIFORMS, (OFX::DoubleParam*)  NULL)
    , _paramMaxVec2 (NBUNIFORMS, (Double2DParam*)     NULL)
    , _enableGPU(0)
    , _cpuDriver(0)
    , _imageShaderID(1)
    , _imageShaderUniformsID(1)
    , _imageShaderUpdateParams(false)
    , _imageShaderExtraParameters()
    , _imageShaderHasMouse(false)
    , _imageShaderInputEnabled(NBINPUTS)
    , _imageShaderInputLabel(NBINPUTS)
    , _imageShaderInputHint(NBINPUTS)
    , _imageShaderInputFilter(NBINPUTS, eFilterMipmap)
    , _imageShaderInputWrap(NBINPUTS, eWrapRepeat)
    , _imageShaderBBox(eBBoxDefault)
    , _imageShaderCompiled(false)
    , _openGLContextData()
    , _openGLContextAttached(false)
{
    try {
        _imageShaderMutex.reset(new Mutex);
        _rendererInfoMutex.reset(new Mutex);
#if defined(HAVE_OSMESA)
        _osmesaMutex.reset(new Mutex);
#endif
    } catch (const std::exception& e) {
#      ifdef DEBUG
        std::cout << "ERROR in createInstance(): OFX::Multithread::Mutex creation returned " << e.what() << std::endl;
#      endif
    }

    _dstClip = fetchClip(kOfxImageEffectOutputClipName);
    assert( _dstClip && (!_dstClip->isConnected() || _dstClip->getPixelComponents() == OFX::ePixelComponentRGBA ||
                         _dstClip->getPixelComponents() == OFX::ePixelComponentAlpha) );
    switch ( getContext() ) {
    case OFX::eContextFilter:
        _srcClips[0] = fetchClip(kOfxImageEffectSimpleSourceClipName);
        for (unsigned j = 1; j < NBINPUTS; ++j) {
            _srcClips[j] = fetchClip( std::string(kClipChannel) + unsignedToString(j) );
        }
        break;
    case OFX::eContextGenerator:
    case OFX::eContextGeneral:
    default:
        for (unsigned j = 0; j < NBINPUTS; ++j) {
            _srcClips[j] = fetchClip( std::string(kClipChannel) + unsignedToString(j) );
        }
        break;
    }
    for (unsigned i = 0; i < NBINPUTS; ++i) {
        assert( (!_srcClips[i] && getContext() == OFX::eContextGenerator) ||
                ( _srcClips[i] && (_srcClips[i]->getPixelComponents() == OFX::ePixelComponentRGBA ||
                                   _srcClips[i]->getPixelComponents() == OFX::ePixelComponentAlpha) ) );
        std::string nb = unsignedToString(i);
        _inputEnable[i] = fetchBooleanParam(kParamInputEnable + nb);
        _inputLabel[i] = fetchStringParam(kParamInputLabel + nb);
        _inputHint[i] = fetchStringParam(kParamInputHint + nb);
        _inputFilter[i] = fetchChoiceParam(kParamInputFilter + nb);
        _inputWrap[i] = fetchChoiceParam(kParamInputWrap + nb);
        assert(_inputEnable[i] && _inputLabel[i] && _inputHint[i] && _inputFilter[i] && _inputWrap[i]);
    }

    _bbox = fetchChoiceParam(kParamBBox);
    _format = fetchChoiceParam(kParamFormat);
    _formatSize = fetchInt2DParam(kParamFormatSize);
    _formatPar = fetchDoubleParam(kParamFormatPAR);
    assert(_bbox && _format && _formatSize && _formatPar);
    _imageShaderFileName = fetchStringParam(kParamImageShaderFileName);
    _imageShaderSource = fetchStringParam(kParamImageShaderSource);
    _imageShaderCompile = fetchPushButtonParam(kParamImageShaderCompile);
    _imageShaderTriggerRender = fetchIntParam(kParamImageShaderTriggerRender);
    _imageShaderParamsUpdated = fetchBooleanParam(kParamImageShaderParamsUpdated);
    assert(_imageShaderFileName && _imageShaderSource && _imageShaderCompile && _imageShaderTriggerRender && _imageShaderParamsUpdated);
    _mouseParams = fetchBooleanParam(kParamMouseParams);
    assert(_mouseParams);
    _mousePosition = fetchDouble2DParam(kParamMousePosition);
    _mouseClick = fetchDouble2DParam(kParamMouseClick);
    _mousePressed = fetchBooleanParam(kParamMousePressed);
    assert(_mousePosition && _mousePressed && _mouseClick);
    _groupExtra = fetchGroupParam(kGroupExtraParameters);
    _paramCount = fetchIntParam(kParamCount);
    assert(_groupExtra && _paramCount);
    for (unsigned i = 0; i < NBUNIFORMS; ++i) {
        // generate the number string
        std::string nb = unsignedToString(i);
        _paramGroup[i]      = fetchGroupParam   (kGroupParameter  + nb);
        _paramType[i]       = fetchChoiceParam  (kParamType       + nb);
        _paramName[i]       = fetchStringParam  (kParamName       + nb);
        _paramLabel[i]      = fetchStringParam  (kParamLabel      + nb);
        _paramHint[i]       = fetchStringParam  (kParamHint       + nb);
        _paramValueBool[i]  = fetchBooleanParam (kParamValueBool  + nb);
        _paramValueInt[i]   = fetchIntParam     (kParamValueInt   + nb);
        _paramValueFloat[i] = fetchDoubleParam  (kParamValueFloat + nb);
        _paramValueVec2[i]  = fetchDouble2DParam(kParamValueVec2  + nb);
        _paramValueVec3[i]  = fetchDouble3DParam(kParamValueVec3  + nb);
        _paramValueVec4[i]  = fetchRGBAParam    (kParamValueVec4  + nb);
        _paramDefaultBool[i]  = fetchBooleanParam (kParamDefaultBool  + nb);
        _paramDefaultInt[i]   = fetchIntParam     (kParamDefaultInt   + nb);
        _paramDefaultFloat[i] = fetchDoubleParam  (kParamDefaultFloat + nb);
        _paramDefaultVec2[i]  = fetchDouble2DParam(kParamDefaultVec2  + nb);
        _paramDefaultVec3[i]  = fetchDouble3DParam(kParamDefaultVec3  + nb);
        _paramDefaultVec4[i]  = fetchRGBAParam    (kParamDefaultVec4  + nb);
        _paramMinInt[i]   = fetchIntParam     (kParamMinInt   + nb);
        _paramMinFloat[i] = fetchDoubleParam  (kParamMinFloat + nb);
        _paramMinVec2[i]  = fetchDouble2DParam(kParamMinVec2  + nb);
        _paramMaxInt[i]   = fetchIntParam     (kParamMaxInt   + nb);
        _paramMaxFloat[i] = fetchDoubleParam  (kParamMaxFloat + nb);
        _paramMaxVec2[i]  = fetchDouble2DParam(kParamMaxVec2  + nb);
        assert(_paramGroup[i] && _paramType[i] && _paramName[i] && _paramLabel[i] && _paramHint[i] && _paramValueBool[i] && _paramValueInt[i] && _paramValueFloat[i] && _paramValueVec2[i] && _paramValueVec3[i] && _paramValueVec4[i]);
    }
#if defined(OFX_SUPPORTS_OPENGLRENDER) && defined(HAVE_OSMESA)
    _enableGPU = fetchBooleanParam(kParamEnableGPU);
    assert(_enableGPU);
    const OFX::ImageEffectHostDescription &gHostDescription = *OFX::getImageEffectHostDescription();
    if (!gHostDescription.supportsOpenGLRender) {
        _enableGPU->setEnabled(false);
        setSupportsOpenGLRender(false);
    } else {
        setSupportsOpenGLRender( _enableGPU->getValue() );
    }
#endif
    updateExtra();
    updateVisibility();
    updateClips();
    initOpenGL();
#if defined(HAVE_OSMESA)
    if ( OSMesaDriverSelectable() ) {
        _cpuDriver = fetchChoiceParam(kParamCPUDriver);
    }
    initMesa();
#endif
    _imageShaderCompile->setEnabled(false); // always compile on first render

    // Trigger a render, so that the shader is compiled and parameters are updated.
    // OpenFX allows this, see http://openfx.sourceforge.net/Documentation/1.4/ofxProgrammingReference.html#SettingParams
    // ... but also forbids this, see http://openfx.sourceforge.net/Documentation/1.4/ofxProgrammingReference.html#OfxParameterSuiteV1_paramSetValue
    // TODO: only do if necessary
    _imageShaderTriggerRender->setValue(_imageShaderTriggerRender->getValue() + 1);
}

ShadertoyPlugin::~ShadertoyPlugin()
{
    exitOpenGL();
#if defined(HAVE_OSMESA)
    exitMesa();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/** @brief render for the filter */

////////////////////////////////////////////////////////////////////////////////
// basic plugin render function, just a skelington to instantiate templates from

// the overridden render function
void
ShadertoyPlugin::render(const OFX::RenderArguments &args)
{
    if ( !kSupportsRenderScale && ( (args.renderScale.x != 1.) || (args.renderScale.y != 1.) ) ) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    for (unsigned i = 0; i < NBINPUTS; ++i) {
        assert( kSupportsMultipleClipPARs   || !_srcClips[i] || _srcClips[i]->getPixelAspectRatio() == _dstClip->getPixelAspectRatio() );
        assert( kSupportsMultipleClipDepths || !_srcClips[i] || _srcClips[i]->getPixelDepth()       == _dstClip->getPixelDepth() );
    }

    bool openGLRender = false;
#if defined(OFX_SUPPORTS_OPENGLRENDER)
    openGLRender = args.openGLEnabled;

    // do the rendering
    if (openGLRender) {
        return renderGL(args);
    }
#endif
#ifdef HAVE_OSMESA
    if (!openGLRender) {
        return renderMesa(args);
    }
#endif // HAVE_OSMESA
    OFX::throwSuiteStatusException(kOfxStatFailed);
}

// overriding getRegionOfDefinition is necessary to tell the host that we do not support render scale
bool
ShadertoyPlugin::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args,
                                       OfxRectD &rod)
{
    if ( !kSupportsRenderScale && ( (args.renderScale.x != 1.) || (args.renderScale.y != 1.) ) ) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }

    const double time = args.time;
    int bboxChoice = _bbox->getValueAtTime(time);
    if (bboxChoice == eBBoxDefault) {
        // use the default RoD
        return false;
    }
    if (bboxChoice == eBBoxFormat) {
        int w, h;
        _formatSize->getValueAtTime(time, w, h);
        double par = _formatPar->getValueAtTime(time);
        OfxRectI pixelFormat;
        pixelFormat.x1 = pixelFormat.y1 = 0;
        pixelFormat.x2 = w;
        pixelFormat.y2 = h;
        OfxPointD renderScale = {1., 1.};
        OFX::Coords::toCanonical(pixelFormat, renderScale, par, &rod);

        return true;
    }
    /*if (bboxChoice == eBBoxSize) {
        _size->getValueAtTime(time, rod.x2, rod.y2);
        _btmLeft->getValueAtTime(time, rod.x1, rod.y1);
        rod.x2 += rod.x1;
        rod.y2 += rod.y1;

        return true;
       }*/

    if (bboxChoice >= eBBoxIChannel) {
        unsigned i = bboxChoice - eBBoxIChannel;
        if ( _srcClips[i]->isConnected() ) {
            rod = _srcClips[i]->getRegionOfDefinition(time);

            return true;
        }

        // use the default RoD
        return false;
    }

    std::vector<OfxRectD> rods;
    for (unsigned i = 0; i < NBINPUTS; ++i) {
        if ( _srcClips[i]->isConnected() ) {
            rods.push_back( _srcClips[i]->getRegionOfDefinition(time) );
        }
    }
    if (rods.size() == 0) {
        return false;
    }
    rod = rods[0];
    if (bboxChoice == eBBoxUnion) { //union
        for (unsigned i = 1; i < rods.size(); ++i) {
            OFX::Coords::rectBoundingBox(rod, rods[i], &rod);
        }
    } else {  //intersection
        for (unsigned i = 1; i < rods.size(); ++i) {
            OFX::Coords::rectIntersection(rod, rods[i], &rod);
        }
        // may return an empty RoD if intersection is empty
    }

    return true;
} // ShadertoyPlugin::getRegionOfDefinition

void
ShadertoyPlugin::getRegionsOfInterest(const OFX::RegionsOfInterestArguments &args,
                                      OFX::RegionOfInterestSetter &rois)
{
    if ( !kSupportsRenderScale && ( (args.renderScale.x != 1.) || (args.renderScale.y != 1.) ) ) {
        OFX::throwSuiteStatusException(kOfxStatFailed);

        return;
    }

    // The effect requires full images to render any region
    for (unsigned i = 0; i < NBINPUTS; ++i) {
        OfxRectD srcRoI;

        if ( _srcClips[i] && _srcClips[i]->isConnected() ) {
            srcRoI = _srcClips[i]->getRegionOfDefinition(args.time);
            rois.setRegionOfInterest(*_srcClips[i], srcRoI);
        }
    }
}

void
ShadertoyPlugin::getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences)
{
    // We have to do this because the processing code does not support varying components for srcClip and dstClip
    // (The OFX spec doesn't state a default value for this)
    if (_srcClips[0]) {
        clipPreferences.setClipComponents( *_dstClip, _srcClips[0]->getPixelComponents() );
    }
    clipPreferences.setOutputFrameVarying(true);
    clipPreferences.setOutputHasContinousSamples(true);
}

static inline
bool
starts_with(const std::string &str,
            const std::string &prefix)
{
    return (str.substr( 0, prefix.size() ) == prefix);
}

void
ShadertoyPlugin::updateVisibility()
{
    BBoxEnum bbox = (BBoxEnum)_bbox->getValue();
    bool hasFormat = (bbox == eBBoxFormat);

    //bool hasSize = (bbox == eBBoxSize);

    _format->setEnabled(hasFormat);
    _format->setIsSecret(!hasFormat);
    //_size->setEnabled(hasSize);
    //_size->setIsSecret(!hasSize);
    //_recenter->setEnabled(hasSize);
    //_recenter->setIsSecret(!hasSize);
    //_btmLeft->setEnabled(hasSize);
    //_btmLeft->setIsSecret(!hasSize);

    bool mouseParams = _mouseParams->getValue();
    _mousePosition->setIsSecret(!mouseParams);
    _mouseClick->setIsSecret(!mouseParams);
    _mousePressed->setIsSecret(!mouseParams);

    unsigned paramCount = std::max( 0, std::min(_paramCount->getValue(), NBUNIFORMS) );
    for (unsigned i = 0; i < NBUNIFORMS; ++i) {
        updateVisibilityParam(i, i < paramCount);
    }
    for (unsigned i = 0; i < NBINPUTS; ++i) {
        bool enabled = _inputEnable[i]->getValue();
        //_srcClips[i]->setIsSecret(!enabled);
        _inputLabel[i]->setIsSecret(!enabled);
        _inputHint[i]->setIsSecret(!enabled);
        _inputFilter[i]->setIsSecret(!enabled);
        _inputWrap[i]->setIsSecret(!enabled);
    }

}

void
ShadertoyPlugin::updateClips()
{
    for (unsigned i = 0; i < NBINPUTS; ++i) {
        bool enabled = _inputEnable[i]->getValue();
        _srcClips[i]->setIsSecret(!enabled);
        std::string s;
        _inputLabel[i]->getValue(s);
        if ( s.empty() ) {
            std::string iChannelX(kClipChannel);
            iChannelX += unsignedToString(i);
            _srcClips[i]->setLabel(iChannelX);
        } else {
            _srcClips[i]->setLabel(s);
        }
        _inputHint[i]->getValue(s);
        _srcClips[i]->setHint(s);
    }
    
}

void
ShadertoyPlugin::updateVisibilityParam(unsigned i,
                                       bool visible)
{
    UniformTypeEnum paramType = (UniformTypeEnum)_paramType[i]->getValue();
    bool isBool = false;
    bool isInt = false;
    bool isFloat = false;
    bool isVec2 = false;
    bool isVec3 = false;
    bool isVec4 = false;
    std::string name;
    _paramName[i]->getValue(name);
    if (visible && !name.empty()) {
        switch (paramType) {
        case eUniformTypeNone: {
            break;
        }
        case eUniformTypeBool: {
            isBool = true;
            break;
        }
        case eUniformTypeInt: {
            isInt = true;
            break;
        }
        case eUniformTypeFloat: {
            isFloat = true;
            break;
        }
        case eUniformTypeVec2: {
            isVec2 = true;
            break;
        }
        case eUniformTypeVec3: {
            isVec3 = true;
            break;
        }
        case eUniformTypeVec4: {
            isVec4 = true;
            break;
        }
        default: {
            break;
        }
        }
    }

    // close the group if it becomes invisible
    if (!visible) {
        _paramGroup[i]->setOpen(false);
    }
    _paramGroup[i]->setIsSecret(!visible);
    _paramType[i]->setIsSecret(!visible);
    _paramName[i]->setIsSecret(!visible);
    _paramLabel[i]->setIsSecret(!visible || name.empty());
    _paramHint[i]->setIsSecret(!visible || name.empty());
    _paramValueBool[i]->setIsSecret(!isBool);
    _paramValueInt[i]->setIsSecret(!isInt);
    _paramValueFloat[i]->setIsSecret(!isFloat);
    _paramValueVec2[i]->setIsSecret(!isVec2);
    _paramValueVec3[i]->setIsSecret(!isVec3);
    _paramValueVec4[i]->setIsSecret(!isVec4);
    _paramDefaultBool[i]->setIsSecret(!isBool);
    _paramDefaultInt[i]->setIsSecret(!isInt);
    _paramDefaultFloat[i]->setIsSecret(!isFloat);
    _paramDefaultVec2[i]->setIsSecret(!isVec2);
    _paramDefaultVec3[i]->setIsSecret(!isVec3);
    _paramDefaultVec4[i]->setIsSecret(!isVec4);
    _paramMinInt[i]->setIsSecret(!isInt);
    _paramMinFloat[i]->setIsSecret(!isFloat);
    _paramMinVec2[i]->setIsSecret(!isVec2);
    //_paramMinVec3[i]->setIsSecret(!isVec3);
    //_paramMinVec4[i]->setIsSecret(!isVec4);
    _paramMaxInt[i]->setIsSecret(!isInt);
    _paramMaxFloat[i]->setIsSecret(!isFloat);
    _paramMaxVec2[i]->setIsSecret(!isVec2);
    //_paramMaxVec3[i]->setIsSecret(!isVec3);
    //_paramMaxVec4[i]->setIsSecret(!isVec4);
} // ShadertoyPlugin::updateVisibilityParam

// for each extra parameter that has a nonempty name and a type, set the label of its Value param to its name.
// If the label is not the default label, then it was set by the host before plugin creation, and we can assume it comes from a loaded project and there was a previous shader compilation => no need to trigger a render, and we can hide the number of params, the param type, and the param name. We can also close the "Image Shader" group.
void
ShadertoyPlugin::updateExtra()
{
    {
        AutoMutex lock( _imageShaderMutex.get() );
        // only do this if parameters were updated!
        if (_imageShaderUpdateParams) {
            _imageShaderUpdateParams = false;
            beginEditBlock(kParamAuto);
            // Try to avoid setting parameters to the same value, since this maytrigger an unnecessary instancechanged on some hosts
            for (unsigned i = 0; i < NBINPUTS; ++i) {
                if ( _imageShaderInputEnabled[i] != _inputEnable[i]->getValue() ) {
                    _inputEnable[i]->setValue(_imageShaderInputEnabled[i]);
                }
                std::string s;
                _inputLabel[i]->getValue(s);
                if (_imageShaderInputLabel[i] != s) {
                    _inputLabel[i]->setValue(_imageShaderInputLabel[i]);
                }
                _inputHint[i]->getValue(s);
                if (_imageShaderInputHint[i] != s) {
                    _inputHint[i]->setValue(_imageShaderInputHint[i]);
                }
                if ( _imageShaderInputFilter[i] != _inputFilter[i]->getValue() ) {
                    _inputFilter[i]->setValue(_imageShaderInputFilter[i]);
                }
                if ( _imageShaderInputWrap[i] != _inputWrap[i]->getValue() ) {
                    _inputWrap[i]->setValue(_imageShaderInputWrap[i]);
                }
            }
            if ( _imageShaderHasMouse != _mouseParams->getValue() ) {
                _mouseParams->setValue(_imageShaderHasMouse);
            }
            if ( (int)_imageShaderExtraParameters.size() != _paramCount->getValue() ) {
                _paramCount->setValue( _imageShaderExtraParameters.size() );
            }
            for (unsigned i = 0; i < _imageShaderExtraParameters.size(); ++i) {
                const ExtraParameter& p = _imageShaderExtraParameters[i];
                UniformTypeEnum t = p.getType();
                bool tChanged = ( t != (UniformTypeEnum)_paramType[i]->getValue() );
                if (tChanged) {
                    _paramType[i]->setValue( (int)t );
                }
                std::string s;
                _paramName[i]->getValue(s);
                if (p.getName() != s) {
                    _paramName[i]->setValue( p.getName() );
                }
                _paramLabel[i]->getValue(s);
                if (p.getLabel() != s) {
                    _paramLabel[i]->setValue( p.getLabel() );
                }
                _paramHint[i]->getValue(s);
                if (p.getHint() != s) {
                    _paramHint[i]->setValue( p.getHint() );
                }
                switch (t) {
                    case eUniformTypeNone: {
                        if (tChanged) {
                            _paramDefaultBool[i]->resetToDefault();
                            _paramDefaultInt[i]->resetToDefault();
                            _paramMinInt[i]->resetToDefault();
                            _paramMaxInt[i]->resetToDefault();
                            _paramDefaultFloat[i]->resetToDefault();
                            _paramMinFloat[i]->resetToDefault();
                            _paramMaxFloat[i]->resetToDefault();
                            _paramDefaultVec2[i]->resetToDefault();
                            _paramMinVec2[i]->resetToDefault();
                            _paramMaxVec2[i]->resetToDefault();
                            _paramDefaultVec3[i]->resetToDefault();
                            //_paramMinVec3[i]->resetToDefault();
                            //_paramMaxVec3[i]->resetToDefault();
                            _paramDefaultVec4[i]->resetToDefault();
                            //_paramMinVec4[i]->resetToDefault();
                            //_paramMaxVec4[i]->resetToDefault();
                        }
                    }
                    case eUniformTypeBool: {
                        if (tChanged) {
                            _paramDefaultInt[i]->resetToDefault();
                            _paramMinInt[i]->resetToDefault();
                            _paramMaxInt[i]->resetToDefault();
                            _paramDefaultFloat[i]->resetToDefault();
                            _paramMinFloat[i]->resetToDefault();
                            _paramMaxFloat[i]->resetToDefault();
                            _paramDefaultVec2[i]->resetToDefault();
                            _paramMinVec2[i]->resetToDefault();
                            _paramMaxVec2[i]->resetToDefault();
                            _paramDefaultVec3[i]->resetToDefault();
                            //_paramMinVec3[i]->resetToDefault();
                            //_paramMaxVec3[i]->resetToDefault();
                            _paramDefaultVec4[i]->resetToDefault();
                            //_paramMinVec4[i]->resetToDefault();
                            //_paramMaxVec4[i]->resetToDefault();
                        }
                        _paramDefaultBool[i]->setValue(p.getDefault().b);
                        break;
                    }
                    case eUniformTypeInt: {
                        if (tChanged) {
                            _paramDefaultBool[i]->resetToDefault();
                            _paramDefaultFloat[i]->resetToDefault();
                            _paramMinFloat[i]->resetToDefault();
                            _paramMaxFloat[i]->resetToDefault();
                            _paramDefaultVec2[i]->resetToDefault();
                            _paramMinVec2[i]->resetToDefault();
                            _paramMaxVec2[i]->resetToDefault();
                            _paramDefaultVec3[i]->resetToDefault();
                            //_paramMinVec3[i]->resetToDefault();
                            //_paramMaxVec3[i]->resetToDefault();
                            _paramDefaultVec4[i]->resetToDefault();
                            //_paramMinVec4[i]->resetToDefault();
                            //_paramMaxVec4[i]->resetToDefault();
                        }
                        _paramDefaultInt[i]->setValue(p.getDefault().i);
                        _paramMinInt[i]->setValue(p.getMin().i);
                        _paramMaxInt[i]->setValue(p.getMax().i);
                        break;
                    }
                    case eUniformTypeFloat: {
                        if (tChanged) {
                            _paramDefaultBool[i]->resetToDefault();
                            _paramDefaultInt[i]->resetToDefault();
                            _paramMinInt[i]->resetToDefault();
                            _paramMaxInt[i]->resetToDefault();
                            _paramDefaultVec2[i]->resetToDefault();
                            _paramMinVec2[i]->resetToDefault();
                            _paramMaxVec2[i]->resetToDefault();
                            _paramDefaultVec3[i]->resetToDefault();
                            //_paramMinVec3[i]->resetToDefault();
                            //_paramMaxVec3[i]->resetToDefault();
                            _paramDefaultVec4[i]->resetToDefault();
                            //_paramMinVec4[i]->resetToDefault();
                            //_paramMaxVec4[i]->resetToDefault();
                        }
                        _paramDefaultFloat[i]->setValue(p.getDefault().f[0]);
                        _paramMinFloat[i]->setValue(p.getMin().f[0]);
                        _paramMaxFloat[i]->setValue(p.getMax().f[0]);
                        break;
                    }
                    case eUniformTypeVec2: {
                        if (tChanged) {
                            _paramDefaultBool[i]->resetToDefault();
                            _paramDefaultInt[i]->resetToDefault();
                            _paramMinInt[i]->resetToDefault();
                            _paramMaxInt[i]->resetToDefault();
                            _paramDefaultFloat[i]->resetToDefault();
                            _paramMinFloat[i]->resetToDefault();
                            _paramMaxFloat[i]->resetToDefault();
                            _paramDefaultVec3[i]->resetToDefault();
                            //_paramMinVec3[i]->resetToDefault();
                            //_paramMaxVec3[i]->resetToDefault();
                            _paramDefaultVec4[i]->resetToDefault();
                            //_paramMinVec4[i]->resetToDefault();
                            //_paramMaxVec4[i]->resetToDefault();
                        }
                        _paramDefaultVec2[i]->setValue(p.getDefault().f[0], p.getDefault().f[1]);
                        _paramMinVec2[i]->setValue(p.getMin().f[0], p.getMin().f[1]);
                        _paramMaxVec2[i]->setValue(p.getMax().f[0], p.getMax().f[1]);
                        break;
                    }
                    case eUniformTypeVec3: {
                        if (tChanged) {
                            _paramDefaultBool[i]->resetToDefault();
                            _paramDefaultInt[i]->resetToDefault();
                            _paramMinInt[i]->resetToDefault();
                            _paramMaxInt[i]->resetToDefault();
                            _paramDefaultFloat[i]->resetToDefault();
                            _paramMinFloat[i]->resetToDefault();
                            _paramMaxFloat[i]->resetToDefault();
                            _paramDefaultVec2[i]->resetToDefault();
                            _paramMinVec2[i]->resetToDefault();
                            _paramMaxVec2[i]->resetToDefault();
                            _paramDefaultVec4[i]->resetToDefault();
                            //_paramMinVec4[i]->resetToDefault();
                            //_paramMaxVec4[i]->resetToDefault();
                        }
                        _paramDefaultVec3[i]->setValue(p.getDefault().f[0], p.getDefault().f[1], p.getDefault().f[2]);
                        //_paramMinVec3[i]->setValue(p.getMin().f[0], p.getMin().f[1], p.getMin().f[2])
                        //_paramMaxVec3[i]->setValue(p.getMax().f[0], p.getMax().f[1], p.getMax().f[2]);
                        break;
                    }
                    case eUniformTypeVec4: {
                        if (tChanged) {
                            _paramDefaultBool[i]->resetToDefault();
                            _paramDefaultInt[i]->resetToDefault();
                            _paramMinInt[i]->resetToDefault();
                            _paramMaxInt[i]->resetToDefault();
                            _paramDefaultFloat[i]->resetToDefault();
                            _paramMinFloat[i]->resetToDefault();
                            _paramMaxFloat[i]->resetToDefault();
                            _paramDefaultVec2[i]->resetToDefault();
                            _paramMinVec2[i]->resetToDefault();
                            _paramMaxVec2[i]->resetToDefault();
                            _paramDefaultVec3[i]->resetToDefault();
                            //_paramMinVec3[i]->resetToDefault();
                            //_paramMaxVec3[i]->resetToDefault();
                        }
                        _paramDefaultVec4[i]->setDefault(p.getDefault().f[0], p.getDefault().f[1], p.getDefault().f[2], p.getDefault().f[3]);
                        //_paramMinVec4[i]->setValue(p.getMin().f[0], p.getMin().f[1], p.getMin().f[2], p.getMin().f[3]);
                        //_paramMaxVec4[i]->setValue(p.getMax().f[0], p.getMax().f[1], p.getMax().f[2], p.getMax().f[3]);
                        break;
                    }
                    default: {
                        assert(false);
                        if (tChanged) {
                            _paramDefaultBool[i]->resetToDefault();
                            _paramDefaultInt[i]->resetToDefault();
                            _paramMinInt[i]->resetToDefault();
                            _paramMaxInt[i]->resetToDefault();
                            _paramDefaultFloat[i]->resetToDefault();
                            _paramMinFloat[i]->resetToDefault();
                            _paramMaxFloat[i]->resetToDefault();
                            _paramDefaultVec2[i]->resetToDefault();
                            _paramMinVec2[i]->resetToDefault();
                            _paramMaxVec2[i]->resetToDefault();
                            _paramDefaultVec3[i]->resetToDefault();
                            //_paramMinVec3[i]->resetToDefault();
                            //_paramMaxVec3[i]->resetToDefault();
                            _paramDefaultVec4[i]->resetToDefault();
                            //_paramMinVec4[i]->resetToDefault();
                            //_paramMaxVec4[i]->resetToDefault();
                        }
                    }
                }
            }
            for (unsigned i = _imageShaderExtraParameters.size(); i < NBUNIFORMS; ++i) {
                bool tChanged = ((UniformTypeEnum)_paramType[i]->getValue() != eUniformTypeNone);
                if (tChanged) {
                    _paramDefaultBool[i]->resetToDefault();
                    _paramDefaultInt[i]->resetToDefault();
                    _paramMinInt[i]->resetToDefault();
                    _paramMaxInt[i]->resetToDefault();
                    _paramDefaultFloat[i]->resetToDefault();
                    _paramMinFloat[i]->resetToDefault();
                    _paramMaxFloat[i]->resetToDefault();
                    _paramDefaultVec2[i]->resetToDefault();
                    _paramMinVec2[i]->resetToDefault();
                    _paramMaxVec2[i]->resetToDefault();
                    _paramDefaultVec3[i]->resetToDefault();
                    //_paramMinVec3[i]->resetToDefault();
                    //_paramMaxVec3[i]->resetToDefault();
                    _paramDefaultVec4[i]->resetToDefault();
                    //_paramMinVec4[i]->resetToDefault();
                    //_paramMaxVec4[i]->resetToDefault();
                }
            }
            _bbox->setValue((int)_imageShaderBBox);
            endEditBlock();
        } // if (_imageShaderUpdateParams)
    }

    // update GUI
    unsigned paramCount = std::max( 0, std::min(_paramCount->getValue(), NBUNIFORMS) );
    for (unsigned i = 0; i < paramCount; ++i) {
        UniformTypeEnum t = (UniformTypeEnum)_paramType[i]->getValue();
        if (t == eUniformTypeNone) {
            continue;
        }
        std::string name;
        std::string label;
        std::string hint;
        _paramName[i]->getValue(name);
        _paramLabel[i]->getValue(label);
        _paramHint[i]->getValue(hint);
        if ( label.empty() ) {
            label = name;
        }
#if 0
        if ( !name.empty() ) {
            if (! hint.empty() ) {
                hint += '\n';
            }
            hint += "This parameter corresponds to 'uniform ";
            hint += mapUniformTypeToStr(t);
            hint += " " + name + "'.";
        }
#endif
        if ( name.empty() ) {
            _paramGroup[i]->setLabel( kGroupParameterLabel + unsignedToString(i) );
        } else {
            _paramGroup[i]->setLabel(name);
        }
        switch (t) {
            case eUniformTypeBool: {
                if ( !label.empty() ) {
                    _paramValueBool[i]->setLabel(label);
                }
                if ( !hint.empty() ) {
                    _paramValueBool[i]->setHint(hint);
                }
                bool v;
                _paramDefaultBool[i]->getValue(v);
                _paramValueBool[i]->setDefault(v);
                break;
            }
            case eUniformTypeInt: {
                if ( !label.empty() ) {
                    _paramValueInt[i]->setLabel(label);
                }
                if ( !hint.empty() ) {
                    _paramValueInt[i]->setHint(hint);
                }
                int v;
                _paramDefaultInt[i]->getValue(v);
                int vmin, vmax;
                _paramMinInt[i]->getValue(vmin);
                _paramMaxInt[i]->getValue(vmax);
                _paramValueInt[i]->setDefault(v);
                _paramValueInt[i]->setRange(vmin, vmax);
                _paramValueInt[i]->setDisplayRange(vmin, vmax);
                break;
            }
            case eUniformTypeFloat: {
                if ( !label.empty() ) {
                    _paramValueFloat[i]->setLabel(label);
                }
                if ( !hint.empty() ) {
                    _paramValueFloat[i]->setHint(hint);
                }
                double v;
                _paramDefaultFloat[i]->getValue(v);
                double vmin, vmax;
                _paramMinFloat[i]->getValue(vmin);
                _paramMaxFloat[i]->getValue(vmax);
                _paramValueFloat[i]->setDefault(v);
                _paramValueFloat[i]->setRange(vmin, vmax);
                _paramValueFloat[i]->setDisplayRange(vmin, vmax);
                break;
            }
            case eUniformTypeVec2: {
                if ( !label.empty() ) {
                    _paramValueVec2[i]->setLabel(label);
                }
                if ( !hint.empty() ) {
                    _paramValueVec2[i]->setHint(hint);
                }
                double v0, v1;
                _paramDefaultVec2[i]->getValue(v0, v1);
                double v0min, v1min, v0max, v1max;
                _paramMinVec2[i]->getValue(v0min, v1min);
                _paramMaxVec2[i]->getValue(v0max, v1max);
                _paramValueVec2[i]->setDefault(v0, v1);
                _paramValueVec2[i]->setRange(v0min, v1min, v0max, v1max);
                _paramValueVec2[i]->setDisplayRange(v0min, v1min, v0max, v1max);
                break;
            }
            case eUniformTypeVec3: {
                if ( !label.empty() ) {
                    _paramValueVec3[i]->setLabel(label);
                }
                if ( !hint.empty() ) {
                    _paramValueVec3[i]->setHint(hint);
                }
                double v0, v1, v2;
                _paramDefaultVec3[i]->getValue(v0, v1, v2);
                //double v0min, v1min, v2min, v0max, v1max, v2max;
                //_paramMinVec3[i]->getValue(v0min, v1min, v2min);
                //_paramMaxVec3[i]->getValue(v0max, v1max, v2max);
                _paramValueVec3[i]->setDefault(v0, v1, v2);
                //_paramValueVec3[i]->setRange(v0min, v1min, v2min, v0max, v1max, v2max);
                //_paramValueVec3[i]->setDisplayRange(v0min, v1min, v2min, v0max, v1max, v2max);
                break;
            }
            case eUniformTypeVec4: {
                if ( !label.empty() ) {
                    _paramValueVec4[i]->setLabel(label);
                }
                if ( !hint.empty() ) {
                    _paramValueVec4[i]->setHint(hint);
                }
                double v0, v1, v2, v3;
                _paramDefaultVec4[i]->getValue(v0, v1, v2, v3);
                //double v0min, v1min, v2min, v0max, v1max, v2max;
                //_paramMinVec4[i]->getValue(v0min, v1min, v2min);
                //_paramMaxVec4[i]->getValue(v0max, v1max, v2max);
                _paramValueVec4[i]->setDefault(v0, v1, v2, v3);
                //_paramValueVec4[i]->setRange(v0min, v1min, v2min, v0max, v1max, v2max);
                //_paramValueVec4[i]->setDisplayRange(v0min, v1min, v2min, v0max, v1max, v2max);
                break;
            }
            default:
                assert(false);
                break;
        }
    }
}

// reset the extra parameters to their default value
void
ShadertoyPlugin::resetParamsValues()
{
    beginEditBlock(kParamResetParams);
    unsigned paramCount = std::max( 0, std::min(_paramCount->getValue(), NBUNIFORMS) );
    for (unsigned i = 0; i < paramCount; ++i) {
        UniformTypeEnum t = (UniformTypeEnum)_paramType[i]->getValue();
        if (t == eUniformTypeNone) {
            continue;
        }
        switch (t) {
            case eUniformTypeBool: {
                bool v;
                _paramDefaultBool[i]->getValue(v);
                _paramValueBool[i]->setValue(v);
                break;
            }
            case eUniformTypeInt: {
                int v;
                _paramDefaultInt[i]->getValue(v);
                _paramValueInt[i]->setValue(v);
                break;
            }
            case eUniformTypeFloat: {
                double v;
                _paramDefaultFloat[i]->getValue(v);
                _paramValueFloat[i]->setValue(v);
                break;
            }
            case eUniformTypeVec2: {
                double v0, v1;
                _paramDefaultVec2[i]->getValue(v0, v1);
                _paramValueVec2[i]->setValue(v0, v1);
                break;
            }
            case eUniformTypeVec3: {
                double v0, v1, v2;
                _paramDefaultVec3[i]->getValue(v0, v1, v2);
                _paramValueVec3[i]->setValue(v0, v1, v2);
                break;
            }
            case eUniformTypeVec4: {
                double v0, v1, v2, v3;
                _paramDefaultVec4[i]->getValue(v0, v1, v2, v3);
                _paramValueVec4[i]->setValue(v0, v1, v2, v3);
                break;
            }
            default:
                assert(false);
                break;
        }
    }
    endEditBlock();
}

void
ShadertoyPlugin::changedParam(const OFX::InstanceChangedArgs &args,
                              const std::string &paramName)
{
    const double time = args.time;
    if ( (paramName == kParamBBox) && (args.reason == OFX::eChangeUserEdit) ) {
        updateVisibility();
    } else if (paramName == kParamFormat) {
        //the host does not handle the format itself, do it ourselves
        OFX::EParamFormat format = (OFX::EParamFormat)_format->getValueAtTime(time);
        int w = 0, h = 0;
        double par = -1;
        getFormatResolution(format, &w, &h, &par);
        assert(par != -1);
        _formatPar->setValue(par);
        _formatSize->setValue(w, h);
    } else if ( (paramName == kParamImageShaderFileName) ||
                ( paramName == kParamImageShaderReload) ) {
        // load image shader from file
        std::string imageShaderFileName;
        _imageShaderFileName->getValueAtTime(time, imageShaderFileName);
        if ( !imageShaderFileName.empty() ) {
            std::ifstream t( imageShaderFileName.c_str() );
            if ( t.bad() ) {
                sendMessage(OFX::Message::eMessageError, "", std::string("Error: Cannot open file ") + imageShaderFileName);
            } else {
                std::string str;
                t.seekg(0, std::ios::end);
                str.reserve( t.tellg() );
                t.seekg(0, std::ios::beg);
                str.assign( ( std::istreambuf_iterator<char>(t) ),
                            std::istreambuf_iterator<char>() );
                _imageShaderSource->setValue(str);
            }
        }
    } else if (paramName == kParamImageShaderCompile) {
        {
            AutoMutex lock( _imageShaderMutex.get() );
            // mark that image shader must be recompiled on next render
            ++_imageShaderID;
            _imageShaderUpdateParams = false;
            _imageShaderCompiled = false;
        }
        _imageShaderCompile->setEnabled(false);
        // trigger a new render
        clearPersistentMessage();
        _imageShaderTriggerRender->setValue(_imageShaderTriggerRender->getValueAtTime(time) + 1);
    } else if (paramName == kParamAuto || paramName == kParamImageShaderParamsUpdated) {
        bool recompile = true;
        {
            AutoMutex lock( _imageShaderMutex.get() );
            if (_imageShaderUpdateParams && _imageShaderCompiled) {
                _imageShaderCompiled = false; //_imageShaderUpdateParams is reset by updateExtra()
                recompile = false; // parameters were updated (second click in a host that doesn't support seValue() from render(), probably), we just need to update the Gui
            } else {
                // same as kParamImageShaderCompile above, except ask for param update
                // mark that image shader must be recompiled on next render
                ++_imageShaderID;
                _imageShaderUpdateParams = true;
                _imageShaderCompiled = false;
            }
        }
        if (recompile) {
            // same as kParamImageShaderCompile above
            _imageShaderCompile->setEnabled(false);
            // trigger a new render which updates params and inputs info
            clearPersistentMessage();
            _imageShaderTriggerRender->setValue(_imageShaderTriggerRender->getValueAtTime(time) + 1);
        } else {
            updateExtra();
            updateVisibility();
            updateClips();
        }
    } else if (paramName == kParamResetParams) {
        resetParamsValues();
    } else if (paramName == kParamImageShaderSource) {
        _imageShaderCompile->setEnabled(true);
    } else if ( ( (paramName == kParamCount) ||
                  starts_with(paramName, kParamName) ) && (args.reason == eChangeUserEdit) ) {
        {
            AutoMutex lock( _imageShaderMutex.get() );
            // mark that image shader must be recompiled on next render
            ++_imageShaderUniformsID;
        }
        //updateExtra();
        updateVisibility();
    } else if (paramName == kParamMouseParams) {
        updateVisibility();
    } else if ( starts_with(paramName, kParamType) && (args.reason == eChangeUserEdit) ) {
        {
            AutoMutex lock( _imageShaderMutex.get() );
            // mark that image shader must be recompiled on next render
            ++_imageShaderUniformsID;
        }
        //updateVisibilityParam(i, i < paramCount);
        updateVisibility();
    } else if ( ( starts_with(paramName, kParamName) ||
                 starts_with(paramName, kParamLabel) ||
                 starts_with(paramName, kParamHint) ||
                 starts_with(paramName, kParamDefault) ||
                 starts_with(paramName, kParamMin) ||
                 starts_with(paramName, kParamMax) ) && (args.reason == eChangeUserEdit) ) {
        updateExtra();
    } else if ( ( starts_with(paramName, kParamInputLabel) ||
                 starts_with(paramName, kParamInputHint) ) && (args.reason == eChangeUserEdit) ) {
        updateClips();
    } else if ( starts_with(paramName, kParamInputEnable) && (args.reason == eChangeUserEdit) ) {
        updateClips();
        updateVisibility();
    } else if ( (paramName == kParamImageShaderSource) && (args.reason == eChangeUserEdit) ) {
        _imageShaderCompile->setEnabled(true);
    } else if (paramName == kParamRendererInfo) {
        std::string message;
        {
            AutoMutex lock( _rendererInfoMutex.get() );
            message = _rendererInfo;
        }
        if ( message.empty() ) {
            sendMessage(OFX::Message::eMessageMessage, "", "OpenGL renderer info not yet available.\n"
                        "Please execute at least one image render and try again.");
        } else {
            sendMessage(OFX::Message::eMessageMessage, "", message);
        }
#if defined(HAVE_OSMESA)
    } else if (paramName == kParamEnableGPU) {
        setSupportsOpenGLRender( _enableGPU->getValueAtTime(args.time) );
        {
            AutoMutex lock( _rendererInfoMutex.get() );
            _rendererInfo.clear();
        }
    } else if (paramName == kParamCPUDriver) {
        {
            AutoMutex lock( _rendererInfoMutex.get() );
            _rendererInfo.clear();
        }
#endif
    }
} // ShadertoyPlugin::changedParam

mDeclarePluginFactory(ShadertoyPluginFactory,; , {});
void
ShadertoyPluginFactory::load()
{
    // we can't be used on hosts that don't support the OpenGL suite
    // returning an error here causes a blank menu entry in Nuke
    //#if defined(OFX_SUPPORTS_OPENGLRENDER) && !defined(HAVE_OSMESA)
    //const ImageEffectHostDescription &gHostDescription = *OFX::getImageEffectHostDescription();
    //if (!gHostDescription.supportsOpenGLRender) {
    //    throwHostMissingSuiteException(kOfxOpenGLRenderSuite);
    //}
    //#endif
}

void
ShadertoyPluginFactory::describe(OFX::ImageEffectDescriptor &desc)
{
    // returning an error here crashes Nuke
    //#if defined(OFX_SUPPORTS_OPENGLRENDER) && !defined(HAVE_OSMESA)
    //const ImageEffectHostDescription &gHostDescription = *OFX::getImageEffectHostDescription();
    //if (!gHostDescription.supportsOpenGLRender) {
    //    throwHostMissingSuiteException(kOfxOpenGLRenderSuite);
    //}
    //#endif

    // basic labels
    desc.setLabel(kPluginName);
    desc.setPluginGrouping(kPluginGrouping);
    if (desc.getPropertySet().propGetDimension(kNatronOfxPropDescriptionIsMarkdown, false)) {
        desc.setPluginDescription(kPluginDescriptionMarkdown, false);
        desc.getPropertySet().propSetInt(kNatronOfxPropDescriptionIsMarkdown, 1);
    } else {
        desc.setPluginDescription(kPluginDescription);
    }

    // add the supported contexts
    desc.addSupportedContext(eContextGenerator);
    desc.addSupportedContext(eContextFilter);
    desc.addSupportedContext(eContextGeneral);

    // add supported pixel depths
    desc.addSupportedBitDepth(eBitDepthUByte);
    desc.addSupportedBitDepth(eBitDepthUShort);
    desc.addSupportedBitDepth(eBitDepthFloat);

    // set a few flags
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(kSupportsMultiResolution);
    desc.setSupportsTiles(kSupportsTiles);
    desc.setTemporalClipAccess(false);
    // We can render both fields in a fielded images in one hit if there is no animation
    // So set the flag that allows us to do this
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);
    // say we can support multiple pixel depths and let the clip preferences action deal with it all.
    desc.setSupportsMultipleClipDepths(kSupportsMultipleClipDepths);
    // we support OpenGL rendering (could also say "needed" here)
#ifdef OFX_SUPPORTS_OPENGLRENDER
#ifdef HAVE_OSMESA
    desc.setSupportsOpenGLRender(true);
#else
    desc.setNeedsOpenGLRender(true);
    desc.setSupportsRenderQuality(true);

    /*
     * If a host supports OpenGL rendering then it flags this with the string
     * property ::kOfxImageEffectOpenGLRenderSupported on its descriptor property
     * set. Effects that cannot run without OpenGL support should examine this in
     * ::kOfxActionDescribe action and return a ::kOfxStatErrMissingHostFeature
     * status flag if it is not set to "true".
     */
    const ImageEffectHostDescription &gHostDescription = *OFX::getImageEffectHostDescription();
    if (!gHostDescription.supportsOpenGLRender) {
        throwSuiteStatusException(kOfxStatErrMissingHostFeature);
    }
#endif
#endif
#ifdef OFX_EXTENSIONS_NATRON
    desc.setChannelSelector(ePixelComponentNone);
#endif

    // some hosts may have the multithread suite, but no mutex capability (e.g. Sony Catalyst)
    try {
        ShadertoyPlugin::Mutex m;
        desc.setRenderThreadSafety(eRenderFullySafe);
    } catch (const std::exception &e) {
#      ifdef DEBUG
        std::cout << "ERROR in describe(): Mutex creation returned " << e.what() << std::endl;
#      endif
        desc.setRenderThreadSafety(eRenderInstanceSafe);
    }
} // ShadertoyPluginFactory::describe


static void
defineBooleanSub(OFX::ImageEffectDescriptor &desc,
                 const std::string& nb,
                 const std::string& name,
                 const std::string& label,
                 const std::string& hint,
                 bool isExtraParam,
                 PageParamDescriptor *page,
                 GroupParamDescriptor *group)
{
    OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(name + nb);
    param->setLabel(label + nb);
    param->setHint(hint);
    param->setEvaluateOnChange(isExtraParam);
    param->setAnimates(isExtraParam);
    if (page) {
        page->addChild(*param);
    }
    if (group) {
        param->setParent(*group);
    }
}

static void
defineBoolean(OFX::ImageEffectDescriptor &desc,
              const std::string& nb,
              PageParamDescriptor *page,
              GroupParamDescriptor *group)
{
    defineBooleanSub(desc, nb, kParamDefaultBool, kParamDefaultLabel, kParamDefaultHint, false, page, group);
}

static void
defineIntSub(OFX::ImageEffectDescriptor &desc,
             const std::string& nb,
             const std::string& name,
             const std::string& label,
             const std::string& hint,
             bool isExtraParam,
             int defaultValue,
             PageParamDescriptor *page,
             GroupParamDescriptor *group)
{
    OFX::IntParamDescriptor* param = desc.defineIntParam(name + nb);
    param->setLabel(label + nb);
    param->setHint(hint);
    param->setRange(INT_MIN, INT_MAX);
    param->setDisplayRange(INT_MIN, INT_MAX);
    param->setDefault(defaultValue);
    param->setEvaluateOnChange(isExtraParam);
    param->setAnimates(isExtraParam);
    if (page) {
        page->addChild(*param);
    }
    if (group) {
        param->setParent(*group);
    }
}

static void
defineInt(OFX::ImageEffectDescriptor &desc,
             const std::string& nb,
             PageParamDescriptor *page,
             GroupParamDescriptor *group)
{
    defineIntSub(desc, nb, kParamDefaultInt, kParamDefaultLabel, kParamDefaultHint, false, 0, page, group);
    defineIntSub(desc, nb, kParamMinInt, kParamMinLabel, kParamMinHint, false, INT_MIN, page, group);
    defineIntSub(desc, nb, kParamMaxInt, kParamMaxLabel, kParamMaxHint, false, INT_MAX, page, group);
}

static void
defineDoubleSub(OFX::ImageEffectDescriptor &desc,
             const std::string& nb,
             const std::string& name,
             const std::string& label,
             const std::string& hint,
             bool isExtraParam,
             double defaultValue,
             PageParamDescriptor *page,
             GroupParamDescriptor *group)
{
    OFX::DoubleParamDescriptor* param = desc.defineDoubleParam(name + nb);
    param->setLabel(label + nb);
    param->setHint(hint);
    param->setRange(-DBL_MAX, DBL_MAX);
    param->setDisplayRange(-DBL_MAX, DBL_MAX);
    param->setDefault(defaultValue);
    param->setEvaluateOnChange(isExtraParam);
    param->setAnimates(isExtraParam);
    if (page) {
        page->addChild(*param);
    }
    if (group) {
        param->setParent(*group);
    }
}

static void
defineDouble(OFX::ImageEffectDescriptor &desc,
          const std::string& nb,
          PageParamDescriptor *page,
          GroupParamDescriptor *group)
{
    defineDoubleSub(desc, nb, kParamDefaultFloat, kParamDefaultLabel, kParamDefaultHint, false, 0, page, group);
    defineDoubleSub(desc, nb, kParamMinFloat, kParamMinLabel, kParamMinHint, false, -DBL_MAX, page, group);
    defineDoubleSub(desc, nb, kParamMaxFloat, kParamMaxLabel, kParamMaxHint, false, DBL_MAX, page, group);
}

static void
defineDouble2DSub(OFX::ImageEffectDescriptor &desc,
                const std::string& nb,
                const std::string& name,
                const std::string& label,
                const std::string& hint,
                bool isExtraParam,
                double defaultValue,
                PageParamDescriptor *page,
                GroupParamDescriptor *group)
{
    OFX::Double2DParamDescriptor* param = desc.defineDouble2DParam(name + nb);
    param->setLabel(label + nb);
    param->setHint(hint);
    param->setRange(-DBL_MAX, -DBL_MAX, DBL_MAX, DBL_MAX);
    param->setDisplayRange(-DBL_MAX, -DBL_MAX, DBL_MAX, DBL_MAX);
    param->setDefault(defaultValue, defaultValue);
    param->setEvaluateOnChange(isExtraParam);
    param->setAnimates(isExtraParam);
    if (page) {
        page->addChild(*param);
    }
    if (group) {
        param->setParent(*group);
    }
}

static void
defineDouble2D(OFX::ImageEffectDescriptor &desc,
             const std::string& nb,
             PageParamDescriptor *page,
             GroupParamDescriptor *group)
{
    defineDouble2DSub(desc, nb, kParamDefaultVec2, kParamDefaultLabel, kParamDefaultHint, false, 0, page, group);
    defineDouble2DSub(desc, nb, kParamMinVec2, kParamMinLabel, kParamMinHint, false, -DBL_MAX, page, group);
    defineDouble2DSub(desc, nb, kParamMaxVec2, kParamMaxLabel, kParamMaxHint, false, DBL_MAX, page, group);
}

static void
defineDouble3DSub(OFX::ImageEffectDescriptor &desc,
                  const std::string& nb,
                  const std::string& name,
                  const std::string& label,
                  const std::string& hint,
                  bool isExtraParam,
                  double defaultValue,
                  PageParamDescriptor *page,
                  GroupParamDescriptor *group)
{
    OFX::Double3DParamDescriptor* param = desc.defineDouble3DParam(name + nb);
    param->setLabel(label + nb);
    param->setHint(hint);
    param->setRange(-DBL_MAX, -DBL_MAX, -DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX);
    param->setDisplayRange(-DBL_MAX, -DBL_MAX, -DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX);
    param->setDefault(defaultValue, defaultValue, defaultValue);
    param->setEvaluateOnChange(isExtraParam);
    param->setAnimates(isExtraParam);
    if (page) {
        page->addChild(*param);
    }
    if (group) {
        param->setParent(*group);
    }
}

static void
defineDouble3D(OFX::ImageEffectDescriptor &desc,
               const std::string& nb,
               PageParamDescriptor *page,
               GroupParamDescriptor *group)
{
    defineDouble3DSub(desc, nb, kParamDefaultVec3, kParamDefaultLabel, kParamDefaultHint, false, 0, page, group);
    //defineDouble3DSub(desc, nb, kParamMinVec3, kParamMinLabel, kParamMinHint, false, -DBL_MAX, page, group);
    //defineDouble3DSub(desc, nb, kParamMaxVec3, kParamMaxLabel, kParamMaxHint, false, DBL_MAX, page, group);
}

static void
defineRGBASub(OFX::ImageEffectDescriptor &desc,
                  const std::string& nb,
                  const std::string& name,
                  const std::string& label,
                  const std::string& hint,
                  bool isExtraParam,
                  double defaultValue,
                  PageParamDescriptor *page,
                  GroupParamDescriptor *group)
{
    OFX::RGBAParamDescriptor* param = desc.defineRGBAParam(name + nb);
    param->setLabel(label + nb);
    param->setHint(hint);
    param->setRange(-DBL_MAX, -DBL_MAX, -DBL_MAX, -DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX);
    param->setDisplayRange(-DBL_MAX, -DBL_MAX, -DBL_MAX, -DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX);
    param->setDefault(defaultValue, defaultValue, defaultValue, defaultValue);
    param->setEvaluateOnChange(isExtraParam);
    param->setAnimates(isExtraParam);
    if (page) {
        page->addChild(*param);
    }
    if (group) {
        param->setParent(*group);
    }
}

static void
defineRGBA(OFX::ImageEffectDescriptor &desc,
               const std::string& nb,
               PageParamDescriptor *page,
               GroupParamDescriptor *group)
{
    defineRGBASub(desc, nb, kParamDefaultVec4, kParamDefaultLabel, kParamDefaultHint, false, 0, page, group);
    //defineRGBASub(desc, nb, kParamMinVec4, kParamMinLabel, kParamMinHint, false, -DBL_MAX, page, group);
    //defineRGBASub(desc, nb, kParamMaxVec4, kParamMaxLabel, kParamMaxHint, false, DBL_MAX, page, group);
}

void
ShadertoyPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc,
                                          OFX::ContextEnum context)
{
#if defined(OFX_SUPPORTS_OPENGLRENDER) && !defined(HAVE_OSMESA)
    const ImageEffectHostDescription &gHostDescription = *OFX::getImageEffectHostDescription();
    if (!gHostDescription.supportsOpenGLRender) {
        throwHostMissingSuiteException(kOfxOpenGLRenderSuite);
    }
#endif

    // Source clip only in the filter context
    // create the mandated source clip
    {
        ClipDescriptor *srcClip = desc.defineClip( (context == eContextFilter) ?
                                                   kOfxImageEffectSimpleSourceClipName :
                                                   kClipChannel "0" );
        srcClip->addSupportedComponent(ePixelComponentRGBA);
        srcClip->addSupportedComponent(ePixelComponentAlpha);
        srcClip->setTemporalClipAccess(false);
        srcClip->setSupportsTiles(kSupportsTiles);
        srcClip->setIsMask(false);
        srcClip->setOptional( !(context == eContextFilter) );
    }
    for (unsigned i = 1; i < NBINPUTS; ++i) {
        std::string iChannelX(kClipChannel);
        iChannelX += unsignedToString(i);
        ClipDescriptor *srcClip = desc.defineClip(iChannelX);
        srcClip->addSupportedComponent(ePixelComponentRGBA);
        srcClip->addSupportedComponent(ePixelComponentAlpha);
        srcClip->setTemporalClipAccess(false);
        srcClip->setSupportsTiles(kSupportsTiles);
        srcClip->setIsMask(false);
        srcClip->setOptional(true);
    }
    // create the mandated output clip
    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
    dstClip->setSupportsTiles(kSupportsTiles);

    // make some pages and to things in
    PageParamDescriptor *page = desc.definePageParam("Controls");

    {
        OFX::Double2DParamDescriptor* param = desc.defineDouble2DParam(kParamMousePosition);
        param->setLabel(kParamMousePositionLabel);
        param->setHint(kParamMousePositionHint);
        param->setDoubleType(eDoubleTypeXYAbsolute);
        param->setDefaultCoordinateSystem(eCoordinatesCanonical); // Nuke defaults to Normalized for XY and XYAbsolute!
        param->setUseHostNativeOverlayHandle(true);
        param->setAnimates(true);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        OFX::Double2DParamDescriptor* param = desc.defineDouble2DParam(kParamMouseClick);
        param->setLabel(kParamMouseClickLabel);
        param->setHint(kParamMouseClickHint);
        param->setDoubleType(eDoubleTypeXYAbsolute);
        param->setDefaultCoordinateSystem(eCoordinatesCanonical); // Nuke defaults to Normalized for XY and XYAbsolute!
        param->setUseHostNativeOverlayHandle(true);
        param->setAnimates(true);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(kParamMousePressed);
        param->setLabel(kParamMousePressedLabel);
        param->setHint(kParamMousePressedHint);
        param->setAnimates(true);
        if (page) {
            page->addChild(*param);
        }
    }

    for (unsigned i = 0; i < NBUNIFORMS; ++i) {
        // generate the number string
        std::string nb = unsignedToString(i);
        defineBooleanSub(desc, nb, kParamValueBool, kParamValueLabel, kParamValueHint, true, page, NULL);
        defineIntSub(desc, nb, kParamValueInt, kParamValueLabel, kParamValueHint, true, 0, page, NULL);
        defineDoubleSub(desc, nb, kParamValueFloat, kParamValueLabel, kParamValueHint, true, 0, page, NULL);
        defineDouble2DSub(desc, nb, kParamValueVec2, kParamValueLabel, kParamValueHint, true, 0, page, NULL);
        defineDouble3DSub(desc, nb, kParamValueVec3, kParamValueLabel, kParamValueHint, true, 0, page, NULL);
        defineRGBASub(desc, nb, kParamValueVec4, kParamValueLabel, kParamValueHint, true, 0, page, NULL);
    }

    {
        OFX::GroupParamDescriptor* group = desc.defineGroupParam(kGroupImageShader);
        if (group) {
            group->setLabel(kGroupImageShaderLabel);
            group->setOpen(false);
           //group->setAsTab();
        }

        {
            OFX::StringParamDescriptor* param = desc.defineStringParam(kParamImageShaderFileName);
            param->setLabel(kParamImageShaderFileNameLabel);
            param->setHint(kParamImageShaderFileNameHint);
            param->setStringType(eStringTypeFilePath);
            param->setFilePathExists(true);
            param->setLayoutHint(eLayoutHintNoNewLine, 1);
            param->setEvaluateOnChange(false); // render is triggered using kParamImageShaderTriggerRender
            param->setAnimates(false);
            if (page) {
                page->addChild(*param);
            }
            if (group) {
                param->setParent(*group);
            }
        }

        {
            OFX::PushButtonParamDescriptor* param = desc.definePushButtonParam(kParamImageShaderReload);
            param->setLabel(kParamImageShaderReloadLabel);
            param->setHint(kParamImageShaderReloadHint);
            if (page) {
                page->addChild(*param);
            }
            if (group) {
                param->setParent(*group);
            }
        }

        {
            OFX::StringParamDescriptor* param = desc.defineStringParam(kParamImageShaderSource);
            param->setLabel(kParamImageShaderSourceLabel);
            param->setHint(kParamImageShaderSourceHint);
            param->setStringType(eStringTypeMultiLine);
            param->setDefault(kParamImageShaderDefault);
            param->setEvaluateOnChange(false); // render is triggered using kParamImageShaderTriggerRender
            param->setAnimates(false);
            if (page) {
                page->addChild(*param);
            }
            if (group) {
                param->setParent(*group);
            }
        }

        {
            OFX::PushButtonParamDescriptor* param = desc.definePushButtonParam(kParamImageShaderCompile);
            param->setLabel(kParamImageShaderCompileLabel);
            param->setHint(kParamImageShaderCompileHint);
            param->setLayoutHint(eLayoutHintNoNewLine, 1);
            if (page) {
                page->addChild(*param);
            }
            if (group) {
                param->setParent(*group);
            }
        }

        {
            OFX::PushButtonParamDescriptor* param = desc.definePushButtonParam(kParamAuto);
            param->setLabel(kParamAutoLabel);
            param->setHint(kParamAutoHint);
            param->setLayoutHint(eLayoutHintNoNewLine, 1);
            if (page) {
                page->addChild(*param);
            }
            if (group) {
                param->setParent(*group);
            }
        }

        {
            OFX::PushButtonParamDescriptor* param = desc.definePushButtonParam(kParamResetParams);
            param->setLabel(kParamResetParamsLabel);
            param->setHint(kParamResetParamsHint);
            if (page) {
                page->addChild(*param);
            }
            if (group) {
                param->setParent(*group);
            }
        }

        {
            // a dummy boolean parameter, used to trigger a new render when the shader is to be recompiled
            OFX::IntParamDescriptor* param = desc.defineIntParam(kParamImageShaderTriggerRender);
            param->setEvaluateOnChange(true);
            param->setAnimates(false);
            param->setIsSecret(true);
            param->setIsPersistant(false);
            if (page) {
                page->addChild(*param);
            }
            if (group) {
                param->setParent(*group);
            }
        }

        {
            // a dummy boolean parameter, used to update parameters GUI when the shader was recompiled
            OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(kParamImageShaderParamsUpdated);
            param->setEvaluateOnChange(false);
            param->setAnimates(false);
            param->setIsSecret(true);
            param->setIsPersistant(false);
            if (page) {
                page->addChild(*param);
            }
            if (group) {
                param->setParent(*group);
            }
        }

        for (unsigned i = 0; i < NBINPUTS; ++i) {
            std::string nb = unsignedToString(i);
            {
                OFX::StringParamDescriptor* param = desc.defineStringParam(kParamInputName + nb);
                param->setLabel("");
                param->setDefault(kClipChannel + nb);
                param->setStringType(OFX::eStringTypeLabel);
                param->setLayoutHint(eLayoutHintNoNewLine, 1);
                if (page) {
                    page->addChild(*param);
                }
                if (group) {
                    param->setParent(*group);
                }
            }
            {
                OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(kParamInputEnable + nb);
                param->setLabel(kParamInputEnableLabel);
                param->setHint(kParamInputEnableHint);
                param->setDefault(true);
                param->setAnimates(false);
                param->setLayoutHint(eLayoutHintNoNewLine, 1);
                if (page) {
                    page->addChild(*param);
                }
                if (group) {
                    param->setParent(*group);
                }
            }
            {
                OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamInputFilter + nb);
                param->setLabel(kParamInputFilterLabel);
                param->setHint(kParamInputFilterHint);
                assert(param->getNOptions() == (int)ShadertoyPlugin::eFilterNearest);
                param->appendOption(kParamInputFilterOptionNearest, kParamInputFilterOptionNearestHint);
                assert(param->getNOptions() == (int)ShadertoyPlugin::eFilterLinear);
                param->appendOption(kParamInputFilterOptionLinear, kParamInputFilterOptionLinearHint);
                assert(param->getNOptions() == (int)ShadertoyPlugin::eFilterMipmap);
                param->appendOption(kParamInputFilterOptionMipmap, kParamInputFilterOptionMipmapHint);
                assert(param->getNOptions() == (int)ShadertoyPlugin::eFilterAnisotropic);
                param->appendOption(kParamInputFilterOptionAnisotropic, kParamInputFilterOptionAnisotropicHint);
                param->setDefault((int)ShadertoyPlugin::eFilterMipmap);
                param->setAnimates(false);
                param->setLayoutHint(eLayoutHintNoNewLine, 1);
                if (page) {
                    page->addChild(*param);
                }
                if (group) {
                    param->setParent(*group);
                }
            }
            {
                OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamInputWrap + nb);
                param->setLabel(kParamInputWrapLabel);
                param->setHint(kParamInputWrapHint);
                assert(param->getNOptions() == (int)ShadertoyPlugin::eWrapRepeat);
                param->appendOption(kParamInputWrapOptionRepeat, kParamInputWrapOptionRepeatHint);
                assert(param->getNOptions() == (int)ShadertoyPlugin::eWrapClamp);
                param->appendOption(kParamInputWrapOptionClamp, kParamInputWrapOptionClampHint);
                assert(param->getNOptions() == (int)ShadertoyPlugin::eWrapMirror);
                param->appendOption(kParamInputWrapOptionMirror, kParamInputWrapOptionMirrorHint);
                param->setDefault((int)ShadertoyPlugin::eWrapRepeat);
                param->setAnimates(false);
                if (page) {
                    page->addChild(*param);
                }
                if (group) {
                    param->setParent(*group);
                }
            }
            {
                OFX::StringParamDescriptor* param = desc.defineStringParam(kParamInputLabel + nb);
                param->setLabel(kParamInputLabelLabel);
                param->setHint(kParamInputLabelHint);
                param->setDefault("");
                param->setAnimates(false);
                if (page) {
                    page->addChild(*param);
                }
                if (group) {
                    param->setParent(*group);
                }
            }
            {
                OFX::StringParamDescriptor* param = desc.defineStringParam(kParamInputHint + nb);
                param->setLabel(kParamInputHintLabel);
                param->setDefault("");
                param->setAnimates(false);
                if (page) {
                    page->addChild(*param);
                }
                if (group) {
                    param->setParent(*group);
                }
            }

        }

        // boundingBox
        {
            ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamBBox);
            param->setLabel(kParamBBoxLabel);
            param->setHint(kParamBBoxHint);
            assert(param->getNOptions() == (int)ShadertoyPlugin::eBBoxDefault);
            param->appendOption(kParamBBoxOptionDefault, kParamBBoxOptionDefaultHint);
            assert(param->getNOptions() == (int)ShadertoyPlugin::eBBoxFormat);
            param->appendOption(kParamBBoxOptionFormat, kParamBBoxOptionFormatHint);
            //assert(param->getNOptions() == (int)eBBoxSize);
            //param->appendOption(kParamBBoxOptionSize, kParamBBoxOptionSizeHint);
            assert(param->getNOptions() == (int)ShadertoyPlugin::eBBoxUnion);
            param->appendOption(kParamBBoxOptionUnion, kParamBBoxOptionUnionHint);
            assert(param->getNOptions() == (int)ShadertoyPlugin::eBBoxIntersection);
            param->appendOption(kParamBBoxOptionIntersection, kParamBBoxOptionIntersectionHint);
            assert(param->getNOptions() == (int)ShadertoyPlugin::eBBoxIChannel);
            for (unsigned i = 0; i < NBINPUTS; ++i) {
                std::string nb = unsignedToString(i);
                param->appendOption(std::string(kParamBBoxOptionIChannel) + nb, std::string(kParamBBoxOptionIChannelHint) + nb + '.');
            }
            param->setAnimates(true);
            param->setDefault( (int)ShadertoyPlugin::eBBoxDefault );
            if (page) {
                page->addChild(*param);
            }
            if (group) {
                param->setParent(*group);
            }
        }

        // format
        {
            ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamFormat);
            param->setLabel(kParamFormatLabel);
            assert(param->getNOptions() == eParamFormatPCVideo);
            param->appendOption(kParamFormatPCVideoLabel);
            assert(param->getNOptions() == eParamFormatNTSC);
            param->appendOption(kParamFormatNTSCLabel);
            assert(param->getNOptions() == eParamFormatPAL);
            param->appendOption(kParamFormatPALLabel);
            assert(param->getNOptions() == eParamFormatHD);
            param->appendOption(kParamFormatHDLabel);
            assert(param->getNOptions() == eParamFormatNTSC169);
            param->appendOption(kParamFormatNTSC169Label);
            assert(param->getNOptions() == eParamFormatPAL169);
            param->appendOption(kParamFormatPAL169Label);
            assert(param->getNOptions() == eParamFormat1kSuper35);
            param->appendOption(kParamFormat1kSuper35Label);
            assert(param->getNOptions() == eParamFormat1kCinemascope);
            param->appendOption(kParamFormat1kCinemascopeLabel);
            assert(param->getNOptions() == eParamFormat2kSuper35);
            param->appendOption(kParamFormat2kSuper35Label);
            assert(param->getNOptions() == eParamFormat2kCinemascope);
            param->appendOption(kParamFormat2kCinemascopeLabel);
            assert(param->getNOptions() == eParamFormat4kSuper35);
            param->appendOption(kParamFormat4kSuper35Label);
            assert(param->getNOptions() == eParamFormat4kCinemascope);
            param->appendOption(kParamFormat4kCinemascopeLabel);
            assert(param->getNOptions() == eParamFormatSquare256);
            param->appendOption(kParamFormatSquare256Label);
            assert(param->getNOptions() == eParamFormatSquare512);
            param->appendOption(kParamFormatSquare512Label);
            assert(param->getNOptions() == eParamFormatSquare1k);
            param->appendOption(kParamFormatSquare1kLabel);
            assert(param->getNOptions() == eParamFormatSquare2k);
            param->appendOption(kParamFormatSquare2kLabel);
            param->setDefault(eParamFormatPCVideo);
            param->setHint(kParamFormatHint);
            param->setAnimates(false);
            desc.addClipPreferencesSlaveParam(*param);
            if (page) {
                page->addChild(*param);
            }
            if (group) {
                param->setParent(*group);
            }
        }

        {
            int w = 0, h = 0;
            double par = -1.;
            getFormatResolution(eParamFormatPCVideo, &w, &h, &par);
            assert(par != -1);
            {
                Int2DParamDescriptor* param = desc.defineInt2DParam(kParamFormatSize);
                param->setLabel(kParamFormatSizeLabel);
                param->setHint(kParamFormatSizeHint);
                param->setIsSecret(true);
                param->setDefault(w, h);
                if (page) {
                    page->addChild(*param);
                }
                if (group) {
                    param->setParent(*group);
                }
            }

            {
                DoubleParamDescriptor* param = desc.defineDoubleParam(kParamFormatPAR);
                param->setLabel(kParamFormatPARLabel);
                param->setHint(kParamFormatPARHint);
                param->setIsSecret(true);
                param->setRange(0., DBL_MAX);
                param->setDisplayRange(0.5, 2.);
                param->setDefault(par);
                if (page) {
                    page->addChild(*param);
                }
                if (group) {
                    param->setParent(*group);
                }
            }
        }

        {
            OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(kParamMouseParams);
            param->setLabel(kParamMouseParamsLabel);
            param->setHint(kParamMouseParamsHint);
            param->setDefault(true);
            param->setAnimates(false);
            if (page) {
                page->addChild(*param);
            }
            if (group) {
                param->setParent(*group);
            }
        }

        {
            OFX::GroupParamDescriptor* sgroup = desc.defineGroupParam(kGroupExtraParameters);
            if (sgroup) {
                sgroup->setLabel(kGroupExtraParametersLabel);
                sgroup->setHint(kGroupExtraParametersHint);
                sgroup->setOpen(false);
            }

            {
                OFX::IntParamDescriptor* param = desc.defineIntParam(kParamCount);
                param->setLabel(kParamCountLabel);
                param->setHint(kParamCountHint);
                param->setRange(0, NBUNIFORMS);
                param->setDisplayRange(0, NBUNIFORMS);
                param->setAnimates(false);
                if (page) {
                    page->addChild(*param);
                }
                if (sgroup) {
                    param->setParent(*sgroup);
                }
            }

            for (unsigned i = 0; i < NBUNIFORMS; ++i) {
                // generate the number string
                std::string nb = unsignedToString(i);
                OFX::GroupParamDescriptor* pgroup = desc.defineGroupParam(kGroupParameter + nb);
                if (pgroup) {
                    pgroup->setLabel(kGroupParameterLabel + nb);
                    pgroup->setOpen(false);
                }

                {
                    OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(std::string(kParamType) + nb);
                    param->setLabel(kParamTypeLabel);
                    param->setHint(kParamTypeHint);
                    assert(param->getNOptions() == ShadertoyPlugin::eUniformTypeNone);
                    param->appendOption(kParamTypeOptionNone);
                    assert(param->getNOptions() == ShadertoyPlugin::eUniformTypeBool);
                    param->appendOption(kParamTypeOptionBool);
                    assert(param->getNOptions() == ShadertoyPlugin::eUniformTypeInt);
                    param->appendOption(kParamTypeOptionInt);
                    assert(param->getNOptions() == ShadertoyPlugin::eUniformTypeFloat);
                    param->appendOption(kParamTypeOptionFloat);
                    assert(param->getNOptions() == ShadertoyPlugin::eUniformTypeVec2);
                    param->appendOption(kParamTypeOptionVec2);
                    assert(param->getNOptions() == ShadertoyPlugin::eUniformTypeVec3);
                    param->appendOption(kParamTypeOptionVec3);
                    assert(param->getNOptions() == ShadertoyPlugin::eUniformTypeVec4);
                    param->appendOption(kParamTypeOptionVec4);
                    param->setEvaluateOnChange(true);
                    param->setAnimates(false);
                    param->setLayoutHint(eLayoutHintNoNewLine, 1);
                    if (page) {
                        page->addChild(*param);
                    }
                    if (pgroup) {
                        param->setParent(*pgroup);
                    }
                }
                {
                    OFX::StringParamDescriptor* param = desc.defineStringParam(std::string(kParamName) + nb);
                    param->setLabel(kParamNameLabel);
                    param->setHint(kParamNameHint);
                    param->setEvaluateOnChange(true);
                    param->setAnimates(false);
                    param->setLayoutHint(eLayoutHintNoNewLine, 1);
                    if (page) {
                        page->addChild(*param);
                    }
                    if (pgroup) {
                        param->setParent(*pgroup);
                    }
                }
                {
                    OFX::StringParamDescriptor* param = desc.defineStringParam(std::string(kParamLabel) + nb);
                    param->setLabel(kParamLabelLabel);
                    param->setHint(kParamLabelHint);
                    param->setEvaluateOnChange(false);
                    param->setAnimates(false);
                    if (page) {
                        page->addChild(*param);
                    }
                    if (pgroup) {
                        param->setParent(*pgroup);
                    }
                }
                {
                    OFX::StringParamDescriptor* param = desc.defineStringParam(std::string(kParamHint) + nb);
                    param->setLabel(kParamHintLabel);
                    param->setHint(kParamHintHint);
                    param->setEvaluateOnChange(false);
                    param->setAnimates(false);
                    if (page) {
                        page->addChild(*param);
                    }
                    if (pgroup) {
                        param->setParent(*pgroup);
                    }
                }
                defineBoolean(desc, nb, page, pgroup);
                defineInt(desc, nb, page, pgroup);
                defineDouble(desc, nb, page, pgroup);
                defineDouble2D(desc, nb, page, pgroup);
                defineDouble3D(desc, nb, page, pgroup);
                defineRGBA(desc, nb, page, pgroup);
                
                if (page && pgroup) {
                    page->addChild(*pgroup);
                }
                if (sgroup) {
                    pgroup->setParent(*sgroup);
                }
            }

            if (page && sgroup) {
                page->addChild(*sgroup);
            }
            if (group) {
                sgroup->setParent(*group);
            }
        }

        if (page && group) {
            page->addChild(*group);
        }
    }


#if defined(OFX_SUPPORTS_OPENGLRENDER) && defined(HAVE_OSMESA)
    {
        OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(kParamEnableGPU);
        param->setLabel(kParamEnableGPULabel);
        param->setHint(kParamEnableGPUHint);
        const OFX::ImageEffectHostDescription &gHostDescription = *OFX::getImageEffectHostDescription();
        // Resolve advertises OpenGL support in its host description, but never calls render with OpenGL enabled
        if ( gHostDescription.supportsOpenGLRender && (gHostDescription.hostName != "DaVinciResolveLite") ) {
            param->setDefault(true);
            if (gHostDescription.APIVersionMajor * 100 + gHostDescription.APIVersionMinor < 104) {
                // Switching OpenGL render from the plugin was introduced in OFX 1.4
                param->setEnabled(false);
            }
        } else {
            param->setDefault(false);
            param->setEnabled(false);
        }

        if (page) {
            page->addChild(*param);
        }
    }
#endif
#if defined(HAVE_OSMESA)
    if ( ShadertoyPlugin::OSMesaDriverSelectable() ) {
        OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamCPUDriver);
        param->setLabel(kParamCPUDriverLabel);
        param->setHint(kParamCPUDriverHint);
        assert(param->getNOptions() == ShadertoyPlugin::eCPUDriverSoftPipe);
        param->appendOption(kParamCPUDriverOptionSoftPipe);
        assert(param->getNOptions() == ShadertoyPlugin::eCPUDriverLLVMPipe);
        param->appendOption(kParamCPUDriverOptionLLVMPipe);
        param->setDefault(kParamCPUDriverDefault);
        param->setAnimates(false);
        if (page) {
            page->addChild(*param);
        }
    }

#endif

    {
        OFX::PushButtonParamDescriptor* param = desc.definePushButtonParam(kParamRendererInfo);
        param->setLabel(kParamRendererInfoLabel);
        param->setHint(kParamRendererInfoHint);
        if (page) {
            page->addChild(*param);
        }
    }
} // ShadertoyPluginFactory::describeInContext

OFX::ImageEffect*
ShadertoyPluginFactory::createInstance(OfxImageEffectHandle handle,
                                       OFX::ContextEnum /*context*/)
{
    return new ShadertoyPlugin(handle);
}

static ShadertoyPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)

#endif // OFX_SUPPORTS_OPENGLRENDER
