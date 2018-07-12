#ifndef __ACTION_HEADER_H__
#define __ACTION_HEADER_H__
#include "auto_factory.h"

template<typename... Args>
CAction* produce_msg(const std::string &key, Args &&... args)
{
    return CAuto_regist_factory<Args...>::get().produce(key, std::forward<Args>(args)...);
}

template<typename... Args>
std::shared_ptr<CAction> produce_msg_shared(const std::string &key, Args &&... args)
{
    return CAuto_regist_factory<Args...>::get().produce_shared(key, std::forward<Args>(args)...);
}

class do_action_1 : public CAction
{
public:
    do_action_1(int value)
    {
        printf("do_action_1. value: %d\n", value);
    }

    void do_action()
    {
        printf("do_action1.\n");
    }
};

class do_action_2 : public CAction
{
public:
    do_action_2(int value1, int value2)
    {
        printf("do_action_2. value: %d\t%d\n", value1, value2);
    }

    void do_action()
    {
        printf("do_action2.\n");
    }
};

class do_action_3 : public CAction
{
public:
    do_action_3(std::string value)
    {
        printf("do_action_3. value: %s\n", value.c_str());
    }
    
    void do_action()
    {
        printf("do_action3.\n");
    }
};

REGISTER_MESSAGE1(do_action_1, "action1", int);
REGISTER_MESSAGE1(do_action_2, "action2", int, int);
REGISTER_MESSAGE1(do_action_3, "action3", std::string);

#endif