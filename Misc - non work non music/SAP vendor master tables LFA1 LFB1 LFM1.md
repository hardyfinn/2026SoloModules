---
title: SAP vendor master tables LFA1 LFB1 LFM1
date: 2026-11-03
source: Misc - non work non music · thread "SAP table LFB1"
topic: Tech & Work
tags: [sap, vendor-master, s4hana, data-tables]
summary: |
  LFA1 holds client-wide vendor general data (one row per vendor), LFB1 the accounting data per company code (key LIFNR + BUKRS), and LFM1 the purchasing data per purchasing organisation (key LIFNR + EKORG).
  In S/4HANA vendors are Business Partners, but these tables are still filled via CVI and still read.
---

# SAP vendor master tables LFA1 LFB1 LFM1

## Summary
LFA1 holds client-wide vendor general data, LFB1 the accounting data per company code, and LFM1 the purchasing data per purchasing organisation.
In S/4HANA vendors are Business Partners, but these tables are still filled and read.

## Question
> what is contained in SAP table LFB1

## Answer
LFB1 is the vendor master's **company code** data. You create a vendor once in LFA1 (name, address, tax numbers), then extend it to each company code that deals with it, and each extension is one row in LFB1. Its key is client + vendor number (LIFNR) + company code (BUKRS).

It holds the accounting side of the vendor. That means the reconciliation account (AKONT), payment terms (ZTERM), allowed payment methods (ZWELS), payment block (ZAHLS), house bank (HBKID), the double invoice check (REPRF), withholding tax, the accounting clerk, and posting block and deletion flags per company code.

Related tables include LFA1 (general data), LFM1 (purchasing org data), LFBK (bank details), LFB5 (dunning) and LFBW (withholding tax). The customer-side equivalents are KNA1 and KNB1. In S/4HANA, vendors are maintained as Business Partners, but LFB1 is still filled in and still read.

I've written a fuller field-by-field reference here: [lfb1-vendor-company-code-data.md](/mnt/project-files/sap/lfb1-vendor-company-code-data.md)

### LFB1 field reference (from project file sap/lfb1-vendor-company-code-data.md)

#### SAP table LFB1: Vendor master (company code data)

LFB1 holds the part of a vendor (supplier) master record that is specific to one **company code**. A vendor is created once at the general level (table LFA1), and then "extended" to each company code that does business with it. Each extension is one row in LFB1. This is where the accounting side of the vendor lives: which reconciliation account it posts to, how it gets paid, and its payment terms for that company code.

In S/4HANA, vendors are Business Partners, but LFB1 is still populated (via Customer/Vendor Integration, CVI) and is still the table most reports and programs read.

##### Key fields

| Field | Meaning |
|---|---|
| MANDT | Client |
| LIFNR | Vendor account number |
| BUKRS | Company code |

So one vendor with three company codes has three LFB1 rows.

##### Main data fields

**Account management**

| Field | Meaning |
|---|---|
| AKONT | Reconciliation account in the general ledger (the G/L account that vendor postings roll up to) |
| ZUAWA | Sort key (how line items are sorted/assigned, e.g. by document date) |
| ALTKN | Previous (legacy) account number |
| BEGRU | Authorization group |
| FDGRV | Planning group (cash management) |
| QSSKZ / QLAND | Withholding tax code / withholding tax country |
| MINDK | Minority indicator |
| ERNAM / ERDAT | Created by / created on |

**Payment transactions**

| Field | Meaning |
|---|---|
| ZTERM | Terms of payment key |
| ZWELS | Allowed payment methods (e.g. "TC" = transfer, cheque) |
| ZAHLS | Payment block key |
| HBKID | House bank used to pay this vendor |
| REPRF | Check for double invoices |
| TOGRU | Tolerance group |
| XVERR | Clearing with customer allowed (offset vendor against customer) |
| LNRZB | Alternative payee |
| XPORE | Pay all items individually |
| UZAWE | Payment method supplement |
| KULTG | Check cashing time (days) |
| GUZTE | Payment terms for credit memos |

**Correspondence**

| Field | Meaning |
|---|---|
| BUSAB | Accounting clerk |
| EIKTO | Our account number at the vendor |
| ZSABE | Clerk at the vendor |
| TLFNS / TLFXS | Accounting clerk's phone / fax at the vendor |
| INTAD | Clerk's internet address |
| MGRUP | Dunning notice grouping key |
| XAUSZ | Send account statements indicator |

**Status flags**

| Field | Meaning |
|---|---|
| SPERR | Posting block for this company code |
| LOEVM | Deletion flag for this company code |
| NODEL | Deletion block |

##### Related tables

- **LFA1**: vendor general data, one row per vendor (name, address, country, tax numbers, central blocks). Join on LIFNR.
- **LFB5**: dunning data per company code (key: LIFNR, BUKRS, MABER).
- **LFM1 / LFM2**: purchasing organization data (order currency, purchasing payment terms, incoterms). LFM1 is to purchasing what LFB1 is to accounting.
- **LFBK**: vendor bank details.
- **LFBW**: withholding tax types per company code (extended withholding tax).
- **LFC1 / LFC3**: vendor transaction figures / special G/L transaction figures.
- **BSIK / BSAK**: open / cleared vendor line items (in S/4HANA, compatibility views over ACDOCA/BSEG).
- **BUT000 / CVI_VEND_LINK**: Business Partner header and the link between Business Partner and vendor number (S/4HANA).

The customer-side equivalents are KNA1 (general) and KNB1 (company code).

##### Where you see it in the GUI

Transactions XK03 / FK03 (display vendor) or, in S/4HANA, BP with the role "FI Vendor": the "Company code data" screens. To look at the raw table, use SE16N or SE11 with LFB1.

## Question 2
> create create similar for LFA1

## Answer 2
Here's the LFA1 version: [lfa1-vendor-general-data.pdf](/mnt/project-files/sap/lfa1-vendor-general-data.pdf). It follows the same layout as the LFB1 PDF.

LFA1 has one row per vendor, keyed only by client and vendor number (LIFNR). It holds the client-wide data: name and address, account group (KTOKK), tax and VAT numbers, and the central posting, purchasing and deletion blocks. The PDF lists the fields and the related tables.

### LFA1 field reference (from project file sap/lfa1-vendor-general-data.md)

#### SAP table LFA1: Vendor master (general data)

LFA1 holds the **general**, client-wide part of a vendor (supplier) master record. There is exactly one row per vendor, and it applies across all company codes and purchasing organizations. It carries who the vendor is: name, address, country, tax numbers, and central blocks. The accounting data per company code sits in LFB1, and the purchasing data per purchasing organization sits in LFM1.

In S/4HANA, vendors are Business Partners (BUT000 and related tables), but LFA1 is still populated through Customer/Vendor Integration (CVI) and is still what most reports and programs read.

##### Key fields

| Field | Meaning |
|---|---|
| MANDT | Client |
| LIFNR | Vendor account number |

##### Main data fields

**Control and classification**

| Field | Meaning |
|---|---|
| KTOKK | Vendor account group (controls number range and which fields are required) |
| KUNNR | Customer number, when the vendor is also a customer (used for clearing) |
| KONZS | Group key (corporate group) |
| VBUND | Trading partner (company ID for intercompany consolidation) |
| BEGRU | Authorization group |
| BRSCH | Industry key |
| LNRZA | Alternative payee (central) |
| XZEMP | Alternative payee allowed in document |
| FISKN | Account number of the master record with fiscal address |
| ERNAM / ERDAT | Created by / created on |

**Name and address**

| Field | Meaning |
|---|---|
| ANRED | Title (Company, Mr, Ms) |
| NAME1 to NAME4 | Name lines |
| SORTL | Sort field / search term |
| STRAS | Street and house number |
| PFACH / PSTL2 | PO box / PO box postal code |
| PSTLZ | Postal code |
| ORT01 / ORT02 | City / district |
| REGIO | Region (state, province, county) |
| LAND1 | Country key |
| SPRAS | Language key |
| TELF1 / TELF2 / TELFX | Telephone 1 / telephone 2 / fax |
| ADRNR | Address number, the link to central address management (ADRC) |

**Tax data**

| Field | Meaning |
|---|---|
| STCD1 to STCD5 | Tax numbers 1 to 5 (meaning depends on country) |
| STCEG | VAT registration number |
| STKZU | Liable for VAT |
| STKZN | Natural person |
| STKZA | Equalization tax indicator |
| FITYP | Tax type |
| STCDT | Tax number type |
| TXJCD | Tax jurisdiction (US and similar) |

**Status flags and blocks**

| Field | Meaning |
|---|---|
| SPERR | Central posting block (all company codes) |
| SPERM | Central purchasing block (all purchasing organizations) |
| SPERZ | Payment block |
| LOEVM | Central deletion flag |
| NODEL | Central deletion block |
| SPERQ | Block due to quality reasons |

**Other**

| Field | Meaning |
|---|---|
| DLGRP | Service agent procedure group |
| WERKS | Plant (when the vendor is a plant, e.g. stock transfers) |
| LTSNA | Vendor sub-range relevant |
| DUEFL | Data transfer status |
| SCACD | Standard carrier access code (transport) |

##### Related tables

- **LFB1**: company code data (reconciliation account, payment terms, payment methods). Join on LIFNR. One row per vendor and company code.
- **LFM1 / LFM2**: purchasing organization data (order currency, purchasing terms, incoterms).
- **LFBK**: vendor bank details (country, bank key, account, IBAN via TIBAN).
- **LFB5**: dunning data per company code.
- **LFBW**: withholding tax types per company code.
- **LFAS**: EU VAT registration numbers (several per vendor).
- **LFZA**: permitted alternative payees.
- **WYT3**: partner functions (ordering address, invoicing party, etc.).
- **ADRC / ADR6**: central address and email address, linked via ADRNR.
- **BUT000 / CVI_VEND_LINK**: Business Partner header and the link between Business Partner and vendor number (S/4HANA).

The customer-side equivalent is KNA1, with KNB1 for company code data.

##### Where you see it in the GUI

Transactions XK03 (display vendor centrally), FK03 (accounting view) or MK03 (purchasing view), or in S/4HANA, BP with the role "FI Vendor" or "Supplier": the general data screens. To look at the raw table, use SE16N or SE11 with LFA1.

## Question 3
> what does table LFM1 do?

## Answer 3
LFM1 holds the vendor's **purchasing organization** data. It's the purchasing counterpart of LFB1. LFB1 is how accounting deals with a vendor in each company code, and LFM1 is how purchasing deals with it in each purchasing organization. Its key is client + vendor (LIFNR) + purchasing organization (EKORG).

The main fields are:
- order currency (WAERS)
- purchasing payment terms (ZTERM), which default into purchase orders
- incoterms (INCO1/INCO2)
- minimum order value (MINBW)
- the GR-based invoice verification flag (WEBRE)
- order acknowledgment required (KZABS)
- automatic PO allowed (KZAUT)
- ERS / evaluated receipt settlement (XERSY)
- purchasing group (EKGRP)
- planned delivery time (PLIFZ)
- the vendor's salesperson and phone (VERKF/TELF1)
- a purchasing block (SPERM) and deletion flag (LOEVM) per purchasing organization

LFM2 sits one level below it. It holds the same kind of data at a finer level, such as per plant or per vendor sub-range. You see LFM1 in MK03, or in XK03 under the purchasing data screens.

Want a PDF for this one too?

## Sources
- Project file: sap/lfb1-vendor-company-code-data.md
- Project file: sap/lfa1-vendor-general-data.pdf
