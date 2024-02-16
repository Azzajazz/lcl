#include <stdbool.h>

int main() {
    int a;
    a = 1;
    if (a == 1) {
        int b;
        {
            bool c;
            b = 2;
        }
    }
    else {
        bool c;
        a = 2;
    }
}

int func2(bool bool_val, int int_val) {
    if (bool_val) {
        int_val = int_val + 1;
    }
    return int_val;
}
