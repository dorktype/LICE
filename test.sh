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
    echo "$2" | ./gmcc 2> /dev/null
    if [ $1 -ne 0 ]; then
        if [ $? -ne 0 ]; then echo "failed"; exit; fi
    else
        if [ ! $? -ne 0 ]; then echo "failed"; exit; fi
    fi

    out="$(./program)"
    if [ $1 == 0 ]; then
        assert "$out" "$3"
    fi
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

test_compile 0 'int a=1;a;'                '1'
test_compile 0 'printf("a");int a=0;a;'    'a0'
test_compile 0 'printf("%s","a");'         'a1'
test_compile 0 "printf(\"%c\",'a');"       'a1'

test_compile 1 '"a"+1;'
test_compile 1 'int a="1";'
