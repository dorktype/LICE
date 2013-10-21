#!/bin/sh

assert() {
    if [ "$1" != "$2" ]; then
        echo "Assertion: $1 != $2"
    fi
}

test_ast() {
    out="$(echo "$1" | ./gmcc --dump-ast)"
    if [ $? -ne 0 ]; then
        echo "failed $1"
        exit
    fi
    assert "$out" "$2"
}

test_gm() {
    echo $1 | ./gmcc 2> /tmp/gmcc_log
    if [ $? -ne 0 ]; then
        echo "failed to compile \"$1\" [$(cat /tmp/gmcc_log)]"
        exit
    fi

    out=$(./program)
    assert "$out" "$2"
}


test_ast       '1;'                        '1'
test_ast       "'a';"                      "'a'"
test_ast       '"hello";'                  '"hello"'
test_ast       'int a=1;'                  '(decl int a 1)'
test_ast       'int a=1;a;'                '(decl int a 1)a'
test_ast       'a();'                      'a()'
test_ast       'a(1);'                     'a(1)'
test_ast       'a(1,2,3,4);'               'a(1,2,3,4)'
test_ast       'int a=1;b(a);'             '(decl int a 1)b(a)'
test_ast       '1+2-3+4;'                  '(+ (- (+ 1 2) 3) 4)'
test_ast       '1+2*3+4;'                  '(+ (+ 1 (* 2 3)) 4)'
test_ast       '1*2+3*4;'                  '(+ (* 1 2) (* 3 4))'
test_ast       '1/2+3/4;'                  '(+ (/ 1 2) (/ 3 4))'
test_ast       '1/2/3/4;'                  '(/ (/ (/ 1 2) 3) 4)'

test_gm        '0;'                        '0'
test_gm        '1+2;'                      '3'
test_gm        '1+2+3+4;'                  '10'
test_gm        '1 + 2;'                    '3'  # whitespace test
test_gm        '4/2+6/3;'                  '4'
test_gm        "'a'+1;"                    '98' # ascii 98 == 'a'
