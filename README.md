LICE is an experiment to see how close I can get to a C compiler looking
at the sources of existing ones like TCC, GCC and SubC, while still
keeping the code easy to understand and well documented for my own sake.

### Types
There are currently only two types supported, int and char, as
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
Lice currently supports three statements
<table align="right">
 <tr><th>Statement</th><th>Description</th></tr>
 <tr><th>if</th><th>Your standard if statement</th></tr>
 <tr><th>for</th><th>Your standard for statement</th></tr>
 <tr><th>return</th><th>Your standard return statement</th></tr>
</table>

### Operators
Lice only supports the following operators
<table align="right">
 <tr><th>Operators</th><th>Description</th></tr>
 <tr><th>+,-,/,*</th><th>Arithmetic operators</th></tr>
 <tr><th>=</th><th>Assignment operator</th></tr>
 <tr><th>==</th><th>Equality operator</th></tr>
 <tr><th>&lt;</th><th>Less than operator</th></tr>
 <tr><th>&gt;</th><th>Greater than operator</th></tr>
 <tr><th>!</th><th>Boolean not operator</th></tr>
 <tr><th>(,)</th><th>Parethesis</th></tr>
 <tr><th>[]</th><th>Array subscript operator</th></tr>
 <tr><th>++,--</th><th>Postfix operators only (e.g a++) </th></tr>
 <tr><th>?,:</th><th>Ternary operator</th></tr>
</table>

Lice doesn't support the short-hand operators:
    `+=, -=, *=, /= ...`
There is also no prefix operators

### Constructs:
There is no support for structures, switches or unions. There
is however support for array initializer lists, e.g
```
int a[] = { 1, 2, 3 };
int a[2] = { 1, 2 };
```

There is also support for multidimensional arrays:
`int a[1][2][3][4];`

### Pointers
Pointers work how they typically would work, you can define one,
take the address of one and have as many layers of referencing
and dereferencing as you want. Pointer arithmetic works as well.
Pointers to functions aren't supported. You can also dereference
pointers into assignments, e.g *a=b; works, as does *(a+N)=b;

### Comments
C block comments and C++ line comments are supported, as well as
line continuation.

### Globals
Currently unimplemented

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
