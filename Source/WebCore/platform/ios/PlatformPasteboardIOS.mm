/*
 * Copyright (C) 2013 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "PlatformPasteboard.h"

#import "Color.h"
#import "Image.h"
#import "Pasteboard.h"
#import "SharedBuffer.h"
#import "URL.h"
#import "UTIUtilities.h"
#import "WebItemProviderPasteboard.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/UIImage.h>
#import <UIKit/UIPasteboard.h>
#import <pal/spi/ios/UIKitSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIImage)
SOFT_LINK_CLASS(UIKit, UIPasteboard)

namespace WebCore {

PlatformPasteboard::PlatformPasteboard()
    : m_pasteboard([getUIPasteboardClass() generalPasteboard])
{
}

#if ENABLE(DATA_INTERACTION)
PlatformPasteboard::PlatformPasteboard(const String& name)
{
    if (name == "data interaction pasteboard")
        m_pasteboard = [WebItemProviderPasteboard sharedInstance];
    else
        m_pasteboard = [getUIPasteboardClass() generalPasteboard];
}
#else
PlatformPasteboard::PlatformPasteboard(const String&)
    : m_pasteboard([getUIPasteboardClass() generalPasteboard])
{
}
#endif

void PlatformPasteboard::getTypes(Vector<String>& types)
{
    for (NSString *pasteboardType in [m_pasteboard pasteboardTypes])
        types.append(pasteboardType);
}

void PlatformPasteboard::getTypesByFidelityForItemAtIndex(Vector<String>& types, int index)
{
    if (index >= [m_pasteboard numberOfItems] || ![m_pasteboard respondsToSelector:@selector(pasteboardTypesByFidelityForItemAtIndex:)])
        return;

    NSArray *pasteboardTypesByFidelity = [m_pasteboard pasteboardTypesByFidelityForItemAtIndex:index];
    for (NSString *typeIdentifier in pasteboardTypesByFidelity)
        types.append(typeIdentifier);
}

RefPtr<SharedBuffer> PlatformPasteboard::bufferForType(const String&)
{
    return nullptr;
}

void PlatformPasteboard::getPathnamesForType(Vector<String>&, const String&)
{
}

int PlatformPasteboard::numberOfFiles()
{
    return [m_pasteboard respondsToSelector:@selector(numberOfFiles)] ? [m_pasteboard numberOfFiles] : 0;
}

Vector<String> PlatformPasteboard::filenamesForDataInteraction()
{
    if (![m_pasteboard respondsToSelector:@selector(droppedFileURLs)])
        return { };

    Vector<String> filenames;
    for (NSURL *fileURL in [m_pasteboard droppedFileURLs])
        filenames.append(fileURL.path);

    return filenames;
}

String PlatformPasteboard::stringForType(const String& type)
{
    NSArray *values = [m_pasteboard valuesForPasteboardType:type inItemSet:[NSIndexSet indexSetWithIndex:0]];
    for (id value in values) {
        if ([value isKindOfClass:[NSURL class]])
            return [(NSURL *)value absoluteString];

        if ([value isKindOfClass:[NSAttributedString class]])
            return [(NSAttributedString *)value string];

        if ([value isKindOfClass:[NSString class]])
            return (NSString *)value;
    }
    return String();
}

Color PlatformPasteboard::color()
{
    return Color();
}

URL PlatformPasteboard::url()
{
    return URL();
}

long PlatformPasteboard::copy(const String&)
{
    return 0;
}

long PlatformPasteboard::addTypes(const Vector<String>&)
{
    return 0;
}

long PlatformPasteboard::setTypes(const Vector<String>&)
{
    return 0;
}

long PlatformPasteboard::setBufferForType(SharedBuffer*, const String&)
{
    return 0;
}

long PlatformPasteboard::setPathnamesForType(const Vector<String>&, const String&)
{
    return 0;
}

long PlatformPasteboard::setStringForType(const String&, const String&)
{
    return 0;
}

long PlatformPasteboard::changeCount() const
{
    return [m_pasteboard changeCount];
}

String PlatformPasteboard::uniqueName()
{
    return String();
}

static NSString *webIOSPastePboardType = @"iOS rich content paste pasteboard type";

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000

static void registerItemToPasteboard(WebItemProviderRegistrationInfoList *representationsToRegister, id <AbstractPasteboard> pasteboard)
{
    if (UIItemProvider *itemProvider = representationsToRegister.itemProvider)
        [pasteboard setItemProviders:@[ itemProvider ]];
    else
        [pasteboard setItemProviders:@[ ]];

    if ([pasteboard respondsToSelector:@selector(stageRegistrationList:)])
        [pasteboard stageRegistrationList:representationsToRegister];
}

#else

static RetainPtr<NSDictionary> richTextRepresentationsForPasteboardWebContent(const PasteboardWebContent& content)
{
    RetainPtr<NSMutableDictionary> representations = adoptNS([[NSMutableDictionary alloc] init]);

    ASSERT(content.clientTypes.size() == content.clientData.size());
    for (size_t i = 0, size = content.clientTypes.size(); i < size; ++i)
        [representations setValue:content.clientData[i]->createNSData().get() forKey:content.clientTypes[i]];

    if (content.dataInWebArchiveFormat) {
        [representations setValue:(NSData *)content.dataInWebArchiveFormat->createNSData().get() forKey:WebArchivePboardType];
        // Flag for UIKit to know that this copy contains rich content. This will trigger a two-step paste.
        [representations setValue:webIOSPastePboardType forKey:webIOSPastePboardType];
    }

    if (content.dataInRTFDFormat)
        [representations setValue:content.dataInRTFDFormat->createNSData().get() forKey:(NSString *)kUTTypeFlatRTFD];
    if (content.dataInRTFFormat)
        [representations setValue:content.dataInRTFFormat->createNSData().get() forKey:(NSString *)kUTTypeRTF];

    return representations;
}

#endif

#if ENABLE(DATA_INTERACTION)

static void addRepresentationsForPlainText(WebItemProviderRegistrationInfoList *itemsToRegister, const String& plainText)
{
    if (plainText.isEmpty())
        return;

    NSURL *platformURL = [NSURL URLWithString:plainText];
    if (URL(platformURL).isValid())
        [itemsToRegister addRepresentingObject:platformURL];

    [itemsToRegister addData:[(NSString *)plainText dataUsingEncoding:NSUTF8StringEncoding] forType:(NSString *)kUTTypeUTF8PlainText];
}

bool PlatformPasteboard::allowReadingURLAtIndex(const URL& url, int index) const
{
    NSItemProvider *itemProvider = (NSUInteger)index < [m_pasteboard itemProviders].count ? [[m_pasteboard itemProviders] objectAtIndex:index] : nil;
    for (NSString *type in itemProvider.registeredTypeIdentifiers) {
        if (UTTypeConformsTo((CFStringRef)type, kUTTypeURL))
            return true;
    }

    return url.isValid();
}

#else

bool PlatformPasteboard::allowReadingURLAtIndex(const URL&, int) const
{
    return true;
}

#endif

void PlatformPasteboard::write(const PasteboardWebContent& content)
{
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
    auto representationsToRegister = adoptNS([[WebItemProviderRegistrationInfoList alloc] init]);

    [representationsToRegister addData:[webIOSPastePboardType dataUsingEncoding:NSUTF8StringEncoding] forType:webIOSPastePboardType];

    ASSERT(content.clientTypes.size() == content.clientData.size());
    for (size_t i = 0, size = content.clientTypes.size(); i < size; ++i)
        [representationsToRegister addData:content.clientData[i]->createNSData().get() forType:content.clientTypes[i]];

    if (content.dataInWebArchiveFormat)
        [representationsToRegister addData:content.dataInWebArchiveFormat->createNSData().get() forType:WebArchivePboardType];

    if (content.dataInAttributedStringFormat) {
        NSAttributedString *attributedString = [NSKeyedUnarchiver unarchiveObjectWithData:content.dataInAttributedStringFormat->createNSData().get()];
        if (attributedString)
            [representationsToRegister addRepresentingObject:attributedString];
    }

    if (content.dataInRTFDFormat)
        [representationsToRegister addData:content.dataInRTFDFormat->createNSData().get() forType:(NSString *)kUTTypeFlatRTFD];

    if (content.dataInRTFFormat)
        [representationsToRegister addData:content.dataInRTFFormat->createNSData().get() forType:(NSString *)kUTTypeRTF];

    if (!content.dataInStringFormat.isEmpty())
        addRepresentationsForPlainText(representationsToRegister.get(), content.dataInStringFormat);

    registerItemToPasteboard(representationsToRegister.get(), m_pasteboard.get());
#else
    RetainPtr<NSMutableDictionary> representations = adoptNS([[NSMutableDictionary alloc] init]);
    [representations addEntriesFromDictionary:richTextRepresentationsForPasteboardWebContent(content).autorelease()];

    NSString *textAsString = content.dataInStringFormat;
    [representations setValue:[textAsString dataUsingEncoding:NSUTF8StringEncoding] forKey:(NSString *)kUTTypeUTF8PlainText];
    [representations setValue:[textAsString dataUsingEncoding:NSUTF16StringEncoding] forKey:(NSString *)kUTTypeUTF16PlainText];
    // FIXME: We vend "public.text" here for backwards compatibility with pre-iOS 11 apps. In the future, we should stop vending this UTI,
    // and instead set data for concrete plain text types. See <https://bugs.webkit.org/show_bug.cgi?id=173317>.
    [representations setValue:textAsString forKey:(NSString *)kUTTypeText];

    // Explicitly cast m_pasteboard to UIPasteboard * to work around rdar://problem/33383354.
    ASSERT([m_pasteboard isKindOfClass:getUIPasteboardClass()]);
    [(UIPasteboard *)m_pasteboard setItems:@[representations.get()]];
#endif
}

void PlatformPasteboard::write(const PasteboardImage& pasteboardImage)
{
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
    auto representationsToRegister = adoptNS([[WebItemProviderRegistrationInfoList alloc] init]);

    auto& types = pasteboardImage.clientTypes;
    auto& data = pasteboardImage.clientData;
    ASSERT(types.size() == data.size());
    for (size_t i = 0, size = types.size(); i < size; ++i)
        [representationsToRegister addData:data[i]->createNSData().get() forType:types[i]];

    if (pasteboardImage.resourceData && !pasteboardImage.resourceMIMEType.isEmpty()) {
        auto utiOrMIMEType = pasteboardImage.resourceMIMEType;
        if (!isDeclaredUTI(utiOrMIMEType))
            utiOrMIMEType = UTIFromMIMEType(utiOrMIMEType);

        auto imageData = pasteboardImage.resourceData->createNSData();
        [representationsToRegister addData:imageData.get() forType:(NSString *)utiOrMIMEType];
        [representationsToRegister setPreferredPresentationSize:pasteboardImage.imageSize];
        [representationsToRegister setSuggestedName:pasteboardImage.suggestedName];
    }

    // FIXME: When writing a PasteboardImage, we currently always place the image data at a higer fidelity than the
    // associated image URL. However, in the case of an image enclosed by an anchor, we might want to consider the
    // the URL (i.e. the anchor's href attribute) to be a higher fidelity representation.
    if (!pasteboardImage.url.url.isEmpty()) {
        if (NSURL *nsURL = pasteboardImage.url.url)
            [representationsToRegister addRepresentingObject:nsURL];
    }

    registerItemToPasteboard(representationsToRegister.get(), m_pasteboard.get());
#else
    RetainPtr<NSMutableDictionary> representations = adoptNS([[NSMutableDictionary alloc] init]);
    if (!pasteboardImage.resourceMIMEType.isNull()) {
        [representations setObject:pasteboardImage.resourceData->createNSData().get() forKey:pasteboardImage.resourceMIMEType];
        if (!pasteboardImage.url.url.isNull())
            [representations setObject:(NSURL *)pasteboardImage.url.url forKey:(NSString *)kUTTypeURL];
    }

    // Explicitly cast m_pasteboard to UIPasteboard * to work around rdar://problem/33383354.
    ASSERT([m_pasteboard isKindOfClass:getUIPasteboardClass()]);
    [(UIPasteboard *)m_pasteboard setItems:@[representations.get()]];
#endif
}

void PlatformPasteboard::write(const String& pasteboardType, const String& text)
{
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
    auto representationsToRegister = adoptNS([[WebItemProviderRegistrationInfoList alloc] init]);
    [representationsToRegister setPreferredPresentationStyle:WebPreferredPresentationStyleInline];

    NSString *pasteboardTypeAsNSString = pasteboardType;
    if (!text.isEmpty() && pasteboardTypeAsNSString.length) {
        auto pasteboardTypeAsCFString = (CFStringRef)pasteboardTypeAsNSString;
        if (UTTypeConformsTo(pasteboardTypeAsCFString, kUTTypeURL) || UTTypeConformsTo(pasteboardTypeAsCFString, kUTTypeText))
            addRepresentationsForPlainText(representationsToRegister.get(), text);
        else
            [representationsToRegister addData:[pasteboardTypeAsNSString dataUsingEncoding:NSUTF8StringEncoding] forType:pasteboardType];
    }

    registerItemToPasteboard(representationsToRegister.get(), m_pasteboard.get());
#else
    RetainPtr<NSDictionary> representations = adoptNS([[NSMutableDictionary alloc] init]);

    NSString *textAsString = text;
    if (pasteboardType == String(kUTTypeURL)) {
        [representations setValue:adoptNS([[NSURL alloc] initWithString:text]).get() forKey:pasteboardType];
        [representations setValue:textAsString forKey:(NSString *)kUTTypeText];
    } else if (!pasteboardType.isNull())
        [representations setValue:textAsString forKey:pasteboardType];

    auto cfPasteboardType = pasteboardType.createCFString();
    if (UTTypeConformsTo(cfPasteboardType.get(), kUTTypeText) || UTTypeConformsTo(cfPasteboardType.get(), kUTTypeURL)) {
        [representations setValue:[textAsString dataUsingEncoding:NSUTF8StringEncoding] forKey:(NSString *)kUTTypeUTF8PlainText];
        [representations setValue:[textAsString dataUsingEncoding:NSUTF16StringEncoding] forKey:(NSString *)kUTTypeUTF16PlainText];
    }

    // Explicitly cast m_pasteboard to UIPasteboard * to work around rdar://problem/33383354.
    ASSERT([m_pasteboard isKindOfClass:getUIPasteboardClass()]);
    [(UIPasteboard *)m_pasteboard setItems:@[representations.get()]];
#endif
}

void PlatformPasteboard::write(const PasteboardURL& url)
{
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
    auto representationsToRegister = adoptNS([[WebItemProviderRegistrationInfoList alloc] init]);
    [representationsToRegister setPreferredPresentationStyle:WebPreferredPresentationStyleInline];

    if (NSURL *nsURL = url.url) {
        if (!url.title.isEmpty())
            nsURL._title = url.title;
        [representationsToRegister addRepresentingObject:nsURL];
    }

    registerItemToPasteboard(representationsToRegister.get(), m_pasteboard.get());
#else
    write(kUTTypeURL, url.url.string());
#endif
}

int PlatformPasteboard::count()
{
    return [m_pasteboard numberOfItems];
}

RefPtr<SharedBuffer> PlatformPasteboard::readBuffer(int index, const String& type)
{
    NSIndexSet *indexSet = [NSIndexSet indexSetWithIndex:index];

    RetainPtr<NSArray> pasteboardItem = [m_pasteboard dataForPasteboardType:type inItemSet:indexSet];

    if (![pasteboardItem count])
        return nullptr;
    return SharedBuffer::create([pasteboardItem.get() objectAtIndex:0]);
}

String PlatformPasteboard::readString(int index, const String& type)
{
    NSIndexSet *indexSet = [NSIndexSet indexSetWithIndex:index];

    RetainPtr<NSArray> pasteboardItem = [m_pasteboard valuesForPasteboardType:type inItemSet:indexSet];

    if (![pasteboardItem count])
        return String();

    id value = [pasteboardItem objectAtIndex:0];
    
    if (type == String(kUTTypePlainText) || type == String(kUTTypeHTML)) {
        ASSERT([value isKindOfClass:[NSString class]]);
        return [value isKindOfClass:[NSString class]] ? value : nil;
    }
    if (type == String(kUTTypeText)) {
        ASSERT([value isKindOfClass:[NSString class]] || [value isKindOfClass:[NSAttributedString class]]);
        if ([value isKindOfClass:[NSString class]])
            return value;
        if ([value isKindOfClass:[NSAttributedString class]])
            return [(NSAttributedString *)value string];
    } else if (type == String(kUTTypeURL)) {
        ASSERT([value isKindOfClass:[NSURL class]]);
        if ([value isKindOfClass:[NSURL class]] && allowReadingURLAtIndex((NSURL *)value, index))
            return [(NSURL *)value absoluteString];
    }

    return String();
}

URL PlatformPasteboard::readURL(int index, const String& type, String& title)
{
    NSIndexSet *indexSet = [NSIndexSet indexSetWithIndex:index];

    RetainPtr<NSArray> pasteboardItem = [m_pasteboard valuesForPasteboardType:type inItemSet:indexSet];

    if (![pasteboardItem count])
        return URL();

    id value = [pasteboardItem objectAtIndex:0];
    ASSERT([value isKindOfClass:[NSURL class]]);
    if (![value isKindOfClass:[NSURL class]])
        return URL();

    if (!allowReadingURLAtIndex((NSURL *)value, index))
        return { };

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
    title = [value _title];
#else
    UNUSED_PARAM(title);
#endif

    return (NSURL *)value;
}

void PlatformPasteboard::updateSupportedTypeIdentifiers(const Vector<String>& types)
{
    if (![m_pasteboard respondsToSelector:@selector(updateSupportedTypeIdentifiers:)])
        return;

    NSMutableArray *typesArray = [NSMutableArray arrayWithCapacity:types.size()];
    for (auto type : types)
        [typesArray addObject:(NSString *)type];

    [m_pasteboard updateSupportedTypeIdentifiers:typesArray];
}

}
