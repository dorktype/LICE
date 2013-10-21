#!/bin/sh

assert() {
    if [ "$1" != "$2" ]; then
        echo "Assertion: $1 != $2"
    fi
}

test_ast() {
    out="$(echo "$1" | ./gmcc --dump-ast)"
    if [ $? -ne 0 ]; then
        echo "failed"
        exit
    fi
    assert "$out" "$2"
}

test_compile() {
    echo "$1" | ./gmcc
    if [ $? -ne 0 ]; then
        echo "failed"
        exit
    fi

    out="$(./program)"
    assert "$out" "$2"
}

test_ast     '1;'           '1'
test_ast     'a=1;'         '(= a 1)'
test_ast     'a=1;a;'       '(= a 1)a'
test_ast     'a();'         'a()'
test_ast     'a(b);'        'a(b)'
test_ast     'a(b,c,d,e);'  'a(b, c, d, e)'
test_ast     'a=1;b(a);'    '(= a 1)b(a)'
test_ast     '1+2-3+4;'     '(+ (- (+ 1 2) 3) 4)'
test_ast     '1+2*3+4;'     '(+ (+ 1 (* 2 3)) 4)'
test_ast     '1*2+3*4;'     '(+ (* 1 2) (* 3 4))'
test_ast     '1/2+3/4;'     '(+ (/ 1 2) (/ 3 4))'
test_ast     '1/2/3/4;'     '(/ (/ (/ 1 2) 3) 4)'

test_compile '1;'           '1'
test_compile 'a=1;a;'       '1'
