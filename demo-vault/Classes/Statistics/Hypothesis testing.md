---
tags: [stat200, exam2]
---
# Hypothesis testing

## The setup

- Null hypothesis H0: nothing is going on, no effect, no difference
- Alternative Ha: what we're trying to find evidence for
- Pick α (significance level) BEFORE looking at the data, usually 0.05

## p-values

The p-value is the probability of getting data at least this extreme IF the null hypothesis were true. So a tiny p-value means: if nothing were really going on, seeing numbers like these would be a weird coincidence. That makes us doubt the null.

If p < α, reject H0. If p ≥ α, fail to reject H0 (NOT "accept H0", Dr. Brandt takes points off for that).

What the p-value is NOT: it's not the probability that the null is true, and it's not the probability that the effect is real. A small p doesn't mean the effect is big, either. With a huge sample, a tiny meaningless difference can still come out "significant".

## Type I and Type II errors

- Type I: rejecting H0 when it's actually true (false alarm). Probability = α.
- Type II: failing to reject H0 when it's actually false (missed it). Probability = β.

Courtroom analogy: H0 = innocent. Type I = convicting an innocent person, Type II = letting a guilty person go.

## Power

Power = 1 − β = the probability that the test correctly rejects a false null, meaning it catches an effect that really exists. You want at least 0.8.

Power goes up with:
- a larger sample size
- a bigger true effect
- a larger α (but then more Type I errors)
- less variability in the data

This is why studies do a power analysis before collecting data: to figure out how many subjects they need so they don't miss a real effect.
