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

#include "config.h"
#include "WPEInputHandler.h"

#include "NativeWebKeyboardEvent.h"
#include "WPEView.h"

namespace WPE {

InputHandler::InputHandler(View& view)
    : m_view(view)
{
    struct xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_rule_names names = { "evdev", "pc105", "us", "", "" };

    m_xkbKeymap = xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    m_xkbState = xkb_state_new(m_xkbKeymap);

    xkb_context_unref(context);
}

InputHandler::~InputHandler()
{
    xkb_keymap_unref(m_xkbKeymap);
    xkb_state_unref(m_xkbState);
}

void InputHandler::handleKeyboardKey(KeyboardEvent::Raw event)
{
    // Keycode system starts at 8. Go figure.
    int key = event.key + 8;

    m_view.page().handleKeyboardEvent(WebKit::NativeWebKeyboardEvent({
        event.time,
        xkb_state_key_get_one_sym(m_xkbState, key),
        xkb_state_key_get_utf32(m_xkbState, key),
        !!event.state
    }));
}

void InputHandler::handleKeyboardModifiers(KeyboardModifiers)
{
    // FIXME: Implement.
}

void InputHandler::handleTouchDown(TouchEvent::Raw event)
{
    // std::fprintf(stderr, "InputHandler::handleTouchDown()     [%d] (%d,%d)\n", event.id, event.x, event.y);
    ASSERT(m_touchEvents[event.id] == TouchEvent::Null);
    m_touchEvents[event.id] = TouchEvent::Raw{ TouchEvent::Down, event.time, event.id, event.x, event.y };
    dispatchTouchEvent(event.id);
}

void InputHandler::handleTouchUp(TouchEvent::Raw event)
{
    // std::fprintf(stderr, "InputHandler::handleTouchUp()       [%d]\n", event.id);
    ASSERT(m_touchEvents[event.id] == TouchEvent::Down || m_touchEvents[event.id] == TouchEvent::Motion);
    auto& previousEvent = m_touchEvents[event.id];
    int x = previousEvent.x;
    int y = previousEvent.y;
    m_touchEvents[event.id] = TouchEvent::Raw{ TouchEvent::Up, event.time, event.id, x, y };
    dispatchTouchEvent(event.id);
}

void InputHandler::handleTouchMotion(TouchEvent::Raw event)
{
    // std::fprintf(stderr, "InputHandler::handleTouchMotion()   [%d] (%d,%d)\n", event.id, event.x, event.y);
    ASSERT(m_touchEvents[event.id] == TouchEvent::Down || m_touchEvents[event.id] == TouchEvent::Motion);
    m_touchEvents[event.id] = TouchEvent::Raw{ TouchEvent::Motion, event.time, event.id, event.x, event.y };
    dispatchTouchEvent(event.id);
}

static WebKit::WebPlatformTouchPoint::TouchPointState stateForTouchEvent(int mainEventId, const TouchEvent::Raw& event)
{
    if (event.id != mainEventId)
        return WebKit::WebPlatformTouchPoint::TouchStationary;

    switch (event.type) {
    case TouchEvent::Down:
        return WebKit::WebPlatformTouchPoint::TouchPressed;
    case TouchEvent::Motion:
        return WebKit::WebPlatformTouchPoint::TouchMoved;
    case TouchEvent::Up:
        return WebKit::WebPlatformTouchPoint::TouchReleased;
    case TouchEvent::Null:
        ASSERT_NOT_REACHED();
        return WebKit::WebPlatformTouchPoint::TouchStationary;
    };
}

void InputHandler::dispatchTouchEvent(int id)
{
    Vector<WebKit::WebPlatformTouchPoint> touchPoints;
    touchPoints.reserveCapacity(m_touchEvents.size());

    for (auto& event : m_touchEvents) {
        if (event.type == TouchEvent::Null)
            continue;

        touchPoints.uncheckedAppend(WebKit::WebPlatformTouchPoint(event.id, stateForTouchEvent(id, event),
            WebCore::IntPoint(event.x, event.y), WebCore::IntPoint(event.x, event.y)));
    }

    auto& event = m_touchEvents[id];
    m_view.page().handleTouchEvent(WebKit::NativeWebTouchEvent(WPE::TouchEvent{
        WTF::move(touchPoints),
        event.type,
        event.time
    }));

    if (event.type == TouchEvent::Up)
        event = TouchEvent::Raw{ TouchEvent::Null, 0, -1, -1, -1 };
}

} // namespace WPE
