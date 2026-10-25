---
tags: [cs101, project]
---
# Project 2: word counter

Due Oct 10. Read a text file and print the 10 most common words.

## Starter code

The starter code from the course site gives us `read_file(path)` and an empty `count_words(text)` function. We're not allowed to use `collections.Counter`, we have to build the dictionary ourselves.

```python
def count_words(text):
    counts = {}
    for word in text.lower().split():
        word = word.strip(".,!?;:\"'()")
        if word:
            counts[word] = counts.get(word, 0) + 1
    return counts
```

## Sorting by count

```python
top = sorted(counts.items(), key=lambda pair: pair[1], reverse=True)[:10]
```

`key=lambda pair: pair[1]` means sort by the second thing in each (word, count) pair.

## Things I got stuck on

- "The" and "the" were counted separately until I used `.lower()`
- Punctuation stuck to words: "end." vs "end". Fixed with `strip`
- Tested on Frankenstein from Project Gutenberg. The top words are boring ("the", "and", "i"). The extra credit is to filter out stop words.
