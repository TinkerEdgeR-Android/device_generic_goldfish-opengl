/*
* Copyright (C) 2011 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#ifndef _GL_CLIENT_STATE_H_
#define _GL_CLIENT_STATE_H_

#define GL_API
#ifndef ANDROID
#define GL_APIENTRY
#define GL_APIENTRYP
#endif

#include "TextureSharedData.h"

#include <GLES/gl.h>
#include <GLES/glext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <stdio.h>
#include <stdlib.h>
#include "ErrorLog.h"
#include "codec_defs.h"

#include <vector>
#include <map>
#include <set>

// Tracking framebuffer objects:
// which framebuffer is bound,
// and which texture names
// are currently bound to which attachment points.
struct FboProps {
    GLenum target;
    GLuint name;
    bool previouslyBound;
    GLuint colorAttachment0_texture;
    GLuint depthAttachment_texture;
    GLuint stencilAttachment_texture;

    bool colorAttachment0_hasTexObj;
    bool depthAttachment_hasTexObj;
    bool stencilAttachment_hasTexObj;

    GLuint colorAttachment0_rbo;
    GLuint depthAttachment_rbo;
    GLuint stencilAttachment_rbo;

    bool colorAttachment0_hasRbo;
    bool depthAttachment_hasRbo;
    bool stencilAttachment_hasRbo;
};

// Same for Rbo's
struct RboProps {
    GLenum target;
    GLuint name;
    GLenum format;
    bool previouslyBound;
};

// Enum for describing whether a framebuffer attachment
// is a texture or renderbuffer.
enum FboAttachmentType {
    FBO_ATTACHMENT_RENDERBUFFER = 0,
    FBO_ATTACHMENT_TEXTURE = 1,
    FBO_ATTACHMENT_NONE = 2
};

// Tracking FBO format
struct FboFormatInfo {
    FboAttachmentType type;
    GLenum rb_format;
    GLint tex_internalformat;
    GLenum tex_format;
    GLenum tex_type;
    GLsizei tex_multisamples;
};

class GLClientState {
public:
    typedef enum {
        VERTEX_LOCATION = 0,
        NORMAL_LOCATION = 1,
        COLOR_LOCATION = 2,
        POINTSIZE_LOCATION = 3,
        TEXCOORD0_LOCATION = 4,
        TEXCOORD1_LOCATION = 5,
        TEXCOORD2_LOCATION = 6,
        TEXCOORD3_LOCATION = 7,
        TEXCOORD4_LOCATION = 8,
        TEXCOORD5_LOCATION = 9,
        TEXCOORD6_LOCATION = 10,
        TEXCOORD7_LOCATION = 11,
        MATRIXINDEX_LOCATION = 12,
        WEIGHT_LOCATION = 13,
        LAST_LOCATION = 14
    } StateLocation;

    typedef struct {
        GLint enabled;
        GLint size;
        GLenum type;
        GLsizei stride;
        void *data;
        GLuint bufferObject;
        GLenum glConst;
        unsigned int elementSize;
        bool enableDirty;  // true if any enable state has changed since last draw
        bool normalized;
    } VertexAttribState;

    typedef struct {
        int unpack_alignment;

        int unpack_row_length;
        int unpack_image_height;
        int unpack_skip_pixels;
        int unpack_skip_rows;
        int unpack_skip_images;

        int pack_alignment;

        int pack_row_length;
        int pack_skip_pixels;
        int pack_skip_rows;
    } PixelStoreState;

    enum {
        MAX_TEXTURE_UNITS = 256,
    };

public:
    GLClientState(int nLocations = CODEC_MAX_VERTEX_ATTRIBUTES);
    ~GLClientState();
    int nLocations() { return m_nLocations; }
    const PixelStoreState *pixelStoreState() { return &m_pixelStore; }
    int setPixelStore(GLenum param, GLint value);
    GLuint currentArrayVbo() { return m_currentArrayVbo; }
    GLuint currentIndexVbo() { return m_currentIndexVbo; }
    void enable(int location, int state);
    void setState(int  location, int size, GLenum type, GLboolean normalized, GLsizei stride, const void *data);
    void setBufferObject(int location, GLuint id);
    const VertexAttribState  *getState(int location);
    const VertexAttribState  *getStateAndEnableDirty(int location, bool *enableChanged);
    int getLocation(GLenum loc);
    void setActiveTexture(int texUnit) {m_activeTexture = texUnit; };
    int getActiveTexture() const { return m_activeTexture; }
    void setMaxVertexAttribs(int val) {
        m_maxVertexAttribs = val;
        m_maxVertexAttribsDirty = false;
    }

    void unBindBuffer(GLuint id)
    {
        if (m_currentArrayVbo == id) m_currentArrayVbo = 0;
        else if (m_currentIndexVbo == id) m_currentIndexVbo = 0;
    }

    int bindBuffer(GLenum target, GLuint id)
    {
        int err = 0;
        switch(target) {
        case GL_ARRAY_BUFFER:
            m_currentArrayVbo = id;
            break;
        case GL_ELEMENT_ARRAY_BUFFER:
            m_currentIndexVbo = id;
            break;
        default:
            err = -1;
        }
        return err;
    }

    int getBuffer(GLenum target)
    {
      int ret=0;
      switch (target) {
      case GL_ARRAY_BUFFER:
          ret = m_currentArrayVbo;
          break;
      case GL_ELEMENT_ARRAY_BUFFER:
          ret = m_currentIndexVbo;
          break;
      default:
          ret = -1;
      }
      return ret;
    }
    size_t pixelDataSize(GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, int pack) const;
    size_t pboNeededDataSize(GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, int pack) const;
    size_t clearBufferNumElts(GLenum buffer) const;

    void setCurrentProgram(GLint program) { m_currentProgram = program; }
    GLint currentProgram() const { return m_currentProgram; }

    void setNumActiveUniformsInUniformBlock(GLuint program, GLuint uniformBlockIndex, GLint numActiveUniforms);
    size_t numActiveUniformsInUniformBlock(GLuint program, GLuint uniformBlockIndex) const;
    /* OES_EGL_image_external
     *
     * These functions manipulate GL state which interacts with the
     * OES_EGL_image_external extension, to support client-side emulation on
     * top of host implementations that don't have it.
     *
     * Most of these calls should only be used with TEXTURE_2D or
     * TEXTURE_EXTERNAL_OES texture targets; TEXTURE_CUBE_MAP or other extension
     * targets should bypass this. An exception is bindTexture(), which should
     * see all glBindTexture() calls for any target.
     */

    // glActiveTexture(GL_TEXTURE0 + i)
    // Sets the active texture unit. Up to MAX_TEXTURE_UNITS are supported.
    GLenum setActiveTextureUnit(GLenum texture);
    GLenum getActiveTextureUnit() const;

    // glEnable(GL_TEXTURE_(2D|EXTERNAL_OES))
    void enableTextureTarget(GLenum target);

    // glDisable(GL_TEXTURE_(2D|EXTERNAL_OES))
    void disableTextureTarget(GLenum target);

    // Implements the target priority logic:
    // * Return GL_TEXTURE_EXTERNAL_OES if enabled, else
    // * Return GL_TEXTURE_2D if enabled, else
    // * Return the allDisabled value.
    // For some cases passing GL_TEXTURE_2D for allDisabled makes callee code
    // simpler; for other cases passing a recognizable enum like GL_ZERO or
    // GL_INVALID_ENUM is appropriate.
    GLenum getPriorityEnabledTarget(GLenum allDisabled) const;

    // glBindTexture(GL_TEXTURE_*, ...)
    // Set the target binding of the active texture unit to texture. Returns
    // GL_NO_ERROR on success or GL_INVALID_OPERATION if the texture has
    // previously been bound to a different target. If firstUse is not NULL,
    // it is set to indicate whether this is the first use of the texture.
    // For accurate error detection, bindTexture should be called for *all*
    // targets, not just 2D and EXTERNAL_OES.
    GLenum bindTexture(GLenum target, GLuint texture, GLboolean* firstUse);
    void setBoundEGLImage(GLenum target, GLeglImageOES image);

    // Return the texture currently bound to GL_TEXTURE_(2D|EXTERNAL_OES).
    GLuint getBoundTexture(GLenum target) const;
    // Other publicly-visible texture queries
    GLenum queryTexLastBoundTarget(GLuint name) const;
    GLenum queryTexFormat(GLuint name) const;
    GLint queryTexInternalFormat(GLuint name) const;
    GLsizei queryTexWidth(GLsizei level, GLuint name) const;
    GLsizei queryTexHeight(GLsizei level, GLuint name) const;
    GLsizei queryTexDepth(GLsizei level, GLuint name) const;
    bool queryTexEGLImageBacked(GLuint name) const;

    // For AMD GPUs, it is easy for the emulator to segfault
    // (esp. in dEQP) when a cube map is defined using glCopyTexImage2D
    // and uses GL_LUMINANCE as internal format.
    // In particular, the segfault happens when negative components of
    // cube maps are defined before positive ones,
    // This procedure checks internal state to see if we have defined
    // the positive component of a cube map already. If not, it returns
    // which positive component needs to be defined first.
    // If there is no need for the extra definition, 0 is returned.
    GLenum copyTexImageLuminanceCubeMapAMDWorkaround(GLenum target, GLint level,
                                                     GLenum internalformat);

    // Tracks the format of the currently bound texture.
    // This is to pass dEQP tests for fbo completeness.
    void setBoundTextureInternalFormat(GLenum target, GLint format);
    void setBoundTextureFormat(GLenum target, GLenum format);
    void setBoundTextureType(GLenum target, GLenum type);
    void setBoundTextureDims(GLenum target, GLsizei level, GLsizei width, GLsizei height, GLsizei depth);
    void setBoundTextureSamples(GLenum target, GLsizei samples);

    // glTexStorage2D disallows any change in texture format after it is set for a particular texture.
    void setBoundTextureImmutableFormat(GLenum target);
    bool isBoundTextureImmutableFormat(GLenum target) const;

    // glDeleteTextures(...)
    // Remove references to the to-be-deleted textures.
    void deleteTextures(GLsizei n, const GLuint* textures);

    // Render buffer objects
    void addRenderbuffers(GLsizei n, GLuint* renderbuffers);
    void removeRenderbuffers(GLsizei n, const GLuint* renderbuffers);
    bool usedRenderbufferName(GLuint name) const;
    void bindRenderbuffer(GLenum target, GLuint name);
    GLuint boundRenderbuffer() const;
    void setBoundRenderbufferFormat(GLenum format);

    // Frame buffer objects
    void addFramebuffers(GLsizei n, GLuint* framebuffers);
    void removeFramebuffers(GLsizei n, const GLuint* framebuffers);
    bool usedFramebufferName(GLuint name) const;
    void bindFramebuffer(GLenum target, GLuint name);
    void setCheckFramebufferStatus(GLenum status);
    GLenum getCheckFramebufferStatus() const;
    GLuint boundFramebuffer() const;

    // Texture object -> FBO
    void attachTextureObject(GLenum attachment, GLuint texture);
    GLuint getFboAttachmentTextureId(GLenum attachment) const;

    // RBO -> FBO
    void attachRbo(GLenum attachment, GLuint renderbuffer);
    GLuint getFboAttachmentRboId(GLenum attachment) const;

    // FBO attachments in general
    bool attachmentHasObject(GLenum attachment) const;

    void setTextureData(SharedTextureDataMap* sharedTexData);
    // set eglsurface property on default framebuffer
    // if coming from eglMakeCurrent
    void fromMakeCurrent();

    // Queries the format backing the current framebuffer.
    // Type differs depending on whether the attachment
    // is a texture or renderbuffer.
    void getBoundFramebufferFormat(
            GLenum attachment, FboFormatInfo* res_info) const;

private:
    PixelStoreState m_pixelStore;
    VertexAttribState *m_states;
    int m_glesMajorVersion;
    int m_glesMinorVersion;
    int m_maxVertexAttribs;
    bool m_maxVertexAttribsDirty;
    int m_nLocations;
    GLuint m_currentArrayVbo;
    GLuint m_currentIndexVbo;
    int m_activeTexture;
    GLint m_currentProgram;

    bool validLocation(int location) { return (location >= 0 && location < m_nLocations); }

    enum TextureTarget {
        TEXTURE_2D = 0,
        TEXTURE_EXTERNAL = 1,
        TEXTURE_CUBE_MAP = 2,
        TEXTURE_2D_ARRAY = 3,
        TEXTURE_3D = 4,
        TEXTURE_2D_MULTISAMPLE = 5,
        TEXTURE_TARGET_COUNT
    };
    struct TextureUnit {
        unsigned int enables;
        GLuint texture[TEXTURE_TARGET_COUNT];
    };
    struct TextureState {
        TextureUnit unit[MAX_TEXTURE_UNITS];
        TextureUnit* activeUnit;
        // Initialized from shared group.
        SharedTextureDataMap* textureRecs;
    };
    TextureState m_tex;

    // State tracking of cube map definitions.
    // Currently used only for driver workarounds
    // when using GL_LUMINANCE and defining cube maps with
    // glCopyTexImage2D.
    struct CubeMapDef {
        GLuint id;
        GLenum target;
        GLint level;
        GLenum internalformat;
    };
    struct CubeMapDefCompare {
        bool operator() (const CubeMapDef& a,
                         const CubeMapDef& b) const {
            if (a.id != b.id) return a.id < b.id;
            if (a.target != b.target) return a.target < b.target;
            if (a.level != b.level) return a.level < b.level;
            if (a.internalformat != b.internalformat)
                return a.internalformat < b.internalformat;
            return false;
        }
    };
    std::set<CubeMapDef, CubeMapDefCompare> m_cubeMapDefs;
    void writeCopyTexImageState(GLenum target, GLint level,
                                GLenum internalformat);
    GLenum copyTexImageNeededTarget(GLenum target, GLint level,
                                    GLenum internalformat);

    struct RboState {
        GLuint boundRenderbuffer;
        size_t boundRenderbufferIndex;
        std::vector<RboProps> rboData;
    };
    RboState mRboState;
    void addFreshRenderbuffer(GLuint name);
    void setBoundRenderbufferIndex();
    size_t getRboIndex(GLuint name) const;
    RboProps& boundRboProps();
    const RboProps& boundRboProps_const() const;

    struct FboState {
        GLuint boundFramebuffer;
        size_t boundFramebufferIndex;
        std::vector<FboProps> fboData;
        GLenum fboCheckStatus;
    };
    FboState mFboState;
    void addFreshFramebuffer(GLuint name);
    void setBoundFramebufferIndex();
    size_t getFboIndex(GLuint name) const;
    FboProps& boundFboProps();
    const FboProps& boundFboProps_const() const;

    // Querying framebuffer format
    GLenum queryRboFormat(GLuint name) const;
    GLenum queryTexType(GLuint name) const;
    GLsizei queryTexSamples(GLuint name) const;

    static int compareTexId(const void* pid, const void* prec);
    TextureRec* addTextureRec(GLuint id, GLenum target);
    TextureRec* getTextureRec(GLuint id) const;

public:
    void getClientStatePointer(GLenum pname, GLvoid** params);

    template <class T>
    int getVertexAttribParameter(GLuint index, GLenum param, T *ptr)
    {
        bool handled = true;
        const VertexAttribState *vertexAttrib = getState(index);
        if (vertexAttrib == NULL) {
            ERR("getVeterxAttriParameter for non existant index %d\n", index);
            // set gl error;
            return handled;
        }

        switch(param) {
        case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
            *ptr = (T)(vertexAttrib->bufferObject);
            break;
        case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
            *ptr = (T)(vertexAttrib->enabled);
            break;
        case GL_VERTEX_ATTRIB_ARRAY_SIZE:
            *ptr = (T)(vertexAttrib->size);
            break;
        case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
            *ptr = (T)(vertexAttrib->stride);
            break;
        case GL_VERTEX_ATTRIB_ARRAY_TYPE:
            *ptr = (T)(vertexAttrib->type);
            break;
        case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
            *ptr = (T)(vertexAttrib->normalized);
            break;
        case GL_CURRENT_VERTEX_ATTRIB:
            handled = false;
            break;
        default:
            handled = false;
            ERR("unknown vertex-attrib parameter param %d\n", param);
        }
        return handled;
    }

    template <class T>
    bool getClientStateParameter(GLenum param, T* ptr)
    {
        bool isClientStateParam = false;
        switch (param) {
        case GL_CLIENT_ACTIVE_TEXTURE: {
            GLint tex = getActiveTexture() + GL_TEXTURE0;
            *ptr = tex;
            isClientStateParam = true;
            break;
            }
        case GL_VERTEX_ARRAY_SIZE: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::VERTEX_LOCATION);
            *ptr = state->size;
            isClientStateParam = true;
            break;
            }
        case GL_VERTEX_ARRAY_TYPE: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::VERTEX_LOCATION);
            *ptr = state->type;
            isClientStateParam = true;
            break;
            }
        case GL_VERTEX_ARRAY_STRIDE: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::VERTEX_LOCATION);
            *ptr = state->stride;
            isClientStateParam = true;
            break;
            }
        case GL_COLOR_ARRAY_SIZE: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::COLOR_LOCATION);
            *ptr = state->size;
            isClientStateParam = true;
            break;
            }
        case GL_COLOR_ARRAY_TYPE: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::COLOR_LOCATION);
            *ptr = state->type;
            isClientStateParam = true;
            break;
            }
        case GL_COLOR_ARRAY_STRIDE: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::COLOR_LOCATION);
            *ptr = state->stride;
            isClientStateParam = true;
            break;
            }
        case GL_NORMAL_ARRAY_TYPE: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::NORMAL_LOCATION);
            *ptr = state->type;
            isClientStateParam = true;
            break;
            }
        case GL_NORMAL_ARRAY_STRIDE: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::NORMAL_LOCATION);
            *ptr = state->stride;
            isClientStateParam = true;
            break;
            }
        case GL_TEXTURE_COORD_ARRAY_SIZE: {
            const GLClientState::VertexAttribState *state = getState(getActiveTexture() + GLClientState::TEXCOORD0_LOCATION);
            *ptr = state->size;
            isClientStateParam = true;
            break;
            }
        case GL_TEXTURE_COORD_ARRAY_TYPE: {
            const GLClientState::VertexAttribState *state = getState(getActiveTexture() + GLClientState::TEXCOORD0_LOCATION);
            *ptr = state->type;
            isClientStateParam = true;
            break;
            }
        case GL_TEXTURE_COORD_ARRAY_STRIDE: {
            const GLClientState::VertexAttribState *state = getState(getActiveTexture() + GLClientState::TEXCOORD0_LOCATION);
            *ptr = state->stride;
            isClientStateParam = true;
            break;
            }
        case GL_POINT_SIZE_ARRAY_TYPE_OES: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::POINTSIZE_LOCATION);
            *ptr = state->type;
            isClientStateParam = true;
            break;
            }
        case GL_POINT_SIZE_ARRAY_STRIDE_OES: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::POINTSIZE_LOCATION);
            *ptr = state->stride;
            isClientStateParam = true;
            break;
            }
        case GL_MATRIX_INDEX_ARRAY_SIZE_OES: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::MATRIXINDEX_LOCATION);
            *ptr = state->size;
            isClientStateParam = true;
            break;
            }
        case GL_MATRIX_INDEX_ARRAY_TYPE_OES: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::MATRIXINDEX_LOCATION);
            *ptr = state->type;
            isClientStateParam = true;
            break;
            }
        case GL_MATRIX_INDEX_ARRAY_STRIDE_OES: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::MATRIXINDEX_LOCATION);
            *ptr = state->stride;
            isClientStateParam = true;
            break;
            }
        case GL_WEIGHT_ARRAY_SIZE_OES: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::WEIGHT_LOCATION);
            *ptr = state->size;
            isClientStateParam = true;
            break;
            }
        case GL_WEIGHT_ARRAY_TYPE_OES: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::WEIGHT_LOCATION);
            *ptr = state->type;
            isClientStateParam = true;
            break;
            }
        case GL_WEIGHT_ARRAY_STRIDE_OES: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::WEIGHT_LOCATION);
            *ptr = state->stride;
            isClientStateParam = true;
            break;
            }
        case GL_VERTEX_ARRAY_BUFFER_BINDING: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::VERTEX_LOCATION);
            *ptr = state->bufferObject;
            isClientStateParam = true;
            break;
            }
        case GL_NORMAL_ARRAY_BUFFER_BINDING: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::NORMAL_LOCATION);
            *ptr = state->bufferObject;
            isClientStateParam = true;
            break;
            }
        case GL_COLOR_ARRAY_BUFFER_BINDING: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::COLOR_LOCATION);
            *ptr = state->bufferObject;
            isClientStateParam = true;
            break;
            }
        case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING: {
            const GLClientState::VertexAttribState *state = getState(getActiveTexture()+GLClientState::TEXCOORD0_LOCATION);
            *ptr = state->bufferObject;
            isClientStateParam = true;
            break;
            }
        case GL_POINT_SIZE_ARRAY_BUFFER_BINDING_OES: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::POINTSIZE_LOCATION);
            *ptr = state->bufferObject;
            isClientStateParam = true;
            break;
            }
        case GL_MATRIX_INDEX_ARRAY_BUFFER_BINDING_OES: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::MATRIXINDEX_LOCATION);
            *ptr = state->bufferObject;
            isClientStateParam = true;
            break;
            }
        case GL_WEIGHT_ARRAY_BUFFER_BINDING_OES: {
            const GLClientState::VertexAttribState *state = getState(GLClientState::WEIGHT_LOCATION);
            *ptr = state->bufferObject;
            isClientStateParam = true;
            break;
            }
        case GL_ARRAY_BUFFER_BINDING: {
            int buffer = getBuffer(GL_ARRAY_BUFFER);
            *ptr = buffer;
            isClientStateParam = true;
            break;
            }
        case GL_ELEMENT_ARRAY_BUFFER_BINDING: {
            int buffer = getBuffer(GL_ELEMENT_ARRAY_BUFFER);
            *ptr = buffer;
            isClientStateParam = true;
            break;
            }
        case GL_MAX_VERTEX_ATTRIBS: {
            if (m_maxVertexAttribsDirty) {
                isClientStateParam = false;
            } else {
                *ptr = m_maxVertexAttribs;
                isClientStateParam = true;
            }
            break;
            }
        }
        return isClientStateParam;
    }

};
#endif
