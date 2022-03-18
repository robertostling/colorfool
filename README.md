# colorfool

This is a simple programming language based on Chuck Moore's colorForth as
well as my own FOOL (Forth On One Line) language. The main novelty is the use
of (sort of) human-readable machine level code, and the compiler consists of
source/machine code translating itself into more efficient source/machine
code. It can also be viewed as a special kind of token-threaded Forth
interpreter.

To make the above more concrete, we start with a screenshot
of the editor. In the Forth tradition, this is viewing a 64x32 block of code.
The "E compile" at the bottom is part of the editor, not code.

![Contents of blocks/kernel.block](./screenshot.png)

Many things are going on here, in a small amount of space. Let us first
summarize line by line.

* Line 1: Compiling literal values.
* Line 2: If/else statements, loops and variables.
* Line 3: Decimal number constants.
* Line 4: Logical operations, character and string constants.
* Line 5: String output.
* Line 6: String equality.
* Line 7: Creation and lookup of associative lists.
* Line 8: Integer output.
* Line 9: Compiling and executing functions defined with associative lists.
* Line 10: Example definition of such a function (factorial).
* Line 11: Example of calling the factorial function.
* Lines 12--15: Intentionally left blank.
* Line 16: Terminate the program.

Before going deeper into the code, let us start by describing the virtual
machine.

## Virtual machine

This is fairly similar to a standard token-threaded Forth. There is a
128-entry character lookup table starting at memory address 0. During
execution, the following loop is repeated:

    X = memory[IP]
    IP = IP + 1
    W = memory[(X % 128)*2 + 1]
    CALL memory[(X % 128)*2]

That is, the low 7 bits of each memory word determines the function to be
executed. The CALL is a machine instruction, and in the current implementation
it is used to call functions defined in C (see `core.c`).

After the lookup table there are data and return stacks, as in normal Forth,
and then a single built-in H variable (corresponding to HERE in Forth) which
contains a pointer to the first free address on the heap. At the beginning,
this is the address immediately following H itself.

Each memory word encodes one character of code, or an integer value
of at lesat 16 bits. The editor maps color to memory words according to the
following scheme.

### Red characters

Bits 0--6 are the ASCII character displayed. Thus, red characters are used to
execute functions immediately. In practice, red code is used where you would
be in interpretation mode in a traditional Forth, or for immediate words
(macros). Since this is the only color where the ASCII value is in the low 7
bits, red characters are also used for string literals, but they should
generally not be executed.

### Yellow characters

Bits 0--6 encode the value 1. This appends to the heap the value of bits
8--14 as a *red* character. Since red characters are executed directly, this
corresponds to compiling a function call.
The editor displays the contents of bits 8--14.

### White characters

Bits 0--6 encode the value 2. This appends to the heap the value of bits 8--14
as a *yellow* character. Since yellow characters can be interpreted as
compiling code, white characters compile code that compile code. White
characters are reduced to yellow, which are reduced to red, which eventually
produce some effect. The editor displays the contents of bits 8--14.

### Cyan characters

Bits 0--6 encode the value 3. This is a no-op and can be used for comments.
The editor displays the contents of bits 8--14.

### Magenta characters

Bits 0--6 encode the value 4. This updates the character look-up table entry
of the value in bits 8--14, so that the code field points to the machine code
for ENTER, and data field points to the current IP+1. The effect of this is
that whenever the same character occurs in red, the code immediately after the
magenta character is executed. The editor displays the contents of bits 8--14.

## Detailed walk-through

### Line 1

First we define the word `,` (comma). In traditional Forth, this would be:

    : ,  HERE @  !  HERE @  1+  HERE ! ;

Second, we define the word `#` which pushes the constant 0. It is defined as
subtracting an arbitrary literal value (an L) from itself.

Third, we define `.` which will be used as a macro for compiling literal values.
Since there are no built-in constant expressions, the general strategy here is
to compute all constant expressions in red at "compile time", followed by a
red `.` in order to compile the sequence `LX` where `L` corresponds to LIT in
Forth and X is the computed constant.

### Line 2

Here we define `[` and `]` which correspond to Forth BEGIN and AGAIN, i.e. an
infinite loop. `[` simply pushes the current address being compiled to, and
`]` compiles a jump (instruction: `J`) to that address. These are both
intended to be used in red, which means that the `J` is white, so that the
"compiled" code of `]` contains a yellow `J`, which once `]` is executed in
another defintion results in a red `J` in the compiled code of that
definition.

`{` and `|` and `}` correspond to IF, ELSE, THEN in standard Forth. They are
also meant to be used in red, and similar to `]` eventually reduce to a
sequence of conditional `?` and unconditional `J` jumps.

Finally we define `$` which saves the given value to the heap, and compiles
code that pushes the address of that value. It is similar to VARIABLE in
Forth, except that the initial value of the variable should be given.

Note that after defining loops and conditionals, as well as variables, there
is still space left on the 64-character line! I also left some (cyan) blank
spaces in the code for readability.

### Line 3

Integers are encoded by first executing `#` to push 0 to the stack, then the
digits 0--9 multiply the current TOS (top of stack) by 10, followed by adding
up to 9. The digits call each other, which may seem rather inefficient, but
normally numbers are written in red followed by a red `.` (see line 1 above),
so at runtime the integer constant is already stored in a single word. See for
instance on line 4, where `#127.` is used.

### Line 4

Logical negation (`N`) is defined using a conditional expression. A literal
translation to Forth would be:

    : N  IF 0 ELSE 0 1+ THEN ;

Equality (`=`) is simply negated subtraction.

String constants are implemented using `"` which pushes the current IP (i.e.
the address of the character after `"`) and computes the number of characters
until the following `"`. It then puts the address of the following character
on the return stack, to make execution continue from there. The string
constant is thus physically stored directly as part of the "source" code.

Note that single letter literals can be pushed by putting a red `L` in front
of them. Currently there is a redundant implementation of this using `C`,
which also masks out the high bits (but if the letter literal is red, this is
not needed).

## Native words

Below are the words defined in `core.c`.

| Op  | Forth | Arguments | Description
| --- | ----- | --------- | ----------
| B | BYE     | --        | terminate
| D | DUP     | x -- x x  | duplicate TOS
| E | EMIT    | c --      | print low 7 bits of word
| H | HERE    | -- a      | variable containing end of heap pointer
| I | 1+      | n -- n+1  | increment by 1 
| J | BRANCH  | --        | word at IP+1 -> IP
| F |         | --        | like BRANCH but push IP+1 first, high-level call
| L | LIT     | -- x      | push word at IP+1
| O | 2\*     | n -- n\*2  | double
| P | \>R     | x --      | push to return stack
| Q | R\>     | -- x      | pop from return stack
| R | KEY     | -- c      | read one character
| S | SWAP    | x y -- y x | swap TOS and NOS
| V | DROP    | x --      | drop TOS
| ; | EXIT    | --        | exit from subroutine
| @ | @       | a -- x    | load TOS from address in TOS
| ! | !       | x a --    | store NOS to address in TOS
| \+ | \+     | n m -- n+m | add TOS to NOS
| \* | \*     | n m -- n\*m | multiply TOS to NOS
| - | -       | n m -- n-m | subtract TOS from NOS
| & | &       | n m -- n&m | bitwise and TOS to NOS
| % | /MOD    | n m -- n%m n/m | division with remainder
| ? | ?BRANCH | b --      | word at IP+1 -> IP if TOS is zero

## Kernel definitions

This is a summary of the words defined in the code block, several of them are
described in more detail above.

| Op  | Forth | Arguments | Description
| --- | ----- | --------- | ----------
| , | ,       | x --      | append value to heap
| \# |        | -- 0      | push the constant 0
| . |         | x --      | compile LIT x
| { | IF      | -- a      |
| \| | ELSE   | a -- a    |
| } | THEN    | a --      |
| [ | BEGIN   | -- a      | start of infinite loop
| ] | AGAIN   | a --      | end of infinite loop
| N | NOT     | b1 -- b2  | logical not
| U |         | a1 n1 a2 n2 -- b | string equality
| K | .       | n --      | output decimal integer
| A |         | a1 n a2 -- a3 | lookup string (a1 n) in dictionary (a2)
| W | CR      | --        | print a newline
| T | TYPE    | a n --    | print a string
| " | S"      | -- a n    | literal string (note: not compiling anything)
| $ | VARIABLE | x --     | global variable
