for test in Fail*.clay Test*.clay; do
    echo
    echo --------------------
    echo $test
    echo --------------------
    ../../testevaluator $test
done
