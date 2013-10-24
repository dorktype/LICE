LICE is an experiment to see how close I can get to a C compiler looking
at the sources of existing ones like TCC, GCC and SubC, while still
keeping the code easy to understand and well documented for my own sake.

### Types
There are currently only two types supported, `int` and `char`, as
well as the pointer and array versions of those types. There is
no support for signed/unsigned keywords, all types are handled
as signed integer types.

There is no support for floating point operations, not because
the type doesn't exist (which would potentially prevent such
operations) but because stack space isn't aligned on 16 byte
boundry yet to accomodate it, and the registers themselfs aren't
properly saved/restored to allow it, so calling external float-aware
functions will likely crash.

### Functions
Functions are supported but function prototyping isn't which means
the function has to be implemented where it's declared. Functions
are limited to six arguments, and variable arguments are not
supported. Calling external C variable argument functions such
as printf are allowed but the argument count is also limited to six.
You can also call malloc/free for heap allocations.

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
(,)       | Standard Parentheses
[]        | Array subscript operator
++,--     | Postfix operators only (e.g a++)
?,:       | Ternary operator
&,&#124;  | Bitwise operators
->        | Standard pointer syntax sugar operator
>=,<=     | Greater/Less than or equal to operators

Lice doesn't support the short-hand operators `+=, -=, *=, /= ...`

Lice doesn't support prefix operators `++, --`

### Constructs:
Lice supports only three constructs:


 Construct | Description
-----------|:-----------------------------------------------------------
 struct    | Plain structures only, nothing fancy supported yet
 union     | Plain unions only, nothing fancy yet
 array     | Initializer lists arrays are supported, but only one level


There is also support for multidimensional arrays:
`int a[1][2][3][4];`

Support for declaring structures inside functions is also supported.

### Pointers
Pointers work how they typically would work, you can define one,
take the address of one and have as many layers of referencing
and dereferencing as you want. Pointer arithmetic works as well.
Pointers to functions aren't supported. You can also dereference
pointers into assignments, e.g *a=b; works, as does *(a+N)=b;

### Expressions
All typical expressions should work including integer constant expressions
for things like `int a[5*5];`. There are some thing that aren't supported
like type casts, but for the most part they all work, including ternary
expressions, and weird nesting.

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


### Sources
The following sources where used as information and inspiration
in the construction of LICE

-   The dragon book
    http://www.amazon.ca/Compilers-Principles-Techniques-Alfred-Aho/dp/0201100886

-   C99 final draft
    http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1124.pdf

-   SubC compiler sources for ideas
    http://www.t3x.org/subc/

-   TCC
    http://bellard.org/tcc/

-   x86-64/System V ABI
    http://www.x86-64.org/documentation/abi.pdf

-   The coder64 cheat sheet of opcodes and instructions
    http://ref.x86asm.net/coder64.html

-   Let's Build a Compiler, Jack Crenshaw
    http://compilers.iecc.com/crenshaw/
