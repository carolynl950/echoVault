---
tags: [cs101]
---
# Lists and dictionaries

## Lists

Ordered, can change (mutable), indexed from 0.

```python
grades = [88, 92, 75]
grades.append(100)      # add to the end
grades[0]               # 88
grades[-1]              # 100, negative counts from the end
grades[1:3]             # [92, 75], slicing stops before the end index
```

## Dictionaries

Key → value pairs. Look things up by key instead of position.

```python
ages = {"Priya": 19, "Marcus": 21}
ages["Priya"]            # 19
ages["Sam"] = 20         # add a new key
"Sam" in ages            # True
ages.get("Lee", 0)       # 0 instead of crashing on a missing key
```

Looking up a key in a dict is fast no matter how big it is. Searching a list for a value has to check every item.

## Aliasing gotcha

`b = a` doesn't copy a list, both names point to the SAME list. Changing `b` changes `a`. Use `b = a.copy()` or `b = a[:]`.
