LICE is an experiment to see how close I can get to a C compiler looking
at the sources of existing ones like TCC, GCC and SubC, while still
keeping the code easy to understand and well documented for my own sake.

### Types
Lice supports all the default data types in C, `char, short, int, long,
long long, float, double, long double` as well as `signed, unsigned`.
Lice also allows these types for arrays and pointers, while there is support
for the `volatile, restrict, register` and `const` type specifiers, the
semantics of those specifiers aren't implement, they're ignored.
The `void` type is also supported as well.

### Functions
Functions are supported, as is function prototyping. Functions
are limited to six arguments, and variable arguments are not
supported. Calling external C variable argument functions such
as printf are allowed but the argument count is also limited to six.
You can also call malloc/free for heap allocations. Function prototyping
is also limited, Lice will only support trivial prototypes, confusing
declarations aren't handled as of yet.

### Statements
Lice supports the following statements


Statement | Description
----------|:-----------
if        | Standard if statement
for       | Standard for loop statement
while     | Standard while loop statement
do        | Standard do loop statement
return    | Standard return statement

### Operators
Lice supports the following operators


Operators | Description
----------|:-------------------------------------------------------
+,-,/,*   | Arithmetic operators
=         | Assignment operator
==,!=     | Equality operators
<         | Less than operator
\>        | Greater than operator
!         | Boolean not operator
%         | Modulo operator
(,)       | Standard Parentheses
[]        | Array subscript operator
++,--     | Postfix operators only (e.g a++)
?,:       | Ternary operator
&,&#124;  | Bitwise operators
->        | Standard pointer syntax sugar operator
>=,<=     | Greater/Less than or equal to operators
~,^       | Bitwise not and xor
<<,>>     | Bitshift operators

Lice doesn't support the short-hand operators `+=, -=, *=, /= ...`

Lice doesn't support prefix operators `++, --`

### Constructs:
Lice supports only three constructs:


 Construct            | Description
----------------------|:-----------------------------------------------------------
 struct               | Standard structures
 union                | Standard unions
 initializer list     | Works for arrays and structures
 enum                 | Subset enumerations (no forward declaring or assigning to)

Support for declaring structures and unions inside functions is supported as
well as anywhere a typename is expected, e.g a function argument. Initializer
lists work on char arrays as well with the use of strings, e.g:
`struct { char a[6]; } v = { "hello" };` works.

### Pointers
Pointers work how they typically would work, you can define one,
take the address of one and have as many layers of referencing
and dereferencing as you want. Pointer arithmetic works as well.
Pointers to functions aren't supported. You can also dereference
pointers into assignments, e.g *a=b; works, as does *(a+N)=b;

### Expressions
All typical expressions should work including integer constant expressions
for things like `int a[5*5];`. There are some things that aren't supported
like type casts, but for the most part they all work, including ternary
expressions, and weird nesting. The `sizeof` operator, which returns a constant
value expression doesn't support typenames as of yet, only literals are
currently supported as the operand to `sizeof`.

### Comments
C block comments and C++ line comments are supported, as well as
line continuation.

### Code generation
The codegen is unoptimal crap, it produces a 'stack machine'
model which isn't exactly efficent. All registers are saved
and restored while they mostly may not need to be. Return
statements need to emit leave instructions even though they
might not need to be. Operations will always load operands
into the first closest registers and store the result back
into the first, even though that operation may already
put the result into the first.

### Platforms
The code generator produces code compatible in accordance to
the SysV ABI for AMD64. So it should function on any AMD64
Linux or FreeBSD system.

### What does LICE stand for?
This was a topic of serious debate amongst myself and peers. Some
of the ideas were:

-   LICE Isn't Ceee
-   Limitless Internal Compiler Errors
-   Lethargic Inducing C Extrapolator (You get tired of using it real fast)
-   Lamest Implemenation of C, Ever!

I like to think the last one suits the current status of the project
currently. To be fair I don't care what you think it stands for, just
find solace in one of the choices above, or one of your own and stick with
it. If you do happen to find a nice marketable explination of the acronym
you can email me your suggestion and I'll include it here.

### Sources
The following sources where used in the construction of LICE

-   Aho, Alfred V., and Alfred V. Aho. Compilers: Principles, Techniques, & Tools. Boston: Pearson/Addison Wesley, 2007. Print.
    http://www.amazon.ca/Compilers-Principles-Techniques-Alfred-Aho/dp/0201100886

-   Degener, Jutta. "ANSI C Grammar, Lex Specification." ANSI C Grammar (Lex). N.p., 1995. Web.
    http://www.lysator.liu.se/c/ANSI-C-grammar-l.html

-   Matz, Michael, Jan Hubicka, Andreas Jaeger, and Mark Mitchell. "System V Application Binary Interface AMD64 Architecture Processor Supplement." N.p., 07 Oct. 2013. Print.
    http://www.x86-64.org/documentation/abi.pdf

-   Kahan, W., Prof. "IEEE Standard 754 for Binary Floating-Point Arithmetic." N.p., 1 Oct. 1997. Print.
    http://www.eecs.berkeley.edu/~wkahan/ieee754status/IEEE754.PDF

-   Crenshaw, Jack. "Let's Build a Compiler." I.E.C.C., 1995. Web.
    http://compilers.iecc.com/crenshaw/

-   "C99 Final Draft." ISO/IEC, 06 May 2006. Print.
    http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1124.pdf

-   "Instruction Set Reference, A-Z." Intel 64 and IA-32 Architectures Software Developer's Manual. Vol. 2. [Calif.?]: Intel, 2013. N. pag. Print.
    http://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-instruction-set-reference-manual-325383.pdf


### Inspiration
The following where inspiration in the creation of LICE

-   SubC compiler sources for ideas
    http://www.t3x.org/subc/

-   TCC
    http://bellard.org/tcc/

