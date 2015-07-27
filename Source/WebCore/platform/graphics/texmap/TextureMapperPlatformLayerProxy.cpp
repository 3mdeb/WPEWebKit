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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextureMapperPlatformLayerProxy.h"

#if USE(COORDINATED_GRAPHICS_THREADED)

#include "BitmapTextureGL.h"
#include "TextureMapperGL.h"
#include "TextureMapperLayer.h"

const double s_releaseUnusedSecondsTolerance = 1;
const double s_releaseUnusedBuffersTimerInterval = 0.5;

namespace WebCore {

TextureMapperPlatformLayerProxy::TextureMapperPlatformLayerProxy()
    : m_compositor(0)
    , m_targetLayer(0)
    , m_runLoop(RunLoop::current())
    , m_releaseUnusedBuffersTimer(m_runLoop, this, &TextureMapperPlatformLayerProxy::releaseUnusedBuffersTimerFired)
{
}

TextureMapperPlatformLayerProxy::~TextureMapperPlatformLayerProxy()
{
    if (m_targetLayer)
        m_targetLayer->setContentsLayer(nullptr);
}

void TextureMapperPlatformLayerProxy::setCompositor(MutexLocker&, Compositor* compositor)
{
#ifndef NDEBUG
    m_compositorThreadID = WTF::currentThread();
#endif
    ASSERT(compositor);
    m_compositor = compositor;
    m_pushCondition.signal();
}

void TextureMapperPlatformLayerProxy::setTargetLayer(MutexLocker&, TextureMapperLayer* layer)
{
    ASSERT(m_compositorThreadID == WTF::currentThread());
    m_targetLayer = layer;
}

void TextureMapperPlatformLayerProxy::pushNextBuffer(MutexLocker&, std::unique_ptr<TextureMapperPlatformLayerBuffer> newBuffer)
{
    m_pendingBuffer = WTF::move(newBuffer);

    if (m_compositor)
        m_compositor->onNewBufferAvailable();
}

std::unique_ptr<TextureMapperPlatformLayerBuffer> TextureMapperPlatformLayerProxy::getAvailableBuffer(MutexLocker&, const IntSize& size, GC3Dint internalFormat)
{
    std::unique_ptr<TextureMapperPlatformLayerBuffer> availableBuffer;
    for (auto& buffer : m_usedBuffers) {
        if (buffer && buffer->canReuseWithoutReset(size, internalFormat)) {
            availableBuffer = WTF::move(buffer);
            availableBuffer->markUsed();
            break;
        }
    }

    scheduleReleaseUnusedBuffers();
    return availableBuffer;
}

void TextureMapperPlatformLayerProxy::scheduleReleaseUnusedBuffers()
{
    if (m_releaseUnusedBuffersTimer.isActive())
        m_releaseUnusedBuffersTimer.stop();

    m_releaseUnusedBuffersTimer.startOneShot(s_releaseUnusedBuffersTimerInterval);
}

void TextureMapperPlatformLayerProxy::releaseUnusedBuffersTimerFired()
{
    MutexLocker locker(m_pushMutex);
    if (m_usedBuffers.isEmpty())
        return;

    double minUsedTime = monotonicallyIncreasingTime() - s_releaseUnusedSecondsTolerance;
    m_usedBuffers.removeAllMatching([minUsedTime] (const std::unique_ptr<TextureMapperPlatformLayerBuffer>& current) {
        return !current || current->lastUsedTime() < minUsedTime;
    });
}

void TextureMapperPlatformLayerProxy::swapBuffer()
{
    ASSERT(m_compositorThreadID == WTF::currentThread());

    {
        MutexLocker locker(m_pushMutex);
        if (!m_targetLayer || !m_pendingBuffer)
            return;

        if (m_currentBuffer) {
            m_usedBuffers.append(WTF::move(m_currentBuffer));
            scheduleReleaseUnusedBuffers();
        }

        m_currentBuffer = WTF::move(m_pendingBuffer);
        m_pushCondition.signal();
    }

    m_targetLayer->setContentsLayer(m_currentBuffer.get());
}

};

#endif // USE(COORDINATED_GRAPHICS_THREADED)
