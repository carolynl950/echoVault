---
tags: [cs101]
---
# Loops

## for loops

A `for` loop goes through each item in something iterable (a list, a string, a range).

```python
# print each letter on its own line
for letter in "hello":
    print(letter)

# range(start, stop) stops BEFORE stop
for i in range(1, 6):
    print(i)   # 1 2 3 4 5
```

`range(5)` is 0 through 4, not 1 through 5. I got this wrong on the first quiz.

## while loops

Keeps going as long as the condition is true. Use it when you don't know in advance how many times you'll loop, like asking for input until it's valid.

```python
# keep asking until we get a number
answer = input("Pick a number: ")
while not answer.isdigit():
    answer = input("That's not a number, try again: ")
```

Infinite loop = the condition never becomes false. If the terminal freezes, press Ctrl+C.

## break and continue

- `break` exits the loop completely
- `continue` skips to the next iteration

## Common mistakes

- Off-by-one errors with `range`
- Changing a list while looping over it (skips items). Loop over a copy: `for x in items[:]`
- Forgetting to update the variable in a while loop, so it runs forever
