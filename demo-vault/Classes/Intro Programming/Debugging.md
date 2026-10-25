---
tags: [cs101]
---
# Debugging

## Reading a traceback

Read it from the bottom up. The last line is the error type and message. The line just above it shows where it happened in my file. Everything higher up is how the program got there.

Errors I keep hitting:
- `NameError`: misspelled a variable or used it before defining it
- `TypeError`: mixing types, like `"Score: " + 5` (fix: `str(5)` or an f-string)
- `IndexError`: list index out of range, usually off by one
- `KeyError`: dictionary key doesn't exist
- `IndentationError`: mixed tabs and spaces, or forgot to indent after a colon

## My process

1. Reproduce it reliably first
2. Print the variables right before the line that breaks
3. Shrink the input until it's the smallest thing that still breaks
4. Rubber duck it: explain the code out loud line by line. Half the time I find it mid-sentence.

Professor Hale's rule: if you've been stuck for 30 minutes, stop and go to office hours. Don't lose a whole night to it.
