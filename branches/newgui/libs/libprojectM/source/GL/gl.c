/**
 * gl.c - openGL wrapper for GX.
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

static const char *extensions = ""; /*"GL_ARB_texture_rectangle "
                                "GL_EXT_texture_rectangle "
                                "GL_ARB_texture_cube_map "
                                "GL_EXT_texture_cube_map";*/
                                
bool texture2d_enabled = false;
bool linestipple_enabled = false;
bool blend_enabled = false;
bool linesmooth_enabled = false;
bool pointsmooth_enabled = false;
bool polygonsmooth_enabled = false;
bool depthtest_enabled = false;
bool cullmode_enabled = true;

u8 begintype = 0;
u8 elements = 0;

GXColor clearcolour = { 0, 0, 0, 0xFF };
f32 cleardepth = 0 * 0x00FFFFFF;

u8 cullmode = GX_CULL_ALL;

u32 linestipple_factor = 0;
u16 linestipple_pattern = 0;

f32 pointsize = 1;

/**
 * Miscellaneous
 */

void glClearColor (GLclampf red,
                   GLclampf green,
                   GLclampf blue,
                   GLclampf alpha)
{
    clearcolour.r = red * 0xFF;
    clearcolour.g = green * 0xFF;
    clearcolour.b = blue * 0xFF;
    clearcolour.a = alpha * 0xFF;
}

void glClear (GLbitfield mask)
{
    switch (mask) {
        case GL_COLOR_BUFFER_BIT: /* ??? */ break;
        case GL_DEPTH_BUFFER_BIT: /* ??? */ break;
        case GL_STENCIL_BUFFER_BIT: /* ??? */ break;
        default: break;
    }
    
    GX_SetCopyClear(clearcolour, cleardepth);
}

void glBlendFunc (GLenum sfactor, GLenum dfactor)
{
    
}

void glCullFace (GLenum mode)
{
    switch (mode) {
        case GL_FRONT: cullmode = GX_CULL_FRONT;
        case GL_BACK: cullmode = GX_CULL_BACK;
        case GL_FRONT_AND_BACK: cullmode = GX_CULL_ALL;
    }
    
    if (cullmode_enabled)
        GX_SetCullMode(cullmode);
}

void glPointSize (GLfloat size)
{
    GX_SetPointSize(size, GX_TO_ZERO);
}

void glLineWidth (GLfloat width)
{
    GX_SetLineWidth(width, GX_TO_ZERO);
}

void glLineStipple (GLint factor, GLushort pattern)
{
    linestipple_factor = factor;
    linestipple_pattern = pattern;
    
    if (linestipple_enabled) {
        //...
    }
}

void glDrawBuffer (GLenum mode)
{
    
}

void glReadBuffer (GLenum mode)
{
    
}

void glEnable(GLenum type)
{
    switch (type) {
        case GL_TEXTURE_2D:
            texture2d_enabled = true;
            GX_SetNumTexGens(1);
            GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
            GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
            break;
        case GL_LINE_STIPPLE:
            linestipple_enabled = true;
            break;
        case GL_BLEND:
            blend_enabled = true;
            break;
        case GL_LINE_SMOOTH:
            linesmooth_enabled = true;
            break;
        case GL_POINT_SMOOTH:
            pointsmooth_enabled = true;
            break;
        case GL_POLYGON_SMOOTH:
            polygonsmooth_enabled = true;
            break;
        case GL_CULL_FACE:
            cullmode_enabled = true;
            GX_SetCullMode(cullmode);
            break;
        case GL_DEPTH_TEST:
            depthtest_enabled = true;
            break;
    };
}

void glDisable (GLenum type)
{
    switch (type) {
        case GL_TEXTURE_2D:
            texture2d_enabled = false;
            GX_SetNumTexGens(0);
            GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);         
            GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
            break;
        case GL_LINE_STIPPLE:
            linestipple_enabled = false;
            break;
        case GL_BLEND:
            blend_enabled = false;
            break;
        case GL_LINE_SMOOTH:
            linesmooth_enabled = false;
            break;
        case GL_POINT_SMOOTH:
            pointsmooth_enabled = false;
            break;
        case GL_POLYGON_SMOOTH:
            polygonsmooth_enabled = false;
            break;
        case GL_CULL_FACE:
            cullmode_enabled = false;
            GX_SetCullMode(GX_CULL_NONE);
            break;
        case GL_DEPTH_TEST:
            depthtest_enabled = false;
            break;
    };
}

void glEnableClientState (GLenum cap)
{
    switch (cap) {
        case GL_COLOR_ARRAY: break;
        case GL_EDGE_FLAG_ARRAY: break;
        case GL_FOG_COORD_ARRAY: break;
        case GL_INDEX_ARRAY: break;
        case GL_NORMAL_ARRAY: break;
        case GL_SECONDARY_COLOR_ARRAY: break;
        case GL_TEXTURE_COORD_ARRAY: break;
        case GL_VERTEX_ARRAY: break;
        default: break;
    }
}

void glDisableClientState (GLenum cap)
{
    switch (cap) {
        case GL_COLOR_ARRAY: break;
        case GL_EDGE_FLAG_ARRAY: break;
        case GL_FOG_COORD_ARRAY: break;
        case GL_INDEX_ARRAY: break;
        case GL_NORMAL_ARRAY: break;
        case GL_SECONDARY_COLOR_ARRAY: break;
        case GL_TEXTURE_COORD_ARRAY: break;
        case GL_VERTEX_ARRAY: break;
        default: break;
    }
}

void glGetIntegerv (GLenum pname, GLint *params)
{
    switch (pname) {
        case GL_MAX_TEXTURE_SIZE: *params = 1024; break;
        default: break;
    }
}

const GLubyte *glGetString (GLenum name)
{
    switch (name) {
        case GL_EXTENSIONS: return (const GLubyte *) extensions;
        default: return NULL;
    }
}

/**
 * Transformation
 */

void glMatrixMode(GLenum mode)
{
    
}

void glOrtho (GLdouble left, GLdouble right,
              GLdouble bottom, GLdouble top,
              GLdouble near_val, GLdouble far_val)
{
    
}

void glViewport (GLint x, GLint y,
                 GLsizei width, GLsizei height)
{
    GX_SetViewport(x, y, width, height, 0, 1);
}

void glPopMatrix (void)
{
    
}

void glPushMatrix (void)
{
    
}

void glLoadIdentity (void)
{
    
}

void glTranslatef (GLfloat x, GLfloat y, GLfloat z)
{
    
}

void glRotatef (GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    
}

void glScalef(GLfloat x, GLfloat y, GLfloat z)
{
    
}

/**
 * Drawing
 */

void glBegin (GLenum type)
{
    elements = 0;
    switch (type) {
        case GL_POINTS: begintype = GX_POINTS; break;
        case GL_LINES: begintype = GX_LINES; break;
        case GL_LINE_STRIP: begintype = GX_LINESTRIP; break;
        case GL_LINE_LOOP: begintype = 0; break; /* ??? */
        case GL_TRIANGLES: begintype = GX_TRIANGLES; break;
        case GL_TRIANGLE_STRIP: begintype = GX_TRIANGLESTRIP; break;
        case GL_TRIANGLE_FAN: begintype = GX_TRIANGLEFAN; break;
        case GL_QUADS: begintype = GX_QUADS; break; 
        case GL_QUAD_STRIP: begintype = 0; break; /* ??? */
        case GL_POLYGON: begintype = 0; break; /* ??? */
    };
}

void glEnd (void)
{    
    elements = 0;
}


void glColor4f (GLfloat red, GLfloat green,
                GLfloat blue, GLfloat alpha)
{
    
}

void glRectd (GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2)
{
    
}

/**
 * Vertex arrays
 */

void glVertexPointer (GLint size, GLenum type,
                      GLsizei stride, const GLvoid *ptr)
{
    
}

void glColorPointer (GLint size, GLenum type,
                     GLsizei stride, const GLvoid *ptr)
{
    
}

void glTexCoordPointer (GLint size, GLenum type,
                        GLsizei stride, const GLvoid *ptr)
{
    
}

void glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
    
}

void glInterleavedArrays (GLenum format, GLsizei stride,
                          const GLvoid *pointer)
{
    
}

/**
 * Lighting
 */

void glShadeModel (GLenum mode)
{
    
}

/**
 * Textures
 */

void glGenTextures  (GLsizei n, GLuint *textures)
{
    
}

void glDeleteTextures (GLsizei n, const GLuint *textures)
{
    
}

void glBindTexture (GLenum target, GLuint texture)
{
    
}

void glTexImage2D (GLenum target, GLint level,
                   GLint internalFormat,
                   GLsizei width, GLsizei height,
                   GLint border, GLenum format, GLenum type,
                   const GLvoid *pixels)
{
    
    
}

void glCopyTexSubImage2D (GLenum target, GLint level,
                          GLint xoffset, GLint yoffset,
                          GLint x, GLint y,
                          GLsizei width, GLsizei height)
{
    
}

void glCompressedTexImage2DARB (GLenum param1, GLint param2, GLenum param3, 
                                GLsizei param4, GLsizei param5, GLint param6, 
                                GLsizei param7, const GLvoid *param8)
{

}

/**
 * Texture mapping
 */

void glTexEnvf (GLenum target, GLenum pname, GLfloat param)
{
    
}

void glTexParameterf (GLenum target, GLenum pname, GLfloat param)
{
    
}

void glTexParameteri (GLenum target, GLenum pname, GLint param)
{
    
}
