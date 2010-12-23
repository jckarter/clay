if [ x$1 = x ]; then
    echo "usage: $0 testwhatever"
    exit 1
fi

make $1 || exit 1
for test in test/$1/Fail*.clay test/$1/Test*.clay; do
    echo
    echo --------------------
    echo $test
    echo --------------------
    ./$1 $test
done
