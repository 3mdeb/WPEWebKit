/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#include "Pasteboard.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class StaticPasteboard final : public Pasteboard {
public:
    StaticPasteboard();

    void commitToPasteboard(Pasteboard&);

    bool isStatic() const final { return true; }

    bool hasData() final;
    Vector<String> types() final { return m_types; }
    String readString(const String& type) final;

    void writeString(const String& type, const String& data) final;
    void clear() final;
    void clear(const String& type) final;

    void read(PasteboardPlainText&) final { }
    void read(PasteboardWebContentReader&) final { }

    void write(const PasteboardURL&) final { }
    void write(const PasteboardImage&) final { }
    void write(const PasteboardWebContent&) final { }

    Vector<String> readFilenames() final { return { }; }
    bool canSmartReplace() final { return false; }

    void writeMarkup(const String&) final { }
    void writePlainText(const String&, SmartReplaceOption) final { }

#if ENABLE(DRAG_SUPPORT)
    void setDragImage(DragImage, const IntPoint&) final { }
#endif

private:
    Vector<String> m_types;
    HashMap<String, String> m_stringContents;
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StaticPasteboard)
    static bool isType(const WebCore::Pasteboard& pasteboard) { return pasteboard.isStatic(); }
SPECIALIZE_TYPE_TRAITS_END()
