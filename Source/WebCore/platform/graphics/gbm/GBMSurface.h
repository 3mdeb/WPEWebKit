/*
 * Copyright (C) 2015 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GBMSurface_h
#define GBMSurface_h

#if PLATFORM(GBM)

#include "IntSize.h"
#include <tuple>
#include <wtf/HashMap.h>

struct gbm_bo;
struct gbm_surface;

namespace WebCore {

class GLContextEGL;

class GBMSurface {
public:
    class Client {
    public:
        virtual void destroyBuffer(uint32_t) = 0;
    };

    static std::unique_ptr<GBMSurface> create(const IntSize&, Client&);
    GBMSurface(struct gbm_surface*, const IntSize&, Client&);
    ~GBMSurface();

    void resize(const IntSize&);

    std::unique_ptr<GLContextEGL> createGLContext() const;

    void destroyBuffer(uint32_t handle) { m_client.destroyBuffer(handle); }

    using GBMBufferExport = std::tuple<int, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int>;
    GBMBufferExport lockFrontBuffer();
    void releaseBuffer(uint32_t);

private:
    struct gbm_surface* m_surface;
    IntSize m_size;
    Client& m_client;

    static void boDataDestroyCallback(struct gbm_bo*, void*);
    HashMap<uint32_t, struct gbm_bo*> m_lockedBuffers;
};

} // namespace WebCore

#endif // PLATFORM(GBM)

#endif // GBMSurface_h
