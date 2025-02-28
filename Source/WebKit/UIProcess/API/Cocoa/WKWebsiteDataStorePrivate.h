/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#import <WebKit/WKWebsiteDataStore.h>

#if WK_API_ENABLED

NS_ASSUME_NONNULL_BEGIN

@class _WKWebsiteDataStoreConfiguration;

typedef NS_OPTIONS(NSUInteger, _WKWebsiteDataStoreFetchOptions) {
    _WKWebsiteDataStoreFetchOptionComputeSizes = 1 << 0,
} WK_API_AVAILABLE(macosx(10.12), ios(10.0));

@interface WKWebsiteDataStore (WKPrivate)

+ (NSSet<NSString *> *)_allWebsiteDataTypesIncludingPrivate;
+ (BOOL)_defaultDataStoreExists;

- (instancetype)_initWithConfiguration:(_WKWebsiteDataStoreConfiguration *)configuration WK_API_AVAILABLE(macosx(10.13), ios(11.0));

- (void)_fetchDataRecordsOfTypes:(NSSet<NSString *> *)dataTypes withOptions:(_WKWebsiteDataStoreFetchOptions)options completionHandler:(void (^)(NSArray<WKWebsiteDataRecord *> *))completionHandler;

@property (nonatomic, setter=_setResourceLoadStatisticsEnabled:) BOOL _resourceLoadStatisticsEnabled WK_API_AVAILABLE(macosx(10.12), ios(10.0));

// ResourceLoadStatistics SPI for testing.
- (void)_resourceLoadStatisticsSetLastSeen:(double)seconds forHost:(NSString *)host WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsSetIsPrevalentResource:(BOOL)value forHost:(NSString *)host WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsIsPrevalentResource:(NSString *)host completionHandler:(void (^)(BOOL))completionHandler WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsSetHadUserInteraction:(BOOL)value forHost:(NSString *)host WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsHadUserInteraction:(NSString *)host completionHandler:(void (^)(BOOL))completionHandler WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsSetIsGrandfathered:(BOOL)value forHost:(NSString *)host WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsIsGrandfathered:(NSString *)host completionHandler:(void (^)(BOOL))completionHandler WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsSetSubframeUnderTopFrameOrigin:(NSString *)topFrameHostName forHost:(NSString *)host WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsSetSubresourceUnderTopFrameOrigin:(NSString *)topFrameHostName forHost:(NSString *)host WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsSetSubresourceUniqueRedirectTo:(NSString *)hostNameRedirectedTo forHost:(NSString *)host WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsSetTimeToLiveUserInteraction:(double)seconds WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsSetTimeToLiveCookiePartitionFree:(double)seconds WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsSetMinimumTimeBetweenDataRecordsRemoval:(double)seconds WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsSetGrandfatheringTime:(double)seconds WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsSetMaxStatisticsEntries:(size_t)entries WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsSetPruneEntriesDownTo:(size_t)entries WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsProcessStatisticsAndDataRecords WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsUpdateCookiePartitioning WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsSetShouldPartitionCookies:(BOOL)value forHost:(NSString *)host WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsSubmitTelemetry WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsSetNotifyPagesWhenDataRecordsWereScanned:(BOOL)value WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsSetShouldClassifyResourcesBeforeDataRecordsRemoval:(BOOL)value WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsSetNotifyPagesWhenTelemetryWasCaptured:(BOOL)value WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsSetShouldSubmitTelemetry:(BOOL)value WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsClearInMemoryAndPersistentStore WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsClearInMemoryAndPersistentStoreModifiedSinceHours:(unsigned)hours WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_resourceLoadStatisticsResetToConsistentState WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_setResourceLoadStatisticsTestingCallback:(nullable void (^)(WKWebsiteDataStore *, NSString *))callback WK_API_AVAILABLE(macosx(10.13), ios(11.0));

@end

NS_ASSUME_NONNULL_END

#endif
