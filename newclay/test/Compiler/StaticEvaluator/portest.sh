testevaluator="$1"
shift
for test in "$@"; do
    base="$(basename $test)"
    base="${base#Test}"
    base="${base/.clay}"
    echo $base
    mkdir $base
    echo "import show.(show);" >$base/main.clay
    if grep "import __primitives__.\*;" $test >/dev/null; then
        true
    else
        echo "import __primitives__.*;" >>$base/main.clay
    fi
    cat $test >>$base/main.clay
    echo >>$base/main.clay
    echo 'main() { show(..#Main()); }' >>$base/main.clay
    $testevaluator $test >$base/out.txt
done
