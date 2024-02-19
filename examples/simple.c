#include <stdbool.h>

int main() {
    bool a[8];
    bool b;
    b = a[0];
}

int func2(bool bool_val, int int_val) {
    if (bool_val) {
        int_val = int_val + 1;
    }
    return int_val;
}
