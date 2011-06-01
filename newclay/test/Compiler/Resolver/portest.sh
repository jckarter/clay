for test in "$@"; do
    base="$(basename $test)"
    base="${base#Test}"
    base="${base/.clay}"
    echo $base
    mkdir $base
    echo "import show.(show);" >$base/main.clay
    cat $test >>$base/main.clay
    echo >>$base/main.clay
    echo 'main() { show("ok"); }' >>$base/main.clay
    echo "ok" >>$base/out.txt
done
