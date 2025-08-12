#!/bin/sh
cat <<EOF
#include <unity.h>
int main(void) {
    UNITY_BEGIN();
EOF

grep -E '^void test_[A-Za-z0-9_]*\(.*' "$1" |
    sed 's/^void \([A-Za-z0-9_]*\)(.*/    void \1();\
    RUN_TEST(\1);/'

cat <<EOF
    return UNITY_END();
}
EOF
