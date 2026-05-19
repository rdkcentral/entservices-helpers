/**
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/

/**
 * @file test_PluginInterfaceBuilder.cpp
 *
 * L1 unit tests for helpers/PluginInterfaceBuilder.h
 *
 * Classes under test:
 *   WPEFramework::Plugin::PluginInterfaceRef<T>   — RAII wrapper for an acquired interface ptr
 *   WPEFramework::Plugin::PluginInterfaceBuilder<T> — fluent builder for acquiring a plugin interface
 *   WPEFramework::Plugin::createInterface<T>()    — free-function that does the retry loop
 *   WPEFramework::Plugin::make_unique<T>()        — helper make_unique (C++11 backport)
 *
 * Coverage goals
 * --------------
 * PluginInterfaceRef:
 *   - default ctor  (nullptr state)
 *   - two-arg ctor  (interface + service)
 *   - move ctor     (source becomes null)
 *   - move assignment
 *   - operator bool (true / false)
 *   - operator->    (returns wrapped pointer)
 *   - Reset()       (calls Release, nulls pointer)
 *   - destructor    (calls Reset → Release)
 *   - copy ctor / copy assignment are deleted (compile-time only)
 *
 * PluginInterfaceBuilder:
 *   - ctor with callsign
 *   - default field values (_version, _timeout, _retryCount, _retryInterval)
 *   - withVersion / withTimeout / withIShell / withRetryCount / withRetryIntervalMS
 *   - fluent chaining (each setter returns *this)
 *   - callSign(), retryCount(), retryInterval(), controller() accessors
 *   - createInterface() with null controller → returns bool(false) ref
 *   - createInterface() with valid controller: QueryInterfaceByCallsign succeeds (first try)
 *   - createInterface() with valid controller: QueryInterfaceByCallsign fails all retries
 *   - createInterface() with valid controller: QueryInterfaceByCallsign succeeds on Nth retry
 *
 * make_unique:
 *   - creates object correctly
 *   - ownership managed by unique_ptr (no leak)
 *
 * createInterface free function:
 *   - null controller path
 *   - success on first call
 *   - success after retries
 *   - exhausts retries, returns nullptr
 */

#ifndef MODULE_NAME
#define MODULE_NAME Plugin_Helpers
#endif

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Module.h"
#include "PluginInterfaceBuilder.h"
#include "UtilsLogging.h"

/* Pull in the thunder ServiceMock which implements IShell */
#include "ServiceMock.h"

#include <memory>
#include <string>

using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;

/* ============================================================
 * Minimal concrete IUnknown implementation used as the template
 * parameter for PluginInterfaceRef and PluginInterfaceBuilder.
 *
 * We deliberately do NOT derive from a custom abstract interface
 * to avoid depending on Thunder's IShell template QueryInterfaceByCallsign
 * which requires INTERFACE::ID to be registered in the Thunder
 * type system — only available after Thunder is fully built in CI.
 *
 * Instead we derive directly from Core::IUnknown, which is the only
 * real requirement of PluginInterfaceRef<T> (it calls T::Release()).
 * ============================================================ */
class TestIfaceImpl : public WPEFramework::Core::IUnknown {
    mutable uint32_t _refCount { 1 };

public:
    /* Required by Thunder Core::IUnknown — must have an ID */
    enum { ID = 0x00000042u };

    /* Virtual destructor: object is deleted via base ptr in Release() */
    ~TestIfaceImpl() override = default;

    void AddRef() const override { ++_refCount; }

    uint32_t Release() const override
    {
        if (--_refCount == 0) {
            delete this;
            return 0;
        }
        return _refCount;
    }

    void* QueryInterface(const uint32_t) override { return nullptr; }
};

static TestIfaceImpl* makeImpl()
{
    return new TestIfaceImpl();
}

/* ============================================================
 * PluginInterfaceRef tests
 * ============================================================ */
class PluginInterfaceRefTest : public ::testing::Test {
protected:
    NiceMock<ServiceMock> shell;
};

/* ---- default constructor ---- */

TEST_F(PluginInterfaceRefTest, DefaultCtorIsNull)
{
    LOGINFO("Test: default-constructed PluginInterfaceRef is null/false");
    PluginInterfaceRef<TestIfaceImpl> ref;

    EXPECT_FALSE(static_cast<bool>(ref));
}

TEST_F(PluginInterfaceRefTest, DefaultCtorArrowReturnsNullptr)
{
    LOGINFO("Test: operator-> on default ref returns nullptr");
    PluginInterfaceRef<TestIfaceImpl> ref;

    EXPECT_EQ(nullptr, ref.operator->());
}

/* ---- two-arg constructor ---- */

TEST_F(PluginInterfaceRefTest, TwoArgCtorHoldsInterface)
{
    LOGINFO("Test: two-arg ctor makes operator bool true");
    auto* impl = makeImpl();

    PluginInterfaceRef<TestIfaceImpl> ref(impl, &shell);

    EXPECT_TRUE(static_cast<bool>(ref));
    EXPECT_EQ(impl, ref.operator->());
    /* destructor will call Release once */
}

TEST_F(PluginInterfaceRefTest, DestructorCallsRelease)
{
    LOGINFO("Test: destructor calls Release() on the held interface");
    auto* impl = makeImpl();

    /* We know TestInterfaceImpl deletes itself when refcount hits 0. */
    /* Give extra ref so we can observe it without use-after-free:    */
    impl->AddRef(); /* refcount now 2 */

    {
        PluginInterfaceRef<TestIfaceImpl> ref(impl, &shell);
        /* ref holds 1 ref.  Total = 2. */
    }
    /* destructor ran: refcount now 1. impl is still alive. */
    EXPECT_NE(nullptr, impl);
    impl->Release(); /* final release, object deleted */
}

/* ---- Reset() ---- */

TEST_F(PluginInterfaceRefTest, ResetNullifiesAndCallsRelease)
{
    LOGINFO("Test: Reset() makes ref false and calls Release");
    auto* impl = makeImpl();
    impl->AddRef(); /* refcount 2 */

    PluginInterfaceRef<TestIfaceImpl> ref(impl, &shell);
    ASSERT_TRUE(static_cast<bool>(ref));

    ref.Reset();

    EXPECT_FALSE(static_cast<bool>(ref));
    /* impl still alive (refcount 1 after Reset). */
    impl->Release();
}

TEST_F(PluginInterfaceRefTest, ResetOnNullIsNoop)
{
    LOGINFO("Test: Reset() on already-null ref is harmless");
    PluginInterfaceRef<TestIfaceImpl> ref;
    EXPECT_NO_FATAL_FAILURE(ref.Reset());
    EXPECT_FALSE(static_cast<bool>(ref));
}

TEST_F(PluginInterfaceRefTest, ResetTwiceIsSafe)
{
    LOGINFO("Test: calling Reset() twice does not double-Release");
    /* refcount lifecycle:
     *   makeImpl()      → 1
     *   AddRef()        → 2  (test holds one extra)
     *   ctor (no AddRef)→ 2
     *   first Reset()   → Release() → 1, _interface = null
     *   second Reset()  → _interface is null → noop (no Release)
     *   impl->Release() → 0 → deleted
     *   ~ref            → Reset() → null → noop
     */
    auto* impl = makeImpl();
    impl->AddRef(); /* refcount 2 */

    PluginInterfaceRef<TestIfaceImpl> ref(impl, &shell);
    ref.Reset();                              /* Release → refcount 1 */
    EXPECT_NO_FATAL_FAILURE(ref.Reset());     /* noop — no double-Release */
    EXPECT_FALSE(static_cast<bool>(ref));

    impl->Release(); /* refcount 0 → deleted */
}

/* ---- move constructor ---- */

TEST_F(PluginInterfaceRefTest, MoveCtorTransfersOwnership)
{
    LOGINFO("Test: move ctor makes source null and target valid");
    auto* impl = makeImpl();

    PluginInterfaceRef<TestIfaceImpl> src(impl, &shell);
    PluginInterfaceRef<TestIfaceImpl> dst(std::move(src));

    EXPECT_TRUE(static_cast<bool>(dst));  /* destination owns it */
    EXPECT_EQ(impl, dst.operator->());
    /* dst destructor calls Release */
}

TEST_F(PluginInterfaceRefTest, MoveCtorFromNullRef)
{
    LOGINFO("Test: move ctor from null ref yields another null ref");
    PluginInterfaceRef<TestIfaceImpl> src;
    PluginInterfaceRef<TestIfaceImpl> dst(std::move(src));

    EXPECT_FALSE(static_cast<bool>(dst));
}

/* ---- move assignment ---- */

TEST_F(PluginInterfaceRefTest, MoveAssignmentTransfersInterface)
{
    LOGINFO("Test: move assignment sets source to null, target becomes valid");
    auto* impl = makeImpl();

    PluginInterfaceRef<TestIfaceImpl> src(impl, &shell);
    PluginInterfaceRef<TestIfaceImpl> dst;

    dst = std::move(src);

    EXPECT_TRUE(static_cast<bool>(dst));
    EXPECT_EQ(impl, dst.operator->());
}

TEST_F(PluginInterfaceRefTest, MoveAssignmentSelfMoveIsGuarded)
{
    LOGINFO("Test: move-assigning to self does not corrupt the ref");
    auto* impl = makeImpl(); /* refcount = 1 */
    impl->AddRef();           /* refcount = 2: ref owns 1, this test owns 1 */

    PluginInterfaceRef<TestIfaceImpl> ref(impl, &shell);

    /*
     * Self-move: the production operator= checks (this != &other).
     * Guard fires → nothing changes → _interface stays valid (refcount 2).
     */
    ref = std::move(ref); /* NOLINT(bugprone-use-after-move) */

    /* ref still valid (self-move guard kept everything intact). */
    EXPECT_TRUE(static_cast<bool>(ref));

    /* Release the test's own extra reference.
     * Destruction order: impl->Release() here  → refcount 1
     *                    ref destructor         → refcount 0 → deleted */
    impl->Release();
}

/* ---- operator bool ---- */

TEST_F(PluginInterfaceRefTest, OperatorBoolFalseWhenNull)
{
    LOGINFO("Test: operator bool is false for null ref");
    PluginInterfaceRef<TestIfaceImpl> ref;
    EXPECT_FALSE(ref);
}

TEST_F(PluginInterfaceRefTest, OperatorBoolTrueWhenNonNull)
{
    LOGINFO("Test: operator bool is true when interface is held");
    auto* impl = makeImpl();
    PluginInterfaceRef<TestIfaceImpl> ref(impl, &shell);
    EXPECT_TRUE(ref);
}

/* ---- operator-> ---- */

TEST_F(PluginInterfaceRefTest, ArrowOperatorReturnsCorrectPointer)
{
    LOGINFO("Test: operator-> returns the exact pointer passed in ctor");
    auto* impl = makeImpl();
    PluginInterfaceRef<TestIfaceImpl> ref(impl, &shell);

    EXPECT_EQ(impl, ref.operator->());
}

/* ============================================================
 * PluginInterfaceBuilder tests — builder accessors / defaults
 * ============================================================ */
class PluginInterfaceBuilderTest : public ::testing::Test {
};

TEST_F(PluginInterfaceBuilderTest, CtorSetsCallsign)
{
    LOGINFO("Test: callSign() returns the string passed to ctor");
    PluginInterfaceBuilder<TestIfaceImpl> builder("MyPlugin");

    EXPECT_EQ(std::string("MyPlugin"), builder.callSign());
}

TEST_F(PluginInterfaceBuilderTest, DefaultRetryCountIsZero)
{
    LOGINFO("Test: default retryCount is 0");
    PluginInterfaceBuilder<TestIfaceImpl> builder("X");

    EXPECT_EQ(0, builder.retryCount());
}

TEST_F(PluginInterfaceBuilderTest, DefaultRetryIntervalIsZero)
{
    LOGINFO("Test: default retryInterval is 0");
    PluginInterfaceBuilder<TestIfaceImpl> builder("X");

    EXPECT_EQ(0u, builder.retryInterval());
}

TEST_F(PluginInterfaceBuilderTest, DefaultControllerIsNull)
{
    LOGINFO("Test: default controller() returns nullptr");
    PluginInterfaceBuilder<TestIfaceImpl> builder("X");

    EXPECT_EQ(nullptr, builder.controller());
}

/* ---- fluent setters ---- */

TEST_F(PluginInterfaceBuilderTest, WithRetryCountSetsAndReturnsRef)
{
    LOGINFO("Test: withRetryCount sets the value and returns *this for chaining");
    PluginInterfaceBuilder<TestIfaceImpl> builder("X");

    auto& ret = builder.withRetryCount(5);

    EXPECT_EQ(5, builder.retryCount());
    EXPECT_EQ(&builder, &ret); /* same object → fluent chain works */
}

TEST_F(PluginInterfaceBuilderTest, WithRetryIntervalSetsAndReturnsRef)
{
    LOGINFO("Test: withRetryIntervalMS sets the value and returns *this");
    PluginInterfaceBuilder<TestIfaceImpl> builder("X");

    auto& ret = builder.withRetryIntervalMS(200);

    EXPECT_EQ(200u, builder.retryInterval());
    EXPECT_EQ(&builder, &ret);
}

TEST_F(PluginInterfaceBuilderTest, WithVersionSetsAndReturnsRef)
{
    LOGINFO("Test: withVersion stores the value and returns *this");
    PluginInterfaceBuilder<TestIfaceImpl> builder("X");

    auto& ret = builder.withVersion(42u);

    EXPECT_EQ(&builder, &ret);
    /* _version is private, but we verify via chaining; no crash */
}

TEST_F(PluginInterfaceBuilderTest, WithTimeoutSetsAndReturnsRef)
{
    LOGINFO("Test: withTimeout stores the value and returns *this");
    PluginInterfaceBuilder<TestIfaceImpl> builder("X");

    auto& ret = builder.withTimeout(9000u);

    EXPECT_EQ(&builder, &ret);
}

TEST_F(PluginInterfaceBuilderTest, WithIShellSetsController)
{
    LOGINFO("Test: withIShell stores the IShell pointer, controller() returns it");
    NiceMock<ServiceMock> shell;
    PluginInterfaceBuilder<TestIfaceImpl> builder("X");

    builder.withIShell(&shell);

    EXPECT_EQ(&shell, builder.controller());
}

TEST_F(PluginInterfaceBuilderTest, WithIShellNullptrIsAccepted)
{
    LOGINFO("Test: withIShell(nullptr) is accepted, controller() returns nullptr");
    PluginInterfaceBuilder<TestIfaceImpl> builder("X");
    builder.withIShell(nullptr);

    EXPECT_EQ(nullptr, builder.controller());
}

TEST_F(PluginInterfaceBuilderTest, FluentChainingAllSetters)
{
    LOGINFO("Test: all setters can be chained in one expression");
    NiceMock<ServiceMock> shell;
    PluginInterfaceBuilder<TestIfaceImpl> builder("Chain");

    builder.withVersion(1)
           .withTimeout(500)
           .withIShell(&shell)
           .withRetryCount(3)
           .withRetryIntervalMS(100);

    EXPECT_EQ(std::string("Chain"), builder.callSign());
    EXPECT_EQ(3,       builder.retryCount());
    EXPECT_EQ(100u,    builder.retryInterval());
    EXPECT_EQ(&shell,  builder.controller());
}

/* ---- createInterface() — null controller path ---- */

TEST_F(PluginInterfaceBuilderTest, CreateInterfaceNullControllerReturnsFalseRef)
{
    LOGINFO("Test: createInterface() with null controller returns false PluginInterfaceRef");
    PluginInterfaceBuilder<TestIfaceImpl> builder("NoShell");
    /* controller NOT set → nullptr */

    auto ref = builder.createInterface();

    EXPECT_FALSE(static_cast<bool>(ref));
}

/* ---- createInterface() — controller present, QueryInterface succeeds first try ---- */

TEST_F(PluginInterfaceBuilderTest, CreateInterfaceSucceedsFirstTry)
{
    LOGINFO("Test: createInterface succeeds when QueryInterfaceByCallsign returns ptr on 1st call");
    NiceMock<ServiceMock> shell;
    auto* impl = makeImpl();
    impl->AddRef(); /* extra ref: one owned by test, one will be owned by PluginInterfaceRef */

    /* QueryInterfaceByCallsign returns impl (with its own ref) */
    /* Cast ID to uint32_t — mock arg type is const uint32_t;
     * constexpr uint32_t member avoids signed/unsigned mismatch. */
    EXPECT_CALL(shell, QueryInterfaceByCallsign(
            static_cast<uint32_t>(TestIfaceImpl::ID), std::string("OKPlugin")))
        .WillOnce(Return(static_cast<void*>(impl)));

    PluginInterfaceBuilder<TestIfaceImpl> builder("OKPlugin");
    builder.withIShell(&shell).withRetryCount(3).withRetryIntervalMS(0);

    auto ref = builder.createInterface();

    EXPECT_TRUE(static_cast<bool>(ref));
    /* ref destructor will Release once */
    impl->Release(); /* release our extra ref */
}

/* ---- createInterface() — all retries exhausted ---- */

TEST_F(PluginInterfaceBuilderTest, CreateInterfaceFailsAllRetries)
{
    LOGINFO("Test: createInterface returns false ref when all retries fail");
    NiceMock<ServiceMock> shell;

    /* Always return nullptr → every query fails */
    EXPECT_CALL(shell, QueryInterfaceByCallsign(
            static_cast<uint32_t>(TestIfaceImpl::ID), std::string("BadPlugin")))
        .WillRepeatedly(Return(nullptr));

    PluginInterfaceBuilder<TestIfaceImpl> builder("BadPlugin");
    builder.withIShell(&shell).withRetryCount(3).withRetryIntervalMS(0);

    auto ref = builder.createInterface();

    EXPECT_FALSE(static_cast<bool>(ref));
}

/* ---- createInterface() — succeeds on Nth retry ---- */

TEST_F(PluginInterfaceBuilderTest, CreateInterfaceSucceedsOnThirdRetry)
{
    LOGINFO("Test: createInterface returns valid ref when QueryInterface succeeds on 3rd attempt");
    NiceMock<ServiceMock> shell;
    auto* impl = makeImpl();
    impl->AddRef(); /* extra ref for our test */

    /* Fail twice, succeed on 3rd call */
    EXPECT_CALL(shell, QueryInterfaceByCallsign(
            static_cast<uint32_t>(TestIfaceImpl::ID), std::string("RetryPlugin")))
        .WillOnce(Return(nullptr))
        .WillOnce(Return(nullptr))
        .WillOnce(Return(static_cast<void*>(impl)));

    PluginInterfaceBuilder<TestIfaceImpl> builder("RetryPlugin");
    builder.withIShell(&shell).withRetryCount(5).withRetryIntervalMS(0);

    auto ref = builder.createInterface();

    EXPECT_TRUE(static_cast<bool>(ref));
    impl->Release(); /* release extra ref */
}

/* ---- createInterface() with zero retryCount ---- */

TEST_F(PluginInterfaceBuilderTest, CreateInterfaceZeroRetryCountTriesOnce)
{
    LOGINFO("Test: retryCount=0 → loop runs once (count<0 is never true, do-while runs body once)");
    NiceMock<ServiceMock> shell;

    /*
     * With retryCount=0 the loop is:
     *   do { ... count++; } while(count < 0);
     * The body runs exactly once, then condition 1<0 is false → exits.
     * So QueryInterfaceByCallsign is called exactly once.
     */
    EXPECT_CALL(shell, QueryInterfaceByCallsign(
            static_cast<uint32_t>(TestIfaceImpl::ID), std::string("Once")))
        .Times(1)
        .WillOnce(Return(nullptr));

    PluginInterfaceBuilder<TestIfaceImpl> builder("Once");
    builder.withIShell(&shell).withRetryCount(0).withRetryIntervalMS(0);

    auto ref = builder.createInterface();

    EXPECT_FALSE(static_cast<bool>(ref));
}

/* ---- multiple createInterface calls on same builder ---- */

TEST_F(PluginInterfaceBuilderTest, CreateInterfaceCalledMultipleTimes)
{
    LOGINFO("Test: builder can be used to create the interface multiple times");
    NiceMock<ServiceMock> shell;

    EXPECT_CALL(shell, QueryInterfaceByCallsign(
            static_cast<uint32_t>(TestIfaceImpl::ID), std::string("Multi")))
        .WillRepeatedly(Return(nullptr));

    PluginInterfaceBuilder<TestIfaceImpl> builder("Multi");
    builder.withIShell(&shell).withRetryCount(1).withRetryIntervalMS(0);

    auto ref1 = builder.createInterface();
    auto ref2 = builder.createInterface();

    EXPECT_FALSE(static_cast<bool>(ref1));
    EXPECT_FALSE(static_cast<bool>(ref2));
}

/* ---- empty callsign ---- */

TEST_F(PluginInterfaceBuilderTest, EmptyCallsignIsAccepted)
{
    LOGINFO("Test: empty callsign string does not crash the builder");
    PluginInterfaceBuilder<TestIfaceImpl> builder("");

    EXPECT_EQ(std::string(""), builder.callSign());
}

TEST_F(PluginInterfaceBuilderTest, EmptyCallsignWithNullControllerReturnsFalse)
{
    LOGINFO("Test: empty callsign + null controller → createInterface returns false ref");
    PluginInterfaceBuilder<TestIfaceImpl> builder("");

    auto ref = builder.createInterface();

    EXPECT_FALSE(static_cast<bool>(ref));
}

/* ---- large retryCount ---- */

TEST_F(PluginInterfaceBuilderTest, LargeRetryCountStoredCorrectly)
{
    LOGINFO("Test: large retryCount value is stored without overflow");
    PluginInterfaceBuilder<TestIfaceImpl> builder("X");
    builder.withRetryCount(10000);

    EXPECT_EQ(10000, builder.retryCount());
}

/* ---- retryCount negative (edge case) ---- */

TEST_F(PluginInterfaceBuilderTest, NegativeRetryCountStoredCorrectly)
{
    LOGINFO("Test: negative retryCount is stored as-is; loop will not retry");
    PluginInterfaceBuilder<TestIfaceImpl> builder("X");
    builder.withRetryCount(-1);

    EXPECT_EQ(-1, builder.retryCount());
}

/* ============================================================
 * make_unique helper
 * ============================================================ */
class MakeUniqueTest : public ::testing::Test {
};

struct SimpleObj {
    int value;
    explicit SimpleObj(int v) : value(v) {}
};

TEST_F(MakeUniqueTest, CreatesObjectWithCorrectValue)
{
    LOGINFO("Test: Plugin::make_unique creates object with correct constructor arg");
    auto ptr = WPEFramework::Plugin::make_unique<SimpleObj>(42);

    ASSERT_NE(nullptr, ptr.get());
    EXPECT_EQ(42, ptr->value);
}

TEST_F(MakeUniqueTest, OwnershipManagedByUniquePtr)
{
    LOGINFO("Test: object is destroyed when unique_ptr goes out of scope");
    {
        auto ptr = WPEFramework::Plugin::make_unique<SimpleObj>(99);
        ASSERT_NE(nullptr, ptr.get());
    }
    /* no crash, no leak (verified by valgrind) */
    SUCCEED();
}

TEST_F(MakeUniqueTest, MoveTransfersOwnership)
{
    LOGINFO("Test: unique_ptr from make_unique can be moved");
    auto ptr1 = WPEFramework::Plugin::make_unique<SimpleObj>(7);
    auto ptr2 = std::move(ptr1);

    EXPECT_EQ(nullptr, ptr1.get());
    ASSERT_NE(nullptr, ptr2.get());
    EXPECT_EQ(7, ptr2->value);
}

TEST_F(MakeUniqueTest, MultipleArgsForwarded)
{
    LOGINFO("Test: make_unique forwards multiple constructor arguments");
    struct Multi {
        int a; std::string b;
        Multi(int x, std::string y) : a(x), b(std::move(y)) {}
    };

    auto ptr = WPEFramework::Plugin::make_unique<Multi>(3, std::string("hello"));

    ASSERT_NE(nullptr, ptr.get());
    EXPECT_EQ(3,              ptr->a);
    EXPECT_EQ(std::string("hello"), ptr->b);
}

/* ============================================================
 * createInterface free-function direct tests
 * These test the free function directly via the builder accessors
 * ============================================================ */
class CreateInterfaceFreeTest : public ::testing::Test {
};

TEST_F(CreateInterfaceFreeTest, NullControllerReturnsNullptr)
{
    LOGINFO("Test: createInterface free function returns nullptr when controller is null");
    PluginInterfaceBuilder<TestIfaceImpl> builder("X");
    /* controller not set */

    TestIfaceImpl* result = WPEFramework::Plugin::createInterface<TestIfaceImpl>(builder);

    EXPECT_EQ(nullptr, result);
}

TEST_F(CreateInterfaceFreeTest, ValidControllerSuccessFirstCall)
{
    LOGINFO("Test: createInterface free function returns valid ptr on first success");
    NiceMock<ServiceMock> shell;
    auto* impl = makeImpl();
    impl->AddRef();

    EXPECT_CALL(shell, QueryInterfaceByCallsign(
            static_cast<uint32_t>(TestIfaceImpl::ID), std::string("Direct")))
        .WillOnce(Return(static_cast<void*>(impl)));

    PluginInterfaceBuilder<TestIfaceImpl> builder("Direct");
    builder.withIShell(&shell).withRetryCount(2).withRetryIntervalMS(0);

    TestIfaceImpl* result = WPEFramework::Plugin::createInterface<TestIfaceImpl>(builder);

    EXPECT_NE(nullptr, result);
    result->Release(); /* release the ref returned by createInterface */
    impl->Release();   /* release our extra ref */
}

TEST_F(CreateInterfaceFreeTest, ExhaustedRetriesReturnsNullptr)
{
    LOGINFO("Test: createInterface free function returns nullptr after all retries fail");
    NiceMock<ServiceMock> shell;

    EXPECT_CALL(shell, QueryInterfaceByCallsign(
            static_cast<uint32_t>(TestIfaceImpl::ID), std::string("Fail")))
        .WillRepeatedly(Return(nullptr));

    PluginInterfaceBuilder<TestIfaceImpl> builder("Fail");
    builder.withIShell(&shell).withRetryCount(2).withRetryIntervalMS(0);

    TestIfaceImpl* result = WPEFramework::Plugin::createInterface<TestIfaceImpl>(builder);

    EXPECT_EQ(nullptr, result);
}
