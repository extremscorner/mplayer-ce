/**
 * glGX.c - openGL wrapper for GX.
 *
 * Copyright (c) 2009 Rhys "Shareese" Koedijk
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * See 'LICENSE' included within this release
 */

#ifndef _GLGX_H_
#define _GLGX_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Miscellaneous
 */

const char *extensions = "";

bool texture2DEnabled = false;
bool lineStippleEnabled = false;
bool blendEnabled = false;
bool lineSmoothEnabled = false;
bool pointSmoothEnabled = false;
bool polygonSmoothEnabled = false;
bool depthTestEnabled = false;
bool cullModeEnabled = true;

GXColor clearColour = { 0, 0, 0, 0xFF };
f32 clearDepth = 0 * 0x00FFFFFF;

u8 cullMode = GX_CULL_ALL;

u32 lineStippleFactor = 0;
u16 lineStipplePattern = 0;

f32 pointsize = 1;

/**
 * Transformation
 */

//...

/**
 * Drawing
 */

u8 beginType = 0;
GXColor colour = { 0, 0, 0, 0xFF };

/**
 * Vertex arrays
 */

//...

/**
 * Lighting
 */

//...

/**
 * Textures
 */

typedef struct _GLtexture {
    GLuint name;
    GXTexObj obj;
    struct _GLtexture *prevTexture;
    struct _GLtexture *nextTexture;
} GLtexture;

GLtexture *textures = NULL;
GLtexture *texture1D = NULL;
GLtexture *texture2D = NULL;
u32 textureCount = 0;

/**
 * Texture mapping
 */

//...

#ifdef __cplusplus
}
#endif

#endif /* _GLGX_H_ */
