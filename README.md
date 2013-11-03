LICE is an experiment to see how close I can get to a C compiler looking
at the sources of existing ones like TCC, GCC and SubC, while still
keeping the code easy to understand and well documented for my own sake.

### Types
Lice supports all the default data types in C, `char, short, int, long,
long long, float, double, long double` as well as `signed, unsigned`.
Lice also allows these types for arrays and pointers, there is no support
for the `volatile, restrict, register` and `const` type specifiers yet.
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
for       | standard for statement
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
The following sources where used as information and inspiration
in the construction of LICE

-   The dragon book
    http://www.amazon.ca/Compilers-Principles-Techniques-Alfred-Aho/dp/0201100886

-   Intel 64 and IA-32 Architectures Instruction Set Reference
    http://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-instruction-set-reference-manual-325383.pdf

-   C99 final draft
    http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1124.pdf

-   x86-64/System V ABI
    http://www.x86-64.org/documentation/abi.pdf

-   The coder64 cheat sheet of opcodes and instructions
    http://ref.x86asm.net/coder64.html

-   Let's Build a Compiler, Jack Crenshaw
    http://compilers.iecc.com/crenshaw/

-   SubC compiler sources for ideas
    http://www.t3x.org/subc/

-   TCC
    http://bellard.org/tcc/

-   IEEE 754, Binary Floating-Point Arithmetic
    http://www.eecs.berkeley.edu/~wkahan/ieee754status/IEEE754.PDF
