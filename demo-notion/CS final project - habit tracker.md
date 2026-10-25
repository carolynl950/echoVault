## Idea

A command-line habit tracker in Python. You add habits ("practice guitar", "cook dinner"), check them off each day, and see your streaks.

## Features

- [x] Add and remove habits
- [x] Mark a habit done for today
- [x] Save to a JSON file so the data survives between runs
- [ ] Current streak and longest streak for each habit
- [ ] Weekly summary table

## Design

The data is a dictionary: habit name → list of dates it was completed (ISO strings like "2026-10-21"). `json.dump` and `json.load` handle saving and loading.

```python
def current_streak(dates):
    # count backwards from today while each day is in the set
    done = set(dates)
    day = date.today()
    streak = 0
    while day.isoformat() in done:
        streak += 1
        day -= timedelta(days=1)
    return streak
```

## Questions for office hours

- Is a JSON file okay or does Hale want us to use SQLite?
- How should I test functions that depend on today's date? (Maybe pass `today` in as a parameter.)
