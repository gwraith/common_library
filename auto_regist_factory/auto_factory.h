#ifndef __Automatic_Registration_Factory__
#define __Automatic_Registration_Factory__

#include <map>
#include <string>
#include <memory>
#include <functional>

class CAction
{
public:
    CAction() {}
    virtual void do_action() {}
    virtual ~CAction() {}
};

template <typename... U>
class CAuto_regist_factory
{
public:
    template <typename T> struct register_t
    {
        register_t(const std::string &key)
        {
            CAuto_regist_factory<U...>::get().map_.emplace(key, [](U&&... u) { return new T(std::forward<U>(u)...); });
        }

        register_t(const std::string& key, std::function<CAction*(U&&...)>&& f)
        {
            CAuto_regist_factory<U...>::get().map_.emplace(key, std::move(f));
        }
    };

    static CAction *produce(const std::string &key, U&&... u)
    {
        auto map_ = CAuto_regist_factory::get().map_;
        if (map_.find(key) == map_.end()) throw std::invalid_argument("error");
        return map_[key](std::forward<U>(u)...);
    }

    static std::shared_ptr<CAction> produce_shared(const std::string &key, U&&... u)
    {
        return std::shared_ptr<CAction>(produce(key, std::forward<U>(u)...));
    }

    static CAuto_regist_factory<U...>& get()
    {
        static CAuto_regist_factory<U...> instance;
        return instance;
    }

private:
    /* 构造函数，拷贝构造函数，移动构造函数均设为私有 */
    CAuto_regist_factory<U...>() = default;
    CAuto_regist_factory<U...>(const CAuto_regist_factory<U...>&) = delete;
    CAuto_regist_factory<U...>(CAuto_regist_factory<U...>&&) = delete;

    std::map<std::string, std::function<CAction*(U&&...)>> map_;
};

#define REGISTER_MESSAGE_VNAME(T) reg_msg_##T##_
#define REGISTER_MESSAGE(T, key) static CAuto_regist_factory<>::register_t<T> REGISTER_MESSAGE_VNAME(T)(key);
#define REGISTER_MESSAGE1(T, key, ...) static CAuto_regist_factory<__VA_ARGS__>::register_t<T> REGISTER_MESSAGE_VNAME(T)(key);
#define REGISTER_MESSAGE2(T, key, f, ...) static CAuto_regist_factory<__VA_ARGS__>::register_t<T> REGISTER_MESSAGE_VNAME(T)(key, f);

#endif
