# Binary compiler

In the previous year, there was a task of writing a compiler of some hand-made programming language.
This compiler consisted form **front-** and **middle-end**, which were used to generate a syntax tree from source code, and then **back-end**, which was used to translate syntax tree into bytecode.
That bytecode was run using softcpu made earlier. 

However, softcpu is a slow thing. Too many excess code is run just for running one or two instructions. So, it's a good idea to compile into native bytecode of the hardware we are using. 

Because there can be potentially many different CPU architectures as opposed to a single softcpu we wrote, and because mixing low-level optimizations with the compilation itself is painful, using some intermediate format for abstraction from hardware should be considered. For llvm this kind of format is known as LLVM IR. For my code, this format is usually mentioned as *compiler output format* or *compiler out lang*

### Compiler output format description

So, main requirements for this kind of format are:   
1) **Abstraction from hardware:** This format should not rely on specific hardware features.  
2) **Ease of translation:** format should be relatively easily translatable into hardware machine code  
3) **Ease of compilation:** it should not be pain to compile the syntax tree of language mentioned before (and possibly other languages) into this  

Accounting this, main design decisions were the following:
1) **Stack-based.**  
    Yes, most modern hardware is rather register than stack based, but stack still usually exists. If stack does not exist, it is relatively easy to manually implement. The way registers work may be highly different on different hardware. But with stack, this can be made not dependent on implementation. To speed things up, optimizing out excess stack push/pops may be desirable, and this can be done on the translation step, accounting all of the hardware features  
2) **Contexts.**  
    Output code is split into different types of blocks by compiler using **'structural instructions'**. These instructions are not actually compiled into something specific, but they help to clean up the stack in the right time. Also, 'structural instructions' are used to define sections of resulting code (saved to sections of ELF file generated), add/remove stack frames, and provide some details on the execution flow (where jump destinations can and can not be). As in language I compile, any kind of recursive blocks are allowed. This greatly simplifies the process of defining functions, as you can just shift into function section, write some code, then go back. Some checks are added to translation to ensure this kind of things is fully safe.  

Now, let's describe the actual implementation of compiler out lang bytecode:

1) **General structure**  
     Bytecode consists of **instructions** of variable length put sequentially into an array. The code includes an implementation of *CompilerOutput*, which is basically dynamically expandable array for convenient compilation. Some 'emit' functions were created for working with it.
2) **Header**  
    Each instruction includes `CompilerInstrHeader` at the start. This header contains size of the instruction (header included) in bytes, argument and return count, and instruction type, which helps to decode further bytes of instruction. Usually, argument and return counts are fixed for each specific instruction, but it's not always the case. For some instructions (e.g. everything of STRUCT type, ret, call) these values can be modified (referred to as *add/pass arguments for instruction* and *request returned values from instruction*) which is accounted on translation. 
3) **Types:**   
    1) `GENERIC`
        The simplest type of instruction. These instructions take a fixed arg count, return a fixed value count, don't (normally) alter control flow. The only value after instrcution header is instruction id (*compilerGenericInstr_t*). 
    2) `CONST` & `LCONST`   
        Push immediate value or symbol raw value on the stack. After header there is either value or symbol name.
    3) `LOAD` & `STORE`  
        Transfer data between top of stack and memory. Mem arg (+ symbol/immediate offset) after header.
    4) `JUMP`  
        Both conditional and unconditional. Condition type and mem arg after header.
    5) `TABLE`  
        Define some kind of symbol. Maybe allocate memory on stack or in some section. Instruction `ADDSECT` also has table type, and is used for defining new sections. Instruction id after header, further data may vary.
    6) `STRUCT` (aka 'structural instruction')  
         Begins and ends of logical blocks. Id after header, further data my vary.
    7) `SPECIAL` Stuff that fits in no category above. Most notably, call and ret. Id after header, further data my vary.
2) **Conditionals.**  
    There can be 8 flag states used for conditional jumps:  
    * always              `(ALW)`
    * never                `(NVR)`
    * (not) equal      `(EQ/NE)`
    * (not) less          `(LT/GE)`
    * (not) greater   `(GT/LE)`

    Compiler only uses signed value, so it's enough. Flags should be set by directly preceeding `cmp` or `test` instruction. Actually, this bit was made based on x86-64 architecture, which is only currently compiled to, but it's like that for ARM as well, and even other types of archietctures don't look as hard to translate into.
3) **Memory.**  
    Jumps and load/store instructions are using the same component (`CompilerMemArgAttr`) for specifying the target memory address. Offset is specified as compiler native type (uint64 in this case) or label, modification by BP or stack argument is allowed. Actually, bp can be emulated, as its only meaning is 'start of where current local variables are stored'


Some more advanced concepts of this format:
1) **Dump utility** ([compiler_out_dump.h](compiler_out_lang/compiler_out_dump.h))  
    Functions, which provide human-readable text representation of compiler out lang instructions.

2) **Sections**  
    All code and data is divided into sections. Section can be initialized (pre-filled with specified data), pre-filled with 0 or 1, uninitialized (program does not care what there is at start).  
    Section has **access attributes** (permission to read/write/execute this memory) and `.entry` **attribute**, which defines if the code in section should gain control at the start of the program. If there are multiple entrypoint sections, they should take control in sequence, in the same order as they are defined. This means that *all entrypoint sections will be merged before execution* This also means, that they can not have different permissions. (permissions are ORed when  merging sections)

3) **Symbol scopes, relocation and prototypes.**  
    Symbols represent constants, labels and memory addresses for data placement. They are defined with `TABLE` type instructions. By default, they have a scope starting from definition until the end of current logical block. Scope can be extended in both directions by using **symbol protoypes** (aka pre-lbls). In this case, symbol scope will be from definition of *prototype* by the end of block *it* is in. If symbol was referenced between prototype and actual definition, that reference is put into special table, and then at the end of translation, the correct value is applied. 'label' symbols (defined to represent absolute or relative-to-other-section address of a place in memory) are considered relocatable, and each their reference is put into relocation table, modified if sections were moved and section offsets are added when running using JIT or writing to executable.
4) **BP and stack memory.**  
Local variables are stored somewhere in memory, at some offsets (defined by labels) and the beginning of this somewhere is stored in bp. For x86 implementation, variables are located on stack. This approach also means that stack frames invalidate any variables defined before.  
Physical presence of *bp* is not required, *bp* can be emulated at compile-time and/or stored somewhere in memory, but for this implementation, CPU's *rbp* is used.
5) **Calling, arguments and returned values.**  
Function arguments should be passed to function at the `CALL` instruction by adding arguments to it. Returned values can also be retrieved from it. Inside the function, returned values are passed as argument of return instruction, arguments are taken by requesting return value from initial stack frame add instruction (or possibly any other structural instruction standing first) In x86-64 implementation, only 6 arguments or 7 return values can be used. These values are passed in registers according to system-V calling convention, but with one key difference: (to support multiple return values) 'zero-th' register is rax, which is cleared on call and used for the first returned value.

### Bytecode output structure

For storing data related to translating compiler output into machine code, special structure `ExecOutput` was created. It stores output sections filled with code, current scope symbols, stack of logical block contexts and relocation info. Complier output is translated instruction-by-instruction into `ExecOutput`, still without having to deal with combining sections on the fly. After this translation, usually additional post-processing is required: symbol references between prototype and definition should be resolved and multiple entrypoint sections should be merged into one.

The fact this structure is filled instruction-by-instruction adds it an interesting property: multiple compiler outputs can be added to it with no problems. With the way sections and labels are implemented (can be shadowed with newer declaration, creating no problems) it will be fully correct. 'entrypoint' code will be run from the first added to last after combining. Because of this, ExecOutput can be used as kind of object file.

### Execution

ExecOutput actually contains executable code we need, but it cannot be just run directly. So some code is required to load it.

#### JIT

First, my own implementation of program loader using ExecOutput format was made. This function takes ExecOutput, allocates memory for required sections, and passes control to entrypoint section. This approach is implemented in file [jit_run.cpp](compiler_out_lang/jit_run.cpp)

#### ELF output
 Instead of loading executable manually each time, it is possible to convert ExecOutput to executable file native to the operating system we are using, so system program loader can run it. For our case, the format of executable file is ELF. 


ELF file consists of multiple segments which are loaded into memory when is run (or rather, *mapped to virtual memory*). The entry point is defined as virtual memory address, which gains control first. So, all we need is to group our sections into segments with same permissions (in theory, up to 8 segments), add two segments for stack and ELF program headers, add headers and write everything to file. Note that file offsets of segments should be (according to [documentation](http://flint.cs.yale.edu/cs422/doc/ELF_Format.pdf)) "congruent, modulo the page size" with their virtual addresses. In practice, that means we should add padding to align segments to page borders. Also, segments themselves may be written into elf file segment header table for debug purposes. Actually, relocations and symbols can also be added to the file, making it an object file, but that's a bit harder, so not implemented because lack of time.

This approach is implemented in file [createElfFile.cpp](compiler_out_lang/createElfFile.cpp)

### Overview

So, the architecture of compiler includes the following:
1) **Front-end**. Parses source code of program (**.wtf** file), and returns it as a crude syntax tree (**.wst** file)
2) **Mid-end**. Takes crude syntax tree, resolves some specific language constructs and returns processed syntax tree (**.wxt** file)
3) **Back-end**. Takes syntax tree and compiles it into 'compiler out lang' bytecode (stored in `CompilerOutput` structure or **.wir** file).
4) **Executable bytecode generator**. Takes compiler out lang representation of program and converts to `ExecOutput`.
5) **Post-processing**. Changes `ExecOutput`, applying delayed changes and removing temporary things.
6) **ELF output** or some other way to either run the executable code directly or save it into executable file.

### Then, performance test.

For testing, simple program, performing calculation of (10ꜝ) 10⁷ times in a loop was used. No input/output was used. (calculation process results were checked using debugger, so calculations are surely correct)
Code, written directly in softcpu asm and executed using softcpu (compiled with O3)      : **11811 ms**  
Code, written in high-level language we compile, compiled into x86 bytecode (unoptimised)   : **710 ms**  
As we can see, even with no optimizations (a ton of pushpops and mem access) compilation to native code outperforms fastest softcpu code **more than 16 times**.  
As a result, compilation into native code is a very effective in terms of performance.





