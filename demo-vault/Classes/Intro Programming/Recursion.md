---
tags: [cs101, exam2]
---
# Recursion

A function that calls itself. Every recursive function needs:

1. A base case that stops it
2. A recursive case that moves toward the base case

```python
def factorial(n):
    # base case
    if n <= 1:
        return 1
    # recursive case: shrink the problem
    return n * factorial(n - 1)
```

## How it actually runs

Each call waits on the call stack until the one it called returns. factorial(4) waits on factorial(3), which waits on factorial(2), down to factorial(1), which returns 1. Then they all resolve back up: 2, 6, 24. Our TA Marcus drew it as a stack of plates and that finally made it click.

Forget the base case and you get `RecursionError: maximum recursion depth exceeded`. Python stops around 1000 calls deep.

## When to use it

Good for problems that contain smaller copies of themselves: folders inside folders, trees, and divide-and-conquer like merge sort. Naive recursive Fibonacci is super slow because it recalculates the same values over and over, so a loop or memoization is better there.
