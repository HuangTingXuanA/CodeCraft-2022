#include <iostream>
#include "solution.h"
using namespace std;

int main(){
    solution::getInstance()->init_nodes();
    solution::getInstance()->slove();
    return 0;
}