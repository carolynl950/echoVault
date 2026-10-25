---
tags: [stat200]
---
# Mean, median, and spread

## Center

- Mean: add everything up and divide by n. Gets pulled by outliers.
- Median: the middle value when sorted. Resistant to outliers.
- Mode: the most common value.

If the data is skewed right (a long tail of big values, like income), mean > median. That's why news reports use median household income.

## Spread

- Range: max − min. Super sensitive to outliers.
- IQR: Q3 − Q1, the middle 50%. Outlier rule: anything below Q1 − 1.5·IQR or above Q3 + 1.5·IQR.
- Standard deviation: roughly the typical distance from the mean. Square the deviations, average them (divide by n − 1 for a sample), then take the square root.

Why n − 1? A sample tends to underestimate how spread out the population is, and dividing by n − 1 corrects for that (Bessel's correction). Dr. Brandt said to just accept it for now.

## Which to report

Skewed data or outliers: median + IQR. Roughly symmetric: mean + standard deviation.
