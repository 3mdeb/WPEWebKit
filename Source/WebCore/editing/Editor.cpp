/*
 * Copyright (C) 2006-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "Editor.h"

#include "AXObjectCache.h"
#include "AlternativeTextController.h"
#include "ApplyStyleCommand.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSPropertyNames.h"
#include "CachedResourceLoader.h"
#include "ClipboardEvent.h"
#include "CompositionEvent.h"
#include "CreateLinkCommand.h"
#include "DataTransfer.h"
#include "DeleteSelectionCommand.h"
#include "DictationAlternative.h"
#include "DictationCommand.h"
#include "DocumentFragment.h"
#include "DocumentMarkerController.h"
#include "Editing.h"
#include "EditorClient.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLCollection.h"
#include "HTMLFormControlElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLSpanElement.h"
#include "HitTestResult.h"
#include "IndentOutdentCommand.h"
#include "InputEvent.h"
#include "InsertListCommand.h"
#include "InsertTextCommand.h"
#include "KeyboardEvent.h"
#include "Logging.h"
#include "MainFrame.h"
#include "ModifySelectionListLevel.h"
#include "NodeList.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "Pasteboard.h"
#include "Range.h"
#include "RemoveFormatCommand.h"
#include "RenderBlock.h"
#include "RenderTextControl.h"
#include "RenderedDocumentMarker.h"
#include "RenderedPosition.h"
#include "ReplaceRangeWithTextCommand.h"
#include "ReplaceSelectionCommand.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SimplifyMarkupCommand.h"
#include "SpellChecker.h"
#include "SpellingCorrectionCommand.h"
#include "StaticPasteboard.h"
#include "StyleProperties.h"
#include "TelephoneNumberDetector.h"
#include "Text.h"
#include "TextCheckerClient.h"
#include "TextCheckingHelper.h"
#include "TextEvent.h"
#include "TextIterator.h"
#include "TypingCommand.h"
#include "UserTypingGestureIndicator.h"
#include "VisibleUnits.h"
#include "markup.h"
#include <pal/system/Sound.h>
#include <pal/text/KillRing.h>
#include <wtf/unicode/CharacterNames.h>

#if PLATFORM(MAC)
#include "ServicesOverlayController.h"
#endif

namespace WebCore {

static bool dispatchBeforeInputEvent(Element& element, const AtomicString& inputType, const String& data = { }, RefPtr<DataTransfer>&& dataTransfer = nullptr, const Vector<RefPtr<StaticRange>>& targetRanges = { }, bool cancelable = true)
{
    if (!element.document().settings().inputEventsEnabled())
        return true;

    return element.dispatchEvent(InputEvent::create(eventNames().beforeinputEvent, inputType, true, cancelable, element.document().defaultView(), data, WTFMove(dataTransfer), targetRanges, 0));
}

static void dispatchInputEvent(Element& element, const AtomicString& inputType, const String& data = { }, RefPtr<DataTransfer>&& dataTransfer = nullptr, const Vector<RefPtr<StaticRange>>& targetRanges = { })
{
    if (element.document().settings().inputEventsEnabled()) {
        // FIXME: We should not be dispatching to the scoped queue here. Normally, input events are dispatched in CompositeEditCommand::apply after the end of the scope,
        // but TypingCommands are special in that existing TypingCommands that are applied again fire input events *from within* the scope by calling typingAddedToOpenCommand.
        // Instead, TypingCommands should always dispatch events synchronously after the end of the scoped queue in CompositeEditCommand::apply. To work around this for the
        // time being, just revert back to calling dispatchScopedEvent.
        element.dispatchScopedEvent(InputEvent::create(eventNames().inputEvent, inputType, true, false, element.document().defaultView(), data, WTFMove(dataTransfer), targetRanges, 0));
    } else
        element.dispatchInputEvent();
}

static String inputEventDataForEditingStyleAndAction(const StyleProperties* style, EditAction action)
{
    if (!style)
        return { };

    switch (action) {
    case EditActionSetColor:
        return style->getPropertyValue(CSSPropertyColor);
    case EditActionSetWritingDirection:
        return style->getPropertyValue(CSSPropertyDirection);
    default:
        return { };
    }
}

static String inputEventDataForEditingStyleAndAction(EditingStyle& style, EditAction action)
{
    return inputEventDataForEditingStyleAndAction(style.style(), action);
}

class ClearTextCommand : public DeleteSelectionCommand {
public:
    ClearTextCommand(Document& document);
    static void CreateAndApply(const RefPtr<Frame> frame);
    
private:
    EditAction editingAction() const override;
};

ClearTextCommand::ClearTextCommand(Document& document)
    : DeleteSelectionCommand(document, false, true, false, false, true)
{
}

EditAction ClearTextCommand::editingAction() const
{
    return EditActionDelete;
}

void ClearTextCommand::CreateAndApply(const RefPtr<Frame> frame)
{
    if (frame->selection().isNone())
        return;

    // Don't leave around stale composition state.
    frame->editor().clear();
    
    const VisibleSelection oldSelection = frame->selection().selection();
    frame->selection().selectAll();
    auto clearCommand = adoptRef(*new ClearTextCommand(*frame->document()));
    clearCommand->setStartingSelection(oldSelection);
    clearCommand->apply();
}

using namespace HTMLNames;
using namespace WTF;
using namespace Unicode;

TemporarySelectionChange::TemporarySelectionChange(Frame& frame, std::optional<VisibleSelection> temporarySelection, TemporarySelectionOptions options)
    : m_frame(frame)
    , m_options(options)
    , m_wasIgnoringSelectionChanges(frame.editor().ignoreSelectionChanges())
#if PLATFORM(IOS)
    , m_appearanceUpdatesWereEnabled(frame.selection().isUpdateAppearanceEnabled())
#endif
{
#if PLATFORM(IOS)
    if (options & TemporarySelectionOptionEnableAppearanceUpdates)
        frame.selection().setUpdateAppearanceEnabled(true);
#endif

    if (options & TemporarySelectionOptionIgnoreSelectionChanges)
        frame.editor().setIgnoreSelectionChanges(true);

    if (temporarySelection) {
        m_selectionToRestore = frame.selection().selection();
        frame.selection().setSelection(temporarySelection.value());
    }
}

TemporarySelectionChange::~TemporarySelectionChange()
{
    if (m_selectionToRestore)
        m_frame->selection().setSelection(m_selectionToRestore.value());

    if (m_options & TemporarySelectionOptionIgnoreSelectionChanges) {
        auto revealSelection = m_options & TemporarySelectionOptionRevealSelection ? Editor::RevealSelection::Yes : Editor::RevealSelection::No;
        m_frame->editor().setIgnoreSelectionChanges(m_wasIgnoringSelectionChanges, revealSelection);
    }

#if PLATFORM(IOS)
    if (m_options & TemporarySelectionOptionEnableAppearanceUpdates)
        m_frame->selection().setUpdateAppearanceEnabled(m_appearanceUpdatesWereEnabled);
#endif
}

// When an event handler has moved the selection outside of a text control
// we should use the target control's selection for this editing operation.
VisibleSelection Editor::selectionForCommand(Event* event)
{
    VisibleSelection selection = m_frame.selection().selection();
    if (!event)
        return selection;
    // If the target is a text control, and the current selection is outside of its shadow tree,
    // then use the saved selection for that text control.
    HTMLTextFormControlElement* textFormControlOfSelectionStart = enclosingTextFormControl(selection.start());
    HTMLTextFormControlElement* textFromControlOfTarget = is<HTMLTextFormControlElement>(*event->target()->toNode()) ? downcast<HTMLTextFormControlElement>(event->target()->toNode()) : nullptr;
    if (textFromControlOfTarget && (selection.start().isNull() || textFromControlOfTarget != textFormControlOfSelectionStart)) {
        if (RefPtr<Range> range = textFromControlOfTarget->selection())
            return VisibleSelection(*range, DOWNSTREAM, selection.isDirectional());
    }
    return selection;
}

// Function considers Mac editing behavior a fallback when Page or Settings is not available.
EditingBehavior Editor::behavior() const
{
    return EditingBehavior(m_frame.settings().editingBehaviorType());
}

EditorClient* Editor::client() const
{
    if (Page* page = m_frame.page())
        return &page->editorClient();
    return nullptr;
}

TextCheckerClient* Editor::textChecker() const
{
    if (EditorClient* owner = client())
        return owner->textChecker();
    return 0;
}

void Editor::handleKeyboardEvent(KeyboardEvent& event)
{
    if (EditorClient* c = client())
        c->handleKeyboardEvent(&event);
}

void Editor::handleInputMethodKeydown(KeyboardEvent& event)
{
    if (EditorClient* c = client())
        c->handleInputMethodKeydown(&event);
}

bool Editor::handleTextEvent(TextEvent& event)
{
    LOG(Editing, "Editor %p handleTextEvent (data %s)", this, event.data().utf8().data());

    // Default event handling for Drag and Drop will be handled by DragController
    // so we leave the event for it.
    if (event.isDrop())
        return false;

    if (event.isPaste()) {
        if (event.pastingFragment()) {
#if PLATFORM(IOS)
            if (client()->performsTwoStepPaste(event.pastingFragment()))
                return true;
#endif
            replaceSelectionWithFragment(*event.pastingFragment(), false, event.shouldSmartReplace(), event.shouldMatchStyle(), EditActionPaste, event.mailBlockquoteHandling());
        } else
            replaceSelectionWithText(event.data(), false, event.shouldSmartReplace(), EditActionPaste);
        return true;
    }

    String data = event.data();
    if (data == "\n") {
        if (event.isLineBreak())
            return insertLineBreak();
        return insertParagraphSeparator();
    }

    return insertTextWithoutSendingTextEvent(data, false, &event);
}

bool Editor::canEdit() const
{
    return m_frame.selection().selection().rootEditableElement();
}

bool Editor::canEditRichly() const
{
    return m_frame.selection().selection().isContentRichlyEditable();
}

// Returns whether caller should continue with "the default processing", which is the same as
// the event handler NOT setting the return value to false
// https://w3c.github.io/clipboard-apis/#fire-a-clipboard-event
static bool dispatchClipboardEvent(RefPtr<Element>&& target, const AtomicString& eventType)
{
    // FIXME: Move the target selection code here.
    if (!target)
        return true;

    DataTransfer::StoreMode storeMode;
    if (eventType == eventNames().pasteEvent)
        storeMode = DataTransfer::StoreMode::Readonly;
    else if (eventType == eventNames().copyEvent || eventType == eventNames().cutEvent)
        storeMode = DataTransfer::StoreMode::ReadWrite;
    else {
        ASSERT(eventType == eventNames().beforecutEvent || eventType == eventNames().beforecopyEvent || eventType == eventNames().beforepasteEvent);
        storeMode = DataTransfer::StoreMode::Invalid;
    }

    auto dataTransfer = DataTransfer::createForCopyAndPaste(storeMode);

    ClipboardEvent::Init init;
    init.bubbles = true;
    init.cancelable = true;
    init.clipboardData = dataTransfer.ptr();
    auto event = ClipboardEvent::create(eventType, init, Event::IsTrusted::Yes);

    target->dispatchEvent(event);
    bool noDefaultProcessing = event->defaultPrevented();
    if (noDefaultProcessing && storeMode == DataTransfer::StoreMode::ReadWrite) {
        auto pasteboard = Pasteboard::createForCopyAndPaste();
        pasteboard->clear();
        downcast<StaticPasteboard>(dataTransfer->pasteboard()).commitToPasteboard(*pasteboard);
    }

    dataTransfer->makeInvalidForSecurity();

    return !noDefaultProcessing;
}

// WinIE uses onbeforecut and onbeforepaste to enables the cut and paste menu items.  They
// also send onbeforecopy, apparently for symmetry, but it doesn't affect the menu items.
// We need to use onbeforecopy as a real menu enabler because we allow elements that are not
// normally selectable to implement copy/paste (like divs, or a document body).

bool Editor::canDHTMLCut()
{
    if (m_frame.selection().selection().isInPasswordField())
        return false;

    return !dispatchClipboardEvent(findEventTargetFromSelection(), eventNames().beforecutEvent);
}

bool Editor::canDHTMLCopy()
{
    if (m_frame.selection().selection().isInPasswordField())
        return false;
    return !dispatchClipboardEvent(findEventTargetFromSelection(), eventNames().beforecopyEvent);
}

bool Editor::canDHTMLPaste()
{
    return !dispatchClipboardEvent(findEventTargetFromSelection(), eventNames().beforepasteEvent);
}

bool Editor::canCut() const
{
    return canCopy() && canDelete();
}

static HTMLImageElement* imageElementFromImageDocument(Document& document)
{
    if (!document.isImageDocument())
        return nullptr;
    
    HTMLElement* body = document.bodyOrFrameset();
    if (!body)
        return nullptr;
    
    Node* node = body->firstChild();
    if (!is<HTMLImageElement>(node))
        return nullptr;
    return downcast<HTMLImageElement>(node);
}

bool Editor::canCopy() const
{
    if (imageElementFromImageDocument(document()))
        return true;
    const VisibleSelection& selection = m_frame.selection().selection();
    return selection.isRange() && !selection.isInPasswordField();
}

bool Editor::canPaste() const
{
    return canEdit();
}

bool Editor::canDelete() const
{
    const VisibleSelection& selection = m_frame.selection().selection();
    return selection.isRange() && selection.rootEditableElement();
}

bool Editor::canDeleteRange(Range* range) const
{
    Node& startContainer = range->startContainer();
    Node& endContainer = range->endContainer();
    
    if (!startContainer.hasEditableStyle() || !endContainer.hasEditableStyle())
        return false;

    if (range->collapsed()) {
        VisiblePosition start(range->startPosition(), DOWNSTREAM);
        VisiblePosition previous = start.previous();
        // FIXME: We sometimes allow deletions at the start of editable roots, like when the caret is in an empty list item.
        if (previous.isNull() || previous.deepEquivalent().deprecatedNode()->rootEditableElement() != startContainer.rootEditableElement())
            return false;
    }
    return true;
}

bool Editor::smartInsertDeleteEnabled()
{   
    return client() && client()->smartInsertDeleteEnabled();
}
    
bool Editor::canSmartCopyOrDelete()
{
    return client() && client()->smartInsertDeleteEnabled() && m_frame.selection().granularity() == WordGranularity;
}

bool Editor::isSelectTrailingWhitespaceEnabled() const
{
    return client() && client()->isSelectTrailingWhitespaceEnabled();
}

bool Editor::deleteWithDirection(SelectionDirection direction, TextGranularity granularity, bool shouldAddToKillRing, bool isTypingAction)
{
    if (!canEdit())
        return false;

    if (m_frame.selection().isRange()) {
        if (isTypingAction) {
            TypingCommand::deleteKeyPressed(document(), canSmartCopyOrDelete() ? TypingCommand::SmartDelete : 0, granularity);
            revealSelectionAfterEditingOperation();
        } else {
            if (shouldAddToKillRing)
                addRangeToKillRing(*selectedRange().get(), KillRingInsertionMode::AppendText);
            deleteSelectionWithSmartDelete(canSmartCopyOrDelete());
            // Implicitly calls revealSelectionAfterEditingOperation().
        }
    } else {
        TypingCommand::Options options = 0;
        if (canSmartCopyOrDelete())
            options |= TypingCommand::SmartDelete;
        if (shouldAddToKillRing)
            options |= TypingCommand::AddsToKillRing;
        switch (direction) {
        case DirectionForward:
        case DirectionRight:
            TypingCommand::forwardDeleteKeyPressed(document(), options, granularity);
            break;
        case DirectionBackward:
        case DirectionLeft:
            TypingCommand::deleteKeyPressed(document(), options, granularity);
            break;
        }
        revealSelectionAfterEditingOperation();
    }

    // FIXME: We should to move this down into deleteKeyPressed.
    // clear the "start new kill ring sequence" setting, because it was set to true
    // when the selection was updated by deleting the range
    if (shouldAddToKillRing)
        setStartNewKillRingSequence(false);

    return true;
}

void Editor::deleteSelectionWithSmartDelete(bool smartDelete, EditAction editingAction)
{
    if (m_frame.selection().isNone())
        return;

    DeleteSelectionCommand::create(document(), smartDelete, true, false, false, true, editingAction)->apply();
}

void Editor::clearText()
{
    ClearTextCommand::CreateAndApply(&m_frame);
}

void Editor::pasteAsPlainText(const String& pastingText, bool smartReplace)
{
    Element* target = findEventTargetFromSelection();
    if (!target)
        return;
    target->dispatchEvent(TextEvent::createForPlainTextPaste(document().domWindow(), pastingText, smartReplace));
}

void Editor::pasteAsFragment(Ref<DocumentFragment>&& pastingFragment, bool smartReplace, bool matchStyle, MailBlockquoteHandling respectsMailBlockquote)
{
    Element* target = findEventTargetFromSelection();
    if (!target)
        return;
    target->dispatchEvent(TextEvent::createForFragmentPaste(document().domWindow(), WTFMove(pastingFragment), smartReplace, matchStyle, respectsMailBlockquote));
}

void Editor::pasteAsPlainTextBypassingDHTML()
{
    pasteAsPlainTextWithPasteboard(*Pasteboard::createForCopyAndPaste());
}

void Editor::pasteAsPlainTextWithPasteboard(Pasteboard& pasteboard)
{
    String text = readPlainTextFromPasteboard(pasteboard);
    if (client() && client()->shouldInsertText(text, selectedRange().get(), EditorInsertAction::Pasted))
        pasteAsPlainText(text, canSmartReplaceWithPasteboard(pasteboard));
}

String Editor::readPlainTextFromPasteboard(Pasteboard& pasteboard)
{
    PasteboardPlainText text;
    pasteboard.read(text);
    return plainTextFromPasteboard(text);
}

#if !PLATFORM(MAC)

String Editor::plainTextFromPasteboard(const PasteboardPlainText& text)
{
    return text.text;
}

#endif

bool Editor::canSmartReplaceWithPasteboard(Pasteboard& pasteboard)
{
    return client() && client()->smartInsertDeleteEnabled() && pasteboard.canSmartReplace();
}

bool Editor::shouldInsertFragment(DocumentFragment& fragment, Range* replacingDOMRange, EditorInsertAction givenAction)
{
    if (!client())
        return false;
    
    auto* child = fragment.firstChild();
    if (is<CharacterData>(child) && fragment.lastChild() == child)
        return client()->shouldInsertText(downcast<CharacterData>(*child).data(), replacingDOMRange, givenAction);

    return client()->shouldInsertNode(&fragment, replacingDOMRange, givenAction);
}

void Editor::replaceSelectionWithFragment(DocumentFragment& fragment, bool selectReplacement, bool smartReplace, bool matchStyle, EditAction editingAction, MailBlockquoteHandling mailBlockquoteHandling)
{
    VisibleSelection selection = m_frame.selection().selection();
    if (selection.isNone() || !selection.isContentEditable())
        return;

    AccessibilityReplacedText replacedText;
    if (AXObjectCache::accessibilityEnabled() && editingAction == EditActionPaste)
        replacedText = AccessibilityReplacedText(selection);

    ReplaceSelectionCommand::CommandOptions options = ReplaceSelectionCommand::PreventNesting | ReplaceSelectionCommand::SanitizeFragment;
    if (selectReplacement)
        options |= ReplaceSelectionCommand::SelectReplacement;
    if (smartReplace)
        options |= ReplaceSelectionCommand::SmartReplace;
    if (matchStyle)
        options |= ReplaceSelectionCommand::MatchStyle;
    if (mailBlockquoteHandling == MailBlockquoteHandling::IgnoreBlockquote)
        options |= ReplaceSelectionCommand::IgnoreMailBlockquote;

    auto command = ReplaceSelectionCommand::create(document(), &fragment, options, editingAction);
    command->apply();
    revealSelectionAfterEditingOperation();

    selection = m_frame.selection().selection();
    if (selection.isInPasswordField())
        return;

    if (AXObjectCache::accessibilityEnabled() && editingAction == EditActionPaste) {
        String text = AccessibilityObject::stringForVisiblePositionRange(command->visibleSelectionForInsertedText());
        replacedText.postTextStateChangeNotification(document().existingAXObjectCache(), AXTextEditTypePaste, text, m_frame.selection().selection());
        command->composition()->setRangeDeletedByUnapply(replacedText.replacedRange());
    }

    if (!isContinuousSpellCheckingEnabled())
        return;

    Node* nodeToCheck = selection.rootEditableElement();
    if (!nodeToCheck)
        return;

    auto rangeToCheck = Range::create(document(), firstPositionInNode(nodeToCheck), lastPositionInNode(nodeToCheck));
    if (auto request = SpellCheckRequest::create(resolveTextCheckingTypeMask(*nodeToCheck, TextCheckingTypeSpelling | TextCheckingTypeGrammar), TextCheckingProcessBatch, rangeToCheck.copyRef(), rangeToCheck.copyRef()))
        m_spellChecker->requestCheckingFor(request.releaseNonNull());
}

void Editor::replaceSelectionWithText(const String& text, bool selectReplacement, bool smartReplace, EditAction editingAction)
{
    RefPtr<Range> range = selectedRange();
    if (!range)
        return;

    replaceSelectionWithFragment(createFragmentFromText(*range, text), selectReplacement, smartReplace, true, editingAction);
}

RefPtr<Range> Editor::selectedRange()
{
    return m_frame.selection().toNormalizedRange();
}

bool Editor::shouldDeleteRange(Range* range) const
{
    if (!range || range->collapsed())
        return false;
    
    if (!canDeleteRange(range))
        return false;

    return client() && client()->shouldDeleteRange(range);
}

bool Editor::tryDHTMLCopy()
{   
    if (m_frame.selection().selection().isInPasswordField())
        return false;

    return !dispatchClipboardEvent(findEventTargetFromSelection(), eventNames().copyEvent);
}

bool Editor::tryDHTMLCut()
{
    if (m_frame.selection().selection().isInPasswordField())
        return false;
    
    return !dispatchClipboardEvent(findEventTargetFromSelection(), eventNames().cutEvent);
}

bool Editor::tryDHTMLPaste()
{
    return !dispatchClipboardEvent(findEventTargetFromSelection(), eventNames().pasteEvent);
}

bool Editor::shouldInsertText(const String& text, Range* range, EditorInsertAction action) const
{
    if (m_frame.mainFrame().loader().shouldSuppressKeyboardInput() && action == EditorInsertAction::Typed)
        return false;

    return client() && client()->shouldInsertText(text, range, action);
}

void Editor::respondToChangedContents(const VisibleSelection& endingSelection)
{
    if (AXObjectCache::accessibilityEnabled()) {
        Node* node = endingSelection.start().deprecatedNode();
        if (AXObjectCache* cache = document().existingAXObjectCache())
            cache->postNotification(node, AXObjectCache::AXValueChanged, TargetObservableParent);
    }

    updateMarkersForWordsAffectedByEditing(true);

    if (client())
        client()->respondToChangedContents();
}

bool Editor::hasBidiSelection() const
{
    if (m_frame.selection().isNone())
        return false;

    Node* startNode;
    if (m_frame.selection().isRange()) {
        startNode = m_frame.selection().selection().start().downstream().deprecatedNode();
        Node* endNode = m_frame.selection().selection().end().upstream().deprecatedNode();
        if (enclosingBlock(startNode) != enclosingBlock(endNode))
            return false;
    } else
        startNode = m_frame.selection().selection().visibleStart().deepEquivalent().deprecatedNode();

    if (!startNode)
        return false;

    auto renderer = startNode->renderer();
    while (renderer && !is<RenderBlockFlow>(*renderer))
        renderer = renderer->parent();

    if (!renderer)
        return false;

    if (!renderer->style().isLeftToRightDirection())
        return true;

    return downcast<RenderBlockFlow>(*renderer).containsNonZeroBidiLevel();
}

TriState Editor::selectionUnorderedListState() const
{
    if (m_frame.selection().isCaret()) {
        if (enclosingElementWithTag(m_frame.selection().selection().start(), ulTag))
            return TrueTriState;
    } else if (m_frame.selection().isRange()) {
        auto* startNode = enclosingElementWithTag(m_frame.selection().selection().start(), ulTag);
        auto* endNode = enclosingElementWithTag(m_frame.selection().selection().end(), ulTag);
        if (startNode && endNode && startNode == endNode)
            return TrueTriState;
    }

    return FalseTriState;
}

TriState Editor::selectionOrderedListState() const
{
    if (m_frame.selection().isCaret()) {
        if (enclosingElementWithTag(m_frame.selection().selection().start(), olTag))
            return TrueTriState;
    } else if (m_frame.selection().isRange()) {
        auto* startNode = enclosingElementWithTag(m_frame.selection().selection().start(), olTag);
        auto* endNode = enclosingElementWithTag(m_frame.selection().selection().end(), olTag);
        if (startNode && endNode && startNode == endNode)
            return TrueTriState;
    }

    return FalseTriState;
}

RefPtr<Node> Editor::insertOrderedList()
{
    if (!canEditRichly())
        return nullptr;
        
    RefPtr<Node> newList = InsertListCommand::insertList(document(), InsertListCommand::OrderedList);
    revealSelectionAfterEditingOperation();
    return newList;
}

RefPtr<Node> Editor::insertUnorderedList()
{
    if (!canEditRichly())
        return nullptr;
        
    RefPtr<Node> newList = InsertListCommand::insertList(document(), InsertListCommand::UnorderedList);
    revealSelectionAfterEditingOperation();
    return newList;
}

bool Editor::canIncreaseSelectionListLevel()
{
    return canEditRichly() && IncreaseSelectionListLevelCommand::canIncreaseSelectionListLevel(&document());
}

bool Editor::canDecreaseSelectionListLevel()
{
    return canEditRichly() && DecreaseSelectionListLevelCommand::canDecreaseSelectionListLevel(&document());
}

RefPtr<Node> Editor::increaseSelectionListLevel()
{
    if (!canEditRichly() || m_frame.selection().isNone())
        return nullptr;
    
    RefPtr<Node> newList = IncreaseSelectionListLevelCommand::increaseSelectionListLevel(&document());
    revealSelectionAfterEditingOperation();
    return newList;
}

RefPtr<Node> Editor::increaseSelectionListLevelOrdered()
{
    if (!canEditRichly() || m_frame.selection().isNone())
        return nullptr;
    
    RefPtr<Node> newList = IncreaseSelectionListLevelCommand::increaseSelectionListLevelOrdered(&document());
    revealSelectionAfterEditingOperation();
    return newList;
}

RefPtr<Node> Editor::increaseSelectionListLevelUnordered()
{
    if (!canEditRichly() || m_frame.selection().isNone())
        return nullptr;
    
    RefPtr<Node> newList = IncreaseSelectionListLevelCommand::increaseSelectionListLevelUnordered(&document());
    revealSelectionAfterEditingOperation();
    return newList;
}

void Editor::decreaseSelectionListLevel()
{
    if (!canEditRichly() || m_frame.selection().isNone())
        return;
    
    DecreaseSelectionListLevelCommand::decreaseSelectionListLevel(&document());
    revealSelectionAfterEditingOperation();
}

void Editor::removeFormattingAndStyle()
{
    RemoveFormatCommand::create(document())->apply();
}

void Editor::clearLastEditCommand() 
{
    m_lastEditCommand = nullptr;
}

Element* Editor::findEventTargetFrom(const VisibleSelection& selection) const
{
    Element* target = selection.start().element();
    if (!target)
        target = document().bodyOrFrameset();
    if (!target)
        return nullptr;

    return target;
}

Element* Editor::findEventTargetFromSelection() const
{
    return findEventTargetFrom(m_frame.selection().selection());
}

void Editor::applyStyle(StyleProperties* style, EditAction editingAction)
{
    if (style)
        applyStyle(EditingStyle::create(style), editingAction);
}

void Editor::applyStyle(RefPtr<EditingStyle>&& style, EditAction editingAction)
{
    if (!style)
        return;

    auto selectionType = m_frame.selection().selection().selectionType();
    if (selectionType == VisibleSelection::NoSelection)
        return;

    String inputTypeName = inputTypeNameForEditingAction(editingAction);
    String inputEventData = inputEventDataForEditingStyleAndAction(*style, editingAction);
    RefPtr<Element> element = m_frame.selection().selection().rootEditableElement();
    if (element && !dispatchBeforeInputEvent(*element, inputTypeName, inputEventData))
        return;

    switch (selectionType) {
    case VisibleSelection::CaretSelection:
        computeAndSetTypingStyle(*style, editingAction);
        break;
    case VisibleSelection::RangeSelection:
        ApplyStyleCommand::create(document(), style.get(), editingAction)->apply();
        break;
    default:
        break;
    }

    client()->didApplyStyle();
    if (element)
        dispatchInputEvent(*element, inputTypeName, inputEventData);
}
    
bool Editor::shouldApplyStyle(StyleProperties* style, Range* range)
{   
    return client()->shouldApplyStyle(style, range);
}
    
void Editor::applyParagraphStyle(StyleProperties* style, EditAction editingAction)
{
    if (!style)
        return;

    auto selectionType = m_frame.selection().selection().selectionType();
    if (selectionType == VisibleSelection::NoSelection)
        return;

    String inputTypeName = inputTypeNameForEditingAction(editingAction);
    String inputEventData = inputEventDataForEditingStyleAndAction(style, editingAction);
    RefPtr<Element> element = m_frame.selection().selection().rootEditableElement();
    if (element && !dispatchBeforeInputEvent(*element, inputTypeName, inputEventData))
        return;

    ApplyStyleCommand::create(document(), EditingStyle::create(style).ptr(), editingAction, ApplyStyleCommand::ForceBlockProperties)->apply();
    client()->didApplyStyle();
    if (element)
        dispatchInputEvent(*element, inputTypeName, inputEventData);
}

void Editor::applyStyleToSelection(StyleProperties* style, EditAction editingAction)
{
    if (!style || style->isEmpty() || !canEditRichly())
        return;

    if (!client() || !client()->shouldApplyStyle(style, m_frame.selection().toNormalizedRange().get()))
        return;
    applyStyle(style, editingAction);
}

void Editor::applyStyleToSelection(Ref<EditingStyle>&& style, EditAction editingAction)
{
    if (style->isEmpty() || !canEditRichly())
        return;

    // FIXME: This is wrong for text decorations since m_mutableStyle is empty.
    if (!client() || !client()->shouldApplyStyle(style->styleWithResolvedTextDecorations().ptr(), m_frame.selection().toNormalizedRange().get()))
        return;

    applyStyle(WTFMove(style), editingAction);
}

void Editor::applyParagraphStyleToSelection(StyleProperties* style, EditAction editingAction)
{
    if (!style || style->isEmpty() || !canEditRichly())
        return;
    
    if (client() && client()->shouldApplyStyle(style, m_frame.selection().toNormalizedRange().get()))
        applyParagraphStyle(style, editingAction);
}

bool Editor::selectionStartHasStyle(CSSPropertyID propertyID, const String& value) const
{
    if (auto editingStyle = EditingStyle::styleAtSelectionStart(m_frame.selection().selection(), propertyID == CSSPropertyBackgroundColor))
        return editingStyle->hasStyle(propertyID, value);
    return false;
}

TriState Editor::selectionHasStyle(CSSPropertyID propertyID, const String& value) const
{
    return EditingStyle::create(propertyID, value)->triStateOfStyle(m_frame.selection().selection());
}

String Editor::selectionStartCSSPropertyValue(CSSPropertyID propertyID)
{
    RefPtr<EditingStyle> selectionStyle = EditingStyle::styleAtSelectionStart(m_frame.selection().selection(),
        propertyID == CSSPropertyBackgroundColor);
    if (!selectionStyle || !selectionStyle->style())
        return String();

    if (propertyID == CSSPropertyFontSize)
        return String::number(selectionStyle->legacyFontSize(&document()));
    return selectionStyle->style()->getPropertyValue(propertyID);
}

void Editor::indent()
{
    IndentOutdentCommand::create(document(), IndentOutdentCommand::Indent)->apply();
}

void Editor::outdent()
{
    IndentOutdentCommand::create(document(), IndentOutdentCommand::Outdent)->apply();
}

static void notifyTextFromControls(Element* startRoot, Element* endRoot)
{
    HTMLTextFormControlElement* startingTextControl = enclosingTextFormControl(firstPositionInOrBeforeNode(startRoot));
    HTMLTextFormControlElement* endingTextControl = enclosingTextFormControl(firstPositionInOrBeforeNode(endRoot));
    if (startingTextControl)
        startingTextControl->didEditInnerTextValue();
    if (endingTextControl && startingTextControl != endingTextControl)
        endingTextControl->didEditInnerTextValue();
}

static bool dispatchBeforeInputEvents(RefPtr<Element> startRoot, RefPtr<Element> endRoot, const AtomicString& inputTypeName, const String& data = { }, RefPtr<DataTransfer>&& dataTransfer = nullptr, const Vector<RefPtr<StaticRange>>& targetRanges = { }, bool cancelable = true)
{
    bool continueWithDefaultBehavior = true;
    if (startRoot)
        continueWithDefaultBehavior &= dispatchBeforeInputEvent(*startRoot, inputTypeName, data, WTFMove(dataTransfer), targetRanges, cancelable);
    if (endRoot && endRoot != startRoot)
        continueWithDefaultBehavior &= dispatchBeforeInputEvent(*endRoot, inputTypeName, data, WTFMove(dataTransfer), targetRanges, cancelable);
    return continueWithDefaultBehavior;
}

static void dispatchInputEvents(RefPtr<Element> startRoot, RefPtr<Element> endRoot, const AtomicString& inputTypeName, const String& data = { }, RefPtr<DataTransfer>&& dataTransfer = nullptr, const Vector<RefPtr<StaticRange>>& targetRanges = { })
{
    if (startRoot)
        dispatchInputEvent(*startRoot, inputTypeName, data, WTFMove(dataTransfer), targetRanges);
    if (endRoot && endRoot != startRoot)
        dispatchInputEvent(*endRoot, inputTypeName, data, WTFMove(dataTransfer), targetRanges);
}

bool Editor::willApplyEditing(CompositeEditCommand& command, Vector<RefPtr<StaticRange>>&& targetRanges) const
{
    if (!command.shouldDispatchInputEvents())
        return true;

    auto* composition = command.composition();
    if (!composition)
        return true;

    return dispatchBeforeInputEvents(composition->startingRootEditableElement(), composition->endingRootEditableElement(), command.inputEventTypeName(), command.inputEventData(), command.inputEventDataTransfer(), targetRanges, command.isBeforeInputEventCancelable());
}

void Editor::appliedEditing(CompositeEditCommand& command)
{
    LOG(Editing, "Editor %p appliedEditing", this);

    document().updateLayout();

    ASSERT(command.composition());
    auto& composition = *command.composition();
    VisibleSelection newSelection(command.endingSelection());

    notifyTextFromControls(composition.startingRootEditableElement(), composition.endingRootEditableElement());

    if (command.isTopLevelCommand()) {
        // Don't clear the typing style with this selection change. We do those things elsewhere if necessary.
        FrameSelection::SetSelectionOptions options = command.isDictationCommand() ? FrameSelection::DictationTriggered : 0;

        changeSelectionAfterCommand(newSelection, options);
    }

    if (command.shouldDispatchInputEvents())
        dispatchInputEvents(composition.startingRootEditableElement(), composition.endingRootEditableElement(), command.inputEventTypeName(), command.inputEventData(), command.inputEventDataTransfer());

    if (command.isTopLevelCommand()) {
        updateEditorUINowIfScheduled();

        m_alternativeTextController->respondToAppliedEditing(&command);

        if (!command.preservesTypingStyle())
            m_frame.selection().clearTypingStyle();

        // Command will be equal to last edit command only in the case of typing
        if (m_lastEditCommand.get() == &command)
            ASSERT(command.isTypingCommand());
        else {
            // Only register a new undo command if the command passed in is
            // different from the last command
            m_lastEditCommand = &command;
            if (client())
                client()->registerUndoStep(m_lastEditCommand->ensureComposition());
        }
        respondToChangedContents(newSelection);
    }
}

bool Editor::willUnapplyEditing(const EditCommandComposition& composition) const
{
    return dispatchBeforeInputEvents(composition.startingRootEditableElement(), composition.endingRootEditableElement(), "historyUndo");
}

void Editor::unappliedEditing(EditCommandComposition& composition)
{
    document().updateLayout();

    notifyTextFromControls(composition.startingRootEditableElement(), composition.endingRootEditableElement());

    VisibleSelection newSelection(composition.startingSelection());
    changeSelectionAfterCommand(newSelection, FrameSelection::defaultSetSelectionOptions());
    dispatchInputEvents(composition.startingRootEditableElement(), composition.endingRootEditableElement(), "historyUndo");

    updateEditorUINowIfScheduled();

    m_alternativeTextController->respondToUnappliedEditing(&composition);

    m_lastEditCommand = nullptr;
    if (auto* client = this->client())
        client->registerRedoStep(composition);
    respondToChangedContents(newSelection);
}

bool Editor::willReapplyEditing(const EditCommandComposition& composition) const
{
    return dispatchBeforeInputEvents(composition.startingRootEditableElement(), composition.endingRootEditableElement(), "historyRedo");
}

void Editor::reappliedEditing(EditCommandComposition& composition)
{
    document().updateLayout();

    notifyTextFromControls(composition.startingRootEditableElement(), composition.endingRootEditableElement());

    VisibleSelection newSelection(composition.endingSelection());
    changeSelectionAfterCommand(newSelection, FrameSelection::defaultSetSelectionOptions());
    dispatchInputEvents(composition.startingRootEditableElement(), composition.endingRootEditableElement(), "historyRedo");
    
    updateEditorUINowIfScheduled();

    m_lastEditCommand = nullptr;
    if (auto* client = this->client())
        client->registerUndoStep(composition);
    respondToChangedContents(newSelection);
}

Editor::Editor(Frame& frame)
    : m_frame(frame)
    , m_killRing(std::make_unique<PAL::KillRing>())
    , m_spellChecker(std::make_unique<SpellChecker>(frame))
    , m_alternativeTextController(std::make_unique<AlternativeTextController>(frame))
    , m_editorUIUpdateTimer(*this, &Editor::editorUIUpdateTimerFired)
#if ENABLE(TELEPHONE_NUMBER_DETECTION) && !PLATFORM(IOS)
    , m_telephoneNumberDetectionUpdateTimer(*this, &Editor::scanSelectionForTelephoneNumbers)
#endif
{
}

Editor::~Editor()
{
}

void Editor::clear()
{
    if (m_compositionNode) {
        m_compositionNode = nullptr;
        if (EditorClient* client = this->client())
            client->discardedComposition(&m_frame);
    }
    m_customCompositionUnderlines.clear();
    m_shouldStyleWithCSS = false;
    m_defaultParagraphSeparator = EditorParagraphSeparatorIsDiv;
}

bool Editor::insertText(const String& text, Event* triggeringEvent, TextEventInputType inputType)
{
    return m_frame.eventHandler().handleTextInputEvent(text, triggeringEvent, inputType);
}

bool Editor::insertTextForConfirmedComposition(const String& text)
{
    return m_frame.eventHandler().handleTextInputEvent(text, 0, TextEventInputComposition);
}

bool Editor::insertDictatedText(const String& text, const Vector<DictationAlternative>& dictationAlternatives, Event* triggeringEvent)
{
    return m_alternativeTextController->insertDictatedText(text, dictationAlternatives, triggeringEvent);
}

bool Editor::insertTextWithoutSendingTextEvent(const String& text, bool selectInsertedText, TextEvent* triggeringEvent)
{
    if (text.isEmpty())
        return false;

    VisibleSelection selection = selectionForCommand(triggeringEvent);
    if (!selection.isContentEditable())
        return false;
    RefPtr<Range> range = selection.toNormalizedRange();

    if (!shouldInsertText(text, range.get(), EditorInsertAction::Typed))
        return true;

    updateMarkersForWordsAffectedByEditing(isSpaceOrNewline(text[0]));

    bool shouldConsiderApplyingAutocorrection = false;
    if (text == " " || text == "\t")
        shouldConsiderApplyingAutocorrection = true;

    if (text.length() == 1 && u_ispunct(text[0]) && !isAmbiguousBoundaryCharacter(text[0]))
        shouldConsiderApplyingAutocorrection = true;

    bool autocorrectionWasApplied = shouldConsiderApplyingAutocorrection && m_alternativeTextController->applyAutocorrectionBeforeTypingIfAppropriate();

    // Get the selection to use for the event that triggered this insertText.
    // If the event handler changed the selection, we may want to use a different selection
    // that is contained in the event target.
    selection = selectionForCommand(triggeringEvent);
    if (selection.isContentEditable()) {
        if (Node* selectionStart = selection.start().deprecatedNode()) {
            Ref<Document> document(selectionStart->document());

            // Insert the text
            if (triggeringEvent && triggeringEvent->isDictation())
                DictationCommand::insertText(document, text, triggeringEvent->dictationAlternatives(), selection);
            else {
                TypingCommand::Options options = 0;
                if (selectInsertedText)
                    options |= TypingCommand::SelectInsertedText;
                if (autocorrectionWasApplied)
                    options |= TypingCommand::RetainAutocorrectionIndicator;
                if (triggeringEvent && triggeringEvent->isAutocompletion())
                    options |= TypingCommand::IsAutocompletion;
                TypingCommand::insertText(document, text, selection, options, triggeringEvent && triggeringEvent->isComposition() ? TypingCommand::TextCompositionFinal : TypingCommand::TextCompositionNone);
            }

            // Reveal the current selection
            if (Frame* editedFrame = document->frame())
                if (Page* page = editedFrame->page()) {
#if PLATFORM(IOS)
                    SelectionRevealMode revealMode = SelectionRevealMode::RevealUpToMainFrame;
#else
                    SelectionRevealMode revealMode = SelectionRevealMode::Reveal;
#endif
                    page->focusController().focusedOrMainFrame().selection().revealSelection(revealMode, ScrollAlignment::alignCenterIfNeeded);
                }
        }
    }

    return true;
}

bool Editor::insertLineBreak()
{
    if (!canEdit())
        return false;

    if (!shouldInsertText("\n", m_frame.selection().toNormalizedRange().get(), EditorInsertAction::Typed))
        return true;

    VisiblePosition caret = m_frame.selection().selection().visibleStart();
    bool alignToEdge = isEndOfEditableOrNonEditableContent(caret);
    bool autocorrectionIsApplied = m_alternativeTextController->applyAutocorrectionBeforeTypingIfAppropriate();
    TypingCommand::insertLineBreak(document(), autocorrectionIsApplied ? TypingCommand::RetainAutocorrectionIndicator : 0);
    revealSelectionAfterEditingOperation(alignToEdge ? ScrollAlignment::alignToEdgeIfNeeded : ScrollAlignment::alignCenterIfNeeded);

    return true;
}

bool Editor::insertParagraphSeparator()
{
    if (!canEdit())
        return false;

    if (!canEditRichly())
        return insertLineBreak();

    if (!shouldInsertText("\n", m_frame.selection().toNormalizedRange().get(), EditorInsertAction::Typed))
        return true;

    VisiblePosition caret = m_frame.selection().selection().visibleStart();
    bool alignToEdge = isEndOfEditableOrNonEditableContent(caret);
    bool autocorrectionIsApplied = m_alternativeTextController->applyAutocorrectionBeforeTypingIfAppropriate();
    TypingCommand::insertParagraphSeparator(document(), autocorrectionIsApplied ? TypingCommand::RetainAutocorrectionIndicator : 0);
    revealSelectionAfterEditingOperation(alignToEdge ? ScrollAlignment::alignToEdgeIfNeeded : ScrollAlignment::alignCenterIfNeeded);

    return true;
}

bool Editor::insertParagraphSeparatorInQuotedContent()
{
    // FIXME: Why is this missing calls to canEdit, canEditRichly, etc.?
    TypingCommand::insertParagraphSeparatorInQuotedContent(document());
    revealSelectionAfterEditingOperation();
    return true;
}

void Editor::cut()
{
    if (tryDHTMLCut())
        return; // DHTML did the whole operation
    if (!canCut()) {
        PAL::systemBeep();
        return;
    }

    performCutOrCopy(CutAction);
}

void Editor::copy()
{
    if (tryDHTMLCopy())
        return; // DHTML did the whole operation
    if (!canCopy()) {
        PAL::systemBeep();
        return;
    }

    performCutOrCopy(CopyAction);
}

void Editor::postTextStateChangeNotificationForCut(const String& text, const VisibleSelection& selection)
{
    if (!AXObjectCache::accessibilityEnabled())
        return;
    if (!text.length())
        return;
    AXObjectCache* cache = document().existingAXObjectCache();
    if (!cache)
        return;
    cache->postTextStateChangeNotification(selection.start().anchorNode(), AXTextEditTypeCut, text, selection.start());
}

void Editor::performCutOrCopy(EditorActionSpecifier action)
{
    RefPtr<Range> selection = selectedRange();
    willWriteSelectionToPasteboard(selection.get());
    if (action == CutAction) {
        if (!shouldDeleteRange(selection.get()))
            return;

        updateMarkersForWordsAffectedByEditing(true);
    }

    if (enclosingTextFormControl(m_frame.selection().selection().start()))
        Pasteboard::createForCopyAndPaste()->writePlainText(selectedTextForDataTransfer(), canSmartCopyOrDelete() ? Pasteboard::CanSmartReplace : Pasteboard::CannotSmartReplace);
    else {
        HTMLImageElement* imageElement = nullptr;
        if (action == CopyAction)
            imageElement = imageElementFromImageDocument(document());

        if (imageElement) {
#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
            writeImageToPasteboard(*Pasteboard::createForCopyAndPaste(), *imageElement, document().url(), document().title());
#else
            Pasteboard::createForCopyAndPaste()->writeImage(*imageElement, document().url(), document().title());
#endif
        } else {
#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
            writeSelectionToPasteboard(*Pasteboard::createForCopyAndPaste());
#else
            // FIXME: Convert all other platforms to match Mac and delete this.
            Pasteboard::createForCopyAndPaste()->writeSelection(*selection, canSmartCopyOrDelete(), m_frame, IncludeImageAltTextForDataTransfer);
#endif
        }
    }

    didWriteSelectionToPasteboard();
    if (action == CutAction) {
        String text;
        if (AXObjectCache::accessibilityEnabled())
            text = AccessibilityObject::stringForVisiblePositionRange(m_frame.selection().selection());
        deleteSelectionWithSmartDelete(canSmartCopyOrDelete(), EditActionCut);
        if (AXObjectCache::accessibilityEnabled())
            postTextStateChangeNotificationForCut(text, m_frame.selection().selection());
    }
}

void Editor::paste()
{
    paste(*Pasteboard::createForCopyAndPaste());
}

void Editor::paste(Pasteboard& pasteboard)
{
    if (tryDHTMLPaste())
        return; // DHTML did the whole operation
    if (!canPaste())
        return;
    updateMarkersForWordsAffectedByEditing(false);
    ResourceCacheValidationSuppressor validationSuppressor(document().cachedResourceLoader());
    if (m_frame.selection().selection().isContentRichlyEditable())
        pasteWithPasteboard(&pasteboard, true);
    else
        pasteAsPlainTextWithPasteboard(pasteboard);
}

void Editor::pasteAsPlainText()
{
    if (tryDHTMLPaste())
        return;
    if (!canPaste())
        return;
    updateMarkersForWordsAffectedByEditing(false);
    pasteAsPlainTextWithPasteboard(*Pasteboard::createForCopyAndPaste());
}

void Editor::performDelete()
{
    if (!canDelete()) {
        PAL::systemBeep();
        return;
    }

    addRangeToKillRing(*selectedRange().get(), KillRingInsertionMode::AppendText);
    deleteSelectionWithSmartDelete(canSmartCopyOrDelete());

    // clear the "start new kill ring sequence" setting, because it was set to true
    // when the selection was updated by deleting the range
    setStartNewKillRingSequence(false);
}

void Editor::simplifyMarkup(Node* startNode, Node* endNode)
{
    if (!startNode)
        return;
    if (endNode) {
        if (&startNode->document() != &endNode->document())
            return;
        // check if start node is before endNode
        Node* node = startNode;
        while (node && node != endNode)
            node = NodeTraversal::next(*node);
        if (!node)
            return;
    }
    
    SimplifyMarkupCommand::create(document(), startNode, endNode ? NodeTraversal::next(*endNode) : nullptr)->apply();
}

void Editor::copyURL(const URL& url, const String& title)
{
    copyURL(url, title, *Pasteboard::createForCopyAndPaste());
}

void Editor::copyURL(const URL& url, const String& title, Pasteboard& pasteboard)
{
    PasteboardURL pasteboardURL;
    pasteboardURL.url = url;
    pasteboardURL.title = title;

#if PLATFORM(MAC)
    pasteboardURL.userVisibleForm = userVisibleString(url);
#endif

    pasteboard.write(pasteboardURL);
}

PasteboardWriterData::URL Editor::pasteboardWriterURL(const URL& url, const String& title)
{
    PasteboardWriterData::URL result;

    result.url = url;
    result.title = title;
#if PLATFORM(MAC)
    result.userVisibleForm = userVisibleString(url);
#endif

    return result;
}

#if !PLATFORM(IOS)

void Editor::copyImage(const HitTestResult& result)
{
    Element* element = result.innerNonSharedElement();
    if (!element)
        return;

    URL url = result.absoluteLinkURL();
    if (url.isEmpty())
        url = result.absoluteImageURL();

#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
    writeImageToPasteboard(*Pasteboard::createForCopyAndPaste(), *element, url, result.altDisplayString());
#else
    Pasteboard::createForCopyAndPaste()->writeImage(*element, url, result.altDisplayString());
#endif
}

#endif

bool Editor::isContinuousSpellCheckingEnabled() const
{
    return client() && client()->isContinuousSpellCheckingEnabled();
}

void Editor::toggleContinuousSpellChecking()
{
    if (client())
        client()->toggleContinuousSpellChecking();
}

bool Editor::isGrammarCheckingEnabled()
{
    return client() && client()->isGrammarCheckingEnabled();
}

void Editor::toggleGrammarChecking()
{
    if (client())
        client()->toggleGrammarChecking();
}

int Editor::spellCheckerDocumentTag()
{
    return client() ? client()->spellCheckerDocumentTag() : 0;
}

#if USE(APPKIT)

void Editor::uppercaseWord()
{
    if (client())
        client()->uppercaseWord();
}

void Editor::lowercaseWord()
{
    if (client())
        client()->lowercaseWord();
}

void Editor::capitalizeWord()
{
    if (client())
        client()->capitalizeWord();
}
    
#endif

#if USE(AUTOMATIC_TEXT_REPLACEMENT)

void Editor::showSubstitutionsPanel()
{
    if (!client()) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }

    if (client()->substitutionsPanelIsShowing()) {
        client()->showSubstitutionsPanel(false);
        return;
    }
    client()->showSubstitutionsPanel(true);
}

bool Editor::substitutionsPanelIsShowing()
{
    if (!client())
        return false;
    return client()->substitutionsPanelIsShowing();
}

void Editor::toggleSmartInsertDelete()
{
    if (client())
        client()->toggleSmartInsertDelete();
}

bool Editor::isAutomaticQuoteSubstitutionEnabled()
{
    return client() && client()->isAutomaticQuoteSubstitutionEnabled();
}

void Editor::toggleAutomaticQuoteSubstitution()
{
    if (client())
        client()->toggleAutomaticQuoteSubstitution();
}

bool Editor::isAutomaticLinkDetectionEnabled()
{
    return client() && client()->isAutomaticLinkDetectionEnabled();
}

void Editor::toggleAutomaticLinkDetection()
{
    if (client())
        client()->toggleAutomaticLinkDetection();
}

bool Editor::isAutomaticDashSubstitutionEnabled()
{
    return client() && client()->isAutomaticDashSubstitutionEnabled();
}

void Editor::toggleAutomaticDashSubstitution()
{
    if (client())
        client()->toggleAutomaticDashSubstitution();
}

bool Editor::isAutomaticTextReplacementEnabled()
{
    return client() && client()->isAutomaticTextReplacementEnabled();
}

void Editor::toggleAutomaticTextReplacement()
{
    if (client())
        client()->toggleAutomaticTextReplacement();
}

bool Editor::isAutomaticSpellingCorrectionEnabled()
{
    return m_alternativeTextController->isAutomaticSpellingCorrectionEnabled();
}

void Editor::toggleAutomaticSpellingCorrection()
{
    if (client())
        client()->toggleAutomaticSpellingCorrection();
}

#endif

bool Editor::shouldEndEditing(Range* range)
{
    return client() && client()->shouldEndEditing(range);
}

bool Editor::shouldBeginEditing(Range* range)
{
    return client() && client()->shouldBeginEditing(range);
}

void Editor::clearUndoRedoOperations()
{
    if (client())
        client()->clearUndoRedoOperations();
}

bool Editor::canUndo()
{
    return client() && client()->canUndo();
}

void Editor::undo()
{
    if (client())
        client()->undo();
}

bool Editor::canRedo()
{
    return client() && client()->canRedo();
}

void Editor::redo()
{
    if (client())
        client()->redo();
}

void Editor::didBeginEditing()
{
    if (client())
        client()->didBeginEditing();
}

void Editor::didEndEditing()
{
    if (client())
        client()->didEndEditing();
}

void Editor::willWriteSelectionToPasteboard(Range* range)
{
    if (client())
        client()->willWriteSelectionToPasteboard(range);
}

void Editor::didWriteSelectionToPasteboard()
{
    if (client())
        client()->didWriteSelectionToPasteboard();
}

void Editor::toggleBold()
{
    command("ToggleBold").execute();
}

void Editor::toggleUnderline()
{
    command("ToggleUnderline").execute();
}

void Editor::setBaseWritingDirection(WritingDirection direction)
{
#if PLATFORM(IOS)
    if (inSameParagraph(m_frame.selection().selection().visibleStart(), m_frame.selection().selection().visibleEnd()) && 
        baseWritingDirectionForSelectionStart() == direction)
        return;
#endif
        
    Element* focusedElement = document().focusedElement();
    if (is<HTMLTextFormControlElement>(focusedElement)) {
        if (direction == NaturalWritingDirection)
            return;

        auto& focusedFormElement = downcast<HTMLTextFormControlElement>(*focusedElement);
        auto directionValue = direction == LeftToRightWritingDirection ? "ltr" : "rtl";
        auto writingDirectionInputTypeName = inputTypeNameForEditingAction(EditActionSetWritingDirection);
        if (!dispatchBeforeInputEvent(focusedFormElement, writingDirectionInputTypeName, directionValue))
            return;

        focusedFormElement.setAttributeWithoutSynchronization(dirAttr, directionValue);
        dispatchInputEvent(focusedFormElement, writingDirectionInputTypeName, directionValue);
        document().updateStyleIfNeeded();
        return;
    }

    RefPtr<MutableStyleProperties> style = MutableStyleProperties::create();
    style->setProperty(CSSPropertyDirection, direction == LeftToRightWritingDirection ? "ltr" : direction == RightToLeftWritingDirection ? "rtl" : "inherit", false);
    applyParagraphStyleToSelection(style.get(), EditActionSetWritingDirection);
}

WritingDirection Editor::baseWritingDirectionForSelectionStart() const
{
    WritingDirection result = LeftToRightWritingDirection;

    Position pos = m_frame.selection().selection().visibleStart().deepEquivalent();
    Node* node = pos.deprecatedNode();
    if (!node)
        return result;

    auto renderer = node->renderer();
    if (!renderer)
        return result;

    if (!renderer->isRenderBlockFlow()) {
        renderer = renderer->containingBlock();
        if (!renderer)
            return result;
    }

    switch (renderer->style().direction()) {
    case LTR:
        return LeftToRightWritingDirection;
    case RTL:
        return RightToLeftWritingDirection;
    }
    
    return result;
}

void Editor::selectComposition()
{
    RefPtr<Range> range = compositionRange();
    if (!range)
        return;
    
    // The composition can start inside a composed character sequence, so we have to override checks.
    // See <http://bugs.webkit.org/show_bug.cgi?id=15781>
    VisibleSelection selection;
    selection.setWithoutValidation(range->startPosition(), range->endPosition());
    m_frame.selection().setSelection(selection, 0);
}

void Editor::confirmComposition()
{
    if (!m_compositionNode)
        return;
    setComposition(m_compositionNode->data().substring(m_compositionStart, m_compositionEnd - m_compositionStart), ConfirmComposition);
}

void Editor::cancelComposition()
{
    if (!m_compositionNode)
        return;
    setComposition(emptyString(), CancelComposition);
}

bool Editor::cancelCompositionIfSelectionIsInvalid()
{
    unsigned start;
    unsigned end;
    if (!hasComposition() || ignoreSelectionChanges() || getCompositionSelection(start, end))
        return false;

    cancelComposition();
    return true;
}

void Editor::confirmComposition(const String& text)
{
    setComposition(text, ConfirmComposition);
}

class SetCompositionScope {
public:
    SetCompositionScope(Frame& frame)
        : m_frame(frame)
        , m_typingGestureIndicator(frame)
    {
        m_frame->editor().setIgnoreSelectionChanges(true);
    }

    ~SetCompositionScope()
    {
        m_frame->editor().setIgnoreSelectionChanges(false);
        if (auto* editorClient = m_frame->editor().client())
            editorClient->didUpdateComposition();
    }

    Ref<Frame> m_frame;
    UserTypingGestureIndicator m_typingGestureIndicator;
};

void Editor::setComposition(const String& text, SetCompositionMode mode)
{
    ASSERT(mode == ConfirmComposition || mode == CancelComposition);
    SetCompositionScope setCompositionScope(m_frame);

    if (mode == CancelComposition)
        ASSERT(text == emptyString());
    else
        selectComposition();

    m_compositionNode = nullptr;
    m_customCompositionUnderlines.clear();

    if (m_frame.selection().isNone())
        return;

    // Always delete the current composition before inserting the finalized composition text if we're confirming our composition.
    // Our default behavior (if the beforeinput event is not prevented) is to insert the finalized composition text back in.
    // We pass TypingCommand::TextCompositionPending here to indicate that we are deleting the pending composition.
    if (mode != CancelComposition)
        TypingCommand::deleteSelection(document(), 0, TypingCommand::TextCompositionPending);

    insertTextForConfirmedComposition(text);

    if (auto* target = document().focusedElement())
        target->dispatchEvent(CompositionEvent::create(eventNames().compositionendEvent, document().domWindow(), text));

    if (mode == CancelComposition) {
        // An open typing command that disagrees about current selection would cause issues with typing later on.
        TypingCommand::closeTyping(&m_frame);
    }
}

void Editor::setComposition(const String& text, const Vector<CompositionUnderline>& underlines, unsigned selectionStart, unsigned selectionEnd)
{
    SetCompositionScope setCompositionScope(m_frame);

    // Updates styles before setting selection for composition to prevent
    // inserting the previous composition text into text nodes oddly.
    // See https://bugs.webkit.org/show_bug.cgi?id=46868
    document().updateStyleIfNeeded();

    selectComposition();

    if (m_frame.selection().isNone())
        return;

    String originalText = selectedText();
    bool isStartingToRecomposeExistingRange = !text.isEmpty() && selectionStart < selectionEnd && !hasComposition();
    if (isStartingToRecomposeExistingRange) {
        // We pass TypingCommand::TextCompositionFinal here to indicate that we are removing composition text that has been finalized.
        TypingCommand::deleteSelection(document(), 0, TypingCommand::TextCompositionFinal);
        const VisibleSelection& currentSelection = m_frame.selection().selection();
        if (currentSelection.isRange()) {
            // If deletion was prevented, then we need to collapse the selection to the end so that the original text will not be recomposed.
            m_frame.selection().setSelection({ currentSelection.end(), currentSelection.end() });
        }
    }

#if PLATFORM(IOS)
    client()->startDelayingAndCoalescingContentChangeNotifications();
#endif

    Element* target = document().focusedElement();
    if (target) {
        // Dispatch an appropriate composition event to the focused node.
        // We check the composition status and choose an appropriate composition event since this
        // function is used for three purposes:
        // 1. Starting a new composition.
        //    Send a compositionstart and a compositionupdate event when this function creates
        //    a new composition node, i.e.
        //    m_compositionNode == 0 && !text.isEmpty().
        //    Sending a compositionupdate event at this time ensures that at least one
        //    compositionupdate event is dispatched.
        // 2. Updating the existing composition node.
        //    Send a compositionupdate event when this function updates the existing composition
        //    node, i.e. m_compositionNode != 0 && !text.isEmpty().
        // 3. Canceling the ongoing composition.
        //    Send a compositionend event when function deletes the existing composition node, i.e.
        //    m_compositionNode != 0 && test.isEmpty().
        RefPtr<CompositionEvent> event;
        if (!m_compositionNode) {
            // We should send a compositionstart event only when the given text is not empty because this
            // function doesn't create a composition node when the text is empty.
            if (!text.isEmpty()) {
                target->dispatchEvent(CompositionEvent::create(eventNames().compositionstartEvent, document().domWindow(), originalText));
                event = CompositionEvent::create(eventNames().compositionupdateEvent, document().domWindow(), text);
            }
        } else if (!text.isEmpty())
            event = CompositionEvent::create(eventNames().compositionupdateEvent, document().domWindow(), text);

        if (event)
            target->dispatchEvent(*event);
    }

    // If text is empty, then delete the old composition here.  If text is non-empty, InsertTextCommand::input
    // will delete the old composition with an optimized replace operation.
    if (text.isEmpty()) {
        TypingCommand::deleteSelection(document(), TypingCommand::PreventSpellChecking, TypingCommand::TextCompositionPending);
        if (target)
            target->dispatchEvent(CompositionEvent::create(eventNames().compositionendEvent, document().domWindow(), text));
    }

    m_compositionNode = nullptr;
    m_customCompositionUnderlines.clear();

    if (!text.isEmpty()) {
        TypingCommand::insertText(document(), text, TypingCommand::SelectInsertedText | TypingCommand::PreventSpellChecking, TypingCommand::TextCompositionPending);

        // Find out what node has the composition now.
        Position base = m_frame.selection().selection().base().downstream();
        Position extent = m_frame.selection().selection().extent();
        Node* baseNode = base.deprecatedNode();
        unsigned baseOffset = base.deprecatedEditingOffset();
        Node* extentNode = extent.deprecatedNode();
        unsigned extentOffset = extent.deprecatedEditingOffset();

        if (is<Text>(baseNode) && baseNode == extentNode && baseOffset + text.length() == extentOffset) {
            m_compositionNode = downcast<Text>(baseNode);
            m_compositionStart = baseOffset;
            m_compositionEnd = extentOffset;
            m_customCompositionUnderlines = underlines;
            for (auto& underline : m_customCompositionUnderlines) {
                underline.startOffset += baseOffset;
                underline.endOffset += baseOffset;
            }
            if (baseNode->renderer())
                baseNode->renderer()->repaint();

            unsigned start = std::min(baseOffset + selectionStart, extentOffset);
            unsigned end = std::min(std::max(start, baseOffset + selectionEnd), extentOffset);
            RefPtr<Range> selectedRange = Range::create(baseNode->document(), baseNode, start, baseNode, end);
            m_frame.selection().setSelectedRange(selectedRange.get(), DOWNSTREAM, false);
        }
    }

#if PLATFORM(IOS)        
    client()->stopDelayingAndCoalescingContentChangeNotifications();
#endif
}

void Editor::ignoreSpelling()
{
    if (!client())
        return;
        
    RefPtr<Range> selectedRange = m_frame.selection().toNormalizedRange();
    if (selectedRange)
        document().markers().removeMarkers(selectedRange.get(), DocumentMarker::Spelling);

    String text = selectedText();
    ASSERT(text.length());
    textChecker()->ignoreWordInSpellDocument(text);
}

void Editor::learnSpelling()
{
    if (!client())
        return;
        
    // FIXME: On Mac OS X, when use "learn" button on "Spelling and Grammar" panel, we don't call this function. It should remove misspelling markers around the learned word, see <rdar://problem/5396072>.

    RefPtr<Range> selectedRange = m_frame.selection().toNormalizedRange();
    if (selectedRange)
        document().markers().removeMarkers(selectedRange.get(), DocumentMarker::Spelling);

    String text = selectedText();
    ASSERT(text.length());
    textChecker()->learnWord(text);
}

#if !PLATFORM(IOS)

void Editor::advanceToNextMisspelling(bool startBeforeSelection)
{
    Ref<Frame> protection(m_frame);

    // The basic approach is to search in two phases - from the selection end to the end of the doc, and
    // then we wrap and search from the doc start to (approximately) where we started.
    
    // Start at the end of the selection, search to edge of document.  Starting at the selection end makes
    // repeated "check spelling" commands work.
    VisibleSelection selection(m_frame.selection().selection());
    Ref<Range> spellingSearchRange = rangeOfContents(document());

    bool startedWithSelection = false;
    if (selection.start().deprecatedNode()) {
        startedWithSelection = true;
        if (startBeforeSelection) {
            VisiblePosition start(selection.visibleStart());
            // We match AppKit's rule: Start 1 character before the selection.
            VisiblePosition oneBeforeStart = start.previous();
            setStart(spellingSearchRange.ptr(), oneBeforeStart.isNotNull() ? oneBeforeStart : start);
        } else
            setStart(spellingSearchRange.ptr(), selection.visibleEnd());
    }

    Position position = spellingSearchRange->startPosition();
    if (!isEditablePosition(position)) {
        // This shouldn't happen in very often because the Spelling menu items aren't enabled unless the
        // selection is editable.
        // This can happen in Mail for a mix of non-editable and editable content (like Stationary), 
        // when spell checking the whole document before sending the message.
        // In that case the document might not be editable, but there are editable pockets that need to be spell checked.

        position = VisiblePosition(firstEditablePositionAfterPositionInRoot(position, document().documentElement())).deepEquivalent();
        if (position.isNull())
            return;
        
        Position rangeCompliantPosition = position.parentAnchoredEquivalent();
        if (rangeCompliantPosition.deprecatedNode())
            spellingSearchRange->setStart(*rangeCompliantPosition.deprecatedNode(), rangeCompliantPosition.deprecatedEditingOffset());
        startedWithSelection = false; // won't need to wrap
    }
    
    // topNode defines the whole range we want to operate on 
    auto* topNode = highestEditableRoot(position);
    // FIXME: lastOffsetForEditing() is wrong here if editingIgnoresContent(highestEditableRoot()) returns true (e.g. a <table>)
    if (topNode)
        spellingSearchRange->setEnd(*topNode, lastOffsetForEditing(*topNode));

    // If spellingSearchRange starts in the middle of a word, advance to the next word so we start checking
    // at a word boundary. Going back by one char and then forward by a word does the trick.
    if (startedWithSelection) {
        VisiblePosition oneBeforeStart = startVisiblePosition(spellingSearchRange.ptr(), DOWNSTREAM).previous();
        if (oneBeforeStart.isNotNull())
            setStart(spellingSearchRange.ptr(), endOfWord(oneBeforeStart));
        // else we were already at the start of the editable node
    }

    if (spellingSearchRange->collapsed())
        return; // nothing to search in
    
    // Get the spell checker if it is available
    if (!client())
        return;
        
    // We go to the end of our first range instead of the start of it, just to be sure
    // we don't get foiled by any word boundary problems at the start.  It means we might
    // do a tiny bit more searching.
    Node& searchEndNodeAfterWrap = spellingSearchRange->endContainer();
    int searchEndOffsetAfterWrap = spellingSearchRange->endOffset();
    
    int misspellingOffset = 0;
    GrammarDetail grammarDetail;
    int grammarPhraseOffset = 0;
    RefPtr<Range> grammarSearchRange;
    String badGrammarPhrase;
    String misspelledWord;

    bool isSpelling = true;
    int foundOffset = 0;
    String foundItem;
    RefPtr<Range> firstMisspellingRange;
    if (unifiedTextCheckerEnabled()) {
        grammarSearchRange = spellingSearchRange->cloneRange();
        foundItem = TextCheckingHelper(*client(), spellingSearchRange).findFirstMisspellingOrBadGrammar(isGrammarCheckingEnabled(), isSpelling, foundOffset, grammarDetail);
        if (isSpelling) {
            misspelledWord = foundItem;
            misspellingOffset = foundOffset;
        } else {
            badGrammarPhrase = foundItem;
            grammarPhraseOffset = foundOffset;
        }
    } else {
        misspelledWord = TextCheckingHelper(*client(), spellingSearchRange).findFirstMisspelling(misspellingOffset, false, firstMisspellingRange);

#if USE(GRAMMAR_CHECKING)
        grammarSearchRange = spellingSearchRange->cloneRange();
        if (!misspelledWord.isEmpty()) {
            // Stop looking at start of next misspelled word
            CharacterIterator chars(*grammarSearchRange);
            chars.advance(misspellingOffset);
            grammarSearchRange->setEnd(chars.range()->startContainer(), chars.range()->startOffset());
        }
    
        if (isGrammarCheckingEnabled())
            badGrammarPhrase = TextCheckingHelper(*client(), *grammarSearchRange).findFirstBadGrammar(grammarDetail, grammarPhraseOffset, false);
#endif
    }
    
    // If we found neither bad grammar nor a misspelled word, wrap and try again (but don't bother if we started at the beginning of the
    // block rather than at a selection).
    if (startedWithSelection && !misspelledWord && !badGrammarPhrase) {
        if (topNode)
            spellingSearchRange->setStart(*topNode, 0);
        // going until the end of the very first chunk we tested is far enough
        spellingSearchRange->setEnd(searchEndNodeAfterWrap, searchEndOffsetAfterWrap);
        
        if (unifiedTextCheckerEnabled()) {
            grammarSearchRange = spellingSearchRange->cloneRange();
            foundItem = TextCheckingHelper(*client(), spellingSearchRange).findFirstMisspellingOrBadGrammar(isGrammarCheckingEnabled(), isSpelling, foundOffset, grammarDetail);
            if (isSpelling) {
                misspelledWord = foundItem;
                misspellingOffset = foundOffset;
            } else {
                badGrammarPhrase = foundItem;
                grammarPhraseOffset = foundOffset;
            }
        } else {
            misspelledWord = TextCheckingHelper(*client(), spellingSearchRange).findFirstMisspelling(misspellingOffset, false, firstMisspellingRange);

#if USE(GRAMMAR_CHECKING)
            grammarSearchRange = spellingSearchRange->cloneRange();
            if (!misspelledWord.isEmpty()) {
                // Stop looking at start of next misspelled word
                CharacterIterator chars(*grammarSearchRange);
                chars.advance(misspellingOffset);
                grammarSearchRange->setEnd(chars.range()->startContainer(), chars.range()->startOffset());
            }

            if (isGrammarCheckingEnabled())
                badGrammarPhrase = TextCheckingHelper(*client(), *grammarSearchRange).findFirstBadGrammar(grammarDetail, grammarPhraseOffset, false);
#endif
        }
    }
    
#if !USE(GRAMMAR_CHECKING)
    ASSERT(badGrammarPhrase.isEmpty());
    UNUSED_PARAM(grammarPhraseOffset);
#else
    if (!badGrammarPhrase.isEmpty()) {
        // We found bad grammar. Since we only searched for bad grammar up to the first misspelled word, the bad grammar
        // takes precedence and we ignore any potential misspelled word. Select the grammar detail, update the spelling
        // panel, and store a marker so we draw the green squiggle later.
        
        ASSERT(badGrammarPhrase.length() > 0);
        ASSERT(grammarDetail.location != -1 && grammarDetail.length > 0);
        
        // FIXME 4859190: This gets confused with doubled punctuation at the end of a paragraph
        RefPtr<Range> badGrammarRange = TextIterator::subrange(*grammarSearchRange, grammarPhraseOffset + grammarDetail.location, grammarDetail.length);
        m_frame.selection().setSelection(VisibleSelection(*badGrammarRange, SEL_DEFAULT_AFFINITY));
        m_frame.selection().revealSelection();
        
        client()->updateSpellingUIWithGrammarString(badGrammarPhrase, grammarDetail);
        document().markers().addMarker(badGrammarRange.get(), DocumentMarker::Grammar, grammarDetail.userDescription);
    } else
#endif
    if (!misspelledWord.isEmpty()) {
        // We found a misspelling, but not any earlier bad grammar. Select the misspelling, update the spelling panel, and store
        // a marker so we draw the red squiggle later.
        
        auto misspellingRange = TextIterator::subrange(spellingSearchRange, misspellingOffset, misspelledWord.length());
        m_frame.selection().setSelection(VisibleSelection(misspellingRange, DOWNSTREAM));
        m_frame.selection().revealSelection();
        
        client()->updateSpellingUIWithMisspelledWord(misspelledWord);
        document().markers().addMarker(misspellingRange.ptr(), DocumentMarker::Spelling);
    }
}

#endif // !PLATFORM(IOS)

String Editor::misspelledWordAtCaretOrRange(Node* clickedNode) const
{
    if (!isContinuousSpellCheckingEnabled() || !clickedNode || !isSpellCheckingEnabledFor(clickedNode))
        return String();

    VisibleSelection selection = m_frame.selection().selection();
    if (!selection.isContentEditable() || selection.isNone())
        return String();

    VisibleSelection wordSelection(selection.base());
    wordSelection.expandUsingGranularity(WordGranularity);
    RefPtr<Range> wordRange = wordSelection.toNormalizedRange();

    // In compliance with GTK+ applications, additionally allow to provide suggestions when the current
    // selection exactly match the word selection.
    if (selection.isRange() && !areRangesEqual(wordRange.get(), selection.toNormalizedRange().get()))
        return String();

    String word = wordRange->text();
    if (word.isEmpty() || !client())
        return String();

    int wordLength = word.length();
    int misspellingLocation = -1;
    int misspellingLength = 0;
    textChecker()->checkSpellingOfString(word, &misspellingLocation, &misspellingLength);

    return misspellingLength == wordLength ? word : String();
}

String Editor::misspelledSelectionString() const
{
    String selectedString = selectedText();
    int length = selectedString.length();
    if (!length || !client())
        return String();

    int misspellingLocation = -1;
    int misspellingLength = 0;
    textChecker()->checkSpellingOfString(selectedString, &misspellingLocation, &misspellingLength);
    
    // The selection only counts as misspelled if the selected text is exactly one misspelled word
    if (misspellingLength != length)
        return String();
    
    // Update the spelling panel to be displaying this error (whether or not the spelling panel is on screen).
    // This is necessary to make a subsequent call to [NSSpellChecker ignoreWord:inSpellDocumentWithTag:] work
    // correctly; that call behaves differently based on whether the spelling panel is displaying a misspelling
    // or a grammar error.
    client()->updateSpellingUIWithMisspelledWord(selectedString);
    
    return selectedString;
}

bool Editor::isSelectionUngrammatical()
{
#if USE(GRAMMAR_CHECKING)
    RefPtr<Range> range = m_frame.selection().toNormalizedRange();
    if (!range || !client())
        return false;
    return TextCheckingHelper(*client(), *range).isUngrammatical();
#else
    return false;
#endif
}

Vector<String> Editor::guessesForMisspelledWord(const String& word) const
{
    ASSERT(word.length());

    Vector<String> guesses;
    if (client())
        textChecker()->getGuessesForWord(word, String(), m_frame.selection().selection(), guesses);
    return guesses;
}

Vector<String> Editor::guessesForMisspelledOrUngrammatical(bool& misspelled, bool& ungrammatical)
{
    if (unifiedTextCheckerEnabled()) {
        RefPtr<Range> range;
        VisibleSelection selection = m_frame.selection().selection();
        if (selection.isCaret() && behavior().shouldAllowSpellingSuggestionsWithoutSelection()) {
            VisibleSelection wordSelection = VisibleSelection(selection.base());
            wordSelection.expandUsingGranularity(WordGranularity);
            range = wordSelection.toNormalizedRange();
        } else
            range = selection.toNormalizedRange();
        if (!range || !client())
            return Vector<String>();
        return TextCheckingHelper(*client(), *range).guessesForMisspelledOrUngrammaticalRange(isGrammarCheckingEnabled(), misspelled, ungrammatical);
    }

    String misspelledWord = behavior().shouldAllowSpellingSuggestionsWithoutSelection() ? misspelledWordAtCaretOrRange(document().focusedElement()) : misspelledSelectionString();
    misspelled = !misspelledWord.isEmpty();
    // Only unified text checker supports guesses for ungrammatical phrases.
    ungrammatical = false;

    if (misspelled)
        return guessesForMisspelledWord(misspelledWord);
    return Vector<String>();
}

void Editor::showSpellingGuessPanel()
{
    if (!client()) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }

    if (client()->spellingUIIsShowing()) {
        client()->showSpellingUI(false);
        return;
    }

#if !PLATFORM(IOS)
    advanceToNextMisspelling(true);
#endif
    client()->showSpellingUI(true);
}

bool Editor::spellingPanelIsShowing()
{
    if (!client())
        return false;
    return client()->spellingUIIsShowing();
}

void Editor::clearMisspellingsAndBadGrammar(const VisibleSelection &movingSelection)
{
    RefPtr<Range> selectedRange = movingSelection.toNormalizedRange();
    if (selectedRange) {
        document().markers().removeMarkers(selectedRange.get(), DocumentMarker::Spelling);
        document().markers().removeMarkers(selectedRange.get(), DocumentMarker::Grammar);
    }
}

void Editor::markMisspellingsAndBadGrammar(const VisibleSelection &movingSelection)
{
    markMisspellingsAndBadGrammar(movingSelection, isContinuousSpellCheckingEnabled() && isGrammarCheckingEnabled(), movingSelection);
}

void Editor::markMisspellingsAfterTypingToWord(const VisiblePosition &wordStart, const VisibleSelection& selectionAfterTyping, bool doReplacement)
{
    Ref<Frame> protection(m_frame);

#if PLATFORM(IOS)
    UNUSED_PARAM(selectionAfterTyping);
    UNUSED_PARAM(doReplacement);
    TextCheckingTypeMask textCheckingOptions = 0;
    if (isContinuousSpellCheckingEnabled())
        textCheckingOptions |= TextCheckingTypeSpelling;
    if (!(textCheckingOptions & TextCheckingTypeSpelling))
        return;

    VisibleSelection adjacentWords = VisibleSelection(startOfWord(wordStart, LeftWordIfOnBoundary), endOfWord(wordStart, RightWordIfOnBoundary));
    markAllMisspellingsAndBadGrammarInRanges(textCheckingOptions, adjacentWords.toNormalizedRange().get(), adjacentWords.toNormalizedRange().get());
#else
#if !USE(AUTOMATIC_TEXT_REPLACEMENT)
    UNUSED_PARAM(doReplacement);
#endif

    if (unifiedTextCheckerEnabled()) {
        m_alternativeTextController->applyPendingCorrection(selectionAfterTyping);

        TextCheckingTypeMask textCheckingOptions = 0;

        if (isContinuousSpellCheckingEnabled())
            textCheckingOptions |= TextCheckingTypeSpelling;

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
        if (doReplacement
            && (isAutomaticQuoteSubstitutionEnabled()
                || isAutomaticLinkDetectionEnabled()
                || isAutomaticDashSubstitutionEnabled()
                || isAutomaticTextReplacementEnabled()
                || ((textCheckingOptions & TextCheckingTypeSpelling) && isAutomaticSpellingCorrectionEnabled())))
            textCheckingOptions |= TextCheckingTypeReplacement;
#endif
        if (!(textCheckingOptions & (TextCheckingTypeSpelling | TextCheckingTypeReplacement)))
            return;

        if (isGrammarCheckingEnabled())
            textCheckingOptions |= TextCheckingTypeGrammar;

        VisibleSelection adjacentWords = VisibleSelection(startOfWord(wordStart, LeftWordIfOnBoundary), endOfWord(wordStart, RightWordIfOnBoundary));
        if (textCheckingOptions & TextCheckingTypeGrammar) {
            VisibleSelection selectedSentence = VisibleSelection(startOfSentence(wordStart), endOfSentence(wordStart));
            markAllMisspellingsAndBadGrammarInRanges(textCheckingOptions, adjacentWords.toNormalizedRange().get(), selectedSentence.toNormalizedRange().get());
        } else
            markAllMisspellingsAndBadGrammarInRanges(textCheckingOptions, adjacentWords.toNormalizedRange().get(), adjacentWords.toNormalizedRange().get());
        return;
    }

    if (!isContinuousSpellCheckingEnabled())
        return;

    // Check spelling of one word
    RefPtr<Range> misspellingRange;
    markMisspellings(VisibleSelection(startOfWord(wordStart, LeftWordIfOnBoundary), endOfWord(wordStart, RightWordIfOnBoundary)), misspellingRange);

    // Autocorrect the misspelled word.
    if (!misspellingRange)
        return;
    
    // Get the misspelled word.
    const String misspelledWord = plainText(misspellingRange.get());
    String autocorrectedString = textChecker()->getAutoCorrectSuggestionForMisspelledWord(misspelledWord);

    // If autocorrected word is non empty, replace the misspelled word by this word.
    if (!autocorrectedString.isEmpty()) {
        VisibleSelection newSelection(*misspellingRange, DOWNSTREAM);
        if (newSelection != m_frame.selection().selection()) {
            if (!m_frame.selection().shouldChangeSelection(newSelection))
                return;
            m_frame.selection().setSelection(newSelection);
        }

        if (!m_frame.editor().shouldInsertText(autocorrectedString, misspellingRange.get(), EditorInsertAction::Typed))
            return;
        m_frame.editor().replaceSelectionWithText(autocorrectedString, false, false, EditActionInsert);

        // Reset the charet one character further.
        m_frame.selection().moveTo(m_frame.selection().selection().end());
        m_frame.selection().modify(FrameSelection::AlterationMove, DirectionForward, CharacterGranularity);
    }

    if (!isGrammarCheckingEnabled())
        return;
    
    // Check grammar of entire sentence
    markBadGrammar(VisibleSelection(startOfSentence(wordStart), endOfSentence(wordStart)));
#endif
}
    
void Editor::markMisspellingsOrBadGrammar(const VisibleSelection& selection, bool checkSpelling, RefPtr<Range>& firstMisspellingRange)
{
#if !PLATFORM(IOS)
    // This function is called with a selection already expanded to word boundaries.
    // Might be nice to assert that here.
    
    // This function is used only for as-you-type checking, so if that's off we do nothing. Note that
    // grammar checking can only be on if spell checking is also on.
    if (!isContinuousSpellCheckingEnabled())
        return;
    
    RefPtr<Range> searchRange(selection.toNormalizedRange());
    if (!searchRange)
        return;
    
    // If we're not in an editable node, bail.
    Node& editableNode = searchRange->startContainer();
    if (!editableNode.hasEditableStyle())
        return;

    if (!isSpellCheckingEnabledFor(&editableNode))
        return;

    // Get the spell checker if it is available
    if (!client())
        return;
    
    TextCheckingHelper checker(*client(), *searchRange);
    if (checkSpelling)
        checker.markAllMisspellings(firstMisspellingRange);
    else {
#if USE(GRAMMAR_CHECKING)
        if (isGrammarCheckingEnabled())
            checker.markAllBadGrammar();
#else
        ASSERT_NOT_REACHED();
#endif
    }    
#else
        UNUSED_PARAM(selection);
        UNUSED_PARAM(checkSpelling);
        UNUSED_PARAM(firstMisspellingRange);
#endif // !PLATFORM(IOS)
}

bool Editor::isSpellCheckingEnabledFor(Node* node) const
{
    if (!node)
        return false;
    Element* element = is<Element>(*node) ? downcast<Element>(node) : node->parentElement();
    if (!element)
        return false;
    if (element->isInUserAgentShadowTree()) {
        if (HTMLTextFormControlElement* textControl = enclosingTextFormControl(firstPositionInOrBeforeNode(element)))
            return textControl->isSpellCheckingEnabled();
    }
    return element->isSpellCheckingEnabled();
}

bool Editor::isSpellCheckingEnabledInFocusedNode() const
{
    return isSpellCheckingEnabledFor(m_frame.selection().selection().start().deprecatedNode());
}

void Editor::markMisspellings(const VisibleSelection& selection, RefPtr<Range>& firstMisspellingRange)
{
    markMisspellingsOrBadGrammar(selection, true, firstMisspellingRange);
}
    
void Editor::markBadGrammar(const VisibleSelection& selection)
{
#if USE(GRAMMAR_CHECKING)
    RefPtr<Range> firstMisspellingRange;
    markMisspellingsOrBadGrammar(selection, false, firstMisspellingRange);
#else
    ASSERT_NOT_REACHED();
#endif
}

void Editor::markAllMisspellingsAndBadGrammarInRanges(TextCheckingTypeMask textCheckingOptions, Range* spellingRange, Range* grammarRange)
{
    ASSERT(unifiedTextCheckerEnabled());

    // There shouldn't be pending autocorrection at this moment.
    ASSERT(!m_alternativeTextController->hasPendingCorrection());

    bool shouldMarkGrammar = textCheckingOptions & TextCheckingTypeGrammar;
    bool shouldShowCorrectionPanel = textCheckingOptions & TextCheckingTypeShowCorrectionPanel;

    // This function is called with selections already expanded to word boundaries.
    if (!client() || !spellingRange || (shouldMarkGrammar && !grammarRange))
        return;

    // If we're not in an editable node, bail.
    Node& editableNode = spellingRange->startContainer();
    if (!editableNode.hasEditableStyle())
        return;

    if (!isSpellCheckingEnabledFor(&editableNode))
        return;

    Range& rangeToCheck = shouldMarkGrammar ? *grammarRange : *spellingRange;
    TextCheckingParagraph paragraphToCheck(rangeToCheck);
    if (paragraphToCheck.isEmpty())
        return;
    Ref<Range> paragraphRange = paragraphToCheck.paragraphRange();

    bool asynchronous = m_frame.settings().asynchronousSpellCheckingEnabled() && !shouldShowCorrectionPanel;

    // In asynchronous mode, we intentionally check paragraph-wide sentence.
    const auto resolvedOptions = resolveTextCheckingTypeMask(editableNode, textCheckingOptions);
    auto request = SpellCheckRequest::create(resolvedOptions, TextCheckingProcessIncremental, asynchronous ? paragraphRange.get() : rangeToCheck, paragraphRange.copyRef());
    if (!request)
        return;

    if (asynchronous) {
        m_spellChecker->requestCheckingFor(request.releaseNonNull());
        return;
    }

    Vector<TextCheckingResult> results;
    checkTextOfParagraph(*textChecker(), paragraphToCheck.text(), resolvedOptions, results, m_frame.selection().selection());
    markAndReplaceFor(request.releaseNonNull(), results);
}

static bool isAutomaticTextReplacementType(TextCheckingType type)
{
    switch (type) {
    case TextCheckingTypeNone:
    case TextCheckingTypeSpelling:
    case TextCheckingTypeGrammar:
        return false;
    case TextCheckingTypeLink:
    case TextCheckingTypeQuote:
    case TextCheckingTypeDash:
    case TextCheckingTypeReplacement:
    case TextCheckingTypeCorrection:
    case TextCheckingTypeShowCorrectionPanel:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static void correctSpellcheckingPreservingTextCheckingParagraph(TextCheckingParagraph& paragraph, Range& rangeToReplace, const String& replacement, int resultLocation, int resultLength)
{
    auto& scope = downcast<ContainerNode>(paragraph.paragraphRange().startContainer().rootNode());

    size_t paragraphLocation;
    size_t paragraphLength;
    TextIterator::getLocationAndLengthFromRange(&scope, &paragraph.paragraphRange(), paragraphLocation, paragraphLength);

    SpellingCorrectionCommand::create(rangeToReplace, replacement)->apply();

    // TextCheckingParagraph may be orphaned after SpellingCorrectionCommand mutated DOM.
    // See <rdar://10305315>, http://webkit.org/b/89526.

    RefPtr<Range> newParagraphRange = TextIterator::rangeFromLocationAndLength(&scope, paragraphLocation, paragraphLength + replacement.length() - resultLength);

    paragraph = TextCheckingParagraph(TextIterator::subrange(*newParagraphRange, resultLocation, replacement.length()), newParagraphRange.get());
}

void Editor::markAndReplaceFor(const SpellCheckRequest& request, const Vector<TextCheckingResult>& results)
{
    Ref<Frame> protection(m_frame);

    TextCheckingTypeMask textCheckingOptions = request.data().mask();
    TextCheckingParagraph paragraph(request.checkingRange(), &request.paragraphRange());

    const bool shouldMarkSpelling = textCheckingOptions & TextCheckingTypeSpelling;
    const bool shouldMarkGrammar = textCheckingOptions & TextCheckingTypeGrammar;
    const bool shouldMarkLink = textCheckingOptions & TextCheckingTypeLink;
    const bool shouldPerformReplacement = textCheckingOptions & (TextCheckingTypeQuote | TextCheckingTypeDash | TextCheckingTypeReplacement);
    const bool shouldShowCorrectionPanel = textCheckingOptions & TextCheckingTypeShowCorrectionPanel;
    const bool shouldCheckForCorrection = shouldShowCorrectionPanel || (textCheckingOptions & TextCheckingTypeCorrection);
#if !USE(AUTOCORRECTION_PANEL)
    ASSERT(!shouldShowCorrectionPanel);
#endif

    // Expand the range to encompass entire paragraphs, since text checking needs that much context.
    int selectionOffset = 0;
    bool useAmbiguousBoundaryOffset = false;
    bool selectionChanged = false;
    bool restoreSelectionAfterChange = false;
    bool adjustSelectionForParagraphBoundaries = false;

    if (shouldPerformReplacement || shouldMarkSpelling || shouldCheckForCorrection) {
        if (m_frame.selection().selection().selectionType() == VisibleSelection::CaretSelection) {
            // Attempt to save the caret position so we can restore it later if needed
            Position caretPosition = m_frame.selection().selection().end();
            selectionOffset = paragraph.offsetTo(caretPosition).releaseReturnValue();
            restoreSelectionAfterChange = true;
            if (selectionOffset > 0 && (selectionOffset > paragraph.textLength() || paragraph.textCharAt(selectionOffset - 1) == newlineCharacter))
                adjustSelectionForParagraphBoundaries = true;
            if (selectionOffset > 0 && selectionOffset <= paragraph.textLength() && isAmbiguousBoundaryCharacter(paragraph.textCharAt(selectionOffset - 1)))
                useAmbiguousBoundaryOffset = true;
        }
    }

    int offsetDueToReplacement = 0;

    for (unsigned i = 0; i < results.size(); i++) {
        const int spellingRangeEndOffset = paragraph.checkingEnd() + offsetDueToReplacement;
        const TextCheckingType resultType = results[i].type;
        const int resultLocation = results[i].location + offsetDueToReplacement;
        const int resultLength = results[i].length;
        const int resultEndLocation = resultLocation + resultLength;
        const String& replacement = results[i].replacement;
        const bool resultEndsAtAmbiguousBoundary = useAmbiguousBoundaryOffset && selectionOffset - 1 <= resultEndLocation;

        // Only mark misspelling if:
        // 1. Current text checking isn't done for autocorrection, in which case shouldMarkSpelling is false.
        // 2. Result falls within spellingRange.
        // 3. The word in question doesn't end at an ambiguous boundary. For instance, we would not mark
        //    "wouldn'" as misspelled right after apostrophe is typed.
        if (shouldMarkSpelling && !shouldShowCorrectionPanel && resultType == TextCheckingTypeSpelling
            && resultLocation >= paragraph.checkingStart() && resultEndLocation <= spellingRangeEndOffset && !resultEndsAtAmbiguousBoundary) {
            ASSERT(resultLength > 0 && resultLocation >= 0);
            auto misspellingRange = paragraph.subrange(resultLocation, resultLength);
            if (!m_alternativeTextController->isSpellingMarkerAllowed(misspellingRange))
                continue;
            misspellingRange->startContainer().document().markers().addMarker(misspellingRange.ptr(), DocumentMarker::Spelling, replacement);
        } else if (shouldMarkGrammar && resultType == TextCheckingTypeGrammar && paragraph.checkingRangeCovers(resultLocation, resultLength)) {
            ASSERT(resultLength > 0 && resultLocation >= 0);
            for (auto& detail : results[i].details) {
                ASSERT(detail.length > 0 && detail.location >= 0);
                if (paragraph.checkingRangeCovers(resultLocation + detail.location, detail.length)) {
                    auto badGrammarRange = paragraph.subrange(resultLocation + detail.location, detail.length);
                    badGrammarRange->startContainer().document().markers().addMarker(badGrammarRange.ptr(), DocumentMarker::Grammar, detail.userDescription);
                }
            }
        } else if (resultEndLocation <= spellingRangeEndOffset && resultEndLocation >= paragraph.checkingStart()
            && isAutomaticTextReplacementType(resultType)) {
            // In this case the result range just has to touch the spelling range, so we can handle replacing non-word text such as punctuation.
            ASSERT(resultLength > 0 && resultLocation >= 0);

            if (shouldShowCorrectionPanel && (resultEndLocation < spellingRangeEndOffset
                || !(resultType & (TextCheckingTypeReplacement | TextCheckingTypeCorrection))))
                continue;

            // Apply replacement if:
            // 1. The replacement length is non-zero.
            // 2. The result doesn't end at an ambiguous boundary.
            //    (FIXME: this is required until 6853027 is fixed and text checking can do this for us
            bool doReplacement = replacement.length() > 0 && !resultEndsAtAmbiguousBoundary;
            auto rangeToReplace = paragraph.subrange(resultLocation, resultLength);

            // adding links should be done only immediately after they are typed
            if (resultType == TextCheckingTypeLink && selectionOffset != resultEndLocation + 1)
                continue;

            if (!(shouldPerformReplacement || shouldCheckForCorrection || shouldMarkLink) || !doReplacement)
                continue;

            String replacedString = plainText(rangeToReplace.ptr());
            const bool existingMarkersPermitReplacement = m_alternativeTextController->processMarkersOnTextToBeReplacedByResult(results[i], rangeToReplace, replacedString);
            if (!existingMarkersPermitReplacement)
                continue;

            if (shouldShowCorrectionPanel) {
                if (resultEndLocation == spellingRangeEndOffset) {
                    // We only show the correction panel on the last word.
                    m_alternativeTextController->show(rangeToReplace, replacement);
                    break;
                }
                // If this function is called for showing correction panel, we ignore other correction or replacement.
                continue;
            }

            VisibleSelection selectionToReplace(rangeToReplace, DOWNSTREAM);
            if (selectionToReplace != m_frame.selection().selection()) {
                if (!m_frame.selection().shouldChangeSelection(selectionToReplace))
                    continue;
            }

            if (resultType == TextCheckingTypeLink) {
                m_frame.selection().setSelection(selectionToReplace);
                selectionChanged = true;
                restoreSelectionAfterChange = false;
                if (canEditRichly())
                    CreateLinkCommand::create(document(), replacement)->apply();
            } else if (canEdit() && shouldInsertText(replacement, rangeToReplace.ptr(), EditorInsertAction::Typed)) {
                correctSpellcheckingPreservingTextCheckingParagraph(paragraph, rangeToReplace, replacement, resultLocation, resultLength);

                if (AXObjectCache* cache = document().existingAXObjectCache()) {
                    if (Element* root = m_frame.selection().selection().rootEditableElement())
                        cache->postNotification(root, AXObjectCache::AXAutocorrectionOccured);
                }

                // Skip all other results for the replaced text.
                while (i + 1 < results.size() && results[i + 1].location + offsetDueToReplacement <= resultLocation)
                    i++;

                selectionChanged = true;
                offsetDueToReplacement += replacement.length() - resultLength;
                if (resultLocation < selectionOffset)
                    selectionOffset += replacement.length() - resultLength;

                if (resultType == TextCheckingTypeCorrection) {
                    auto replacementRange = paragraph.subrange(resultLocation, replacement.length());
                    m_alternativeTextController->recordAutocorrectionResponse(AutocorrectionResponse::Accepted, replacedString, replacementRange.ptr());

                    // Add a marker so that corrections can easily be undone and won't be re-corrected.
                    m_alternativeTextController->markCorrection(replacementRange, replacedString);
                }
            }
        }
    }

    if (selectionChanged) {
        TextCheckingParagraph extendedParagraph(WTFMove(paragraph));
        // Restore the caret position if we have made any replacements
        extendedParagraph.expandRangeToNextEnd();
        if (restoreSelectionAfterChange && selectionOffset >= 0 && selectionOffset <= extendedParagraph.rangeLength()) {
            auto selectionRange = extendedParagraph.subrange(0, selectionOffset);
            m_frame.selection().moveTo(selectionRange->endPosition(), DOWNSTREAM);
            if (adjustSelectionForParagraphBoundaries)
                m_frame.selection().modify(FrameSelection::AlterationMove, DirectionForward, CharacterGranularity);
        } else {
            // If this fails for any reason, the fallback is to go one position beyond the last replacement
            m_frame.selection().moveTo(m_frame.selection().selection().end());
            m_frame.selection().modify(FrameSelection::AlterationMove, DirectionForward, CharacterGranularity);
        }
    }
}

void Editor::changeBackToReplacedString(const String& replacedString)
{
#if !PLATFORM(IOS)
    ASSERT(unifiedTextCheckerEnabled());

    if (replacedString.isEmpty())
        return;

    RefPtr<Range> selection = selectedRange();
    if (!shouldInsertText(replacedString, selection.get(), EditorInsertAction::Pasted))
        return;
    
    m_alternativeTextController->recordAutocorrectionResponse(AutocorrectionResponse::Reverted, replacedString, selection.get());
    TextCheckingParagraph paragraph(*selection);
    replaceSelectionWithText(replacedString, false, false, EditActionInsert);
    auto changedRange = paragraph.subrange(paragraph.checkingStart(), replacedString.length());
    changedRange->startContainer().document().markers().addMarker(changedRange.ptr(), DocumentMarker::Replacement, String());
    m_alternativeTextController->markReversed(changedRange);
#else
    ASSERT_NOT_REACHED();
    UNUSED_PARAM(replacedString);
#endif // !PLATFORM(IOS)
}


void Editor::markMisspellingsAndBadGrammar(const VisibleSelection& spellingSelection, bool markGrammar, const VisibleSelection& grammarSelection)
{
    if (unifiedTextCheckerEnabled()) {
        if (!isContinuousSpellCheckingEnabled())
            return;

        // markMisspellingsAndBadGrammar() is triggered by selection change, in which case we check spelling and grammar, but don't autocorrect misspellings.
        TextCheckingTypeMask textCheckingOptions = TextCheckingTypeSpelling;
        if (markGrammar && isGrammarCheckingEnabled())
            textCheckingOptions |= TextCheckingTypeGrammar;
        markAllMisspellingsAndBadGrammarInRanges(textCheckingOptions, spellingSelection.toNormalizedRange().get(), grammarSelection.toNormalizedRange().get());
        return;
    }

    RefPtr<Range> firstMisspellingRange;
    markMisspellings(spellingSelection, firstMisspellingRange);
    if (markGrammar)
        markBadGrammar(grammarSelection);
}

void Editor::unappliedSpellCorrection(const VisibleSelection& selectionOfCorrected, const String& corrected, const String& correction)
{
    m_alternativeTextController->respondToUnappliedSpellCorrection(selectionOfCorrected, corrected, correction);
}

void Editor::updateMarkersForWordsAffectedByEditing(bool doNotRemoveIfSelectionAtWordBoundary)
{
    if (!document().markers().hasMarkers())
        return;

    if (!m_alternativeTextController->shouldRemoveMarkersUponEditing() && (!textChecker() || textChecker()->shouldEraseMarkersAfterChangeSelection(TextCheckingTypeSpelling)))
        return;

    // We want to remove the markers from a word if an editing command will change the word. This can happen in one of
    // several scenarios:
    // 1. Insert in the middle of a word.
    // 2. Appending non whitespace at the beginning of word.
    // 3. Appending non whitespace at the end of word.
    // Note that, appending only whitespaces at the beginning or end of word won't change the word, so we don't need to
    // remove the markers on that word.
    // Of course, if current selection is a range, we potentially will edit two words that fall on the boundaries of
    // selection, and remove words between the selection boundaries.
    //
    VisiblePosition startOfSelection = m_frame.selection().selection().start();
    VisiblePosition endOfSelection = m_frame.selection().selection().end();
    if (startOfSelection.isNull())
        return;
    // First word is the word that ends after or on the start of selection.
    VisiblePosition startOfFirstWord = startOfWord(startOfSelection, LeftWordIfOnBoundary);
    VisiblePosition endOfFirstWord = endOfWord(startOfSelection, LeftWordIfOnBoundary);
    // Last word is the word that begins before or on the end of selection
    VisiblePosition startOfLastWord = startOfWord(endOfSelection, RightWordIfOnBoundary);
    VisiblePosition endOfLastWord = endOfWord(endOfSelection, RightWordIfOnBoundary);

    if (startOfFirstWord.isNull()) {
        startOfFirstWord = startOfWord(startOfSelection, RightWordIfOnBoundary);
        endOfFirstWord = endOfWord(startOfSelection, RightWordIfOnBoundary);
    }
    
    if (endOfLastWord.isNull()) {
        startOfLastWord = startOfWord(endOfSelection, LeftWordIfOnBoundary);
        endOfLastWord = endOfWord(endOfSelection, LeftWordIfOnBoundary);
    }

    // If doNotRemoveIfSelectionAtWordBoundary is true, and first word ends at the start of selection,
    // we choose next word as the first word.
    if (doNotRemoveIfSelectionAtWordBoundary && endOfFirstWord == startOfSelection) {
        startOfFirstWord = nextWordPosition(startOfFirstWord);
        endOfFirstWord = endOfWord(startOfFirstWord, RightWordIfOnBoundary);
        if (startOfFirstWord == endOfSelection)
            return;
    }

    // If doNotRemoveIfSelectionAtWordBoundary is true, and last word begins at the end of selection,
    // we choose previous word as the last word.
    if (doNotRemoveIfSelectionAtWordBoundary && startOfLastWord == endOfSelection) {
        startOfLastWord = previousWordPosition(startOfLastWord);
        endOfLastWord = endOfWord(startOfLastWord, RightWordIfOnBoundary);
        if (endOfLastWord == startOfSelection)
            return;
    }

    if (startOfFirstWord.isNull() || endOfFirstWord.isNull() || startOfLastWord.isNull() || endOfLastWord.isNull())
        return;

    // Now we remove markers on everything between startOfFirstWord and endOfLastWord.
    // However, if an autocorrection change a single word to multiple words, we want to remove correction mark from all the
    // resulted words even we only edit one of them. For example, assuming autocorrection changes "avantgarde" to "avant
    // garde", we will have CorrectionIndicator marker on both words and on the whitespace between them. If we then edit garde,
    // we would like to remove the marker from word "avant" and whitespace as well. So we need to get the continous range of
    // of marker that contains the word in question, and remove marker on that whole range.
    auto wordRange = Range::create(document(), startOfFirstWord.deepEquivalent(), endOfLastWord.deepEquivalent());

    Vector<RenderedDocumentMarker*> markers = document().markers().markersInRange(wordRange, DocumentMarker::DictationAlternatives);
    for (auto* marker : markers)
        m_alternativeTextController->removeDictationAlternativesForMarker(*marker);

    OptionSet<DocumentMarker::MarkerType> markerTypesToRemove {
        DocumentMarker::CorrectionIndicator,
        DocumentMarker::DictationAlternatives,
        DocumentMarker::SpellCheckingExemption,
        DocumentMarker::Spelling,
#if !PLATFORM(IOS)
        DocumentMarker::Grammar,
#endif
    };
    document().markers().removeMarkers(wordRange.ptr(), markerTypesToRemove, DocumentMarkerController::RemovePartiallyOverlappingMarker);
    document().markers().clearDescriptionOnMarkersIntersectingRange(wordRange, DocumentMarker::Replacement);
}

void Editor::deletedAutocorrectionAtPosition(const Position& position, const String& originalString)
{
    m_alternativeTextController->deletedAutocorrectionAtPosition(position, originalString);
}

RefPtr<Range> Editor::rangeForPoint(const IntPoint& windowPoint)
{
    Document* document = m_frame.documentAtPoint(windowPoint);
    if (!document)
        return nullptr;
    
    Frame* frame = document->frame();
    ASSERT(frame);
    FrameView* frameView = frame->view();
    if (!frameView)
        return nullptr;
    IntPoint framePoint = frameView->windowToContents(windowPoint);
    VisibleSelection selection(frame->visiblePositionForPoint(framePoint));

    return selection.toNormalizedRange();
}

void Editor::revealSelectionAfterEditingOperation(const ScrollAlignment& alignment, RevealExtentOption revealExtentOption)
{
    if (m_ignoreSelectionChanges)
        return;

#if PLATFORM(IOS)
    SelectionRevealMode revealMode = SelectionRevealMode::RevealUpToMainFrame;
#else
    SelectionRevealMode revealMode = SelectionRevealMode::Reveal;
#endif

    m_frame.selection().revealSelection(revealMode, alignment, revealExtentOption);
}

void Editor::setIgnoreSelectionChanges(bool ignore, RevealSelection shouldRevealExistingSelection)
{
    if (m_ignoreSelectionChanges == ignore)
        return;

    m_ignoreSelectionChanges = ignore;
#if PLATFORM(IOS)
    // FIXME: Should suppress selection change notifications during a composition change <https://webkit.org/b/38830> 
    if (!ignore)
        respondToChangedSelection(m_frame.selection().selection(), 0);
#endif
    if (!ignore && shouldRevealExistingSelection == RevealSelection::Yes)
        revealSelectionAfterEditingOperation(ScrollAlignment::alignToEdgeIfNeeded, RevealExtent);
}

RefPtr<Range> Editor::compositionRange() const
{
    if (!m_compositionNode)
        return nullptr;
    unsigned length = m_compositionNode->length();
    unsigned start = std::min(m_compositionStart, length);
    unsigned end = std::min(std::max(start, m_compositionEnd), length);
    if (start >= end)
        return nullptr;
    return Range::create(m_compositionNode->document(), m_compositionNode.get(), start, m_compositionNode.get(), end);
}

bool Editor::getCompositionSelection(unsigned& selectionStart, unsigned& selectionEnd) const
{
    if (!m_compositionNode)
        return false;
    const VisibleSelection& selection = m_frame.selection().selection();
    Position start = selection.start();
    if (start.deprecatedNode() != m_compositionNode)
        return false;
    Position end = selection.end();
    if (end.deprecatedNode() != m_compositionNode)
        return false;

    if (static_cast<unsigned>(start.deprecatedEditingOffset()) < m_compositionStart)
        return false;
    if (static_cast<unsigned>(end.deprecatedEditingOffset()) > m_compositionEnd)
        return false;

    selectionStart = start.deprecatedEditingOffset() - m_compositionStart;
    selectionEnd = start.deprecatedEditingOffset() - m_compositionEnd;
    return true;
}

void Editor::transpose()
{
    if (!canEdit())
        return;

    VisibleSelection selection = m_frame.selection().selection();
    if (!selection.isCaret())
        return;

    // Make a selection that goes back one character and forward two characters.
    VisiblePosition caret = selection.visibleStart();
    VisiblePosition next = isEndOfParagraph(caret) ? caret : caret.next();
    VisiblePosition previous = next.previous();
    if (next == previous)
        return;
    previous = previous.previous();
    if (!inSameParagraph(next, previous))
        return;
    RefPtr<Range> range = makeRange(previous, next);
    if (!range)
        return;
    VisibleSelection newSelection(*range, DOWNSTREAM);

    // Transpose the two characters.
    String text = plainText(range.get());
    if (text.length() != 2)
        return;
    String transposed = text.right(1) + text.left(1);

    // Select the two characters.
    if (newSelection != m_frame.selection().selection()) {
        if (!m_frame.selection().shouldChangeSelection(newSelection))
            return;
        m_frame.selection().setSelection(newSelection);
    }

    // Insert the transposed characters.
    if (!shouldInsertText(transposed, range.get(), EditorInsertAction::Typed))
        return;
    replaceSelectionWithText(transposed, false, false, EditActionInsert);
}

void Editor::addRangeToKillRing(const Range& range, KillRingInsertionMode mode)
{
    addTextToKillRing(plainText(&range), mode);
}

void Editor::addTextToKillRing(const String& text, KillRingInsertionMode mode)
{
    if (m_shouldStartNewKillRingSequence)
        killRing().startNewSequence();

    m_shouldStartNewKillRingSequence = false;

    // If the kill was from a backwards motion, prepend to the kill ring.
    // This will ensure that alternating forward and backward kills will
    // build up the original string in the kill ring without permuting it.
    switch (mode) {
    case KillRingInsertionMode::PrependText:
        killRing().prepend(text);
        break;
    case KillRingInsertionMode::AppendText:
        killRing().append(text);
        break;
    }
}

void Editor::startAlternativeTextUITimer()
{
    m_alternativeTextController->startAlternativeTextUITimer(AlternativeTextTypeCorrection);
}

void Editor::handleAlternativeTextUIResult(const String& correction)
{
    m_alternativeTextController->handleAlternativeTextUIResult(correction);
}


void Editor::dismissCorrectionPanelAsIgnored()
{
    m_alternativeTextController->dismiss(ReasonForDismissingAlternativeTextIgnored);
}

void Editor::changeSelectionAfterCommand(const VisibleSelection& newSelection, FrameSelection::SetSelectionOptions options)
{
    Ref<Frame> protection(m_frame);

    // If the new selection is orphaned, then don't update the selection.
    if (newSelection.start().isOrphan() || newSelection.end().isOrphan())
        return;

    // If there is no selection change, don't bother sending shouldChangeSelection, but still call setSelection,
    // because there is work that it must do in this situation.
    // The old selection can be invalid here and calling shouldChangeSelection can produce some strange calls.
    // See <rdar://problem/5729315> Some shouldChangeSelectedDOMRange contain Ranges for selections that are no longer valid
    bool selectionDidNotChangeDOMPosition = newSelection == m_frame.selection().selection();
    if (selectionDidNotChangeDOMPosition || m_frame.selection().shouldChangeSelection(newSelection))
        m_frame.selection().setSelection(newSelection, options);

    // Some editing operations change the selection visually without affecting its position within the DOM.
    // For example when you press return in the following (the caret is marked by ^):
    // <div contentEditable="true"><div>^Hello</div></div>
    // WebCore inserts <div><br></div> *before* the current block, which correctly moves the paragraph down but which doesn't
    // change the caret's DOM position (["hello", 0]). In these situations the above FrameSelection::setSelection call
    // does not call EditorClient::respondToChangedSelection(), which, on the Mac, sends selection change notifications and
    // starts a new kill ring sequence, but we want to do these things (matches AppKit).
#if PLATFORM(IOS)
    // FIXME: Should suppress selection change notifications during a composition change <https://webkit.org/b/38830>
    if (m_ignoreSelectionChanges)
        return;
#endif
    if (selectionDidNotChangeDOMPosition && client())
        client()->respondToChangedSelection(&m_frame);
}

String Editor::selectedText() const
{
    return selectedText(TextIteratorDefaultBehavior);
}

String Editor::selectedTextForDataTransfer() const
{
    return selectedText(TextIteratorEmitsImageAltText);
}

String Editor::selectedText(TextIteratorBehavior behavior) const
{
    // We remove '\0' characters because they are not visibly rendered to the user.
    return plainText(m_frame.selection().toNormalizedRange().get(), behavior).replaceWithLiteral('\0', "");
}

static inline void collapseCaretWidth(IntRect& rect)
{
    // FIXME: Width adjustment doesn't work for rotated text.
    if (rect.width() == caretWidth)
        rect.setWidth(0);
    else if (rect.height() == caretWidth)
        rect.setHeight(0);
}

IntRect Editor::firstRectForRange(Range* range) const
{
    VisiblePosition startVisiblePosition(range->startPosition(), DOWNSTREAM);

    if (range->collapsed()) {
        // FIXME: Getting caret rect and removing caret width is a very roundabout way to get collapsed range location.
        // In particular, width adjustment doesn't work for rotated text.
        IntRect startCaretRect = RenderedPosition(startVisiblePosition).absoluteRect();
        collapseCaretWidth(startCaretRect);
        return startCaretRect;
    }

    VisiblePosition endVisiblePosition(range->endPosition(), UPSTREAM);

    if (inSameLine(startVisiblePosition, endVisiblePosition))
        return enclosingIntRect(RenderObject::absoluteBoundingBoxRectForRange(range));

    LayoutUnit extraWidthToEndOfLine = 0;
    IntRect startCaretRect = RenderedPosition(startVisiblePosition).absoluteRect(&extraWidthToEndOfLine);
    if (startCaretRect == IntRect())
        return IntRect();

    // When start and end aren't on the same line, we want to go from start to the end of its line.
    bool textIsHorizontal = startCaretRect.width() == caretWidth;
    return textIsHorizontal ?
        IntRect(startCaretRect.x(),
            startCaretRect.y(),
            startCaretRect.width() + extraWidthToEndOfLine,
            startCaretRect.height()) :
        IntRect(startCaretRect.x(),
            startCaretRect.y(),
            startCaretRect.width(),
            startCaretRect.height() + extraWidthToEndOfLine);
}

bool Editor::shouldChangeSelection(const VisibleSelection& oldSelection, const VisibleSelection& newSelection, EAffinity affinity, bool stillSelecting) const
{
#if PLATFORM(IOS)
    if (m_frame.selectionChangeCallbacksDisabled())
        return true;
#endif
    return client() && client()->shouldChangeSelectedRange(oldSelection.toNormalizedRange().get(), newSelection.toNormalizedRange().get(), affinity, stillSelecting);
}

void Editor::computeAndSetTypingStyle(EditingStyle& style, EditAction editingAction)
{
    if (style.isEmpty()) {
        m_frame.selection().clearTypingStyle();
        return;
    }

    // Calculate the current typing style.
    RefPtr<EditingStyle> typingStyle;
    if (auto existingTypingStyle = m_frame.selection().typingStyle())
        typingStyle = existingTypingStyle->copy();
    else
        typingStyle = EditingStyle::create();
    typingStyle->overrideTypingStyleAt(style, m_frame.selection().selection().visibleStart().deepEquivalent());

    // Handle block styles, substracting these from the typing style.
    RefPtr<EditingStyle> blockStyle = typingStyle->extractAndRemoveBlockProperties();
    if (!blockStyle->isEmpty())
        ApplyStyleCommand::create(document(), blockStyle.get(), editingAction)->apply();

    // Set the remaining style as the typing style.
    m_frame.selection().setTypingStyle(WTFMove(typingStyle));
}

void Editor::computeAndSetTypingStyle(StyleProperties& properties, EditAction editingAction)
{
    return computeAndSetTypingStyle(EditingStyle::create(&properties), editingAction);
}

void Editor::textFieldDidBeginEditing(Element* e)
{
    if (client())
        client()->textFieldDidBeginEditing(e);
}

void Editor::textFieldDidEndEditing(Element* e)
{
    dismissCorrectionPanelAsIgnored();
    if (client())
        client()->textFieldDidEndEditing(e);
}

void Editor::textDidChangeInTextField(Element* e)
{
    if (client())
        client()->textDidChangeInTextField(e);
}

bool Editor::doTextFieldCommandFromEvent(Element* e, KeyboardEvent* ke)
{
    if (client())
        return client()->doTextFieldCommandFromEvent(e, ke);

    return false;
}

void Editor::textWillBeDeletedInTextField(Element* input)
{
    if (client())
        client()->textWillBeDeletedInTextField(input);
}

void Editor::textDidChangeInTextArea(Element* e)
{
    if (client())
        client()->textDidChangeInTextArea(e);
}

void Editor::applyEditingStyleToBodyElement() const
{
    auto collection = document().getElementsByTagName(HTMLNames::bodyTag.localName());
    unsigned length = collection->length();
    for (unsigned i = 0; i < length; ++i)
        applyEditingStyleToElement(collection->item(i));
}

void Editor::applyEditingStyleToElement(Element* element) const
{
    ASSERT(!element || is<StyledElement>(*element));
    if (!is<StyledElement>(element))
        return;

    // Mutate using the CSSOM wrapper so we get the same event behavior as a script.
    auto& style = downcast<StyledElement>(*element).cssomStyle();
    style.setPropertyInternal(CSSPropertyWordWrap, "break-word", false);
    style.setPropertyInternal(CSSPropertyWebkitNbspMode, "space", false);
    style.setPropertyInternal(CSSPropertyLineBreak, "after-white-space", false);
}

bool Editor::findString(const String& target, FindOptions options)
{
    Ref<Frame> protection(m_frame);

    VisibleSelection selection = m_frame.selection().selection();

    RefPtr<Range> resultRange = rangeOfString(target, selection.firstRange().get(), options);

    if (!resultRange)
        return false;

    m_frame.selection().setSelection(VisibleSelection(*resultRange, DOWNSTREAM));

    if (!(options & DoNotRevealSelection))
        m_frame.selection().revealSelection();

    return true;
}

RefPtr<Range> Editor::rangeOfString(const String& target, Range* referenceRange, FindOptions options)
{
    if (target.isEmpty())
        return nullptr;

    // Start from an edge of the reference range, if there's a reference range that's not in shadow content. Which edge
    // is used depends on whether we're searching forward or backward, and whether startInSelection is set.
    RefPtr<Range> searchRange(rangeOfContents(document()));

    bool forward = !(options & Backwards);
    bool startInReferenceRange = referenceRange && (options & StartInSelection);
    if (referenceRange) {
        if (forward)
            searchRange->setStart(startInReferenceRange ? referenceRange->startPosition() : referenceRange->endPosition());
        else
            searchRange->setEnd(startInReferenceRange ? referenceRange->endPosition() : referenceRange->startPosition());
    }

    RefPtr<ShadowRoot> shadowTreeRoot = referenceRange ? referenceRange->startContainer().containingShadowRoot() : nullptr;
    if (shadowTreeRoot) {
        if (forward)
            searchRange->setEnd(*shadowTreeRoot, shadowTreeRoot->countChildNodes());
        else
            searchRange->setStart(*shadowTreeRoot, 0);
    }

    RefPtr<Range> resultRange = findPlainText(*searchRange, target, options);
    // If we started in the reference range and the found range exactly matches the reference range, find again.
    // Build a selection with the found range to remove collapsed whitespace.
    // Compare ranges instead of selection objects to ignore the way that the current selection was made.
    if (startInReferenceRange && areRangesEqual(VisibleSelection(*resultRange).toNormalizedRange().get(), referenceRange)) {
        searchRange = rangeOfContents(document());
        if (forward)
            searchRange->setStart(referenceRange->endPosition());
        else
            searchRange->setEnd(referenceRange->startPosition());

        if (shadowTreeRoot) {
            if (forward)
                searchRange->setEnd(*shadowTreeRoot, shadowTreeRoot->countChildNodes());
            else
                searchRange->setStart(*shadowTreeRoot, 0);
        }

        resultRange = findPlainText(*searchRange, target, options);
    }

    // If nothing was found in the shadow tree, search in main content following the shadow tree.
    if (resultRange->collapsed() && shadowTreeRoot) {
        searchRange = rangeOfContents(document());
        if (shadowTreeRoot->shadowHost()) {
            if (forward)
                searchRange->setStartAfter(*shadowTreeRoot->shadowHost());
            else
                searchRange->setEndBefore(*shadowTreeRoot->shadowHost());
        }

        resultRange = findPlainText(*searchRange, target, options);
    }

    // If we didn't find anything and we're wrapping, search again in the entire document (this will
    // redundantly re-search the area already searched in some cases).
    if (resultRange->collapsed() && options & WrapAround) {
        searchRange = rangeOfContents(document());
        resultRange = findPlainText(*searchRange, target, options);
        // We used to return false here if we ended up with the same range that we started with
        // (e.g., the reference range was already the only instance of this text). But we decided that
        // this should be a success case instead, so we'll just fall through in that case.
    }

    return resultRange->collapsed() ? nullptr : resultRange;
}

static bool isFrameInRange(Frame& frame, Range& range)
{
    for (auto* ownerElement = frame.ownerElement(); ownerElement; ownerElement = ownerElement->document().ownerElement()) {
        if (&ownerElement->document() == &range.ownerDocument()) {
            auto result = range.intersectsNode(*ownerElement);
            return !result.hasException() && result.releaseReturnValue();
        }
    }
    return false;
}

unsigned Editor::countMatchesForText(const String& target, Range* range, FindOptions options, unsigned limit, bool markMatches, Vector<RefPtr<Range>>* matches)
{
    if (target.isEmpty())
        return 0;

    RefPtr<Range> searchRange;
    if (range) {
        if (&range->ownerDocument() == &document())
            searchRange = range;
        else if (!isFrameInRange(m_frame, *range))
            return 0;
    }
    if (!searchRange)
        searchRange = rangeOfContents(document());

    Node& originalEndContainer = searchRange->endContainer();
    int originalEndOffset = searchRange->endOffset();

    unsigned matchCount = 0;
    do {
        RefPtr<Range> resultRange(findPlainText(*searchRange, target, options & ~Backwards));
        if (resultRange->collapsed()) {
            if (!resultRange->startContainer().isInShadowTree())
                break;

            searchRange->setStartAfter(*resultRange->startContainer().shadowHost());
            searchRange->setEnd(originalEndContainer, originalEndOffset);
            continue;
        }

        ++matchCount;
        if (matches)
            matches->append(resultRange);
        
        if (markMatches)
            document().markers().addMarker(resultRange.get(), DocumentMarker::TextMatch);

        // Stop looking if we hit the specified limit. A limit of 0 means no limit.
        if (limit > 0 && matchCount >= limit)
            break;

        // Set the new start for the search range to be the end of the previous
        // result range. There is no need to use a VisiblePosition here,
        // since findPlainText will use a TextIterator to go over the visible
        // text nodes. 
        searchRange->setStart(resultRange->endContainer(), resultRange->endOffset());

        Node* shadowTreeRoot = searchRange->shadowRoot();
        if (searchRange->collapsed() && shadowTreeRoot)
            searchRange->setEnd(*shadowTreeRoot, shadowTreeRoot->countChildNodes());
    } while (true);

    return matchCount;
}

void Editor::setMarkedTextMatchesAreHighlighted(bool flag)
{
    if (flag == m_areMarkedTextMatchesHighlighted)
        return;

    m_areMarkedTextMatchesHighlighted = flag;
    document().markers().repaintMarkers(DocumentMarker::TextMatch);
}

#if !PLATFORM(MAC)
void Editor::selectionWillChange()
{
}
#endif

void Editor::respondToChangedSelection(const VisibleSelection&, FrameSelection::SetSelectionOptions options)
{
#if PLATFORM(IOS)
    // FIXME: Should suppress selection change notifications during a composition change <https://webkit.org/b/38830> 
    if (m_ignoreSelectionChanges)
        return;
#endif

    if (client())
        client()->respondToChangedSelection(&m_frame);

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && !PLATFORM(IOS)
    if (shouldDetectTelephoneNumbers())
        m_telephoneNumberDetectionUpdateTimer.startOneShot(0_s);
#endif

    setStartNewKillRingSequence(true);

    if (m_editorUIUpdateTimer.isActive())
        return;

    // Don't check spelling and grammar if the change of selection is triggered by spelling correction itself.
    m_editorUIUpdateTimerShouldCheckSpellingAndGrammar = options & FrameSelection::CloseTyping
        && !(options & FrameSelection::SpellCorrectionTriggered);
    m_editorUIUpdateTimerWasTriggeredByDictation = options & FrameSelection::DictationTriggered;
    m_editorUIUpdateTimer.startOneShot(0_s);
}

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && !PLATFORM(IOS)

bool Editor::shouldDetectTelephoneNumbers()
{
    if (!m_frame.document())
        return false;
    return document().isTelephoneNumberParsingEnabled() && TelephoneNumberDetector::isSupported();
}

void Editor::scanSelectionForTelephoneNumbers()
{
    if (!shouldDetectTelephoneNumbers() || !client())
        return;

    m_detectedTelephoneNumberRanges.clear();

    Vector<RefPtr<Range>> markedRanges;

    FrameSelection& frameSelection = m_frame.selection();
    if (!frameSelection.isRange()) {
        m_frame.mainFrame().servicesOverlayController().selectedTelephoneNumberRangesChanged();
        return;
    }
    RefPtr<Range> selectedRange = frameSelection.toNormalizedRange();

    // Extend the range a few characters in each direction to detect incompletely selected phone numbers.
    static const int charactersToExtend = 15;
    const VisibleSelection& visibleSelection = frameSelection.selection();
    Position start = visibleSelection.start();
    Position end = visibleSelection.end();
    for (int i = 0; i < charactersToExtend; ++i) {
        start = start.previous(Character);
        end = end.next(Character);
    }

    FrameSelection extendedSelection;
    extendedSelection.setStart(start);
    extendedSelection.setEnd(end);
    RefPtr<Range> extendedRange = extendedSelection.toNormalizedRange();

    if (!extendedRange) {
        m_frame.mainFrame().servicesOverlayController().selectedTelephoneNumberRangesChanged();
        return;
    }

    scanRangeForTelephoneNumbers(*extendedRange, extendedRange->text(), markedRanges);

    // Only consider ranges with a detected telephone number if they overlap with the actual selection range.
    for (auto& range : markedRanges) {
        if (rangesOverlap(range.get(), selectedRange.get()))
            m_detectedTelephoneNumberRanges.append(range);
    }

    m_frame.mainFrame().servicesOverlayController().selectedTelephoneNumberRangesChanged();
}

void Editor::scanRangeForTelephoneNumbers(Range& range, const StringView& stringView, Vector<RefPtr<Range>>& markedRanges)
{
    // Don't scan for phone numbers inside editable regions.
    Node& startNode = range.startContainer();
    if (startNode.hasEditableStyle())
        return;

    // relativeStartPosition and relativeEndPosition are the endpoints of the phone number range,
    // relative to the scannerPosition
    unsigned length = stringView.length();
    unsigned scannerPosition = 0;
    int relativeStartPosition = 0;
    int relativeEndPosition = 0;

    auto characters = stringView.upconvertedCharacters();

    while (scannerPosition < length && TelephoneNumberDetector::find(&characters[scannerPosition], length - scannerPosition, &relativeStartPosition, &relativeEndPosition)) {
        // The convention in the Data Detectors framework is that the end position is the first character NOT in the phone number
        // (that is, the length of the range is relativeEndPosition - relativeStartPosition). So subtract 1 to get the same
        // convention as the old WebCore phone number parser (so that the rest of the code is still valid if we want to go back
        // to the old parser).
        --relativeEndPosition;

        ASSERT(scannerPosition + relativeEndPosition < length);

        unsigned subrangeOffset = scannerPosition + relativeStartPosition;
        unsigned subrangeLength = relativeEndPosition - relativeStartPosition + 1;

        RefPtr<Range> subrange = TextIterator::subrange(range, subrangeOffset, subrangeLength);

        markedRanges.append(subrange);
        range.ownerDocument().markers().addMarker(subrange.get(), DocumentMarker::TelephoneNumber);

        scannerPosition += relativeEndPosition + 1;
    }
}

#endif // ENABLE(TELEPHONE_NUMBER_DETECTION) && !PLATFORM(IOS)

void Editor::updateEditorUINowIfScheduled()
{
    if (!m_editorUIUpdateTimer.isActive())
        return;
    m_editorUIUpdateTimer.stop();
    editorUIUpdateTimerFired();
}

void Editor::editorUIUpdateTimerFired()
{
    VisibleSelection oldSelection = m_oldSelectionForEditorUIUpdate;

    m_alternativeTextController->stopPendingCorrection(oldSelection);
    
    bool isContinuousSpellCheckingEnabled = this->isContinuousSpellCheckingEnabled();
    bool isContinuousGrammarCheckingEnabled = isContinuousSpellCheckingEnabled && isGrammarCheckingEnabled();
    if (isContinuousSpellCheckingEnabled) {
        VisibleSelection newAdjacentWords;
        VisibleSelection newSelectedSentence;
        bool caretBrowsing = m_frame.settings().caretBrowsingEnabled();
        if (m_frame.selection().selection().isContentEditable() || caretBrowsing) {
            VisiblePosition newStart(m_frame.selection().selection().visibleStart());
#if !PLATFORM(IOS)
            newAdjacentWords = VisibleSelection(startOfWord(newStart, LeftWordIfOnBoundary), endOfWord(newStart, RightWordIfOnBoundary));
#else
            // If this bug gets fixed, this PLATFORM(IOS) code could be removed:
            // <rdar://problem/7259611> Word boundary code on iPhone gives different results than desktop
            EWordSide startWordSide = LeftWordIfOnBoundary;
            UChar32 c = newStart.characterBefore();
            // FIXME: VisiblePosition::characterAfter() and characterBefore() do not emit newlines the same
            // way as TextIterator, so we do an isStartOfParagraph check here.
            if (isSpaceOrNewline(c) || c == 0xA0 || isStartOfParagraph(newStart)) {
                startWordSide = RightWordIfOnBoundary;
            }
            newAdjacentWords = VisibleSelection(startOfWord(newStart, startWordSide), endOfWord(newStart, RightWordIfOnBoundary));
#endif // !PLATFORM(IOS)
            if (isContinuousGrammarCheckingEnabled)
                newSelectedSentence = VisibleSelection(startOfSentence(newStart), endOfSentence(newStart));
        }

        // When typing we check spelling elsewhere, so don't redo it here.
        // If this is a change in selection resulting from a delete operation,
        // oldSelection may no longer be in the document.
        if (m_editorUIUpdateTimerShouldCheckSpellingAndGrammar && oldSelection.isContentEditable() && oldSelection.start().deprecatedNode() && oldSelection.start().anchorNode()->isConnected()) {
            VisiblePosition oldStart(oldSelection.visibleStart());
            VisibleSelection oldAdjacentWords = VisibleSelection(startOfWord(oldStart, LeftWordIfOnBoundary), endOfWord(oldStart, RightWordIfOnBoundary));
            if (oldAdjacentWords != newAdjacentWords) {
                if (isContinuousGrammarCheckingEnabled) {
                    VisibleSelection oldSelectedSentence = VisibleSelection(startOfSentence(oldStart), endOfSentence(oldStart));
                    markMisspellingsAndBadGrammar(oldAdjacentWords, oldSelectedSentence != newSelectedSentence, oldSelectedSentence);
                } else
                    markMisspellingsAndBadGrammar(oldAdjacentWords, false, oldAdjacentWords);
            }
        }

        if (!textChecker() || textChecker()->shouldEraseMarkersAfterChangeSelection(TextCheckingTypeSpelling)) {
            if (RefPtr<Range> wordRange = newAdjacentWords.toNormalizedRange())
                document().markers().removeMarkers(wordRange.get(), DocumentMarker::Spelling);
        }
        if (!textChecker() || textChecker()->shouldEraseMarkersAfterChangeSelection(TextCheckingTypeGrammar)) {
            if (RefPtr<Range> sentenceRange = newSelectedSentence.toNormalizedRange())
                document().markers().removeMarkers(sentenceRange.get(), DocumentMarker::Grammar);
        }
    }

    // When continuous spell checking is off, existing markers disappear after the selection changes.
    if (!isContinuousSpellCheckingEnabled)
        document().markers().removeMarkers(DocumentMarker::Spelling);
    if (!isContinuousGrammarCheckingEnabled)
        document().markers().removeMarkers(DocumentMarker::Grammar);

    if (!m_editorUIUpdateTimerWasTriggeredByDictation)
        m_alternativeTextController->respondToChangedSelection(oldSelection);

    m_oldSelectionForEditorUIUpdate = m_frame.selection().selection();
}

static Node* findFirstMarkable(Node* node)
{
    while (node) {
        if (!node->renderer())
            return nullptr;
        if (node->renderer()->isTextOrLineBreak())
            return node;
        if (is<HTMLTextFormControlElement>(*node))
            node = downcast<HTMLTextFormControlElement>(*node).visiblePositionForIndex(1).deepEquivalent().deprecatedNode();
        else if (node->firstChild())
            node = node->firstChild();
        else
            node = node->nextSibling();
    }

    return nullptr;
}

bool Editor::selectionStartHasMarkerFor(DocumentMarker::MarkerType markerType, int from, int length) const
{
    Node* node = findFirstMarkable(m_frame.selection().selection().start().deprecatedNode());
    if (!node)
        return false;

    unsigned int startOffset = static_cast<unsigned int>(from);
    unsigned int endOffset = static_cast<unsigned int>(from + length);
    Vector<RenderedDocumentMarker*> markers = document().markers().markersFor(node);
    for (auto* marker : markers) {
        if (marker->startOffset() <= startOffset && endOffset <= marker->endOffset() && marker->type() == markerType)
            return true;
    }

    return false;
}       

TextCheckingTypeMask Editor::resolveTextCheckingTypeMask(const Node& rootEditableElement, TextCheckingTypeMask textCheckingOptions)
{
#if USE(AUTOMATIC_TEXT_REPLACEMENT) && !PLATFORM(IOS)
    bool onlyAllowsTextReplacement = false;
    if (auto* host = rootEditableElement.shadowHost())
        onlyAllowsTextReplacement = is<HTMLInputElement>(host) && downcast<HTMLInputElement>(*host).isSpellcheckDisabledExceptTextReplacement();
    if (onlyAllowsTextReplacement)
        textCheckingOptions &= TextCheckingTypeReplacement;
#else
    UNUSED_PARAM(rootEditableElement);
#endif

    bool shouldMarkSpelling = textCheckingOptions & TextCheckingTypeSpelling;
    bool shouldMarkGrammar = textCheckingOptions & TextCheckingTypeGrammar;
#if !PLATFORM(IOS)
    bool shouldShowCorrectionPanel = textCheckingOptions & TextCheckingTypeShowCorrectionPanel;
    bool shouldCheckForCorrection = shouldShowCorrectionPanel || (textCheckingOptions & TextCheckingTypeCorrection);
#endif

    TextCheckingTypeMask checkingTypes = 0;
    if (shouldMarkSpelling)
        checkingTypes |= TextCheckingTypeSpelling;
    if (shouldMarkGrammar)
        checkingTypes |= TextCheckingTypeGrammar;
#if !PLATFORM(IOS)
    if (shouldCheckForCorrection)
        checkingTypes |= TextCheckingTypeCorrection;
    if (shouldShowCorrectionPanel)
        checkingTypes |= TextCheckingTypeShowCorrectionPanel;

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    bool shouldPerformReplacement = textCheckingOptions & TextCheckingTypeReplacement;
    if (shouldPerformReplacement) {
        if (!onlyAllowsTextReplacement) {
            if (isAutomaticLinkDetectionEnabled())
                checkingTypes |= TextCheckingTypeLink;
            if (isAutomaticQuoteSubstitutionEnabled())
                checkingTypes |= TextCheckingTypeQuote;
            if (isAutomaticDashSubstitutionEnabled())
                checkingTypes |= TextCheckingTypeDash;
            if (shouldMarkSpelling && isAutomaticSpellingCorrectionEnabled())
                checkingTypes |= TextCheckingTypeCorrection;
        }
        if (isAutomaticTextReplacementEnabled())
            checkingTypes |= TextCheckingTypeReplacement;
    }
#endif
#endif // !PLATFORM(IOS)

    return checkingTypes;
}

static RefPtr<Range> candidateRangeForSelection(Frame& frame)
{
    const VisibleSelection& selection = frame.selection().selection();
    return selection.isCaret() ? wordRangeFromPosition(selection.start()) : frame.selection().toNormalizedRange();
}

static bool candidateWouldReplaceText(const VisibleSelection& selection)
{
    // If the character behind the caret in the current selection is anything but a space or a newline then we should
    // replace the whole current word with the candidate.
    UChar32 characterAfterSelection, characterBeforeSelection, twoCharacterBeforeSelection = 0;
    charactersAroundPosition(selection.visibleStart(), characterAfterSelection, characterBeforeSelection, twoCharacterBeforeSelection);
    return !(characterBeforeSelection == '\0' || characterBeforeSelection == '\n' || characterBeforeSelection == ' ');
}

String Editor::stringForCandidateRequest() const
{
    const VisibleSelection& selection = m_frame.selection().selection();
    RefPtr<Range> rangeForCurrentlyTypedString = candidateRangeForSelection(m_frame);
    if (rangeForCurrentlyTypedString && candidateWouldReplaceText(selection))
        return plainText(rangeForCurrentlyTypedString.get());

    return String();
}
    
RefPtr<Range> Editor::contextRangeForCandidateRequest() const
{
    const VisibleSelection& selection = m_frame.selection().selection();
    return makeRange(startOfParagraph(selection.visibleStart()), endOfParagraph(selection.visibleEnd()));
}

RefPtr<Range> Editor::rangeForTextCheckingResult(const TextCheckingResult& result) const
{
    if (!result.length)
        return nullptr;

    RefPtr<Range> contextRange = contextRangeForCandidateRequest();
    if (!contextRange)
        return nullptr;

    return TextIterator::subrange(*contextRange, result.location, result.length);
}

void Editor::handleAcceptedCandidate(TextCheckingResult acceptedCandidate)
{
    const VisibleSelection& selection = m_frame.selection().selection();

    m_isHandlingAcceptedCandidate = true;

    if (auto range = rangeForTextCheckingResult(acceptedCandidate)) {
        if (shouldInsertText(acceptedCandidate.replacement, range.get(), EditorInsertAction::Typed))
            ReplaceRangeWithTextCommand::create(range.get(), acceptedCandidate.replacement)->apply();
    } else
        insertText(acceptedCandidate.replacement, nullptr);

    RefPtr<Range> insertedCandidateRange = rangeExpandedByCharactersInDirectionAtWordBoundary(selection.visibleStart(), acceptedCandidate.replacement.length(), DirectionBackward);
    if (insertedCandidateRange)
        insertedCandidateRange->startContainer().document().markers().addMarker(insertedCandidateRange.get(), DocumentMarker::AcceptedCandidate, acceptedCandidate.replacement);

    m_isHandlingAcceptedCandidate = false;
}

bool Editor::unifiedTextCheckerEnabled() const
{
    return WebCore::unifiedTextCheckerEnabled(&m_frame);
}

Vector<String> Editor::dictationAlternativesForMarker(const DocumentMarker& marker)
{
    return m_alternativeTextController->dictationAlternativesForMarker(marker);
}

void Editor::applyDictationAlternativelternative(const String& alternativeString)
{
    m_alternativeTextController->applyDictationAlternative(alternativeString);
}

void Editor::toggleOverwriteModeEnabled()
{
    m_overwriteModeEnabled = !m_overwriteModeEnabled;
    m_frame.selection().setShouldShowBlockCursor(m_overwriteModeEnabled);
}

Document& Editor::document() const
{
    ASSERT(m_frame.document());
    return *m_frame.document();
}

RefPtr<Range> Editor::adjustedSelectionRange()
{
    // FIXME: Why do we need to adjust the selection to include the anchor tag it's in?
    // Whoever wrote this code originally forgot to leave us a comment explaining the rationale.
    RefPtr<Range> range = selectedRange();
    Node* commonAncestor = range->commonAncestorContainer();
    ASSERT(commonAncestor);
    auto* enclosingAnchor = enclosingElementWithTag(firstPositionInNode(commonAncestor), HTMLNames::aTag);
    if (enclosingAnchor && comparePositions(firstPositionInOrBeforeNode(range->startPosition().anchorNode()), range->startPosition()) >= 0)
        range->setStart(*enclosingAnchor, 0);
    return range;
}

// FIXME: This figures out the current style by inserting a <span>!
const RenderStyle* Editor::styleForSelectionStart(Frame* frame, Node*& nodeToRemove)
{
    nodeToRemove = nullptr;

    if (frame->selection().isNone())
        return nullptr;

    Position position = adjustedSelectionStartForStyleComputation(frame->selection().selection());
    if (!position.isCandidate() || position.isNull())
        return nullptr;

    RefPtr<EditingStyle> typingStyle = frame->selection().typingStyle();
    if (!typingStyle || !typingStyle->style())
        return &position.deprecatedNode()->renderer()->style();

    auto styleElement = HTMLSpanElement::create(*frame->document());

    String styleText = typingStyle->style()->asText() + " display: inline";
    styleElement->setAttribute(HTMLNames::styleAttr, styleText);

    styleElement->appendChild(frame->document()->createEditingTextNode(emptyString()));

    auto positionNode = position.deprecatedNode();
    if (!positionNode || !positionNode->parentNode() || positionNode->parentNode()->appendChild(styleElement).hasException())
        return nullptr;

    nodeToRemove = styleElement.ptr();
    
    frame->document()->updateStyleIfNeeded();
    return styleElement->renderer() ? &styleElement->renderer()->style() : nullptr;
}

const Font* Editor::fontForSelection(bool& hasMultipleFonts) const
{
    hasMultipleFonts = false;

    if (!m_frame.selection().isRange()) {
        Node* nodeToRemove;
        auto* style = styleForSelectionStart(&m_frame, nodeToRemove); // sets nodeToRemove

        const Font* font = nullptr;
        if (style) {
            font = &style->fontCascade().primaryFont();
            if (nodeToRemove)
                nodeToRemove->remove();
        }

        return font;
    }

    RefPtr<Range> range = m_frame.selection().toNormalizedRange();
    if (!range)
        return nullptr;

    Node* startNode = adjustedSelectionStartForStyleComputation(m_frame.selection().selection()).deprecatedNode();
    if (!startNode)
        return nullptr;

    const Font* font = nullptr;
    Node* pastEnd = range->pastLastNode();
    // In the loop below, node should eventually match pastEnd and not become null, but we've seen at least one
    // unreproducible case where this didn't happen, so check for null also.
    for (Node* node = startNode; node && node != pastEnd; node = NodeTraversal::next(*node)) {
        auto renderer = node->renderer();
        if (!renderer)
            continue;
        // FIXME: Are there any node types that have renderers, but that we should be skipping?
        const Font& primaryFont = renderer->style().fontCascade().primaryFont();
        if (!font)
            font = &primaryFont;
        else if (font != &primaryFont) {
            hasMultipleFonts = true;
            break;
        }
    }

    return font;
}

} // namespace WebCore
