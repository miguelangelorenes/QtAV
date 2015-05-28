#include "SurfaceInteropDXVA.h"

namespace QtAV
{
    class EGLWrapper
    {
    public:
        EGLWrapper();

        __eglMustCastToProperFunctionPointerType getProcAddress(const char *procname);
        EGLSurface createPbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list);
        EGLBoolean destroySurface(EGLDisplay dpy, EGLSurface surface);
        EGLBoolean bindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
        EGLBoolean releaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer);

    private:
        typedef __eglMustCastToProperFunctionPointerType(EGLAPIENTRYP EglGetProcAddress)(const char *procname);
        typedef EGLSurface(EGLAPIENTRYP EglCreatePbufferSurface)(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list);
        typedef EGLBoolean(EGLAPIENTRYP EglDestroySurface)(EGLDisplay dpy, EGLSurface surface);
        typedef EGLBoolean(EGLAPIENTRYP EglBindTexImage)(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
        typedef EGLBoolean(EGLAPIENTRYP EglReleaseTexImage)(EGLDisplay dpy, EGLSurface surface, EGLint buffer);

        EglGetProcAddress m_eglGetProcAddress;
        EglCreatePbufferSurface m_eglCreatePbufferSurface;
        EglDestroySurface m_eglDestroySurface;
        EglBindTexImage m_eglBindTexImage;
        EglReleaseTexImage m_eglReleaseTexImage;
    };


    EGLWrapper::EGLWrapper()
    {
    #ifndef QT_OPENGL_ES_2_ANGLE_STATIC
        // Resolve the EGL functions we use. When configured for dynamic OpenGL, no
        // component in Qt will link to libEGL.lib and libGLESv2.lib. We know
        // however that libEGL is loaded for sure, since this is an ANGLE-only path.

    # ifdef QT_DEBUG
    //    HMODULE eglHandle = GetModuleHandle(L"libEGLd.dll");
        HMODULE eglHandle = GetModuleHandle(L"C:/Qt/5.4/msvc2013/bin/libEGLd.dll");
    # else
        HMODULE eglHandle = GetModuleHandle(L"libEGL.dll");
    # endif

        if (!eglHandle)
            qWarning("No EGL library loaded");

        m_eglGetProcAddress = (EglGetProcAddress)GetProcAddress(eglHandle, "eglGetProcAddress");
        m_eglCreatePbufferSurface = (EglCreatePbufferSurface)GetProcAddress(eglHandle, "eglCreatePbufferSurface");
        m_eglDestroySurface = (EglDestroySurface)GetProcAddress(eglHandle, "eglDestroySurface");
        m_eglBindTexImage = (EglBindTexImage)GetProcAddress(eglHandle, "eglBindTexImage");
        m_eglReleaseTexImage = (EglReleaseTexImage)GetProcAddress(eglHandle, "eglReleaseTexImage");
    #else
        // Static ANGLE-only build. There is no libEGL.dll in use.

        m_eglGetProcAddress = ::eglGetProcAddress;
        m_eglCreatePbufferSurface = ::eglCreatePbufferSurface;
        m_eglDestroySurface = ::eglDestroySurface;
        m_eglBindTexImage = ::eglBindTexImage;
        m_eglReleaseTexImage = ::eglReleaseTexImage;
    #endif
    }

    __eglMustCastToProperFunctionPointerType EGLWrapper::getProcAddress(const char *procname)
    {
        Q_ASSERT(m_eglGetProcAddress);
        return m_eglGetProcAddress(procname);
    }

    EGLSurface EGLWrapper::createPbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list)
    {
        Q_ASSERT(m_eglCreatePbufferSurface);
        return m_eglCreatePbufferSurface(dpy, config, attrib_list);
    }

    EGLBoolean EGLWrapper::destroySurface(EGLDisplay dpy, EGLSurface surface)
    {
        Q_ASSERT(m_eglDestroySurface);
        return m_eglDestroySurface(dpy, surface);
    }

    EGLBoolean EGLWrapper::bindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
    {
        Q_ASSERT(m_eglBindTexImage);
        return m_eglBindTexImage(dpy, surface, buffer);
    }

    EGLBoolean EGLWrapper::releaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
    {
        Q_ASSERT(m_eglReleaseTexImage);
        return m_eglReleaseTexImage(dpy, surface, buffer);
    }

    SurfaceInteropDXVA::SurfaceInteropDXVA(IDirect3DDevice9 * d3device)
    {
        _d3device = d3device;
        _egl = nullptr;
        _glTexture = 0;
    }

    SurfaceInteropDXVA::~SurfaceInteropDXVA()
    {

    }

    void SurfaceInteropDXVA::setSurface(IDirect3DSurface9 * surface)
    {
        _dxvaSurface = surface;
    }

    void* SurfaceInteropDXVA::map(SurfaceType type, const VideoFormat& fmt, void* handle, int plane)
    {
        if (!fmt.isRGB())
            return 0;

        if (!handle)
            return NULL;

        if (type == GLTextureSurface)
        {
            HRESULT hr = S_OK;

            if (!_glTexture)
            {
                _glTexture = *((GLint*)handle);
                D3DSURFACE_DESC dxvaDesc;
                hr = _dxvaSurface->GetDesc(&dxvaDesc);

                QOpenGLContext *currentContext = QOpenGLContext::currentContext();
                if (!_egl)
                    _egl = new EGLWrapper;

                HANDLE share_handle = NULL;
                QPlatformNativeInterface *nativeInterface = QGuiApplication::platformNativeInterface();
                _eglDisplay = static_cast<EGLDisplay*>(nativeInterface->nativeResourceForContext("eglDisplay", currentContext));
                _eglConfig = static_cast<EGLConfig*>(nativeInterface->nativeResourceForContext("eglConfig", currentContext));

                bool hasAlpha = currentContext->format().hasAlpha();

                EGLint attribs[] = {
                    EGL_WIDTH, dxvaDesc.Width,
                    EGL_HEIGHT, dxvaDesc.Height,
                    EGL_TEXTURE_FORMAT, hasAlpha ? EGL_TEXTURE_RGBA : EGL_TEXTURE_RGB,
                    EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
                    EGL_NONE
                };

                _pboSurface = _egl->createPbufferSurface( _eglDisplay,
                                                           _eglConfig,
                                                           attribs);

                PFNEGLQUERYSURFACEPOINTERANGLEPROC eglQuerySurfacePointerANGLE = reinterpret_cast<PFNEGLQUERYSURFACEPOINTERANGLEPROC>(_egl->getProcAddress("eglQuerySurfacePointerANGLE"));
                Q_ASSERT(eglQuerySurfacePointerANGLE);
                int ret = eglQuerySurfacePointerANGLE(  _eglDisplay,
                                                        _pboSurface,
                                                        EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE, &share_handle);

                if (share_handle && ret == EGL_TRUE)
                {
                    hr = _d3device->CreateTexture(  dxvaDesc.Width, dxvaDesc.Height, 1,
                                                    D3DUSAGE_RENDERTARGET,
                                                    hasAlpha ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8,
                                                    D3DPOOL_DEFAULT,
                                                    &_dxTexture,
                                                    &share_handle);

                    if (SUCCEEDED(hr))
                    {
                        hr = _dxTexture->GetSurfaceLevel(0, &_dxSurface);
                    }
                }
            }

            if (_glTexture > 0)
            {
                QOpenGLContext::currentContext()->functions()->glBindTexture(GL_TEXTURE_2D, _glTexture);
                hr = _d3device->StretchRect(_dxvaSurface, NULL, _dxSurface, NULL, D3DTEXF_NONE);

                if (SUCCEEDED(hr))
                    _egl->bindTexImage(_eglDisplay, _pboSurface, EGL_BACK_BUFFER);
            }

            return handle;
        }
        else {
            if (type == HostMemorySurface) {
            }
            else {
                return 0;
            }
        }

        return handle;
    }
    void SurfaceInteropDXVA::unmap(void *handle)
    {

    }
    void* SurfaceInteropDXVA::createHandle(SurfaceType type, const VideoFormat& fmt, int plane)
    {
        return nullptr;
    }
}

