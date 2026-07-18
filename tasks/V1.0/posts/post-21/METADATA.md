---
post_number: 21
title: "Code Review Checklist for MCP Security — What to Look For"
topic: Security code review checklist for MCP C++ servers
assigned_to: CONDUCTOR
state: Draft
created_date: 2026-05-31
draft_date: 2026-07-18
deadline: 2026-06-20
---

## Post Metadata
- Post Number: 21
- Series Part: 5B — Code Review & Static Analysis
- Publication Date: 2026-06-20 (3-day cadence from Post 20)
- Migration Date: Sun May 31 03:36:16 PM +08 2026
- Draft Date: 2026-07-18

## Files Present
- 03-blog-draft.md ✓
- 04-scanner-report.md ✓
- 02-auditor-report.md ✓ (pending CVE verification)

## CVEs Used
- CVE-2014-0160 (Heartbleed) — hook only, no CVSS claimed
- CVE-2022-1388 (F5 BIG-IP) — CVSS 9.8
- CVE-2023-22515 (Confluence) — CVSS 10.0
- CVE-2021-41773 (Apache httpd) — CVSS 7.5 / 9.8
- CVE-2021-44228 (Log4Shell) — CVSS 10.0
- CVE-2023-46604 (ActiveMQ) — CVSS 10.0

## Checklist Categories
1. Authentication — AuthenticationHandler
2. Authorization — RoleBasedAccessControl
3. Input Validation — InputValidationHandler + PathValidator
4. Output Sanitization — ResponseSanitizer
5. Tool Execution Safety — ToolExecutionGuard
6. Secrets & Configuration — AuthConfig
