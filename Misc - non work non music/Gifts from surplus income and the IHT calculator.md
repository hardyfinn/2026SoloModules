---
title: Gifts from surplus income and the IHT calculator
date: 2026-11-04
source: Misc - non work non music · thread "Inheritance tax calculator"
topic: Tax & Money
tags: [uk-tax, inheritance-tax, gifts, pensions, spreadsheet]
summary: |
  Regular gifts from surplus income are still gifts but are immediately exempt from IHT under s21 IHTA 1984 if regular, from income and leaving normal living standards intact; records support the IHT403 claim.
  An Excel IHT calculator (gifts, late spouse's unused bands, pension pots from April 2027, "Reducing IHT" tab) is in tax/uk-inheritance-tax-calculator.xlsx.
---

# Gifts from surplus income and the IHT calculator

## Summary
Regular gifts from surplus income are still gifts, but are immediately exempt from inheritance tax under section 21 IHTA 1984 if they meet three conditions.
An Excel IHT calculator covering gifts, a late spouse's unused bands and pension pots was built alongside.

## Question
> Can you create an excel file for calculating inheritance tax in the uk? Should definately include gift relief, dead husband additions to tax free thresholds and pension pot.

> Please add to it with a tab that explains 'gifts from income' and legitimate ways to reduce tax burden in remain years of life.

## Question 2
> What is your view on the following, in light of uk legislation. A pensioner is spending £1000 less than her pension income each month. Can she give this extra income away without it being classed as a gift?

## Answer 2
Legally it's still a gift, but if it meets three conditions it's an **exempt** gift under the "normal expenditure out of income" rule (section 21 of the Inheritance Tax Act 1984). That means it's free of inheritance tax straight away. There's no 7-year wait and no upper limit, so £12,000 a year would be fine.

The conditions are:

1. **It's regular.** It has to be part of a habit, such as a monthly standing order to a grandchild. Saving the surplus and giving it away as one lump later doesn't count.
2. **It comes from income.** Her pension is income, so this is met. If the surplus sits in the bank for a long time, HMRC can start treating it as capital, so it's best to give it away as it builds up.
3. **She can still live as normal.** After the gifts, she must keep her usual standard of living without dipping into savings. Spending £1,000 a month less than she receives suggests she can.

What makes the claim work is records. Her executors claim the exemption on form IHT403 after she dies. A simple yearly note of income, spending and gifts is what wins it. If a gift fails the test, it's treated as an ordinary gift instead, and that's tax-free once she has lived 7 more years.

This is general information, not personal advice. I'm covering it in the "Reducing IHT" tab of the spreadsheet, which is nearly done.

## Answer 1 (the calculator)
The calculator is ready: [uk-inheritance-tax-calculator.xlsx](/mnt/project-files/tax/uk-inheritance-tax-calculator.xlsx). Every result is a live formula, so you only need to type into the yellow cells.

- **Gifts:** applies the exemptions (£3,000 annual with carry-forward, £250 small gifts, wedding gifts, gifts from income), uses up the nil-rate band oldest gift first, and applies taper relief to the tax.
- **Late spouse:** works out your late husband's unused nil-rate band and residence nil-rate band as percentages and adds them to yours.
- **Pensions:** pots count towards the estate only if death is on or after 6 April 2027. The summary shows how much extra tax that change causes.
- **Reducing IHT:** your new tab on gifts from income and other legitimate ways to cut the bill.

I've filled it in with an example (a widow dying in 2027, with a £300k pension) so you can see it working. Overwrite those figures with your own. I checked the rules on GOV.UK and tested the formulas against figures I worked out by hand.

Two points GOV.UK doesn't settle, so I've made assumptions and labelled them in the sheet:
- Pensions count towards the £2m test that reduces the residence band.
- The home's value for the residence band is measured after the mortgage.

## Sources
- Project file: tax/uk-inheritance-tax-calculator.xlsx

Related: [[UK inheritance tax changes 2024-2026]]
