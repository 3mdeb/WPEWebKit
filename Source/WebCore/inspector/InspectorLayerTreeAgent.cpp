/*
 * Copyright (C) 2012, 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorLayerTreeAgent.h"

#include "InspectorDOMAgent.h"
#include "InstrumentingAgents.h"
#include "IntRect.h"
#include "PseudoElement.h"
#include "RenderChildIterator.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderView.h"
#include <inspector/IdentifiersFactory.h>

using namespace Inspector;

namespace WebCore {

InspectorLayerTreeAgent::InspectorLayerTreeAgent(WebAgentContext& context)
    : InspectorAgentBase(ASCIILiteral("LayerTree"), context)
    , m_frontendDispatcher(std::make_unique<Inspector::LayerTreeFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::LayerTreeBackendDispatcher::create(context.backendDispatcher, this))
{
}

InspectorLayerTreeAgent::~InspectorLayerTreeAgent()
{
    reset();
}

void InspectorLayerTreeAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void InspectorLayerTreeAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    ErrorString unused;
    disable(unused);
}

void InspectorLayerTreeAgent::reset()
{
    m_documentLayerToIdMap.clear();
    m_idToLayer.clear();
    m_pseudoElementToIdMap.clear();
    m_idToPseudoElement.clear();
}

void InspectorLayerTreeAgent::enable(ErrorString&)
{
    m_instrumentingAgents.setInspectorLayerTreeAgent(this);
}

void InspectorLayerTreeAgent::disable(ErrorString&)
{
    m_instrumentingAgents.setInspectorLayerTreeAgent(nullptr);
}

void InspectorLayerTreeAgent::layerTreeDidChange()
{
    m_frontendDispatcher->layerTreeDidChange();
}

void InspectorLayerTreeAgent::renderLayerDestroyed(const RenderLayer& renderLayer)
{
    unbind(&renderLayer);
}

void InspectorLayerTreeAgent::pseudoElementDestroyed(PseudoElement& pseudoElement)
{
    unbindPseudoElement(&pseudoElement);
}

void InspectorLayerTreeAgent::layersForNode(ErrorString& errorString, int nodeId, RefPtr<JSON::ArrayOf<Inspector::Protocol::LayerTree::Layer>>& layers)
{
    layers = JSON::ArrayOf<Inspector::Protocol::LayerTree::Layer>::create();

    auto* node = m_instrumentingAgents.inspectorDOMAgent()->nodeForId(nodeId);
    if (!node) {
        errorString = ASCIILiteral("Provided node id doesn't match any known node");
        return;
    }

    auto* renderer = node->renderer();
    if (!renderer) {
        errorString = ASCIILiteral("Node for provided node id doesn't have a renderer");
        return;
    }

    if (is<RenderElement>(*renderer))
        gatherLayersUsingRenderObjectHierarchy(errorString, downcast<RenderElement>(*renderer), layers);
}

void InspectorLayerTreeAgent::gatherLayersUsingRenderObjectHierarchy(ErrorString& errorString, RenderElement& renderer, RefPtr<JSON::ArrayOf<Inspector::Protocol::LayerTree::Layer>>& layers)
{
    if (renderer.hasLayer()) {
        gatherLayersUsingRenderLayerHierarchy(errorString, downcast<RenderLayerModelObject>(renderer).layer(), layers);
        return;
    }

    for (auto& child : childrenOfType<RenderElement>(renderer))
        gatherLayersUsingRenderObjectHierarchy(errorString, child, layers);
}

void InspectorLayerTreeAgent::gatherLayersUsingRenderLayerHierarchy(ErrorString& errorString, RenderLayer* renderLayer, RefPtr<JSON::ArrayOf<Inspector::Protocol::LayerTree::Layer>>& layers)
{
    if (renderLayer->isComposited())
        layers->addItem(buildObjectForLayer(errorString, renderLayer));

    for (renderLayer = renderLayer->firstChild(); renderLayer; renderLayer = renderLayer->nextSibling())
        gatherLayersUsingRenderLayerHierarchy(errorString, renderLayer, layers);
}

Ref<Inspector::Protocol::LayerTree::Layer> InspectorLayerTreeAgent::buildObjectForLayer(ErrorString& errorString, RenderLayer* renderLayer)
{
    RenderObject* renderer = &renderLayer->renderer();
    RenderLayerBacking* backing = renderLayer->backing();
    Node* node = renderer->node();

    bool isReflection = renderLayer->isReflection();
    bool isGenerated = (isReflection ? renderer->parent() : renderer)->isBeforeOrAfterContent();
    bool isAnonymous = renderer->isAnonymous();

    if (renderer->isRenderView())
        node = &renderer->document();
    else if (isReflection && isGenerated)
        node = renderer->parent()->generatingElement();
    else if (isGenerated)
        node = renderer->generatingNode();
    else if (isReflection || isAnonymous)
        node = renderer->parent()->element();

    // Basic set of properties.
    auto layerObject = Inspector::Protocol::LayerTree::Layer::create()
        .setLayerId(bind(renderLayer))
        .setNodeId(idForNode(errorString, node))
        .setBounds(buildObjectForIntRect(renderer->absoluteBoundingBoxRect()))
        .setMemory(backing->backingStoreMemoryEstimate())
        .setCompositedBounds(buildObjectForIntRect(enclosingIntRect(backing->compositedBounds())))
        .setPaintCount(backing->graphicsLayer()->repaintCount())
        .release();

    if (node && node->shadowHost())
        layerObject->setIsInShadowTree(true);

    if (isReflection)
        layerObject->setIsReflection(true);

    if (isGenerated) {
        if (isReflection)
            renderer = renderer->parent();
        layerObject->setIsGeneratedContent(true);
        layerObject->setPseudoElementId(bindPseudoElement(downcast<PseudoElement>(renderer->node())));
        if (renderer->isBeforeContent())
            layerObject->setPseudoElement("before");
        else if (renderer->isAfterContent())
            layerObject->setPseudoElement("after");
    }

    // FIXME: RenderView is now really anonymous but don't tell about it to the frontend before making sure it can handle it.
    if (isAnonymous && !renderer->isRenderView()) {
        layerObject->setIsAnonymous(true);
        const RenderStyle& style = renderer->style();
        if (style.styleType() == FIRST_LETTER)
            layerObject->setPseudoElement("first-letter");
        else if (style.styleType() == FIRST_LINE)
            layerObject->setPseudoElement("first-line");
    }

    return layerObject;
}

int InspectorLayerTreeAgent::idForNode(ErrorString& errorString, Node* node)
{
    if (!node)
        return 0;

    InspectorDOMAgent* domAgent = m_instrumentingAgents.inspectorDOMAgent();
    
    int nodeId = domAgent->boundNodeId(node);
    if (!nodeId)
        nodeId = domAgent->pushNodeToFrontend(errorString, domAgent->boundNodeId(&node->document()), node);

    return nodeId;
}

Ref<Inspector::Protocol::LayerTree::IntRect> InspectorLayerTreeAgent::buildObjectForIntRect(const IntRect& rect)
{
    return Inspector::Protocol::LayerTree::IntRect::create()
        .setX(rect.x())
        .setY(rect.y())
        .setWidth(rect.width())
        .setHeight(rect.height())
        .release();
}

void InspectorLayerTreeAgent::reasonsForCompositingLayer(ErrorString& errorString, const String& layerId, RefPtr<Inspector::Protocol::LayerTree::CompositingReasons>& compositingReasonsResult)
{
    const RenderLayer* renderLayer = m_idToLayer.get(layerId);

    if (!renderLayer) {
        errorString = ASCIILiteral("Could not find a bound layer for the provided id");
        return;
    }

    OptionSet<CompositingReason> reasons = renderLayer->compositor().reasonsForCompositing(*renderLayer);
    auto compositingReasons = Inspector::Protocol::LayerTree::CompositingReasons::create().release();

    if (reasons.contains(CompositingReason::Transform3D))
        compositingReasons->setTransform3D(true);

    if (reasons.contains(CompositingReason::Video))
        compositingReasons->setVideo(true);
    else if (reasons.contains(CompositingReason::Canvas))
        compositingReasons->setCanvas(true);
    else if (reasons.contains(CompositingReason::Plugin))
        compositingReasons->setPlugin(true);
    else if (reasons.contains(CompositingReason::IFrame))
        compositingReasons->setIFrame(true);

    if (reasons.contains(CompositingReason::BackfaceVisibilityHidden))
        compositingReasons->setBackfaceVisibilityHidden(true);

    if (reasons.contains(CompositingReason::ClipsCompositingDescendants))
        compositingReasons->setClipsCompositingDescendants(true);

    if (reasons.contains(CompositingReason::Animation))
        compositingReasons->setAnimation(true);

    if (reasons.contains(CompositingReason::Filters))
        compositingReasons->setFilters(true);

    if (reasons.contains(CompositingReason::PositionFixed))
        compositingReasons->setPositionFixed(true);

    if (reasons.contains(CompositingReason::PositionSticky))
        compositingReasons->setPositionSticky(true);

    if (reasons.contains(CompositingReason::OverflowScrollingTouch))
        compositingReasons->setOverflowScrollingTouch(true);

    if (reasons.contains(CompositingReason::Stacking))
        compositingReasons->setStacking(true);

    if (reasons.contains(CompositingReason::Overlap))
        compositingReasons->setOverlap(true);

    if (reasons.contains(CompositingReason::NegativeZIndexChildren))
        compositingReasons->setNegativeZIndexChildren(true);

    if (reasons.contains(CompositingReason::TransformWithCompositedDescendants))
        compositingReasons->setTransformWithCompositedDescendants(true);

    if (reasons.contains(CompositingReason::OpacityWithCompositedDescendants))
        compositingReasons->setOpacityWithCompositedDescendants(true);

    if (reasons.contains(CompositingReason::MaskWithCompositedDescendants))
        compositingReasons->setMaskWithCompositedDescendants(true);

    if (reasons.contains(CompositingReason::ReflectionWithCompositedDescendants))
        compositingReasons->setReflectionWithCompositedDescendants(true);

    if (reasons.contains(CompositingReason::FilterWithCompositedDescendants))
        compositingReasons->setFilterWithCompositedDescendants(true);

    if (reasons.contains(CompositingReason::BlendingWithCompositedDescendants))
        compositingReasons->setBlendingWithCompositedDescendants(true);

    if (reasons.contains(CompositingReason::IsolatesCompositedBlendingDescendants))
        compositingReasons->setIsolatesCompositedBlendingDescendants(true);

    if (reasons.contains(CompositingReason::Perspective))
        compositingReasons->setPerspective(true);

    if (reasons.contains(CompositingReason::Preserve3D))
        compositingReasons->setPreserve3D(true);

    if (reasons.contains(CompositingReason::WillChange))
        compositingReasons->setWillChange(true);

    if (reasons.contains(CompositingReason::Root))
        compositingReasons->setRoot(true);

    compositingReasonsResult = WTFMove(compositingReasons);
}

String InspectorLayerTreeAgent::bind(const RenderLayer* layer)
{
    if (!layer)
        return emptyString();
    String identifier = m_documentLayerToIdMap.get(layer);
    if (identifier.isNull()) {
        identifier = IdentifiersFactory::createIdentifier();
        m_documentLayerToIdMap.set(layer, identifier);
        m_idToLayer.set(identifier, layer);
    }
    return identifier;
}

void InspectorLayerTreeAgent::unbind(const RenderLayer* layer)
{
    HashMap<const RenderLayer*, String>::iterator iterator = m_documentLayerToIdMap.find(layer);
    if (iterator == m_documentLayerToIdMap.end())
        return;
    m_idToLayer.remove(iterator->value);
    m_documentLayerToIdMap.remove(iterator);
}

String InspectorLayerTreeAgent::bindPseudoElement(PseudoElement* pseudoElement)
{
    if (!pseudoElement)
        return emptyString();
    String identifier = m_pseudoElementToIdMap.get(pseudoElement);
    if (identifier.isNull()) {
        identifier = IdentifiersFactory::createIdentifier();
        m_pseudoElementToIdMap.set(pseudoElement, identifier);
        m_idToPseudoElement.set(identifier, pseudoElement);
    }
    return identifier;
}

void InspectorLayerTreeAgent::unbindPseudoElement(PseudoElement* pseudoElement)
{
    HashMap<PseudoElement*, String>::iterator iterator = m_pseudoElementToIdMap.find(pseudoElement);
    if (iterator == m_pseudoElementToIdMap.end())
        return;
    m_idToPseudoElement.remove(iterator->value);
    m_pseudoElementToIdMap.remove(iterator);
}

} // namespace WebCore
