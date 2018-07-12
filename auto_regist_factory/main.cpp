#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory>
#include "action.h"

int main()
{
    auto p1 = produce_msg_shared("action1", 100);
    p1->do_action();

    auto p2 = produce_msg_shared("action2", 200, 300);
    p2->do_action();

    auto p3 = produce_msg_shared("action3", std::string("gaoyaming"));
    p3->do_action();

    return 0;
}