# Eval queries

Written right after the demo notes, before running any search, so the expected answers can't be tuned to what the search returns. A **hit** means the expected Passage is in the top 5. Each query runs three ways: vector only, keyword only, and hybrid (`$rankFusion`).

The first six are the **pinned examples** shown on the search page. Each one is meant to show off one search mode. The eval decides whether it actually does: if a pinned query doesn't win the way it should, swap it out instead of tweaking the notes.

## Decoys

Several notes share words but not meaning, so keyword search alone can be fooled:

| Word | Means this | Decoy |
|---|---|---|
| power | statistical power (Hypothesis testing) | power chords (Guitar), separation of powers (Constitutional Convention) |
| starter | sourdough starter (Cooking) | starter code (Project 2 - word counter) |
| cell | living cells (Biology) | spreadsheet cells (Monthly budget) |
| bias | sampling bias (Statistics) | Nick's bias as narrator (The Great Gatsby) |
| loop | `for`/`while` loops (Programming) | looper app (Guitar practice log) |
| fermentation | glycolysis without oxygen (Cell respiration) | sourdough starter (Cooking) — same idea, different context |

## Queries

| # | Query | Expected Passage (Note › heading) | Meant to show | Pinned |
|---|---|---|---|---|
| 1 | Plessy v. Ferguson | Reconstruction › End of Reconstruction | keyword (exact name) | ✅ |
| 2 | rubisco | Photosynthesis › Calvin cycle | keyword (rare term) | ✅ |
| 3 | my fingers hurt when I play | Learning chords › Calluses | semantic (no shared words) | ✅ |
| 4 | how do I know if my result is just a fluke | Hypothesis testing › p-values | semantic (no shared words) | ✅ |
| 5 | statistical power | Hypothesis testing › Power | hybrid (decoys: power chords, powers) | ✅ |
| 6 | how often to feed a starter | Sourdough starter › Feeding schedule | hybrid (decoy: starter code) | ✅ |
| 7 | where does the carbon dioxide we exhale come from | Cell respiration › Krebs cycle | semantic | |
| 8 | Great Compromise | Constitutional Convention › Great Compromise | keyword | |
| 9 | how to stop food from steaming in the pan | Searing and the Maillard reaction › Getting a good sear | semantic | |
| 10 | why do newspapers report median income | Mean median and spread › Center | hybrid | |
| 11 | maximum recursion depth exceeded | Recursion › How it actually runs | keyword (error message) | |
| 12 | cell turns red when negative | Monthly budget › My spreadsheet | hybrid (decoy: biology cells) | |
| 13 | can we trust the narrator | The Great Gatsby › Nick as narrator | semantic | |
| 14 | does sleeping more cause better grades | Stats project - sleep and grades › Interpretation (Notion) | hybrid | |
| 15 | what happens to enzymes when it gets too hot | Enzymes › What affects enzyme activity | semantic | |

Also counted as hits: #5, `2026-10-03` (the daily note about the power question on the midterm); #15, Enzyme lab report › Discussion (Notion).

## Results

_Filled in at the eval step._
