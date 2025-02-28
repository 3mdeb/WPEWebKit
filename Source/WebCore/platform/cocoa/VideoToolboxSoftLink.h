/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <VideoToolbox/VideoToolbox.h>
#include <wtf/SoftLinking.h>

typedef struct OpaqueVTImageRotationSession* VTImageRotationSessionRef;

SOFT_LINK_FRAMEWORK_FOR_HEADER(WebCore, VideoToolbox)

SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, VideoToolbox, VTSessionCopyProperty, OSStatus, (VTSessionRef session, CFStringRef propertyKey, CFAllocatorRef allocator, void* propertyValueOut), (session, propertyKey, allocator, propertyValueOut))
#define VTSessionCopyProperty softLink_VideoToolbox_VTSessionCopyProperty
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, VideoToolbox, VTDecompressionSessionCreate, OSStatus, (CFAllocatorRef allocator, CMVideoFormatDescriptionRef videoFormatDescription, CFDictionaryRef videoDecoderSpecification, CFDictionaryRef destinationImageBufferAttributes, const VTDecompressionOutputCallbackRecord* outputCallback, VTDecompressionSessionRef* decompressionSessionOut), (allocator, videoFormatDescription, videoDecoderSpecification, destinationImageBufferAttributes, outputCallback, decompressionSessionOut))
#define VTDecompressionSessionCreate softLink_VideoToolbox_VTDecompressionSessionCreate
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, VideoToolbox, VTDecompressionSessionCanAcceptFormatDescription, Boolean, (VTDecompressionSessionRef session, CMFormatDescriptionRef newFormatDesc), (session, newFormatDesc))
#define VTDecompressionSessionCanAcceptFormatDescription softLink_VideoToolbox_VTDecompressionSessionCanAcceptFormatDescription
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, VideoToolbox, VTDecompressionSessionWaitForAsynchronousFrames, OSStatus, (VTDecompressionSessionRef session), (session))
#define VTDecompressionSessionWaitForAsynchronousFrames softLink_VideoToolbox_VTDecompressionSessionWaitForAsynchronousFrames
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, VideoToolbox, VTDecompressionSessionDecodeFrameWithOutputHandler, OSStatus, (VTDecompressionSessionRef session, CMSampleBufferRef sampleBuffer, VTDecodeFrameFlags decodeFlags, VTDecodeInfoFlags *infoFlagsOut, VTDecompressionOutputHandler outputHandler), (session, sampleBuffer, decodeFlags, infoFlagsOut, outputHandler))
#define VTDecompressionSessionDecodeFrameWithOutputHandler softLink_VideoToolbox_VTDecompressionSessionDecodeFrameWithOutputHandler
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, VideoToolbox, VTImageRotationSessionCreate, OSStatus, (CFAllocatorRef allocator, uint32_t rotationDegrees, VTImageRotationSessionRef* imageRotationSessionOut), (allocator, rotationDegrees, imageRotationSessionOut))
#define VTImageRotationSessionCreate softLink_VideoToolbox_VTImageRotationSessionCreate
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, VideoToolbox, VTImageRotationSessionSetProperty, OSStatus, (VTImageRotationSessionRef session, CFStringRef propertyKey, CFTypeRef propertyValue), (session, propertyKey, propertyValue))
#define VTImageRotationSessionSetProperty softLink_VideoToolbox_VTImageRotationSessionSetProperty
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, VideoToolbox, VTImageRotationSessionTransferImage, OSStatus, (VTImageRotationSessionRef session, CVPixelBufferRef sourceBuffer, CVPixelBufferRef destinationBuffer), (session, sourceBuffer, destinationBuffer))
#define VTImageRotationSessionTransferImage softLink_VideoToolbox_VTImageRotationSessionTransferImage
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebCore, VideoToolbox, VTIsHardwareDecodeSupported, Boolean, (CMVideoCodecType codecType), (codecType))
#define VTIsHardwareDecodeSupported softLink_VideoToolbox_VTIsHardwareDecodeSupported
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebCore, VideoToolbox, VTGetGVADecoderAvailability, OSStatus, (uint32_t* totalInstanceCountOut, uint32_t* freeInstanceCountOut), (totalInstanceCountOut, freeInstanceCountOut))
#define VTGetGVADecoderAvailability softLink_VideoToolbox_VTGetGVADecoderAvailability
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebCore, VideoToolbox, VTCreateCGImageFromCVPixelBuffer, OSStatus, (CVPixelBufferRef pixelBuffer, CFDictionaryRef options, CGImageRef* imageOut), (pixelBuffer, options, imageOut))
#define VTCreateCGImageFromCVPixelBuffer softLink_VideoToolbox_VTCreateCGImageFromCVPixelBuffer
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, VideoToolbox, kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder, CFStringRef)
#define kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder get_VideoToolbox_kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, VideoToolbox, kVTDecompressionPropertyKey_PixelBufferPool, CFStringRef)
#define kVTDecompressionPropertyKey_PixelBufferPool get_VideoToolbox_kVTDecompressionPropertyKey_PixelBufferPool()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, VideoToolbox, kVTImageRotationPropertyKey_EnableHighSpeedTransfer, CFStringRef)
#define kVTImageRotationPropertyKey_EnableHighSpeedTransfer get_VideoToolbox_kVTImageRotationPropertyKey_EnableHighSpeedTransfer()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, VideoToolbox, kVTImageRotationPropertyKey_FlipHorizontalOrientation, CFStringRef)
#define kVTImageRotationPropertyKey_FlipHorizontalOrientation get_VideoToolbox_kVTImageRotationPropertyKey_FlipHorizontalOrientation()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, VideoToolbox, kVTImageRotationPropertyKey_FlipVerticalOrientation, CFStringRef)
#define kVTImageRotationPropertyKey_FlipVerticalOrientation get_VideoToolbox_kVTImageRotationPropertyKey_FlipVerticalOrientation()
