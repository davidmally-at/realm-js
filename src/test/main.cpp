#define CATCH_CONFIG_MAIN
#include <vector>

#include "catch_amalgamated.hpp"
#include "common/object/interfaces.hpp"
#include "logger.hpp"
#include "test_bed.hpp"

using Catch::Matchers::Contains;
using namespace std;
using namespace realm;


struct MockedCollection: public IOCollection{
    double N = 1000;
    MockedCollection(double start): N{start} {}
    realm::Mixed get(std::string) override{
        return realm::Mixed(N);
    }

    void set(std::string key, realm::Mixed val) override{
        N = val.get_double();
    }

    void remove(std::string key) override {
        N = 0;
    }
};

struct MockedGetterSetter {
    IOCollection *collection{nullptr};

    MockedGetterSetter(IOCollection *_collection): collection{_collection}{}

    void set(accessor::Arguments args) {
        printf("args->property_name: %s \n", args.property_name.c_str());
        std::cout << "property name: " << args.property_name << "\n";
        double N = JSValueToNumber(args.context, args.value, nullptr);
        collection->set("N", realm::Mixed(N));
    }

    JSValueRef get(accessor::Arguments args) {
        return JSValueMakeNumber(args.context, collection->get(args.property_name).get_double());
    }

    ~MockedGetterSetter(){
    }
};

struct TNull : public ObjectObserver {
    IOCollection* get_collection() { return nullptr; }
};

struct T1 : public ObjectObserver {
    int call_count = 0;
    void subscribe(Subscriber*) { call_count++; }

    void remove_subscription(const Subscriber*) {
        call_count++;
        // Making Sure that unsubscribe_all & subscribe has been successfully
        // invoked.
        REQUIRE(call_count == 3);
    }
    void unsubscribe_all() { call_count++; }

    static void test_for_null_data_method(method::Arguments arguments) {
        SECTION(
            "This callback should have null values for observer and "
            "collection.") {
            REQUIRE(true == JSValueIsBoolean(arguments.context, arguments.get(0)));
            REQUIRE(arguments.collection == nullptr);
            REQUIRE(arguments.observer == nullptr);
        }
    }

    static void removeTest(method::Arguments args){

    }

    static void methods(method::Arguments args) {
        SECTION(
            "This callback should have non-null values for observer and "
            "collection.") {

            auto context = args.context;

            REQUIRE(args.collection != nullptr);
            REQUIRE(args.observer != nullptr);

            args.observer->subscribe(nullptr);
            args.observer->unsubscribe_all();
            args.observer->remove_subscription(nullptr);

            double n = JSValueToNumber(context, args.get(0), nullptr);
            args.collection->set("test", n);
            realm::Mixed _num = args.collection->get("test");

            /*
             * jsc_object line 11
             * dictionary.doSomething(28850);
             * we test here that we successfully read the argument.
             *
             */
            REQUIRE(_num.get_double() == 28850);
        }
    }
};

TEST_CASE("Testing Logger#get_level") {
    REQUIRE(realm::common::logger::Logger::get_level("all") ==
            realm::common::logger::LoggerLevel::all);
    REQUIRE(realm::common::logger::Logger::get_level("debug") ==
            realm::common::logger::LoggerLevel::debug);
    REQUIRE_THROWS_WITH(realm::common::logger::Logger::get_level("coffeebabe"),
                        "Bad log level");
}

JSValueRef Test(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                size_t argumentCount, const JSValueRef arguments[],
                JSValueRef* exception) {
    SECTION("The Object should contain accessor *X* and method *doSomething*") {
        REQUIRE(JSValueIsBoolean(ctx, arguments[0]) == true);
        bool val = JSValueToBoolean(ctx, arguments[0]);
        REQUIRE(val == true);
    }

    return JSValueMakeUndefined(ctx);
}

/*
    test_accessor(obj, key, number)
    example:

    test_accessor(dictionary, 'X', 666)  // Will look for the field X and 666.
 */
JSValueRef GetterSetter(JSContextRef ctx, JSObjectRef function,
                        JSObjectRef thisObject, size_t argumentCount,
                        const JSValueRef arguments[], JSValueRef* exception) {
    SECTION("Testing accessors I/O for input X") {
        auto accessor_name = JSC_VM::s("X");
        REQUIRE(true == JSValueIsObject(ctx, arguments[0]));

        auto obj = (JSObjectRef)arguments[0];
        REQUIRE(true ==
                JSObjectHasProperty(ctx, obj, (JSStringRef)arguments[1]));

        JSValueRef v = JSObjectGetProperty(ctx, obj, accessor_name, NULL);
        REQUIRE(true == JSValueIsNumber(ctx, v));
        double _v = JSValueToNumber(ctx, v, NULL);
        double _match = JSValueToNumber(ctx, arguments[2], NULL);
        REQUIRE(_match == _v);
    }

    return JSValueMakeUndefined(ctx);
}

TEST_CASE("Testing Object creation on JavascriptCore.") {
    JSC_VM jsc_vm;

    jsc_vm.make_gbl_fn("assert_true", &Test);
    jsc_vm.make_gbl_fn("test_accessor", &GetterSetter);

    /*
     *  JavascriptObject Instantiation and configuration into JSC.
     *  With null_dictionary is just a Javascript object without a private C++
     * object.
     */
    common::JavascriptObject<MockedGetterSetter>* null_dict =
        new common::JavascriptObject<MockedGetterSetter>{jsc_vm.globalContext};

    TNull* tnull = nullptr;
    null_dict->template add_method<int, T1::test_for_null_data_method>("hello");
    null_dict->template add_method<int, T1::test_for_null_data_method>("alo");
    null_dict->set_observer(tnull);
    JSObjectRef null_dict_js_object = null_dict->get_object();

    common::JavascriptObject<MockedGetterSetter>::finalize(null_dict_js_object, [=]() {
        /*
         *  Private object should be deallocated just once.
         */

        REQUIRE(null_dict != nullptr);
        delete null_dict;
    });

    // Adds object to the JS global scope. This way we can call the functions
    // from the VM like this null_dictionary.hello() null_dictionary.alo() for
    // more information look at the jsc_object.js
    jsc_vm.set_obj_prop("null_dictionary", null_dict_js_object);

    /*
     *  Javascript object with private C++ object.
     *  To provide a private object we just need to pass a C++ object that has a
     * IOCollection* get_collection() method and/or ObjectSubscriber.
     */
    common::JavascriptObject<MockedGetterSetter>* _dict =
        new common::JavascriptObject<MockedGetterSetter>{jsc_vm.globalContext};
    _dict->template add_method<int, T1::methods>("doSomething");
    _dict->add_key("X");
    _dict->add_key("A");
    _dict->add_key("B");
    _dict->add_key("C");

    _dict->set_collection(new MockedCollection(666));
    _dict->set_observer(new T1);

    auto dict_js_object = _dict->get_object();
    common::JavascriptObject<MockedGetterSetter>::finalize(dict_js_object, [=]() {
        /*
         *  Private object should be deallocated just once.
         */
        REQUIRE(_dict != nullptr);
        delete _dict;
    });

    // Adds object to the JS global scope.
    jsc_vm.set_obj_prop("dictionary", dict_js_object);

    /*
     *
     *  Running a script on the VM.
     *
     *  First we check the object with properties and methods are constructed
     *
     *   test(dictionary)
     *
     *  To test that we added hello method we send a boolean and we check it
     *  above using T1 struct.
     *
     *  dictionary.hello(true)
     *
     */
    jsc_vm.load_into_vm("./jsc_object.js");
}
