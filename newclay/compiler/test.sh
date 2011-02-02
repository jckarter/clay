if [ x$1 = x ]; then
    echo "usage: $0 testwhatever [suite]"
    exit 1
fi

runner=$1

if [ x$2 = x ]; then
    suite=$1
else
    suite=$2
fi

for test in test/$suite/Fail*.clay test/$suite/Test*.clay; do
    echo
    echo ---------------------------------------------
    echo $test
    echo ---------------------------------------------
    cat $test
    echo ---------------------------------------------

    ./$runner $test
done
