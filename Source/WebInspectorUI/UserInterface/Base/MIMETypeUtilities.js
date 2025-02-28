/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WI.fileExtensionForURL = function(url)
{
    let lastPathComponent = parseURL(url).lastPathComponent;
    if (!lastPathComponent)
        return null;

    let index = lastPathComponent.lastIndexOf(".");
    if (index === -1)
        return null;

    if (index === lastPathComponent.length - 1)
        return null;

    return lastPathComponent.substr(index + 1);
};

WI.mimeTypeForFileExtension = function(extension)
{
    const extensionToMIMEType = {
        // Document types.
        "html": "text/html",
        "xhtml": "application/xhtml+xml",
        "xml": "text/xml",

        // Script types.
        "js": "text/javascript",
        "json": "application/json",
        "clj": "text/x-clojure",
        "coffee": "text/x-coffeescript",
        "ls": "text/x-livescript",
        "ts": "text/typescript",

        // Stylesheet types.
        "css": "text/css",
        "less": "text/x-less",
        "sass": "text/x-sass",
        "scss": "text/x-scss",

        // Image types.
        "bmp": "image/bmp",
        "gif": "image/gif",
        "jpeg": "image/jpeg",
        "jpg": "image/jpeg",
        "pdf": "application/pdf",
        "png": "image/png",
        "tif": "image/tiff",
        "tiff": "image/tiff",

        // Font types and Media types are ignored for now.

        // Miscellaneous types.
        "svg": "image/svg+xml",
        "txt": "text/plain",
        "xsl": "text/xsl"
    };

    return extensionToMIMEType[extension] || null;
};

WI.fileExtensionForMIMEType = function(mimeType)
{
    const mimeTypeToExtension = {
        // Document types.
        "text/html": "html",
        "application/xhtml+xml": "xhtml",
        "text/xml": "xml",

        // Script types.
        "text/javascript": "js",
        "application/json": "json",
        "text/x-clojure": "clj",
        "text/x-coffeescript": "coffee",
        "text/x-livescript": "ls",
        "text/typescript": "ts",

        // Stylesheet types.
        "text/css": "css",
        "text/x-less": "less",
        "text/x-sass": "sass",
        "text/x-scss": "scss",

        // Image types.
        "image/bmp": "bmp",
        "image/gif": "gif",
        "image/jpeg": "jpg",
        "application/pdf": "pdf",
        "image/png": "png",
        "image/tiff": "tiff",

        // Font types and Media types are ignored for now.

        // Miscellaneous types.
        "image/svg+xml": "svg",
        "text/plain": "txt",
        "text/xsl": "xsl",
    };

    let extension = mimeTypeToExtension[mimeType];
    if (extension)
        return extension;

    if (mimeType.endsWith("+json"))
        return "json";
    if (mimeType.endsWith("+xml"))
        return "xml";

    return null;
};
