/*
 * Copyright (C) 2016-2017 TATA ELXSI
 * Copyright (C) 2016-2017 Metrological
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebKitOpenCDMPrivateEncKey.h"

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && USE(OCDM)

#include "CDM.h"
#include "CDMSession.h"
#include <gst/gst.h>
#include "WebKitOpenCDMSessionEncKey.h"
#include "WebKitMediaKeyError.h"

GST_DEBUG_CATEGORY_EXTERN(webkit_media_opencdm_decrypt_debug_category);
#define GST_CAT_DEFAULT webkit_media_opencdm_decrypt_debug_category

namespace WebCore {

String WebKitOpenCDMPrivateEncKey::s_openCdmKeySystem("\0");
std::unique_ptr<OpenCdm> WebKitOpenCDMPrivateEncKey::s_openCdm(nullptr);

bool WebKitOpenCDMPrivateEncKey::supportsKeySystem(const String& keySystem)
{
    s_openCdmKeySystem = keySystem;
    return getOpenCdmInstance()->IsTypeSupported(keySystem.utf8().data(), "");
}

bool WebKitOpenCDMPrivateEncKey::supportsKeySystemAndMimeType(const String& keySystem, const String& mimeType)
{
    if (!supportsKeySystem(keySystem))
        return false;

    return getOpenCdmInstance()->IsTypeSupported(keySystem.utf8().data(), mimeType.utf8().data());
}

std::unique_ptr<CDMSession> WebKitOpenCDMPrivateEncKey::createSession(CDMSessionClient* client)
{
    getOpenCdmInstance()->SelectKeySystem(s_openCdmKeySystem.utf8().data());
    return std::make_unique<WebKitOpenCDMSessionEncKey>(client, getOpenCdmInstance());
}

OpenCdm* WebKitOpenCDMPrivateEncKey::getOpenCdmInstance()
{
    if (!s_openCdm)
        s_openCdm = std::make_unique<OpenCdm>();
    return s_openCdm.get();
}

} // namespace WebCore

#endif // ENABLE(LEGACY_ENCRYPTED_MEDIA) && USE(OCDM)
