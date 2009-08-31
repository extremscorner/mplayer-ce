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

#define GX_MAX_TEXTURE_SIZE     1024
#define DEFAULT_FIFO_SIZE       256 * 1024

/**
 * GX
 */

GXRModeObj *rmode = NULL;
void *xfb[2] = { NULL, NULL };
u32 fb = 0;

unsigned char gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN(32);

/**
 * Miscellaneous
 */

bool texture2DEnabled = false;
bool lineStippleEnabled = false;
bool blendEnabled = false;
bool lineSmoothEnabled = false;
bool pointSmoothEnabled = false;
bool polygonSmoothEnabled = false;
bool depthTestEnabled = false;
bool cullFaceEnabled = true;

bool colourArrayEnabled = false;
bool edgeFlagArrayEnabled = false;
bool indexArrayEnabled = false;
bool normalArrayEnabled = false;
bool texCoordArrayEnabled = false;
bool vertexArrayEnabled = false;

u32 insideBeginEndPair = 0;

GXColor clearColour = { 0, 0, 0, 0 };
f32 clearDepth = 0 * 0x00FFFFFF;

u32 depthMode = GX_LESS;
u32 cullMode = GX_CULL_ALL;
u32 windingMode = GL_CCW;

u32 lineStippleFactor = 0;
u16 lineStipplePattern = 0;

f32 pointsize = 1;

u8 drawTEVRegister = GX_TEVREG0;
u8 readTEVRegister = GX_TEVREG0;

const char *extensions = "";

/**
 * Transformation
 */

Mtx view;
Mtx perspective;

/**
 * Drawing
 */

typedef struct _GLtexcoord {
    f32 s;
    f32 t;
} GLtexcoord;

typedef struct _GLnormal {
    f32 x;
    f32 y;
    f32 z;
} GLnormal;

typedef struct _GLvertex {
    f32 x;
    f32 y;
    f32 z;
    GXColor colour;
    GLnormal normal;
    GLtexcoord texCoord;
    struct _GLvertex *prevVertex;
    struct _GLvertex *nextVertex;
} GLvertex;

u8 primitiveType = GX_POINTS;

GXColor colour = { 0xFF, 0xFF, 0xFF, 0xFF };
GLnormal normal = { 0.0f, 0.0f, 0.0f };
GLtexcoord texCoord = { 0.0f, 0.0f };

GLvertex *verticies = NULL;
GLvertex *vertex = NULL;
u32 vertexCount = 0;

void glVerticiesInvalidateAll ();
void glVertexUpload (GLvertex *_vert);

/**
 * Vertex arrays
 */

typedef struct _GLvertexarray {
    GLint size;
    GLenum type;
    GLsizei stride;
    const GLvoid *ptr;
} GLvertexarray;

GLvertexarray colourArray = { 0 };
GLvertexarray normalArray = { 0 };
GLvertexarray texCoordArray = { 0 };
GLvertexarray vertexArray = { 0 };

f32 glVertexArrayGetCoord (GLvertexarray *vertexArray, int index);
void glVertexArrayNext (GLvertexarray *vertexArray);

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
    u8 minfilt;
    u8 magfilt;
    u8 wrap_s;
    u8 wrap_t;
    f32 priority;
    struct _GLtexture *prevTexture;
    struct _GLtexture *nextTexture;
} GLtexture;

GLtexture *textures = NULL;
GLtexture *texture1D = NULL;
GLtexture *texture2D = NULL;
u8 texture1DMap = GX_TEXMAP0;
u8 texture2DMap = GX_TEXMAP1;
u32 textureCount = 0;

GLuint glTextureNextFreeName ();
GLtexture *glTextureGet (GLuint _name);

/**
 * Texture mapping
 */

//...

#ifdef __cplusplus
}
#endif

#endif /* _GLGX_H_ */
