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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

#include <ogcsys.h>
#include <gctypes.h>
#include <gccore.h>

#include "GL/gl.h"
#include "GL/glGX.h"
#include "wipemalloc.h"

/**
 * GX
 */

void gxInit ()
{
    // Initialise the video system
    VIDEO_Init();
    
    // Obtain the preferred video mode from the system
    rmode = VIDEO_GetPreferredMode(NULL);
    
#if defined(__wii__)

    // Widescreen fix
    if (CONF_GetAspectRatio() == CONF_ASPECT_16_9) {
        rmode->viWidth = VI_MAX_WIDTH_PAL - 12;
        rmode->viXOrigin = ((VI_MAX_WIDTH_PAL - rmode->viWidth) / 2) + 2;
    }

#endif

    // Set the video mode
    VIDEO_Configure(rmode);
    
    // Allocate the framebuffers (double buffered)
    xfb[0] = (u32 *) MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    xfb[1] = (u32 *) MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    
    // Clear the framebuffers and line up the first buffer for display
    VIDEO_ClearFrameBuffer(rmode, xfb[0], COLOR_BLACK);
    VIDEO_ClearFrameBuffer(rmode, xfb[1], COLOR_BLACK);
    VIDEO_SetNextFramebuffer(xfb[0]);
    
    // Flush the display
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();
    
    memset(gp_fifo, 0, GX_DEFAULT_FIFO_SIZE);
    GX_Init(gp_fifo, GX_DEFAULT_FIFO_SIZE);
    
    // ===========================================================================
    
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    f32 yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
    u32 xfbHeight = GX_SetDispCopyYScale(yscale);
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, xfbHeight);
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetFieldMode(rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE:GX_DISABLE));
    
    GX_SetCullMode(GX_CULL_ALL);
    GX_CopyDisp(xfb[fb], GX_TRUE);
    GX_SetDispCopyGamma(GX_GM_1_0);
    
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_NRM, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0); // vertex
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0); // normals
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGB8, 0); // color
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0); // texture
    
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    
    GX_SetNumTexGens(1);
    GX_InvalidateTexAll();
    
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    
    guVector cam = { 0.0F, 0.0F, 0.0F },
              up = { 0.0F, 1.0F, 0.0F },
            look = { 0.0F, 0.0F, -1.0F };

    guLookAt(view, &cam, &up, &look);
    
    // ===========================================================================
    
    // Initialise openGL
    glInit();
}

void gxDestroy ()
{
    
}

void gxSwapBuffers ()
{
    // Flip the framebuffer and flush the display
    fb ^= 1;
    GX_DrawDone();
    GX_SetColorUpdate(GX_TRUE);
    GX_CopyDisp(xfb[fb], GX_TRUE);
    VIDEO_SetNextFramebuffer(xfb[fb]);
    VIDEO_Flush();
    VIDEO_WaitVSync();
}

void glInit ()
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    
    //...
}

/**
 * Miscellaneous
 */

void glClearColor (GLclampf _red,
                   GLclampf _green,
                   GLclampf _blue,
                   GLclampf _alpha)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // Set the clear colour
    clearColour.r = _red * 255;
    clearColour.g = _green * 255;
    clearColour.b = _blue * 255;
    clearColour.a = _alpha * 255;
    GX_SetCopyClear(clearColour, clearDepth);
}

void glClear (GLbitfield _mask)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // Figure out which buffer we are to clear
    switch (_mask) {
        case GL_COLOR_BUFFER_BIT: /* ??? */ break;
        case GL_DEPTH_BUFFER_BIT: /* ??? */ break;
        case GL_ACCUM_BUFFER_BIT: /* ??? */ break;
        case GL_STENCIL_BUFFER_BIT: /* ??? */ break;
        default: return; /* GL_INVALID_VALUE */
    }
    
    // Clear the specified buffer
    // TODO: This correctly...
    GX_SetCopyClear(clearColour, clearDepth);
}

void glBlendFunc (GLenum _sfactor, GLenum _dfactor)
{    
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // Determine the source blending mode
    switch (_sfactor) {
        case GL_ZERO: blendModeSrc = GX_BL_ZERO; break;
        case GL_ONE: blendModeSrc = GX_BL_ONE; break;
        case GL_SRC_COLOR: blendModeSrc = GX_BL_SRCCLR; break;
        case GL_ONE_MINUS_SRC_COLOR: blendModeSrc = GX_BL_SRCCLR; break; /* ??? */
        case GL_DST_COLOR: blendModeSrc = GX_BL_DSTCLR; break;
        case GL_ONE_MINUS_DST_COLOR: blendModeSrc = GX_BL_DSTCLR; break; /* ??? */
        case GL_SRC_ALPHA: blendModeSrc = GX_BL_SRCALPHA; break;
        case GL_ONE_MINUS_SRC_ALPHA: blendModeSrc = GX_BL_SRCALPHA; break; /* ??? */
        case GL_DST_ALPHA: blendModeSrc = GX_BL_DSTALPHA; break;
        case GL_ONE_MINUS_DST_ALPHA: blendModeSrc = GX_BL_DSTALPHA; break; /* ??? */
        default: return; /* GL_INVALID_ENUM */
    }
    
    // Determine the destination blending mode
    switch (_dfactor) {
        case GL_ZERO: blendModeDst = GX_BL_ZERO; break;
        case GL_ONE: blendModeDst = GX_BL_ONE; break;
        case GL_SRC_COLOR: blendModeDst = GX_BL_SRCCLR; break;
        case GL_ONE_MINUS_SRC_COLOR: blendModeDst = GX_BL_SRCCLR; break; /* ??? */
        case GL_DST_COLOR: blendModeDst = GX_BL_DSTCLR; break;
        case GL_ONE_MINUS_DST_COLOR: blendModeDst = GX_BL_DSTCLR; break; /* ??? */
        case GL_SRC_ALPHA: blendModeDst = GX_BL_SRCALPHA; break;
        case GL_ONE_MINUS_SRC_ALPHA: blendModeDst = GX_BL_SRCALPHA; break; /* ??? */
        case GL_DST_ALPHA: blendModeDst = GX_BL_DSTALPHA; break;
        case GL_ONE_MINUS_DST_ALPHA: blendModeDst = GX_BL_DSTALPHA; break; /* ??? */
        default: return; /* GL_INVALID_ENUM */
    }
    
    // Set the blend mode (if enabled)
    if (blendEnabled)
        GX_SetBlendMode(GX_BM_BLEND, blendModeSrc, blendModeDst, GX_LO_CLEAR);
}

void glCullFace (GLenum _mode)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // Determine the culling mode
    switch (_mode) {
        case GL_FRONT: cullMode = GX_CULL_FRONT;
        case GL_BACK: cullMode = GX_CULL_BACK;
        case GL_FRONT_AND_BACK: cullMode = GX_CULL_ALL;
        default: return; /* GL_INVALID_ENUM */
    }
    
    // Set the culling mode (if enabled)
    if (cullFaceEnabled)
        GX_SetCullMode(cullMode);
}

void glPointSize (GLfloat _size)
{
    // Sanity check
    if (_size <= 0)
        return; /* GL_INVALID_VALUE */
    
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // Set the point size
    GX_SetPointSize(_size, GX_TO_ZERO);
}

void glLineWidth (GLfloat _width)
{
    // Sanity check
    if (_width <= 0)
        return; /* GL_INVALID_VALUE */
    
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // Set the line width
    GX_SetLineWidth(_width, GX_TO_ZERO);
}

void glLineStipple (GLint _factor, GLushort _pattern)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // Set the line stipple mode
    lineStippleFactor = _factor;
    lineStipplePattern = _pattern;
    
    if (lineStippleEnabled) {
        //...
    }
}

void glDrawBuffer (GLenum _mode)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    switch (_mode) {
        
        case GL_NONE:
            /* ??? */
            break;
            
        case GL_FRONT_AND_BACK:
            /* ??? */
            break;
            
        case GL_FRONT:
        case GL_FRONT_LEFT:
        case GL_FRONT_RIGHT:
            drawTEVRegister = GX_TEVREG0;
            break;
        case GL_BACK:
        case GL_BACK_LEFT:
        case GL_BACK_RIGHT:
            drawTEVRegister = GX_TEVREG2;
            break;
            
        case GL_LEFT: break; /* ??? */
        case GL_RIGHT: break; /* ??? */
    
        default: return; /* GL_INVALID_ENUM */
        
    }
}

void glReadBuffer (GLenum _mode)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    switch (_mode) {

        case GL_FRONT:
        case GL_FRONT_LEFT:
        case GL_FRONT_RIGHT:
            readTEVRegister = GX_TEVREG0;
            break;
        case GL_BACK:
        case GL_BACK_LEFT:
        case GL_BACK_RIGHT:
            readTEVRegister = GX_TEVREG2;
            break;
            
        case GL_LEFT: break; /* ??? */
        case GL_RIGHT: break; /* ??? */
        
        default: return; /* GL_INVALID_ENUM */
        
    }
}

void glEnable(GLenum _type)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // Enable the specified server-side capability
    switch (_type) {
        case GL_TEXTURE_2D:
            texture2DEnabled = true;
            GX_SetNumTexGens(1);
            GX_SetTevOp(GX_TEVSTAGE0 + tevStage, tevMode);
            break;
        case GL_LINE_STIPPLE:
            lineStippleEnabled = true;
            break;
        case GL_BLEND:
            blendEnabled = true;
            GX_SetBlendMode(GX_BM_NONE, blendModeSrc, blendModeDst, GX_LO_CLEAR);
            GX_SetAlphaUpdate(GX_TRUE);
            break;
        case GL_LINE_SMOOTH:
            lineSmoothEnabled = true;
            break;
        case GL_POINT_SMOOTH:
            pointSmoothEnabled = true;
            break;
        case GL_POLYGON_SMOOTH:
            polygonSmoothEnabled = true;
            break;
        case GL_CULL_FACE:
            cullFaceEnabled = true;
            GX_SetCullMode(cullMode);
            break;
        case GL_DEPTH_TEST:
            depthTestEnabled = true;
            GX_SetZMode(GX_TRUE, depthMode, GX_TRUE /* ??? */);
            break;
        default: return; /* GL_INVALID_ENUM */
    };
}

void glDisable (GLenum _type)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // Disable the specified server-side capability
    switch (_type) {
        case GL_TEXTURE_2D:
            texture2DEnabled = false;
            GX_SetNumTexGens(0);
            GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);         
            break;
        case GL_LINE_STIPPLE:
            lineStippleEnabled = false;
            break;
        case GL_BLEND:
            blendEnabled = false;
            GX_SetBlendMode(GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_CLEAR);
            GX_SetAlphaUpdate(GX_FALSE);
            break;
        case GL_LINE_SMOOTH:
            lineSmoothEnabled = false;
            break;
        case GL_POINT_SMOOTH:
            pointSmoothEnabled = false;
            break;
        case GL_POLYGON_SMOOTH:
            polygonSmoothEnabled = false;
            break;
        case GL_CULL_FACE:
            cullFaceEnabled = false;
            GX_SetCullMode(GX_CULL_NONE);
            break;
        case GL_DEPTH_TEST:
            depthTestEnabled = false;
            GX_SetZMode(GX_FALSE, GX_NEVER, GX_FALSE);
            break;
        default: return; /* GL_INVALID_ENUM */
    };
}

void glEnableClientState (GLenum _cap)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // Enable the specified client-side state
    switch (_cap) {
        case GL_COLOR_ARRAY: colourArrayEnabled = true; break;
        case GL_EDGE_FLAG_ARRAY: edgeFlagArrayEnabled = true; break;
        case GL_INDEX_ARRAY: indexArrayEnabled = true; break;
        case GL_NORMAL_ARRAY: normalArrayEnabled = true; break;
        case GL_TEXTURE_COORD_ARRAY: texCoordArrayEnabled = true; break;
        case GL_VERTEX_ARRAY: vertexArrayEnabled = true; break;
        default: break;
    }
}

void glDisableClientState (GLenum _cap)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // Disable the specified client-side state
    switch (_cap) {
        case GL_COLOR_ARRAY: colourArrayEnabled = false; break;
        case GL_EDGE_FLAG_ARRAY: edgeFlagArrayEnabled = false; break;
        case GL_INDEX_ARRAY: indexArrayEnabled = false; break;
        case GL_NORMAL_ARRAY: normalArrayEnabled = false; break;
        case GL_TEXTURE_COORD_ARRAY: texCoordArrayEnabled = false; break;
        case GL_VERTEX_ARRAY: vertexArrayEnabled = false; break;
        default: break;
    }
}

void glGetIntegerv (GLenum _pname, GLint *_params)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // Sanity check
    if (!_params)
        return; /* GL_INVALID_VALUE */
        
    // Get the specified integer constant
    switch (_pname) {
        case GL_MAX_TEXTURE_SIZE: *_params = GX_MAX_TEXTURE_SIZE; break;
        default: return; /* GL_INVALID_ENUM */
    }
}

const GLubyte *glGetString (GLenum _name)
{
    // Sanity check
    if (insideBeginEndPair)
        return NULL; /* GL_INVALID_OPERATION */
    
    // Get the specified string constant
    switch (_name) {
        case GL_EXTENSIONS: return (const GLubyte *) extensions;
        default: return NULL; /* GL_INVALID_ENUM */
    }
}

void glFlush ()
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // Finished drawing
    GX_DrawDone();
}

/**
 * Transformation
 */

void glMatrixIdentity (Mtx44 mtx)
{
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            if (i == j)
                mtx[i][j] = 1.0f;
            else
                mtx[i][j] = 0.0f;
}

void glMatrixCopy (Mtx44 src, Mtx44 dst)
{
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            dst[i][j] = src[i][j];
}

void glMatrixMode(GLenum _mode)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // Determine the current matrix stack
    switch (_mode) {
        case GL_MODELVIEW:
            matrixStack = modelViewMatrixStack;
            matrixStackDepth = &modelViewMatrixStackDepth;
            break;
        case GL_PROJECTION:
            matrixStack = projectionMatrixStack;
            matrixStackDepth = &projectionMatrixStackDepth;
            break;
        case GL_TEXTURE:
            matrixStack = textureMatrixStack;
            matrixStackDepth = &textureMatrixStackDepth;
            break;
        default: return; /* GL_INVALID_ENUM */
    }
    
    // Grab the current matrix
    matrix = &matrixStack[*matrixStackDepth];
}

void glOrtho (GLdouble _left, GLdouble _right,
              GLdouble _bottom, GLdouble _top,
              GLdouble _near_val, GLdouble _far_val)
{
    Mtx44 temp;
    
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // Sanity check
    if (!matrix ||
        _left == _right ||
        _bottom == _top ||
        _near_val == _far_val)
        return;
    
    // Multiply the current matrix with an orthographic matrix
    guOrtho(temp, _top, _bottom, _left, _right, _near_val, _far_val);
    guMtxConcat(*matrix, temp, *matrix); /* ??? */
    
    // Set the orthographic matrix
    GX_LoadProjectionMtx(*matrix, GX_ORTHOGRAPHIC);
}

void glViewport (GLint _x, GLint _y,
                 GLsizei _width, GLsizei _height)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // Sanity check
    if (_width < 0 || _height < 0)
        return; /* GL_INVALID_VALUE */
    
    // Set the viewport
    GX_SetViewport(_x, _y, _width, _height, 0, 1);
    GX_SetScissor(_x, _y, _width, _height);
}

void glPopMatrix (void)
{    
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // Sanity check
    if (!matrix || !matrixStack || !matrixStackDepth)
        return; /* GL_INVALID_VALUE */

    // Sanity check
    if (*matrixStackDepth <= 0)
        return; /* GL_UNDERFLOW */

    // Pop the current matrix from the current stack
    matrix = &matrixStack[*matrixStackDepth--];
}

void glPushMatrix (void)
{
    Mtx44 *new_matrix = NULL;
    
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // Sanity check
    if (!matrix || !matrixStack || !matrixStackDepth)
        return; /* GL_INVALID_VALUE */

    // Sanity check
    if (*matrixStackDepth >= (GL_MAX_STACK_SIZE - 1))
        return; /* GL_OVERFLOW */

    // Push a new matrix onto the current stack
    new_matrix = &matrixStack[*matrixStackDepth++];
    glMatrixCopy(*matrix, *new_matrix);
    matrix = new_matrix;
}

void glLoadIdentity (void)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // Sanity check
    if (!matrix)
        return /* GL_INVALID_VALUE */
        
    // Replace the current matrix with the identity matrix
    glMatrixIdentity(*matrix);
}

void glTranslatef (GLfloat _x, GLfloat _y, GLfloat _z)
{
    Mtx temp;
    
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // Multiply the current matrix by the translation matrix
    guMtxIdentity(temp);
    guMtxTrans(temp, _x, _y, _z);
    guMtxConcat(*matrix, temp, *matrix);
}

void glRotatef (GLfloat _angle, GLfloat _x, GLfloat _y, GLfloat _z)
{
    Mtx temp;
    guVector axis;
    
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // Build the axis of rotation
    axis.x = _x;
    axis.y = _y;
    axis.z = _z;
    
    // Multiply the current matrix by the rotation matrix
    guMtxIdentity(temp);
    guMtxRotAxisDeg(temp, &axis, _angle);
    guMtxConcat(*matrix, temp, *matrix);
}

void glScalef(GLfloat _x, GLfloat _y, GLfloat _z)
{
    Mtx temp;
    
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // Multiply the current matrix by the general scaling matrix
    guMtxIdentity(temp);
    guMtxScale(temp, _x, _y, _z);
    guMtxConcat(*matrix, temp, *matrix);
}

/**
 * Drawing
 */

void glBegin (GLenum _type)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // We are now in a begin/end pair
    insideBeginEndPair = true;
    
    // Destory all verticies
    glVerticiesInvalidateAll();
    
    // Determine the primitive type
    switch (_type) {
        case GL_POINTS: primitiveType = GX_POINTS; break;
        case GL_LINES: primitiveType = GX_LINES; break;
        case GL_LINE_STRIP: primitiveType = GX_LINESTRIP; break;
        case GL_LINE_LOOP: primitiveType = GX_POINTS; break; /* ??? */
        case GL_TRIANGLES: primitiveType = GX_TRIANGLES; break;
        case GL_TRIANGLE_STRIP: primitiveType = GX_TRIANGLESTRIP; break;
        case GL_TRIANGLE_FAN: primitiveType = GX_TRIANGLEFAN; break;
        case GL_QUADS: primitiveType = GX_QUADS; break; 
        case GL_QUAD_STRIP: primitiveType = GX_POINTS; break; /* ??? */
        case GL_POLYGON: primitiveType = GX_POINTS; break; /* ??? */
        default: return; /* GL_INVALID_ENUM */
    };
}

void glEnd (void)
{
    u16 count = 0;
    
    // Sanity check
    if (!insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // Short circuit case were we don't actually have to do anything
    if (vertexCount == 0)
        return;

    // Map the currently bound 2D texture (if enabled)
    if (texture2DEnabled) {
        GX_LoadTexObj(&texture2D->obj, texture2DMap);
    }

    // Determine the number of verticies we are to upload (depends on culling mode)
    if (cullFaceEnabled)
        count = vertexCount;
    else
        count = vertexCount * 2;
    
    // ===========================================================================
    GX_Begin(primitiveType, GX_VTXFMT0, count);
    // ===========================================================================

    bool cw = true;
    bool ccw = true;
    GLvertex *vert = verticies;
    
    if (cullFaceEnabled) {
        cw = false;
        ccw = false;                            
        switch (windingMode) {
            case GL_CW: cw = true; break;
            case GL_CCW: ccw = true; break;
        }                         
    }
    
    // CW
    if (cw) {
        for (; vert; vert = vert->next) {
            glVertexUpload(vert);        
        }
    }
    
    // CCW
    if (ccw) {
        for (; vert && vert->next; vert = vert->next);
        for (; vert; vert = vert->prev) {
            glVertexUpload(vert);        
        }
    }
    
    // ===========================================================================
    GX_End();
    // ===========================================================================

    // Destory all verticies
    glVerticiesInvalidateAll();
    
    // We are no longer in a begin/end pair
    insideBeginEndPair = false;    
}

void glVerticiesInvalidateAll ()
{
    GLvertex *vert = verticies;
    GLvertex *nextVert = NULL;
    
    // Destroy all verticies
    while (vert) {
        nextVert = vert->next;
        wipefree(vert);
        vert = nextVert;
    }
    
    // Reset verticies
    verticies = NULL;
    vertex = NULL;
    vertexCount = 0;
}

void glVertexUpload (GLvertex *_vert)
{
    DCFlushRange(_vert, sizeof(GLvertex));
    GX_Position3f32(_vert->x, _vert->y, _vert->z); 
    GX_Normal3f32(_vert->normal.x, _vert->normal.y, _vert->normal.z);
    GX_Color3f32(_vert->colour.r, _vert->colour.g, _vert->colour.b);
    GX_TexCoord2f32(_vert->texCoord.s, _vert->texCoord.t);
}

void glVertex2f (GLfloat _x, GLfloat _y)
{
    glVertex3f(_x, _y, 0);
}

void glVertex3f (GLfloat _x, GLfloat _y, GLfloat _z)
{
    // Allocate the vertex
    GLvertex *vert = wipemalloc(sizeof(GLvertex));
    if (!vert)
        return;
    
    // Setup the vertex
    vert->x = _x;
    vert->y = _y;
    vert->x = _z;
    vert->colour.r = colour.r;
    vert->colour.g = colour.g;
    vert->colour.b = colour.b;
    vert->colour.a = colour.a;
    vert->normal.x = normal.x;
    vert->normal.y = normal.y;
    vert->normal.z = normal.z;
    vert->texCoord.s = texCoord.s;
    vert->texCoord.t = texCoord.t;
    
    // Insert the vertex into the double-linked FILO list of allocated verticies
    if (verticies) {
        vert->next = verticies;
        verticies->prev = vert;
    }
    verticies = vert;
    vertexCount++;
    
}

void glNormal3f (GLfloat _x, GLfloat _y, GLfloat _z)
{
    normal.x = _x;
    normal.y = _y;
    normal.z = _z;
}

void glColor3f (GLfloat _red, GLfloat _green, GLfloat _blue)
{
    // Set the current colour
    colour.r = _red * 255;
    colour.g = _green * 255;
    colour.b = _blue * 255;
    colour.a = 0.0f;
}

void glColor4f (GLfloat _red, GLfloat _green,
                GLfloat _blue, GLfloat _alpha)
{
    // Set the current colour
    colour.r = _red * 255;
    colour.g = _green * 255;
    colour.b = _blue * 255;
    colour.a = _alpha * 255;
}

void glTexCoord2f (GLfloat _s, GLfloat _t)
{
    texCoord.s = _s;
    texCoord.t = _t;
};

void glRectd (GLdouble _x1, GLdouble _y1, GLdouble _x2, GLdouble _y2)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // TODO: Disable depth buffer?, rectangle z should be 0
    //glDisable(GL_DEPTH_TEST);
    
    // If the second vertex is above and to the right of the first
    // then build the rectangle with counterclockwise winding
    if (_x2 > _x1 && _y2 > _y1) {
        glBegin(GX_QUADS);
        glVertex2f(_x1, _y2);
        glVertex2f(_x2, _y2);
        glVertex2f(_x2, _y1);
        glVertex2f(_x1, _y1);
        glEnd();
    } else {
        glBegin(GX_QUADS);
        glVertex2f(_x1, _y1);
        glVertex2f(_x2, _y1);
        glVertex2f(_x2, _y2);
        glVertex2f(_x1, _y2);
        glEnd();
    }
}

/**
 * Vertex arrays
 */

void glVertexPointer (GLint _size, GLenum _type,
                      GLsizei _stride, const GLvoid *_ptr)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // Sanity check
    if ((_size != 2 && _size != 3 && _size != 4) ||
        _stride < 0 ||
        !_ptr)
        return; /* GL_INVALID_VALUE */
    
    // Set the vertex array
    vertexArray.size = _size;
    vertexArray.type = _type;
    vertexArray.stride = _stride;
    vertexArray.ptr = _ptr;    
}

void glColorPointer (GLint _size, GLenum _type,
                     GLsizei _stride, const GLvoid *_ptr)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // Sanity check
    if ((_size != 2 && _size != 3 && _size != 4) ||
        _stride < 0 ||
        !_ptr)
        return; /* GL_INVALID_VALUE */
        
    // Set the colour array
    colourArray.size = _size;
    colourArray.type = _type;
    colourArray.stride = _stride;
    colourArray.ptr = _ptr;
}

void glTexCoordPointer (GLint _size, GLenum _type,
                        GLsizei _stride, const GLvoid *_ptr)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // Sanity check
    if ((_size != 2 && _size != 3 && _size != 4) ||
        _stride < 0 ||
        !_ptr)
        return; /* GL_INVALID_VALUE */
    
    // Set the texture coordinate array
    texCoordArray.size = _size;
    texCoordArray.type = _type;
    texCoordArray.stride = _stride;
    texCoordArray.ptr = _ptr;
}

f32 glVertexArrayGetCoord (GLvertexarray *vertexArray, int index)
{
    switch (vertexArray->type) {
        case GL_SHORT: {
            GLshort *val = (GLshort *) (vertexArray->ptr + (index * (vertexArray->stride + sizeof(GLshort))));
            return (f32) *val;
        }
        case GL_INT: {
            GLint *val = (GLint *) (vertexArray->ptr + (index * (vertexArray->stride + sizeof(GLint))));
            return (f32) *val;
        }
        case GL_FLOAT: {
            GLfloat *val = (GLfloat *) (vertexArray->ptr + (index * (vertexArray->stride + sizeof(GLfloat))));
            return (f32) *val;
        }
        case GL_DOUBLE: {
            GLdouble *val = (GLdouble *) (vertexArray->ptr + (index * (vertexArray->stride + sizeof(GLdouble))));
            return (f32) *val;
        }
    }
    
    return 0;
}

void glVertexArrayNext (GLvertexarray *vertexArray)
{
    switch (vertexArray->type) {
        case GL_SHORT: vertexArray->ptr += (vertexArray->stride + sizeof(GLshort));
        case GL_INT: vertexArray->ptr += (vertexArray->stride + sizeof(GLint));
        case GL_FLOAT: vertexArray->ptr += (vertexArray->stride + sizeof(GLfloat));
        case GL_DOUBLE: vertexArray->ptr += (vertexArray->stride + sizeof(GLdouble));
    }
}

void glDrawArrays (GLenum _mode, GLint _first, GLsizei _count)
{
    const void *ptr = NULL;
    int i;
    
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
    
    // Sanity check
    if (_count < 0)
        return; /* GL_INVALID_VALUE */

    // Short circuit cases were we don't actually have to do anything
    if (!vertexArrayEnabled)
        return;
    
    // Let us begin...
    glBegin(_mode);
    
    if (colourArrayEnabled) {
        ptr = colourArray.ptr;
        for (i = _first; i < _count; i++) {
            switch (colourArray.size) {
                case 3:
                    glColor3f(glVertexArrayGetCoord(&colourArray, 0),
                              glVertexArrayGetCoord(&colourArray, 1),
                              glVertexArrayGetCoord(&colourArray, 2)); 
                    break;
                case 4:
                    glColor4f(glVertexArrayGetCoord(&colourArray, 0),
                              glVertexArrayGetCoord(&colourArray, 1),
                              glVertexArrayGetCoord(&colourArray, 2),
                              glVertexArrayGetCoord(&colourArray, 3)); 
                    break;
            }
            glVertexArrayNext(&colourArray);
        }
    }
    
    if (normalArrayEnabled) {
        ptr = normalArray.ptr;
        for (i = _first; i < _count; i++) {
            switch (normalArray.size) {
                case 3:
                    glNormal3f(glVertexArrayGetCoord(&normalArray, 0),
                               glVertexArrayGetCoord(&normalArray, 1),
                               glVertexArrayGetCoord(&normalArray, 2)); 
                    break;
            }
            glVertexArrayNext(&normalArray);
        }
    }
    
    if (texCoordArrayEnabled) {
        ptr = texCoordArray.ptr;
        for (i = _first; i < _count; i++) {
            switch (normalArray.size) {
                case 2:
                    glTexCoord2f(glVertexArrayGetCoord(&texCoordArray, 0),
                                 glVertexArrayGetCoord(&texCoordArray, 1)); 
                    break;
            }
            glVertexArrayNext(&texCoordArray);
        }
    }
    
    if (vertexArrayEnabled) {
        ptr = vertexArray.ptr;
        for (i = _first; i < _count; i++) {
            switch (vertexArray.size) {
                case 2:
                    glVertex2f(glVertexArrayGetCoord(&vertexArray, 0),
                               glVertexArrayGetCoord(&vertexArray, 1)); 
                    break;
                case 3:
                    glVertex3f(glVertexArrayGetCoord(&vertexArray, 0),
                               glVertexArrayGetCoord(&vertexArray, 1),
                               glVertexArrayGetCoord(&vertexArray, 2)); 
                    break;
            }
            glVertexArrayNext(&vertexArray);
        }
    }
    
    // All done
    glEnd();
    
}

void glInterleavedArrays (GLenum _format, GLsizei _stride,
                          const GLvoid *_pointer)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // Sanity check
    if (_stride < 0)
        return; /* GL_INVALID_VALUE */
    
    //...
}

/**
 * Lighting
 */

void glShadeModel (GLenum _mode)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // Determine the shading mode
    switch (_mode) {
        case GL_FLAT: /* ??? */ break;
        case GL_SMOOTH: /* ??? */ break;
        default: return; /* GL_INVALUD_ENUM */
    }
}

/**
 * Textures
 */

GLuint glTextureNextFreeName ()
{
    int name = 0;
    GLtexture *tex = textures;
    
    // Iterate through all textures till an unused name is found
    while (tex) {
        if (tex->name == name) {
            tex = textures;
            name++;
        } else {
            tex = tex->next;
        }
    }
    
    return name;
}

void glGenTextures (GLsizei _n, GLuint *_textures)
{
    int i;
    
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // Sanity check
    if (_n < 0 || !_textures)
        return; /* GL_INVALID_VALUE */
    
    // Allocate the specified number of textures
    for(i = 0; i < _n; i++) {
        
        // Allocate the next texture
        GLtexture *tex = wipemalloc(sizeof(GLtexture));
        if (!tex) {
            _textures[i] = 0;
            continue;
        }
        
        // Setup the texture
        tex->name = glTextureNextFreeName();
        tex->wrap_s = GX_REPEAT;
        tex->wrap_t = GX_REPEAT;
        tex->minfilt = GX_NEAR_MIP_LIN;
        tex->magfilt = GX_LINEAR;
        tex->priority = 0.0f;
        
        // Insert the texture into the double-linked FILO list of allocated textures
        if (textures) {
            tex->next = textures;
            textures->prev = tex;
        }
        textures = tex;
        textureCount++;
        
        // Pass the textures name back to our caller
        _textures[i] = tex->name;
        
    }
    
}

void glDeleteTextures (GLsizei _n, const GLuint *_textures)
{
    int i;
    
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // Sanity check
    if (_n < 0 || !_textures)
        return; /* GL_INVALID_VALUE */
    
    // Destroy the specified textures
    for(i = 0; i < _n; i++) {
        GLtexture *tex = glTextureGet(_textures[i]);
        if (tex) {
            
            // TODO: Destroy tex->obj?
            
            // Remove the texture from the double-linked FILO list of allocated textures
            textureCount--;
            if (tex->next)
                tex->next->prev = tex->prev;
            if (tex->prev)
                tex->prev->next = tex->next;
            else
                textures = tex->next;
            
            // Free the texture
            wipefree(tex);
            
        }
    }
    
}

GLtexture *glTextureGet (GLuint _name)
{
    GLtexture *tex = textures;
    
    // Find the texture with the specified name (if possible)
    while (tex) {
        if (tex->name == _name)
            return tex;
        tex = tex->next;
    }
    
    return NULL;
}

void glBindTexture (GLenum _target, GLuint _texture)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // Bind the texture to the specified target
    switch (_target) {
        case GL_TEXTURE_1D: texture1D = glTextureGet(_texture); break;
        case GL_TEXTURE_2D: texture2D = glTextureGet(_texture); break;
        default: return; /* GL_INVALUD_ENUM */
    }
}

void glTexImage2D (GLenum _target, GLint _level,
                   GLint _internalFormat,
                   GLsizei _width, GLsizei _height,
                   GLint _border, GLenum _format, GLenum _type,
                   const GLvoid *_pixels)
{
    u8 format;

    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // Sanity check
    if (!texture2D ||
        _level < 0 || _level > log(GX_MAX_TEXTURE_SIZE) ||
        _width < 64 || _width > GX_MAX_TEXTURE_SIZE + 2 ||
        _height < 64 || _height > GX_MAX_TEXTURE_SIZE + 2)
        return; /* GL_INVALID_VALUE */
    
    // Determine the textures format
    switch (_internalFormat) {
        
        case 0: /* ??? */ break;
        
        case 4:
        case GL_RGBA:
        case GL_RGBA8: format = GX_TF_RGBA8; break;
        
        // Everything else, unsupported...
        // TODO: Support more formats!?
        default: return; /* GL_INVALID_VALUE */
        
    }

    // Determine the format of the pixel data
    switch (_format) {
        case GL_LUMINANCE: /* ??? */ break;
        case GL_RGBA: break;
        
        // Everything else, unsupported...
        // TODO: Support more formats!?
        default: return; /* GL_INVALID_VALUE */
        
    }
    
    // Determine the data type of the pixel data
    switch (_type) {
        case GL_UNSIGNED_BYTE: break;
        case GL_FLOAT: /* ??? */ break;
        
        // Everything else, unsupported...
        // TODO: Support more formats!?
        default: return; /* GL_INVALID_VALUE */
        
    }
    
    // TODO: Borders!?
    
    // Initialise the texture
    if (_target == GL_TEXTURE_2D) {
        GX_InitTexObj(&texture2D->obj, (void *) _pixels, _width, _height,
                      format, texture2D->wrap_s, texture2D->wrap_t, _level);
    } else if (_target == GL_PROXY_TEXTURE_2D) {
        GX_InitTexObj(&texture2D->obj, NULL, _width, _height,
                      format, texture2D->wrap_s, texture2D->wrap_t, _level); /* ??? */
    }
    
    // Set the textures filter mode
    GX_InitTexObjFilterMode(&texture2D->obj, texture2D->minfilt, texture2D->magfilt);
    
}

void glCopyTexSubImage2D (GLenum _target, GLint _level,
                          GLint _xoffset, GLint _yoffset,
                          GLint _x, GLint _y,
                          GLsizei _width, GLsizei _height)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // Sanity check
    if (!texture2D ||
        _level < 0 || _level > log(GX_MAX_TEXTURE_SIZE))
        return; /* GL_INVALID_VALUE */
    
    //...
}

void glCompressedTexImage2DARB (GLenum param1, GLint param2, GLenum param3, 
                                GLsizei param4, GLsizei param5, GLint param6, 
                                GLsizei param7, const GLvoid *param8)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // Sanity check
    if (!texture2D)
        return; /* GL_INVALID_VALUE */
    
    //...
}

/**
 * Texture mapping
 */

void glTexEnvf (GLenum _target, GLenum _pname, GLfloat _param)
{
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
        
    // Sanity check
    if (_target != GL_TEXTURE_ENV)
        return; /* GL_INVALID_ENUM */
    
    // Determine which texture environment parameter we are setting
    switch (_pname) {
        
        // Texture environment operation mode
        case GL_TEXTURE_ENV_MODE:
            switch ((GLint) _param) {
                case GL_MODULATE: tevMode = GX_MODULATE; break;
                case GL_DECAL: tevMode = GX_DECAL; break;
                case GL_BLEND: tevMode = GX_BLEND; break;
                case GL_REPLACE: tevMode = GX_REPLACE; break;
                default: return; /* GL_INVALID_ENUM */
            }
            if (texture2DEnabled)
                GX_SetTevOp(GX_TEVSTAGE0 + tevStage, tevMode);
            break;

        default: return; /* GL_INVALID_ENUM */
        
    }
}

void glTexParameterf (GLenum _target, GLenum _pname, GLfloat _param)
{
    GLtexture *tex = NULL;
    
    // Sanity check
    if (insideBeginEndPair)
        return; /* GL_INVALID_OPERATION */
            
    // Determine which texture this operation will be applied to
    switch (_target) {
        case GL_TEXTURE_1D: tex = texture1D; break;
        case GL_TEXTURE_2D: tex = texture2D; break;
        default: return; /* GL_INVALID_ENUM */
    }
    
    // Sanity check
    if (tex) {
            
        // Determine which texture parameter we are setting
        switch (_pname) {
            
            // Minifying filter mode
            case GL_TEXTURE_MIN_FILTER:
                switch ((GLint) _param) {
                    case GL_NEAREST: tex->minfilt = GX_NEAR; break;
                    case GL_LINEAR: tex->minfilt = GX_LINEAR; break;
                    case GL_NEAREST_MIPMAP_NEAREST: tex->minfilt = GX_NEAR_MIP_NEAR; break;
                    case GL_LINEAR_MIPMAP_NEAREST: tex->minfilt = GX_LIN_MIP_NEAR; break;
                    case GL_NEAREST_MIPMAP_LINEAR: tex->minfilt = GX_NEAR_MIP_LIN; break;
                    case GL_LINEAR_MIPMAP_LINEAR: tex->minfilt = GX_LIN_MIP_LIN; break;
                    default: return; /* GL_INVALID_ENUM */
                }
                GX_InitTexObjFilterMode(&tex->obj, tex->minfilt, tex->magfilt);
                break;
                
            // Magnification filter mode
            case GL_TEXTURE_MAG_FILTER:
                switch ((GLint) _param) {
                    case GL_NEAREST: tex->magfilt = GX_NEAR; break;
                    case GL_LINEAR: tex->magfilt = GX_LINEAR; break;
                    default: return; /* GL_INVALID_ENUM */
                }
                GX_InitTexObjFilterMode(&tex->obj, tex->minfilt, tex->magfilt);
                break;
                
            // 's' coordinate wrap mode
            case GL_TEXTURE_WRAP_S:
                switch ((GLint) _param) {
                    case GL_CLAMP: tex->wrap_s = GX_CLAMP; break;
                    case GL_REPEAT: tex->wrap_s = GX_REPEAT; break;
                    default: return; /* GL_INVALID_ENUM */
                }
                GX_InitTexObjWrapMode(&tex->obj, tex->wrap_s, tex->wrap_t);
                break;
                
            // 't' coordinate wrap mode
            case GL_TEXTURE_WRAP_T:
                switch ((GLint) _param) {
                    case GL_CLAMP: tex->wrap_t = GX_CLAMP; break;
                    case GL_REPEAT: tex->wrap_t = GX_REPEAT; break;
                    default: return; /* GL_INVALID_ENUM */
                }
                GX_InitTexObjWrapMode(&tex->obj, tex->wrap_s, tex->wrap_t);
                break;
                
            // Resident priority
            // NOTE: We don't really need this?
            case GL_TEXTURE_PRIORITY:
                tex->priority = _param;
                break;

            default: return; /* GL_INVALID_ENUM */
            
        }

    }
}

void glTexParameteri (GLenum _target, GLenum _pname, GLint _param)
{
    // Muhahahahaha...
    glTexParameterf(_target, _pname, (GLfloat) _param);
}
