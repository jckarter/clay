#!/bin/sh
llvm_dir=$1
shift

cat >/tmp/test-libclang.$$.c <<END
#include <clang-c/Index.h>
int main() {
    CXCursor c;
    CXType t = clang_getTypedefDeclUnderlyingType(c);
}
END

echo $llvm_dir/bin/clang $* -lclang -o /tmp/test-libclang.$$.x /tmp/test-libclang.$$.c \
    >>/tmp/test-libclang.$$.stdout.log

$llvm_dir/bin/clang $* -lclang -o /tmp/test-libclang.$$.x /tmp/test-libclang.$$.c \
    >>/tmp/test-libclang.$$.stdout.log 2>/tmp/test-libclang.$$.stderr.log
exit $?
