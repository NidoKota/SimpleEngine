#pragma once

#include <iostream>
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iterator>

#if defined(__ANDROID__)
#include "AndroidOut.h"
#endif

namespace Vulkan_Test {
    using namespace std;

    inline void coutMultiThread(std::string str) {
#if defined(__ANDROID__)
        aout << str << std::endl;
#else
        cout << str;
#endif
    }

    inline void cerrMultiThread(std::string str) {
#if defined(__ANDROID__)
        aout << str << std::endl;
#else
        cerr << str;
#endif
    }

    inline string getTimeStamp() {
        auto now = chrono::system_clock::now();
        auto nowC = chrono::system_clock::to_time_t(now);
        auto nowMs = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
        stringstream ss;
        ss << put_time(localtime(&nowC), "%T") << '.' << setfill('0') << setw(3) << nowMs.count();
        return ss.str();
    }

    inline std::string getIndexStr(size_t index) {
        std::stringstream s;
        for (size_t i = 0; i < index; i++) {
            s << "    ";
        }
        return s.str();
    }

    template<class T, class UniqueT>
    inline std::shared_ptr<std::vector<T>> unwrapHandles(std::vector<UniqueT> &uniques) {
        std::shared_ptr<std::vector<T>> result = std::make_shared<std::vector<T>>();
        result->reserve(uniques.size());

        std::transform(uniques.begin(), uniques.end(), std::back_inserter(*result),
                       [](const UniqueT &unique) -> const T & {
                           return unique.get();
                       }
        );

        return result;
    }

    inline int currentLogIndex = 0;
}

#define PUBLIC_GET_PRIVATE_SET(type, name) \
public: type& Get##name(){ return name; } \
private: type name

#define SS2STR(x) \
([&]() -> std::string { std::stringstream ss; ss << std::boolalpha << x; return ss.str(); })()

#define FILE_LINE_STR \
__FILE__ << ":" << std::setw(3) << std::setfill('0') << __LINE__ << std::setfill(' ')

#define SET_LOG_INDEX(x) \
do { Vulkan_Test::currentLogIndex = x; } while (false)

#define LOG(x) \
do { Vulkan_Test::coutMultiThread(SS2STR(Vulkan_Test::getTimeStamp() << " " << FILE_LINE_STR << " " <<Vulkan_Test:: getIndexStr(Vulkan_Test::currentLogIndex) << x << "\n")); } while (false)

#define LOGERR(x) \
do { Vulkan_Test::cerrMultiThread(SS2STR(Vulkan_Test::getTimeStamp() << " " << FILE_LINE_STR << " " << Vulkan_Test::getIndexStr(Vulkan_Test::currentLogIndex) << x << "\n")); } while (false)
