#!/bin/sh

assert() {
    if [ "$1" != "$2" ]; then
        echo "Assertion: $1 != $2"
    fi
}

test_ast() {
    out="$(echo "int f(){$1}" | ./lice --dump-ast)"
    if [ $? -ne 0 ]; then
        echo "failed $1"
        exit
    fi
    assert "$out" "(int)f()$2"
}

test_gm() {
    echo "int f(){$1} int main(){printf(\"%d\", f());}" | ./lice > /tmp/gmcc.S
    if [ $? -ne 0 ]; then
        echo "failed to compile"
        exit
    fi

    gcc /tmp/gmcc.S -o program
    out=$(./program)
    assert "$out" "$2"
}


test_ast       '1;'                        '{1;}'
test_ast       "'a';"                      "{'a';}"
test_ast       '"hello";'                  '{"hello";}'
test_ast       'int a=1;'                  '{(decl int a 1);}'
test_ast       'int a=1;a;'                '{(decl int a 1);a;}'
test_ast       'a();'                      '{(int)a();}'
test_ast       'a(1);'                     '{(int)a(1);}'
test_ast       'a(1,2,3,4);'               '{(int)a(1,2,3,4);}'
test_ast       'int a=1;b(a);'             '{(decl int a 1);(int)b(a);}'
test_ast       '1+2-3+4;'                  '{(+ (- (+ 1 2) 3) 4);}'
test_ast       '1+2*3+4;'                  '{(+ (+ 1 (* 2 3)) 4);}'
test_ast       '1*2+3*4;'                  '{(+ (* 1 2) (* 3 4));}'
test_ast       '1/2+3/4;'                  '{(+ (/ 1 2) (/ 3 4));}'
test_ast       '1/2/3/4;'                  '{(/ (/ (/ 1 2) 3) 4);}'
test_ast       'int a=1;*&a;'              '{(decl int a 1);(* (& a));}'
test_ast       'int a[]={1,2,3};'          '{(decl int[3] a {1,2,3});}'
test_ast       'int a[2]={1,2};'           '{(decl int[2] a {1,2});}'
test_ast       'int a[2][2][2][2];'        '{(decl int[2][2][2][2] a);}'

test_gm        '0;'                        '0'
test_gm        '1+2;'                      '3'
test_gm        '1+2+3+4;'                  '10'
test_gm        '1 + 2;'                    '3'  # whitespace test
test_gm        '4/2+6/3;'                  '4'
test_gm        "'a'+1;"                    '98' # ascii 98 == 'b'

test_gm        'printf("a");3;'            'a3'
test_gm        'printf("%s", "abc");3;'    'abc3'

test_gm        'int a=61;int *b=&a;*b;'    '61'
test_gm        'int a[]={1};int *b=a;*b;'  '1'
test_gm        'if(0){1;}else{0;}'         '0'
test_gm        'if(1){0;}else{1;}'         '0'

cat tests/beer.c | ./lice | gcc -xassembler - -o beer; ./beer
cat tests/factorial.c | ./lice | gcc -xassembler - -o factorial; ./factorial

rm -f beer
rm -f factorial
