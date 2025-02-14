// https://github.com/kunitoki/LuaBridge3
// Copyright 2019, Dmitry Tarakanov
// SPDX-License-Identifier: MIT

#include "TestBase.h"

#include "LuaBridge/Map.h"

#include <map>

namespace {
struct Unregistered {
    bool operator<(const Unregistered& other) const {
        return true;
    }
};

struct Data
{
    /* explicit */ Data(int i) : i(i) {}

    int i;
};

bool operator==(const Data& lhs, const Data& rhs)
{
    return lhs.i == rhs.i;
}

bool operator<(const Data& lhs, const Data& rhs)
{
    return lhs.i < rhs.i;
}

std::ostream& operator<<(std::ostream& lhs, const Data& rhs)
{
    lhs << "{" << rhs.i << "}";
    return lhs;
}

std::map<Data, Data> processValues(const std::map<Data, Data>& data)
{
    return data;
}

std::map<Data, Data> processPointers(const std::map<Data, const Data*>& data)
{
    std::map<Data, Data> result;

    for (const auto& item : data)
        result.emplace(item.first, *item.second);

    return result;
}
} // namespace

namespace std {
template <>
struct hash<Unregistered>
{
    std::size_t operator()(const Unregistered& value) const
    {
        return 0;
    }
};
} // namespace std

struct MapTests : TestBase
{
};

TEST_F(MapTests, LuaRef)
{
    {
        using Map = std::map<int, char>;

        const Map expected { {1, 'a'}, {2, 'b'}, {3, 'c'} };

        runLua("result = {'a', 'b', 'c'}");

        Map actual = result();
        EXPECT_EQ(expected, actual);
        EXPECT_EQ(expected, result<Map>());
    }

    {
        using Map = std::map<int, std::string>;
        
        const Map expected { {1, "abcdef"}, {2, "bcdef"}, {3, "cdef"} };

        runLua("result = {'abcdef', 'bcdef', 'cdef'}");

        Map actual = result();
        EXPECT_EQ(expected, actual);
        EXPECT_EQ(expected, result<Map>());
    }

    {
        using Map = std::map<luabridge::LuaRef, luabridge::LuaRef>;

        const Map expected {
            { luabridge::LuaRef(L, false), luabridge::LuaRef(L, true) },
            { luabridge::LuaRef(L, 'a'), luabridge::LuaRef(L, "abc") },
            { luabridge::LuaRef(L, 1), luabridge::LuaRef(L, 5) },
            { luabridge::LuaRef(L, 3.14), luabridge::LuaRef(L, -1.1) },
        };

        runLua("result = {[false] = true, a = 'abc', [1] = 5, [3.14] = -1.1}");

        auto resultRef = result();
        EXPECT_TRUE(resultRef.isInstance<Map>());

        Map actual = resultRef;
        EXPECT_EQ(expected, actual);

        EXPECT_EQ(expected, result<Map>());
    }
}

TEST_F(MapTests, CastToMap)
{
    using StrToInt = std::map<std::string, int>;
    runLua("result = {[1] = 2, a = 3}");
    ASSERT_EQ((StrToInt{{"1", 2}, {"a", 3}}), result().cast<StrToInt>());

    using IntToInt = std::map<int, int>;
    runLua("result = {[1] = 2, a = 3}");

#if LUABRIDGE_HAS_EXCEPTIONS
    ASSERT_THROW((result().cast<IntToInt>()), std::exception);
#else
    ASSERT_DEATH((result().cast<IntToInt>()), "");
#endif
}

TEST_F(MapTests, PassToFunction)
{
    runLua("function foo (map) "
           "  result = map "
           "end");

    auto foo = luabridge::getGlobal(L, "foo");
    using Int2Bool = std::map<int, bool>;

    resetResult();

    Int2Bool lvalue{{10, false}, {20, true}, {30, true}};
    foo(lvalue);
    ASSERT_TRUE(result().isTable());
    ASSERT_EQ(lvalue, result<Int2Bool>());

    resetResult();

    const Int2Bool constLvalue = lvalue;
    foo(constLvalue);
    ASSERT_TRUE(result().isTable());
    ASSERT_EQ(constLvalue, result<Int2Bool>());
}

TEST_F(MapTests, PassFromLua)
{
    luabridge::getGlobalNamespace(L)
        .beginClass<Data>("Data")
        .addConstructor<void (*)(int)>()
        .endClass()
        .addFunction("processValues", &processValues)
        .addFunction("processPointers", &processPointers);

    {
        resetResult();
        runLua("result = processValues ({[Data (-1)] = Data (2)})");
        std::map<Data, Data> expected{{Data(-1), Data(2)}};
        const auto actual = result<std::map<Data, Data>>();
        ASSERT_EQ(expected, actual);
    }

    {
        resetResult();
        runLua("result = processPointers ({[Data (3)] = Data (-4)})");
        std::map<Data, Data> expected{{Data(3), Data(-4)}};
        const auto actual = result<std::map<Data, Data>>();
        ASSERT_EQ(expected, actual);
    }
}

TEST_F(MapTests, UnregisteredClass)
{
    std::error_code ec;

    {
#if LUABRIDGE_HAS_EXCEPTIONS
        bool result;
        ASSERT_THROW((result = luabridge::push(L, std::map<Unregistered, int>{ { Unregistered(), 1 } }, ec)), std::exception);
#else
        ASSERT_FALSE((luabridge::push(L, std::map<Unregistered, int>{ { Unregistered(), 1 } }, ec)));
#endif
    }

    {
#if LUABRIDGE_HAS_EXCEPTIONS
        bool result;
        ASSERT_THROW((result = luabridge::push(L, std::map<int, Unregistered>{ { 1, Unregistered() } }, ec)), std::exception);
#else
        ASSERT_FALSE((luabridge::push(L, std::map<int, Unregistered>{ { 1, Unregistered() } }, ec)));
#endif
    }
}

TEST_F(MapTests, IsInstance)
{
    std::error_code ec;

    ASSERT_TRUE((luabridge::push(L, std::map<std::string, int>{ { "x", 1 }, { "y", 2 }, { "z", 3 } }, ec)));
    EXPECT_TRUE((luabridge::isInstance<std::map<std::string, int>>(L, -1)));
    
    lua_pop(L, 1);
    
    ASSERT_TRUE((luabridge::push(L, 1, ec)));
    EXPECT_FALSE((luabridge::isInstance<std::map<std::string, int>>(L, -1)));
}

TEST_F(MapTests, StackOverflow)
{
    exhaustStackSpace();
    
    std::map<std::string, int> value{ { "x", 1 }, { "y", 2 }, { "z", 3 } };
    
    std::error_code ec;
    ASSERT_FALSE(luabridge::push(L, value, ec));
}
