////////////////////////////////////////////////////////////////////////////
//
// Copyright 2021 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#pragma once

// This allow us to run the JSC tests on our Mac's locally.
#if __APPLE__
#include <JavaScriptCore/JavaScriptCore.h>
#endif

#include <iostream>
#include <vector>

#include "common/object/error_handling.hpp"
#include "common/object/interfaces.hpp"
#include "realm/object-store/shared_realm.hpp"

namespace realm {
namespace common {

struct PrivateStore {
    void *accessor_data = nullptr;
    ObjectObserver *observer = nullptr;
    IOCollection *collection = nullptr;
    std::function<void()> finalizer = nullptr;
    std::unordered_map<std::string, bool> keys;
};

struct JSCoreString{
    std::string _str;
    JSStringRef jsc_str;


    JSCoreString(std::string __str): _str{__str}{}
    JSCoreString(JSStringRef __str): jsc_str{__str}{}

    operator std::string(){
        std::string str;
        size_t sizeUTF8 = JSStringGetMaximumUTF8CStringSize(jsc_str);
        str.reserve(sizeUTF8);
        JSStringGetUTF8CString(jsc_str, _str.data(), sizeUTF8);
        return str;
    }

    operator JSStringRef(){
        return JSStringCreateWithUTF8CString(_str.c_str());
    }

    //TODO release JSString
    ~JSCoreString(){}
};


template <typename GetterSetter>
class JavascriptObject {
   private:
    JSClassDefinition _class;
    JSContextRef context;
    JSObjectRef object{nullptr};
    std::vector<JSStaticFunction> methods;
    std::vector<JSCoreString> accessors;
    PrivateStore *private_object;

    //TODO leak here use JSStringRelease
    static std::string to_string(JSContextRef context, JSStringRef value) {
        std::string str;
        size_t sizeUTF8 = JSStringGetMaximumUTF8CStringSize(value);
        str.reserve(sizeUTF8);
        JSStringGetUTF8CString(value, str.data(), sizeUTF8);
        return str;
    }

    static PrivateStore *get_private(JSObjectRef object) {
        PrivateStore *store =
            static_cast<PrivateStore *>(JSObjectGetPrivate(object));
        return store;
    }

    template <void cb(method::Arguments)>
    static JSValueRef function_call(JSContextRef ctx, JSObjectRef function,
                                    JSObjectRef thisObject,
                                    size_t argumentCount,
                                    const JSValueRef _arguments[],
                                    JSValueRef *exception) {
        PrivateStore *_private = get_private(thisObject);
        ObjectObserver *observer = _private->observer;
        IOCollection *collection = _private->collection;

        try {
            cb({ctx, observer, collection, argumentCount, _arguments});
        } catch (InvalidTransactionException &error) {
            *exception = _throw_error(ctx, error.what());
        }

        return JSValueMakeUndefined(ctx);
    }

    static void dispose(JSObjectRef object) {
        PrivateStore *_private = get_private(object);
        if (_private->finalizer != nullptr) {
            _private->finalizer();
        } else {
            std::cout << "Warning: No finalizer was specified.";
        }
    }

    static bool contains_key(JSContextRef ctx, JSObjectRef object, std::string key){
        auto keys = get_private(object)->keys;
        return keys[key.c_str()] == true;
    }

    static JSValueRef getter(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
    {
        IOCollection *collection = get_private(object)->collection;
        std::string key = to_string(ctx, propertyName);

        if(!contains_key(ctx,object, key)){
           return JSValueMakeNull(ctx);
        }

        GetterSetter gs{collection};
        return gs.get(accessor::Arguments{ctx,object, key.c_str(), 0,exception});
    }

    static bool setter(JSContextRef ctx, JSObjectRef object,
                     JSStringRef propertyName, JSValueRef value,
                     JSValueRef *exception) {
        std::string key = to_string(ctx, propertyName);
        std::cout << "key -> " << key.c_str();
        if(!contains_key(ctx,object, key)){
            std::cout << " ignore \n";
            return false;
        }
        std::cout << " ok \n";

        IOCollection *collection = get_private(object)->collection;
        GetterSetter gs{collection};
        gs.set(accessor::Arguments{ctx,object, key, value, exception});
        return true;
    }

    static bool has_property(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName){
        auto key = to_string(ctx, propertyName);
        return contains_key(ctx,object, key);
    }

    JSClassRef make_class() {
        methods.push_back({0});
        _class.staticFunctions = methods.data();

        return JSClassCreate(&_class);
    }

    JSObjectRef lazily_build_object(){
        if(object == nullptr){
            auto class_instance = make_class();
            return JSObjectMake(context, class_instance, private_object);
        }else{
            return object;
        }
    }

   public:
    JavascriptObject(JSContextRef _ctx, std::string name = "js_object")
        : context{_ctx} {
        _class = kJSClassDefinitionEmpty;
        _class.className = name.c_str();
        _class.finalize = dispose;
        _class.getProperty = getter;
        _class.setProperty = setter;
        _class.hasProperty = has_property;

        private_object = new PrivateStore{nullptr, nullptr, nullptr};
    }

    void dbg() {
        std::cout << "methods size: " << methods.size() << " \n";
        std::cout << "accessors size: " << accessors.size() << " \n";
    }

    template <class VM, void callback(method::Arguments)>
    void add_method(std::string&& name) {
        JSStaticFunction method_definition{
            name.c_str(),
            function_call<callback>,
            kJSPropertyAttributeDontEnum
        };

        methods.push_back(method_definition);
    }

    void add_key(std::string name) {
        private_object->keys[name.c_str()] = true;
   }

    void set_collection(IOCollection *collection) {
        private_object->collection = collection;
    }

    void set_observer(ObjectObserver *observer) {
        private_object->observer = observer;
    }

    JSObjectRef get_object() {
        object = lazily_build_object();
        return object;
    }

    template <typename RemovalCallback>
    static void finalize(JSObjectRef object, RemovalCallback &&callback,
                         void *_unused = nullptr) {
        /*
         *  JSObject and Self only apply for NodeJS.
         */
        PrivateStore *store = get_private(object);
        store->finalizer = std::move(callback);
        JSObjectSetPrivate(object, store);
    }
};

}  // namespace common
}  // namespace realm
