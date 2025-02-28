/*
    This file is part of the WebKit open source project.
    This file has been generated by generate-bindings.pl. DO NOT MODIFY!

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "JSTestCEReactions.h"

#include "CustomElementReactionQueue.h"
#include "HTMLNames.h"
#include "JSDOMAttribute.h"
#include "JSDOMBinding.h"
#include "JSDOMConstructorNotConstructable.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertStrings.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMOperation.h"
#include "JSDOMWrapperCache.h"
#include "JSTestCEReactionsStringifier.h"
#include <runtime/FunctionPrototype.h>
#include <runtime/JSCInlines.h>
#include <wtf/GetPtr.h>

using namespace JSC;

namespace WebCore {

// Functions

JSC::EncodedJSValue JSC_HOST_CALL jsTestCEReactionsPrototypeFunctionMethodWithCEReactions(JSC::ExecState*);

// Attributes

JSC::EncodedJSValue jsTestCEReactionsConstructor(JSC::ExecState*, JSC::EncodedJSValue, JSC::PropertyName);
bool setJSTestCEReactionsConstructor(JSC::ExecState*, JSC::EncodedJSValue, JSC::EncodedJSValue);
JSC::EncodedJSValue jsTestCEReactionsAttributeWithCEReactions(JSC::ExecState*, JSC::EncodedJSValue, JSC::PropertyName);
bool setJSTestCEReactionsAttributeWithCEReactions(JSC::ExecState*, JSC::EncodedJSValue, JSC::EncodedJSValue);
JSC::EncodedJSValue jsTestCEReactionsReflectAttributeWithCEReactions(JSC::ExecState*, JSC::EncodedJSValue, JSC::PropertyName);
bool setJSTestCEReactionsReflectAttributeWithCEReactions(JSC::ExecState*, JSC::EncodedJSValue, JSC::EncodedJSValue);
JSC::EncodedJSValue jsTestCEReactionsStringifierAttribute(JSC::ExecState*, JSC::EncodedJSValue, JSC::PropertyName);
bool setJSTestCEReactionsStringifierAttribute(JSC::ExecState*, JSC::EncodedJSValue, JSC::EncodedJSValue);

class JSTestCEReactionsPrototype : public JSC::JSNonFinalObject {
public:
    using Base = JSC::JSNonFinalObject;
    static JSTestCEReactionsPrototype* create(JSC::VM& vm, JSDOMGlobalObject* globalObject, JSC::Structure* structure)
    {
        JSTestCEReactionsPrototype* ptr = new (NotNull, JSC::allocateCell<JSTestCEReactionsPrototype>(vm.heap)) JSTestCEReactionsPrototype(vm, globalObject, structure);
        ptr->finishCreation(vm);
        return ptr;
    }

    DECLARE_INFO;
    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

private:
    JSTestCEReactionsPrototype(JSC::VM& vm, JSC::JSGlobalObject*, JSC::Structure* structure)
        : JSC::JSNonFinalObject(vm, structure)
    {
    }

    void finishCreation(JSC::VM&);
};

using JSTestCEReactionsConstructor = JSDOMConstructorNotConstructable<JSTestCEReactions>;

template<> JSValue JSTestCEReactionsConstructor::prototypeForStructure(JSC::VM& vm, const JSDOMGlobalObject& globalObject)
{
    UNUSED_PARAM(vm);
    return globalObject.functionPrototype();
}

template<> void JSTestCEReactionsConstructor::initializeProperties(VM& vm, JSDOMGlobalObject& globalObject)
{
    putDirect(vm, vm.propertyNames->prototype, JSTestCEReactions::prototype(vm, globalObject), JSC::PropertyAttribute::DontDelete | JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum);
    putDirect(vm, vm.propertyNames->name, jsNontrivialString(&vm, String(ASCIILiteral("TestCEReactions"))), JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum);
    putDirect(vm, vm.propertyNames->length, jsNumber(0), JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum);
}

template<> const ClassInfo JSTestCEReactionsConstructor::s_info = { "TestCEReactions", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTestCEReactionsConstructor) };

/* Hash table for prototype */

static const HashTableValue JSTestCEReactionsPrototypeTableValues[] =
{
    { "constructor", static_cast<unsigned>(JSC::PropertyAttribute::DontEnum), NoIntrinsic, { (intptr_t)static_cast<PropertySlot::GetValueFunc>(jsTestCEReactionsConstructor), (intptr_t) static_cast<PutPropertySlot::PutValueFunc>(setJSTestCEReactionsConstructor) } },
    { "attributeWithCEReactions", static_cast<unsigned>(JSC::PropertyAttribute::CustomAccessor | JSC::PropertyAttribute::DOMAttribute), NoIntrinsic, { (intptr_t)static_cast<PropertySlot::GetValueFunc>(jsTestCEReactionsAttributeWithCEReactions), (intptr_t) static_cast<PutPropertySlot::PutValueFunc>(setJSTestCEReactionsAttributeWithCEReactions) } },
    { "reflectAttributeWithCEReactions", static_cast<unsigned>(JSC::PropertyAttribute::CustomAccessor | JSC::PropertyAttribute::DOMAttribute), NoIntrinsic, { (intptr_t)static_cast<PropertySlot::GetValueFunc>(jsTestCEReactionsReflectAttributeWithCEReactions), (intptr_t) static_cast<PutPropertySlot::PutValueFunc>(setJSTestCEReactionsReflectAttributeWithCEReactions) } },
    { "stringifierAttribute", static_cast<unsigned>(JSC::PropertyAttribute::CustomAccessor | JSC::PropertyAttribute::DOMAttribute), NoIntrinsic, { (intptr_t)static_cast<PropertySlot::GetValueFunc>(jsTestCEReactionsStringifierAttribute), (intptr_t) static_cast<PutPropertySlot::PutValueFunc>(setJSTestCEReactionsStringifierAttribute) } },
    { "methodWithCEReactions", static_cast<unsigned>(JSC::PropertyAttribute::Function), NoIntrinsic, { (intptr_t)static_cast<NativeFunction>(jsTestCEReactionsPrototypeFunctionMethodWithCEReactions), (intptr_t) (0) } },
};

const ClassInfo JSTestCEReactionsPrototype::s_info = { "TestCEReactionsPrototype", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTestCEReactionsPrototype) };

void JSTestCEReactionsPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    reifyStaticProperties(vm, JSTestCEReactions::info(), JSTestCEReactionsPrototypeTableValues, *this);
}

const ClassInfo JSTestCEReactions::s_info = { "TestCEReactions", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTestCEReactions) };

JSTestCEReactions::JSTestCEReactions(Structure* structure, JSDOMGlobalObject& globalObject, Ref<TestCEReactions>&& impl)
    : JSDOMWrapper<TestCEReactions>(structure, globalObject, WTFMove(impl))
{
}

void JSTestCEReactions::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

}

JSObject* JSTestCEReactions::createPrototype(VM& vm, JSDOMGlobalObject& globalObject)
{
    return JSTestCEReactionsPrototype::create(vm, &globalObject, JSTestCEReactionsPrototype::createStructure(vm, &globalObject, globalObject.objectPrototype()));
}

JSObject* JSTestCEReactions::prototype(VM& vm, JSDOMGlobalObject& globalObject)
{
    return getDOMPrototype<JSTestCEReactions>(vm, globalObject);
}

JSValue JSTestCEReactions::getConstructor(VM& vm, const JSGlobalObject* globalObject)
{
    return getDOMConstructor<JSTestCEReactionsConstructor>(vm, *jsCast<const JSDOMGlobalObject*>(globalObject));
}

void JSTestCEReactions::destroy(JSC::JSCell* cell)
{
    JSTestCEReactions* thisObject = static_cast<JSTestCEReactions*>(cell);
    thisObject->JSTestCEReactions::~JSTestCEReactions();
}

template<> inline JSTestCEReactions* IDLAttribute<JSTestCEReactions>::cast(ExecState& state, EncodedJSValue thisValue)
{
    return jsDynamicDowncast<JSTestCEReactions*>(state.vm(), JSValue::decode(thisValue));
}

template<> inline JSTestCEReactions* IDLOperation<JSTestCEReactions>::cast(ExecState& state)
{
    return jsDynamicDowncast<JSTestCEReactions*>(state.vm(), state.thisValue());
}

EncodedJSValue jsTestCEReactionsConstructor(ExecState* state, EncodedJSValue thisValue, PropertyName)
{
    VM& vm = state->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* prototype = jsDynamicDowncast<JSTestCEReactionsPrototype*>(vm, JSValue::decode(thisValue));
    if (UNLIKELY(!prototype))
        return throwVMTypeError(state, throwScope);
    return JSValue::encode(JSTestCEReactions::getConstructor(state->vm(), prototype->globalObject()));
}

bool setJSTestCEReactionsConstructor(ExecState* state, EncodedJSValue thisValue, EncodedJSValue encodedValue)
{
    VM& vm = state->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* prototype = jsDynamicDowncast<JSTestCEReactionsPrototype*>(vm, JSValue::decode(thisValue));
    if (UNLIKELY(!prototype)) {
        throwVMTypeError(state, throwScope);
        return false;
    }
    // Shadowing a built-in constructor
    return prototype->putDirect(vm, vm.propertyNames->constructor, JSValue::decode(encodedValue));
}

static inline JSValue jsTestCEReactionsAttributeWithCEReactionsGetter(ExecState& state, JSTestCEReactions& thisObject, ThrowScope& throwScope)
{
    UNUSED_PARAM(throwScope);
    UNUSED_PARAM(state);
    auto& impl = thisObject.wrapped();
    JSValue result = toJS<IDLDOMString>(state, throwScope, impl.attributeWithCEReactions());
    return result;
}

EncodedJSValue jsTestCEReactionsAttributeWithCEReactions(ExecState* state, EncodedJSValue thisValue, PropertyName)
{
    return IDLAttribute<JSTestCEReactions>::get<jsTestCEReactionsAttributeWithCEReactionsGetter, CastedThisErrorBehavior::Assert>(*state, thisValue, "attributeWithCEReactions");
}

static inline bool setJSTestCEReactionsAttributeWithCEReactionsSetter(ExecState& state, JSTestCEReactions& thisObject, JSValue value, ThrowScope& throwScope)
{
    UNUSED_PARAM(throwScope);
    CustomElementReactionStack customElementReactionStack;
    auto& impl = thisObject.wrapped();
    auto nativeValue = convert<IDLDOMString>(state, value);
    RETURN_IF_EXCEPTION(throwScope, false);
    AttributeSetter::call(state, throwScope, [&] {
        return impl.setAttributeWithCEReactions(WTFMove(nativeValue));
    });
    return true;
}

bool setJSTestCEReactionsAttributeWithCEReactions(ExecState* state, EncodedJSValue thisValue, EncodedJSValue encodedValue)
{
    return IDLAttribute<JSTestCEReactions>::set<setJSTestCEReactionsAttributeWithCEReactionsSetter>(*state, thisValue, encodedValue, "attributeWithCEReactions");
}

static inline JSValue jsTestCEReactionsReflectAttributeWithCEReactionsGetter(ExecState& state, JSTestCEReactions& thisObject, ThrowScope& throwScope)
{
    UNUSED_PARAM(throwScope);
    UNUSED_PARAM(state);
    auto& impl = thisObject.wrapped();
    JSValue result = toJS<IDLDOMString>(state, throwScope, impl.attributeWithoutSynchronization(WebCore::HTMLNames::reflectattributewithcereactionsAttr));
    return result;
}

EncodedJSValue jsTestCEReactionsReflectAttributeWithCEReactions(ExecState* state, EncodedJSValue thisValue, PropertyName)
{
    return IDLAttribute<JSTestCEReactions>::get<jsTestCEReactionsReflectAttributeWithCEReactionsGetter, CastedThisErrorBehavior::Assert>(*state, thisValue, "reflectAttributeWithCEReactions");
}

static inline bool setJSTestCEReactionsReflectAttributeWithCEReactionsSetter(ExecState& state, JSTestCEReactions& thisObject, JSValue value, ThrowScope& throwScope)
{
    UNUSED_PARAM(throwScope);
    CustomElementReactionStack customElementReactionStack;
    auto& impl = thisObject.wrapped();
    auto nativeValue = convert<IDLDOMString>(state, value);
    RETURN_IF_EXCEPTION(throwScope, false);
    AttributeSetter::call(state, throwScope, [&] {
        return impl.setAttributeWithoutSynchronization(WebCore::HTMLNames::reflectattributewithcereactionsAttr, WTFMove(nativeValue));
    });
    return true;
}

bool setJSTestCEReactionsReflectAttributeWithCEReactions(ExecState* state, EncodedJSValue thisValue, EncodedJSValue encodedValue)
{
    return IDLAttribute<JSTestCEReactions>::set<setJSTestCEReactionsReflectAttributeWithCEReactionsSetter>(*state, thisValue, encodedValue, "reflectAttributeWithCEReactions");
}

static inline JSValue jsTestCEReactionsStringifierAttributeGetter(ExecState& state, JSTestCEReactions& thisObject, ThrowScope& throwScope)
{
    UNUSED_PARAM(throwScope);
    UNUSED_PARAM(state);
    auto& impl = thisObject.wrapped();
    JSValue result = toJS<IDLInterface<TestCEReactionsStringifier>>(state, *thisObject.globalObject(), throwScope, impl.stringifierAttribute());
    return result;
}

EncodedJSValue jsTestCEReactionsStringifierAttribute(ExecState* state, EncodedJSValue thisValue, PropertyName)
{
    return IDLAttribute<JSTestCEReactions>::get<jsTestCEReactionsStringifierAttributeGetter, CastedThisErrorBehavior::Assert>(*state, thisValue, "stringifierAttribute");
}

static inline bool setJSTestCEReactionsStringifierAttributeSetter(ExecState& state, JSTestCEReactions& thisObject, JSValue value, ThrowScope& throwScope)
{
    UNUSED_PARAM(throwScope);
    auto id = Identifier::fromString(&state.vm(), reinterpret_cast<const LChar*>("stringifierAttribute"), strlen("stringifierAttribute"));
    auto valueToForwardTo = thisObject.get(&state, id);
    RETURN_IF_EXCEPTION(throwScope, false);
    if (UNLIKELY(!valueToForwardTo.isObject())) {
        throwTypeError(&state, throwScope);
        return false;
    }
    auto forwardId = Identifier::fromString(&state.vm(), reinterpret_cast<const LChar*>("value"), strlen("value"));
    PutPropertySlot slot(valueToForwardTo, false);
    asObject(valueToForwardTo)->methodTable(state.vm())->put(asObject(valueToForwardTo), &state, forwardId, value, slot);
    RETURN_IF_EXCEPTION(throwScope, false);
    return true;
}

bool setJSTestCEReactionsStringifierAttribute(ExecState* state, EncodedJSValue thisValue, EncodedJSValue encodedValue)
{
    return IDLAttribute<JSTestCEReactions>::set<setJSTestCEReactionsStringifierAttributeSetter>(*state, thisValue, encodedValue, "stringifierAttribute");
}

static inline JSC::EncodedJSValue jsTestCEReactionsPrototypeFunctionMethodWithCEReactionsBody(JSC::ExecState* state, typename IDLOperation<JSTestCEReactions>::ClassParameter castedThis, JSC::ThrowScope& throwScope)
{
    UNUSED_PARAM(state);
    UNUSED_PARAM(throwScope);
    CustomElementReactionStack customElementReactionStack;
    auto& impl = castedThis->wrapped();
    impl.methodWithCEReactions();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL jsTestCEReactionsPrototypeFunctionMethodWithCEReactions(ExecState* state)
{
    return IDLOperation<JSTestCEReactions>::call<jsTestCEReactionsPrototypeFunctionMethodWithCEReactionsBody>(*state, "methodWithCEReactions");
}

bool JSTestCEReactionsOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, SlotVisitor& visitor)
{
    UNUSED_PARAM(handle);
    UNUSED_PARAM(visitor);
    return false;
}

void JSTestCEReactionsOwner::finalize(JSC::Handle<JSC::Unknown> handle, void* context)
{
    auto* jsTestCEReactions = static_cast<JSTestCEReactions*>(handle.slot()->asCell());
    auto& world = *static_cast<DOMWrapperWorld*>(context);
    uncacheWrapper(world, &jsTestCEReactions->wrapped(), jsTestCEReactions);
}

#if ENABLE(BINDING_INTEGRITY)
#if PLATFORM(WIN)
#pragma warning(disable: 4483)
extern "C" { extern void (*const __identifier("??_7TestCEReactions@WebCore@@6B@")[])(); }
#else
extern "C" { extern void* _ZTVN7WebCore15TestCEReactionsE[]; }
#endif
#endif

JSC::JSValue toJSNewlyCreated(JSC::ExecState*, JSDOMGlobalObject* globalObject, Ref<TestCEReactions>&& impl)
{

#if ENABLE(BINDING_INTEGRITY)
    void* actualVTablePointer = *(reinterpret_cast<void**>(impl.ptr()));
#if PLATFORM(WIN)
    void* expectedVTablePointer = reinterpret_cast<void*>(__identifier("??_7TestCEReactions@WebCore@@6B@"));
#else
    void* expectedVTablePointer = &_ZTVN7WebCore15TestCEReactionsE[2];
#endif

    // If this fails TestCEReactions does not have a vtable, so you need to add the
    // ImplementationLacksVTable attribute to the interface definition
    static_assert(std::is_polymorphic<TestCEReactions>::value, "TestCEReactions is not polymorphic");

    // If you hit this assertion you either have a use after free bug, or
    // TestCEReactions has subclasses. If TestCEReactions has subclasses that get passed
    // to toJS() we currently require TestCEReactions you to opt out of binding hardening
    // by adding the SkipVTableValidation attribute to the interface IDL definition
    RELEASE_ASSERT(actualVTablePointer == expectedVTablePointer);
#endif
    return createWrapper<TestCEReactions>(globalObject, WTFMove(impl));
}

JSC::JSValue toJS(JSC::ExecState* state, JSDOMGlobalObject* globalObject, TestCEReactions& impl)
{
    return wrap(state, globalObject, impl);
}

TestCEReactions* JSTestCEReactions::toWrapped(JSC::VM& vm, JSC::JSValue value)
{
    if (auto* wrapper = jsDynamicDowncast<JSTestCEReactions*>(vm, value))
        return &wrapper->wrapped();
    return nullptr;
}

}
