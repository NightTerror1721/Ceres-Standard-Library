#pragma once

// bool, true and false are part of the language here (ceresc knows them as keywords), so this header adds
// nothing but the macro C99 says it defines, and _Bool for code that spells the type that way.
#define __bool_true_false_are_defined 1
#define _Bool bool
