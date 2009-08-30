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
 * Miscellaneous
 */

void glClearColor (GLclampf _red,
                   GLclampf _green,
                   GLclampf _blue,
                   GLclampf _alpha)
{
    // Set the clear colour
    clearColour.r = _red * 0xFF;
    clearColour.g = _green * 0xFF;
    clearColour.b = _blue * 0xFF;
    clearColour.a = _alpha * 0xFF;
}

void glClear (GLbitfield _mask)
{
    // Figure out which buffer we are to clear
    switch (_mask) {
        case GL_COLOR_BUFFER_BIT: /* ??? */ break;
        case GL_DEPTH_BUFFER_BIT: /* ??? */ break;
        case GL_STENCIL_BUFFER_BIT: /* ??? */ break;
        default: break;
    }
    
    // Clear the specified buffer
    // TODO: Only clear the specified buffer, not all of them
    GX_SetCopyClear(clearColour, clearDepth);
}

void glBlendFunc (GLenum _sfactor, GLenum _dfactor)
{
    u8 mode = GX_BM_LOGIC; /* ??? */
    u8 sfactor = GX_BL_ONE;
    u8 dfactor = GX_BL_ZERO;
    u8 op = GX_LO_SET; /* ??? */
    
    // Determine the source blending factor
    switch (_sfactor) {
        case GL_ZERO:
            sfactor = GX_BL_ZERO;
            break;
        case GL_ONE:
            sfactor = GX_BL_ONE;
            break;
        case GL_SRC_COLOR:
            sfactor = GX_BL_SRCCLR;
            break;
        case GL_ONE_MINUS_SRC_COLOR:
            sfactor = GX_BL_SRCCLR;
            break;
        case GL_DST_COLOR:
            sfactor = GX_BL_DSTCLR;
            break;
        case GL_ONE_MINUS_DST_COLOR:
            sfactor = GX_BL_DSTCLR;
            break;
        case GL_SRC_ALPHA:
            sfactor = GX_BL_SRCALPHA;
            break;
        case GL_ONE_MINUS_SRC_ALPHA:
            sfactor = GX_BL_SRCALPHA;
            break;
        case GL_DST_ALPHA:
            sfactor = GX_BL_DSTALPHA;
            break;
        case GL_ONE_MINUS_DST_ALPHA:
            sfactor = GX_BL_DSTALPHA;
            break;
        default: return;
    }
    
    // Determine the destination blending factor
    switch (_dfactor) {
        case GL_ZERO:
            sfactor = GX_BL_ZERO;
            break;
        case GL_ONE:
            sfactor = GX_BL_ONE;
            break;
        case GL_SRC_COLOR:
            sfactor = GX_BL_SRCCLR;
            break;
        case GL_ONE_MINUS_SRC_COLOR:
            sfactor = GX_BL_SRCCLR;
            break;
        case GL_DST_COLOR:
            sfactor = GX_BL_DSTCLR;
            break;
        case GL_ONE_MINUS_DST_COLOR:
            sfactor = GX_BL_DSTCLR;
            break;
        case GL_SRC_ALPHA:
            sfactor = GX_BL_SRCALPHA;
            break;
        case GL_ONE_MINUS_SRC_ALPHA:
            sfactor = GX_BL_SRCALPHA;
            break;
        case GL_DST_ALPHA:
            sfactor = GX_BL_DSTALPHA;
            break;
        case GL_ONE_MINUS_DST_ALPHA:
            sfactor = GX_BL_DSTALPHA;
            break;
        default: return;
    }
    
    // Set the blend mode
    GX_SetBlendMode(mode, sfactor, dfactor, op);
}

void glCullFace (GLenum _mode)
{
    // Determine the culling mode
    switch (_mode) {
        case GL_FRONT: cullMode = GX_CULL_FRONT;
        case GL_BACK: cullMode = GX_CULL_BACK;
        case GL_FRONT_AND_BACK: cullMode = GX_CULL_ALL;
    }
    
    // Set the culling mode (if enabled)
    if (cullModeEnabled)
        GX_SetCullMode(cullMode);
}

void glPointSize (GLfloat _size)
{
    // Set the point size
    GX_SetPointSize(_size, GX_TO_ZERO);
}

void glLineWidth (GLfloat _width)
{
    // Set the line width
    GX_SetLineWidth(_width, GX_TO_ZERO);
}

void glLineStipple (GLint _factor, GLushort _pattern)
{
    // Set the line stipple mode
    lineStippleFactor = _factor;
    lineStipplePattern = _pattern;
    
    if (lineStippleEnabled) {
        //...
    }
}

void glDrawBuffer (GLenum _mode)
{
    
}

void glReadBuffer (GLenum _mode)
{
    
}

void glEnable(GLenum _type)
{
    switch (_type) {
        case GL_TEXTURE_2D:
            texture2DEnabled = true;
            GX_SetNumTexGens(1);
            GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
            GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
            break;
        case GL_LINE_STIPPLE:
            lineStippleEnabled = true;
            break;
        case GL_BLEND:
            blendEnabled = true;
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
            cullModeEnabled = true;
            GX_SetCullMode(cullMode);
            break;
        case GL_DEPTH_TEST:
            depthTestEnabled = true;
            break;
    };
}

void glDisable (GLenum _type)
{
    switch (_type) {
        case GL_TEXTURE_2D:
            texture2DEnabled = false;
            GX_SetNumTexGens(0);
            GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);         
            GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
            break;
        case GL_LINE_STIPPLE:
            lineStippleEnabled = false;
            break;
        case GL_BLEND:
            blendEnabled = false;
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
            cullModeEnabled = false;
            GX_SetCullMode(GX_CULL_NONE);
            break;
        case GL_DEPTH_TEST:
            depthTestEnabled = false;
            break;
    };
}

void glEnableClientState (GLenum _cap)
{
    switch (_cap) {
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

void glDisableClientState (GLenum _cap)
{
    switch (_cap) {
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

void glGetIntegerv (GLenum _pname, GLint *_params)
{
    // Get the specified system integer constant
    switch (_pname) {
        case GL_MAX_TEXTURE_SIZE: *_params = 1024; break;
        default: break;
    }
}

const GLubyte *glGetString (GLenum _name)
{
    // Get the specified system string constant
    switch (_name) {
        case GL_EXTENSIONS: return (const GLubyte *) extensions;
        default: return NULL;
    }
}

/**
 * Transformation
 */

void glMatrixMode(GLenum _mode)
{
    
}

void glOrtho (GLdouble _left, GLdouble _right,
              GLdouble _bottom, GLdouble _top,
              GLdouble _near_val, GLdouble _far_val)
{
    
}

void glViewport (GLint _x, GLint _y,
                 GLsizei _width, GLsizei _height)
{
    GX_SetViewport(_x, _y, _width, _height, 0, 1);
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

void glTranslatef (GLfloat _x, GLfloat _y, GLfloat _z)
{
    
}

void glRotatef (GLfloat _angle, GLfloat _x, GLfloat _y, GLfloat _z)
{
    
}

void glScalef(GLfloat _x, GLfloat _y, GLfloat _z)
{
    
}

/**
 * Drawing
 */

void glBegin (GLenum _type)
{
    switch (_type) {
        case GL_POINTS: beginType = GX_POINTS; break;
        case GL_LINES: beginType = GX_LINES; break;
        case GL_LINE_STRIP: beginType = GX_LINESTRIP; break;
        case GL_LINE_LOOP: beginType = 0; break; /* ??? */
        case GL_TRIANGLES: beginType = GX_TRIANGLES; break;
        case GL_TRIANGLE_STRIP: beginType = GX_TRIANGLESTRIP; break;
        case GL_TRIANGLE_FAN: beginType = GX_TRIANGLEFAN; break;
        case GL_QUADS: beginType = GX_QUADS; break; 
        case GL_QUAD_STRIP: beginType = 0; break; /* ??? */
        case GL_POLYGON: beginType = 0; break; /* ??? */
    };
}

void glEnd (void)
{

}


void glColor4f (GLfloat _red, GLfloat _green,
                GLfloat _blue, GLfloat _alpha)
{
    // Set the current colour
    colour.r = _red * 0xFF;
    colour.g = _green * 0xFF;
    colour.b = _blue * 0xFF;
    colour.a = _alpha * 0xFF;
}

void glRectd (GLdouble _x1, GLdouble _y1, GLdouble _x2, GLdouble _y2)
{
    
}

/**
 * Vertex arrays
 */

void glVertexPointer (GLint _size, GLenum _type,
                      GLsizei _stride, const GLvoid *_ptr)
{
    
}

void glColorPointer (GLint _size, GLenum _type,
                     GLsizei _stride, const GLvoid *_ptr)
{
    
}

void glTexCoordPointer (GLint _size, GLenum _type,
                        GLsizei _stride, const GLvoid *_ptr)
{
    
}

void glDrawArrays (GLenum _mode, GLint _first, GLsizei _count)
{
    
}

void glInterleavedArrays (GLenum _format, GLsizei _stride,
                          const GLvoid *_pointer)
{
    
}

/**
 * Lighting
 */

void glShadeModel (GLenum _mode)
{
    
}

/**
 * Textures
 */

GLuint glNextFreeTextureName ()
{
    int name = 0;
    GLtexture *tex = textures;
    
    // Iterate through all textures till an unused name is found
    while (tex) {
        if (tex->name == name) {
            tex = textures;
            name++;
        } else {
            tex = tex->nextTexture;
        }
    }
    
    return name;
}

GLtexture *glGetTexture (GLuint _name)
{
    GLtexture *tex = textures;

    // Find the texture with the specified name (if possible)
    while (tex) {
        if (tex->name == _name)
            return tex;
        tex = tex->nextTexture;
    }

    return NULL;
}

void glGenTextures (GLsizei _n, GLuint *_textures)
{
    int i;
    
    // Sanity check
    if (_n < 0 || !_textures)
        return;
    
    // Allocate the specified number of textures
    for(i = 0; i < _n; i++) {
        
        // Allocate the next texture
        GLtexture *tex = wipemalloc(sizeof(GLtexture));
        if (!tex)
            continue;
        
        // Find a unique name for this texture
        tex->name = glNextFreeTextureName();
        
        // Insert the texture into the double-linked FILO list of allocated textures
        if (textures) {
            tex->nextTexture = textures;
            textures->prevTexture = tex;
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
    if (_n < 0 || !_textures)
        return;
    
    // Free the specified textures
    for(i = 0; i < _n; i++) {
        GLtexture *tex = glGetTexture(_textures[i]);
        if (tex) {
            
            // TODO: Free tex->obj?
            
            // Remove the texture from the double-linked FILO list of allocated textures
            textureCount--;
            if (tex->nextTexture)
                tex->nextTexture->prevTexture = tex->prevTexture;
            if (tex->prevTexture)
                tex->prevTexture->nextTexture = tex->nextTexture;
            else
                textures = tex->nextTexture;
            
            // Free the texture
            wipefree(tex);
            
        }
    }
    
}

void glBindTexture (GLenum _target, GLuint _texture)
{
    // Bind the texture to the specified target
    switch (_target) {
        case GL_TEXTURE_1D: texture1D = glGetTexture(_texture); break;
        case GL_TEXTURE_2D: texture2D = glGetTexture(_texture); break;
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
    if (!texture2D ||
        _width < 64 ||
        _height < 64)
        return;
    
    // Determine the textures format
    switch (_internalFormat) {
        case 4:
        case GL_RGBA:
        case GL_RGBA8: format = GX_TF_RGBA8; break;
        
        // Everything else, unsupported...
        // TODO: Support more formats!?
        default: return;
        
    }

    // Determine the format of the pixel data
    switch (_format) {
        case GL_RGBA: break;
        
        // Everything else, unsupported...
        // TODO: Support more formats!?
        default: return;
        
    }
    
    // Determine the data type of the pixel data
    switch (_type) {
        case GL_UNSIGNED_BYTE: break;
        
        // Everything else, unsupported...
        // TODO: Support more formats!?
        default: return;
        
    }
    
    // TODO: Borders!?
    
    // TODO: level-of-detail / minimap support!?
    
    // Initialise the texture
    if (_target == GL_TEXTURE_2D)
        GX_InitTexObj(&texture2D->obj, (void*) _pixels, _width, _height, format, 1, 1, GX_FALSE);
    else if (_target == GL_PROXY_TEXTURE_2D)
        GX_InitTexObj(&texture2D->obj, NULL, _width, _height, format, 1, 1, GX_FALSE); /* ??? */

}

void glCopyTexSubImage2D (GLenum _target, GLint _level,
                          GLint _xoffset, GLint _yoffset,
                          GLint _x, GLint _y,
                          GLsizei _width, GLsizei _height)
{
    // Sanity check
    if (!texture2D)
        return;
    
    //...
}

void glCompressedTexImage2DARB (GLenum param1, GLint param2, GLenum param3, 
                                GLsizei param4, GLsizei param5, GLint param6, 
                                GLsizei param7, const GLvoid *param8)
{
    // Sanity check
    if (!texture2D)
        return;
    
    //...
}

/**
 * Texture mapping
 */

void glTexEnvf (GLenum _target, GLenum _pname, GLfloat _param)
{
    
}

void glTexParameterf (GLenum _target, GLenum _pname, GLfloat _param)
{
    
}

void glTexParameteri (GLenum _target, GLenum _pname, GLint _param)
{
    
}
