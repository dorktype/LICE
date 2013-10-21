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

test_compile() {
    echo "$1" | ./gmcc
    if [ $? -ne 0 ]; then
        echo "failed $1"
        exit
    fi

    out="$(./program)"
    assert "$out" "$2"
}

test_ast     '1;'                        '1'
test_ast     "'a';"                      "'a'"
test_ast     '"hello";'                  '"hello"'
test_ast     'int a=1;'                  '(decl int a 1)'
test_ast     'int a=1;a;'                '(decl int a 1)a'
test_ast     'a();'                      'a()'
test_ast     'a(1);'                     'a(1)'
test_ast     'a(1,2,3,4);'               'a(1,2,3,4)'
test_ast     'int a=1;b(a);'             '(decl int a 1)b(a)'
test_ast     '1+2-3+4;'                  '(+ (- (+ 1 2) 3) 4)'
test_ast     '1+2*3+4;'                  '(+ (+ 1 (* 2 3)) 4)'
test_ast     '1*2+3*4;'                  '(+ (* 1 2) (* 3 4))'
test_ast     '1/2+3/4;'                  '(+ (/ 1 2) (/ 3 4))'
test_ast     '1/2/3/4;'                  '(/ (/ (/ 1 2) 3) 4)'

test_compile 'int a=1;a;'                '1'
test_compile 'printf("a");int a=0;a;'    'a0'
test_compile 'printf("%s","a");'         'a1'
test_compile "printf(\"%c\",'a');"       'a1'
