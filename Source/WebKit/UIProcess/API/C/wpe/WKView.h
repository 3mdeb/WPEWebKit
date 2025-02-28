/*
 * Copyright (C) 2014 Igalia S.L.
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

#ifndef WKView_h
#define WKView_h

#include <WebKit/WKBase.h>
#include <WebKit/WKGeometry.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wpe_view_backend;

WK_EXPORT WKViewRef WKViewCreate(WKPageConfigurationRef);
WK_EXPORT WKViewRef WKViewCreateWithViewBackend(struct wpe_view_backend*, WKPageConfigurationRef);

WK_EXPORT WKPageRef WKViewGetPage(WKViewRef);

WK_EXPORT struct wpe_view_backend* WKViewGetViewBackend(WKViewRef);

enum {
    kWKViewStateIsInWindow = 1 << 0,
    kWKViewStateIsVisible = 1 << 1,
};
typedef uint32_t WKViewState;

WK_EXPORT void WKViewSetViewState(WKViewRef, WKViewState);

typedef void (*WKViewFrameDisplayed)(WKViewRef view, const void* clientInfo);

typedef struct WKViewClientBase {
    int version;
    const void* clientInfo;
} WKViewClientBase;

typedef struct WKViewClientV0 {
    WKViewClientBase base;

    // version 0
    WKViewFrameDisplayed frameDisplayed;
} WKViewClientV0;

WK_EXPORT void WKViewSetViewClient(WKViewRef, const WKViewClientBase*);

#ifdef __cplusplus
}
#endif

#endif // WKView_h
