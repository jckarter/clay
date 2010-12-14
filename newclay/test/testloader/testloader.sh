for test in Fail*.clay Test*.clay; do
    echo
    echo --------------------
    echo $test
    echo --------------------
    ../../testloader $test
done
