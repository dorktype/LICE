### Epilogue
LICE is a work in progress C99 compiler designed as a solution for
teaching myself and others about the C programming language, how to
write a compiler for it, and code generation.

Part of the philosophy behind LICE is to provide a public domain
implementation of a working conformant C99 compiler. As well as borrowing
extensions and ideas from existing compilers to ensure a wider range of
support.

### Status
LICE as it stands is fairly complete, the base language support for C99
only lacks a few trivial things, albeit important ones. For instance, while
most type specifiers and complicated declarations are accepted, the semantics
and behaviour of them aren't handled. So, to outline the inconsistencies,
a list has been provided below.

-   Most constructs excluding designated initializers are, for the most part,
    fully supported.

-   Direct function calls are fully supported, but limited; for instance,
    function calls cannot exceed six arguments. Some calls may fail since
    stack alignment isn't fully correct, which may break floating point
    operations.

-   Indirect function calls aren't supported, but declaring, and taking
    the address of functions are.

-   Literals of all kinds are supported, but exotic escape sequences in
    string literals or character literals aren't.

-   Compound literals are supported, but they aren't supported in function
    definitions or global variable initializers.

-   There is no support for copying structures, or passing structures by
    copy through functions.

-   String concatenation for adjacent strings, and other preprocessor-aware
    operations aren't supported since there isn't a preprocessor stage.

-   Prefix increment and decrement operators aren't supported.

-   Taking the address of a structure for assigning to structure field of pointer
    type of that same structure isn't supported.

-   There is no comma operator.

-   Boolean operations aren't properly converted to 0 or 1 in accordance
    to the SystemV ABI.

-   Conformant implicit and explicit arithmetic conversion isn't supported,
    at least not inline with what the standard defines as promotion ranking.
    Similarly, implicit type conversion isn't correct either.

-   There is no support for logical right shift operations, so code like
    `((unsigned)-1) >> 31)` won't give correct results.

-   Conditional tests that use a `float, double`, or `long double` type
    condition aren't casted to boolean type, so they may give wrong
    results.

-   Omitting the semicolon at the end of a structure or union member list
    isn't supported.

-   Return values are not booleanized in accordance to the SystemV ABI.

-   Unspecified fields of a literal structure aren't default initialized
    to zero.

-   Initializing global variable with a pointer to another global variable
    isn't supported.

-   Structures cannot be initialized with literal struct.

-   Bitfields aren't supported.

-   Floating point constants beginning with `.` aren't supported.

-   Certain line markers like CR,LF, CRLF, and EOF aren't accepted

-   Labels and goto aren't supported.

-   Designated array initializers aren't supported.

-   Initializer lists, while they are supported, are only supported for one
    level of nesting, simlarly nested designatiors aren't supported either.

-   The C99 `typeof` keyword isn't supported.

-   Character literals are interpreted as type `char`, opposed to type `int`,
    this is a direct violation of the C standard.

-   Typedef names don't share the same namespace as ordinary identifiers, this
    is a direct violation of the C standard.

-   Various popular extensions like case ranges, flexible array members, alignof,
    binary literals, statement expressions, __builtin_return_address, etc,
    aren't supported.

-   Old K&R C style functions aren't supported.

-   Unicode character literals aren't supported.

### Prologue
If you don't find yourself needing any of the stuff which is marked as being
unsupported above then you may find that LICE will happily compile your
source into x86-64 assembly. The code generation is close from optimal.
LICE treats the entire system as a giant stack machine, since it's easier
to generate code that way. The problem is it's hardly efficent. All local
variables are assigned on the stack for operations. All operations operate
from the stack and write back the result to the stack location that is
the destination operand for that operation. Eventually an intermediate
representation will be added which will perform trivial optimization before
assembly generation.

### Porting
LICE isn't easily retargetable since the parser and ast components of the
compiler make huge architectual specific assumptions. Assuming you wanted
to port you'd need to change the size of types respectfully inside ast.c,
and strucute / type alignment inside parser.c, this should be farily
trivial since both of those things exist at the top of each of those files
respectfully. After that is acomplished ast_result_type will need to be
changed to reflect the correct type conversion, that both conforms to
the ABI for your target, and the standard. The final stage is duplicating
gen.c and changing it to produce assembly for that architecture.


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

-   "C11 Final Draft." ISO/IEC, 12 April 2011. Print.
    http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf

-   "Instruction Set Reference, A-Z." Intel 64 and IA-32 Architectures Software Developer's Manual. Vol. 2. [Calif.?]: Intel, 2013. N. pag. Print.
    http://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-instruction-set-reference-manual-325383.pdf

-   Bendersky, Eli. "Complete C99 parser in pure Python." pycparser. N.p., N.d. Web.
    https://github.com/eliben/pycparser

### Inspiration
The following projects were seen as inspiration in the construciton of
LICE.

-   SubC
    http://www.t3x.org/subc/

-   TCC
    http://bellard.org/tcc/

-   lcc
    https://sites.google.com/site/lccretargetablecompiler/
