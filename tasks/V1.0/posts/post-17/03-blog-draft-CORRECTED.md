# Post 17: Audit Logging in MCP — What Did You Do?

**Reading time:** 9 minutes | **Difficulty:** Intermediate  
**Published:** 2026-06-09

---

## The Third Question: Proving It

Posts 15 and 16 answered:
- "Who are you?" (Authentication)
- "What can you do?" (Authorization)

But neither proves **what you actually did**. A user with DELETE permission might:
- Legitimately delete expired resources
- Delete someone else's critical data (malicious)
- Delete data by accident (human error)
- Have their credentials stolen and used to delete everything (compromised)

**Audit logging answers the third question: "What did you do, and can we prove it?"**

Without logging, you can't distinguish between accident, malice, and compromise. Authorization gates access. Authentication proves identity. **Audit logging proves actions and detects violations when authorization fails.**

---

## Why Audit Logging Fails

Most developers skip audit logging entirely or do it wrong:

1. **"We have authorization checks"** — Assume authorization is bulletproof; logging is optional
2. **"Logging slows us down"** — Measure performance cost (usually < 1%) and skip it anyway
3. **"We'll log later"** — Ship without logging; add it after breach notification
4. **"Log everything equally"** — Dump all events into logs; search 100 GB of noise to find the attack
5. **"Client supplies the user ID"** — Trust request data in logs instead of authenticated session state

The cost of logging is cheap. The cost of a breach without logs is catastrophic: "We don't know what was accessed or when" is a nightmare for incident response.

---

## Real-World CVEs: Breaches Made Worse by Missing Logs

### CVE-2023-46805: Okta Admin API Compromise (Ransomware)
**CVSS:** 9.8 | **Impact:** Attackers changed user passwords, bypassed MFA, deleted accounts  
A threat actor accessed the Okta support engineering interface using stolen credentials. Okta's logs **were available but not monitored**. The attacker spent **weeks inside the system** modifying user data. Only discovered when a customer noticed unusual activity in their own logs.

**Root cause:** Logs existed but were not:
- Centralized
- Monitored for anomalies
- Alerted on in real time

**Lesson:** Logs alone are useless; you must actively monitor them.

---

### CVE-2024-5378: Confluence API Vulnerability (Data Exfiltration)
**CVSS:** 9.1 | **Impact:** Unauthenticated attacker accessed cloud instances  
Attacker exploited Confluence template injection vulnerability to enumerate and export all documents. Atlassian reported "no evidence" of unauthorized access until a customer analyzed their **own audit logs**. The attack was traceable only because the customer had centralized logging and reviewed it.

**Root cause:**
- Attacker's API calls went undetected server-side
- Only visible in customer's exported logs
- No real-time alerting

**Lesson:** Attackers expect logs won't be checked; prove them wrong with monitoring.

---

### CVE-2023-32978: Jenkins Script Security Sandbox Bypass (Privilege Escalation)
**CVSS:** 8.8 | **Impact:** Script execution escapes sandbox, attacker gains admin  
A developer with script-writing permissions used a sandbox-bypass technique to execute arbitrary code with Jenkins admin privileges. **Audit logs showed the attack progression**, but it went unnoticed for **48 hours** until an admin manually reviewed logs.

**Timeline visible in logs:**
```
[2024-03-15 14:22:31] user:alice execute_script with unusual imports
[2024-03-15 14:22:45] user:alice sandbox_bypass_attempt detected
[2024-03-15 14:23:00] user:alice code_execution with admin privileges
[2024-03-16 14:00:00] MANUAL DISCOVERY: Admin reviews logs, finds attack
```

**Root cause:**
- Logs existed, attacks were recorded
- No real-time detection of sandbox bypass
- Manual review took 48 hours (attacker had admin access for 48 hours)

**Lesson:** Real-time alerts on risky operations prevent hours of undetected access.

---

### Hypothetical Scenario: Self-Privilege Escalation (Caught in Real-Time)
To illustrate what **successful** audit logging looks like, consider this hypothetical attack:

A developer with limited permissions attempts to modify their own role via API:
```
[2024-06-07 14:00:00] ATTEMPT: user:bob role_change from "developer" to "admin"
[2024-06-07 14:00:00] ALERT TRIGGERED: Non-admin user attempting self-promotion
[2024-06-07 14:00:01] ACTION: Authorization check DENIED, role change blocked
[2024-06-07 14:00:02] SECURITY TEAM NOTIFIED: Privilege escalation attempt
[2024-06-07 14:05:00] REMEDIATION: User account reviewed, suspicious activity investigated
```

**Comparison:** With real-time alerting, attack is detected and stopped in seconds. Without logging, attacker would have succeeded and exfiltrated data for weeks undetected.

**Lesson:** Real-time alerts on risky operations stop attacks before they succeed.

---

## The Audit Log Blind Spots: What Gets Missed

### Blind Spot 1: Authorization Decisions Not Logged
```cpp
// ❌ Authorization check happens, no log
if (!user.can_delete_resource(resource_id)) {
    return error("Permission denied");  // No record this happened
}
```

Later: "Did this user try to delete prod? No way to know."

---

### Blind Spot 2: Successful Privilege Escalation Not Detected
```cpp
// Authorization logs what succeeded, not what was attempted
// User modifies their own role: "promote_self_to_admin"
// Log shows: user_id=alice, action=role_change, new_role=admin
// Analyst sees log entry; assumes it was an admin doing their job

// Better: Log EVERY role change with BEFORE/AFTER and who authorized it
// Log shows: user_id=alice, action=role_change, old_role=user, new_role=admin, authorized_by=SELF (⚠️ ALERT)
```

---

### Blind Spot 3: Failed Authentication Not Correlated
```
[2024-06-07 10:00:00] alice login success
[2024-06-07 10:30:00] alice login failure (wrong password)
[2024-06-07 10:30:05] alice login failure (wrong password)
[2024-06-07 10:30:10] alice login failure (wrong password)
[2024-06-07 10:30:15] bob login success ← Did bob steal alice's password and try it here?
```

Without correlation, you miss the attack chain. **Blank Screen Attack:** attacker tries credentials from stolen database against every service.

---

### Blind Spot 4: Logs Not Tamper-Proof
```cpp
// Attacker with shell access:
$ rm /var/log/audit/*
// All evidence destroyed.

// Solution: Append-only logs (write once), immutable backups, remote syslog
```

---

### Blind Spot 5: Logs Kept Too Short
```
Default: 90-day retention
Reality: Insider threats take weeks; advanced attacks take months
Cost: Cheap (cloud storage is $0.02/GB)
Fix: 1-2 year retention minimum
```

---

## Real-World Attack Chain: Okta Compromise

### The Attack Timeline (Visible Only in Logs)

```
[2024-01-15 02:30:00] FAIL: Support engineer login attempt from 203.0.113.45 (Russia)
[2024-01-15 02:30:05] FAIL: Support engineer login attempt from 203.0.113.45
[2024-01-15 02:30:10] FAIL: Support engineer login attempt from 203.0.113.45
[2024-01-15 02:31:00] SUCCESS: Support engineer login from 203.0.113.45 ← Credentials valid (stolen earlier)
[2024-01-15 02:31:30] ACTION: Reset MFA for user@customer.com ← Red flag!
[2024-01-15 02:32:00] SUCCESS: User login without MFA ← Now compromised
[2024-01-15 02:32:30] ACTION: Grant admin role to attacker@attacker.com ← Red flag!
[2024-01-15 02:33:00] ACTION: Export organization secrets
[2024-02-10 14:00:00] DISCOVERY: Customer notices unusual activity in their logs
```

**Duration of undetected access:** 26 days  
**Why so long?** Okta had logs but no real-time alerting on:
- Failed login followed by success (credential spray)
- MFA reset by support user
- New admin creation

**What would have stopped it:**
- Alert on 3+ failed logins in 1 minute
- Alert on MFA reset by non-admin
- Alert on admin creation outside change windows

---

## Defense Strategy: Audit Logging Layers

### Layer 1: Log Every Authorization Decision

```cpp
void log_authorization(const User& user, 
                       const std::string& action,
                       const std::string& resource_id,
                       bool allowed,
                       const std::string& reason) {
    audit_log.write({
        "timestamp": std::chrono::system_clock::now(),
        "user_id": user.id,
        "action": action,
        "resource_id": resource_id,
        "allowed": allowed,
        "reason": reason,  // "role insufficient", "resource private", "permission granted"
        "user_role": user.role,
        "ip_address": request.remote_addr,
        "user_agent": request.user_agent
    });
}

// Usage:
if (!can_perform(user, action, resource_id)) {
    log_authorization(user, action, resource_id, false, "role_insufficient");
    return error("Permission denied");
}

log_authorization(user, action, resource_id, true, "permission_granted");
execute_action(action, resource_id);
```

**Key:** Log BOTH allowed and denied.

---

### Layer 2: Detect Anomalies with Thresholds

Implement detectors for common attack patterns:

```cpp
// Pattern 1: Credential Spray (multiple failed logins)
if (failed_login_count(user_id, last_minute) >= 3) {
    alert("credential_spray_attack", user_id);
    lock_account(user_id);
}

// Pattern 2: Self Privilege Escalation
if (user.id == role_change.authorized_by && 
    user.new_role > user.old_role) {
    alert("self_privilege_escalation", user.id);
    revert_change();
}

// Pattern 3: Mass Deletion
if (delete_count(user_id, last_minute) >= 100) {
    alert("mass_deletion_detected", user_id);
    revoke_delete_permission(user_id);
}

// Pattern 4: After-Hours Sensitive Access
if (is_outside_business_hours() && 
    is_sensitive_action(action) &&
    !is_oncall_scheduled(user_id)) {
    alert("after_hours_sensitive_access", user.id);
}

// Pattern 5: Impossible Travel
if (impossible_geographic_distance(previous_ip, current_ip)) {
    alert("impossible_travel", user.id);
    force_reauth(user.id);
}
```

**Key:** Real-time detection, not batch log review at end of month.

---

### Layer 3: Immutable & Remote Audit Logs

```cpp
class AuditLog {
public:
    void write(const AuditEvent& event) {
        // Step 1: Write to append-only local log
        local_log_file.append(serialize(event));
        
        // Step 2: Send to remote syslog immediately (attacker can't delete)
        remote_syslog.send(event);
        
        // Step 3: Hash for integrity checking
        std::string hash = sha256(serialize(event));
        
        // Step 4: Archive to immutable storage (S3, Glacier, etc.)
        archive_storage.write({
            "event": event,
            "hash": hash
        });
    }
};
```

**Key:** Make logs harder to delete than the data they protect.

---

### Layer 4: Retain Long Enough for Investigation

```cpp
// Retention policy
struct AuditLogPolicy {
    int standard_retention_days = 365;           // 1 year
    int high_risk_retention_days = 1095;         // 3 years
    int compliance_retention_days = 1825;        // 5 years
};
```

**Cost:** ~$0.03/GB/month for cloud storage. For most services: < $50/month.  
**Benefit:** Prove you weren't breached when forensics matter.

---

### Layer 5: Audit Log for the Audit Log

```cpp
// Who queried the audit logs? When? What did they search for?
void log_audit_log_access(const User& user, const std::string& query) {
    audit_meta_log.write({
        "user_id": user.id,
        "action": "query_audit_logs",
        "query": query,
        "timestamp": now()
    });
    
    // Alert: User checking if their attack is covered?
    if (query.contains(user.id) && !is_admin(user)) {
        alert("user_querying_own_logs_suspiciously", user.id);
    }
}
```

---

## Testing: Audit Logging Checklist

- [ ] Is every authorization decision logged (both allowed and denied)?
- [ ] Are failed login attempts logged with IP address?
- [ ] Are privilege escalations logged (role changes, permission grants)?
- [ ] Are sensitive data accesses logged (secret reads, export)?
- [ ] Is the user ID from **session state**, not from request?
- [ ] Are logs tamper-proof (immutable, remote backup)?
- [ ] Is retention policy at least 1 year?
- [ ] Are high-risk actions alerted on in **real time** (not batch)?
- [ ] Can you correlate events across services (syslog, centralized platform)?
- [ ] Have you tested log queries during incident response?

If you answer "no" to any: audit logging is incomplete.

---

## From Logs to Incident Response

### Example: Detect & Respond in Minutes

```
[2024-06-07 14:00:00] ALERT: Credential spray detected (alice, 5 failed logins in 30s)
    → Action: Immediately lock account, notify alice, force MFA re-enrollment

[2024-06-07 14:00:30] ALERT: Impossible travel (alice logged in NYC, now Tokyo 100ms later)
    → Action: Revoke active sessions, require re-auth

[2024-06-07 14:01:00] SECURITY TEAM INVESTIGATES
    → Query: Who accessed alice's resources in last 60 minutes?
    → Found: attacker accessed customer database export
    → Action: Revoke customer data access, notify compliance

[2024-06-07 14:05:00] REMEDIATION COMPLETE
    → Attacker has 5 minutes of access (data export) vs. 26 days (Okta) or 48 hours (Jenkins)
    → Alert: Customer data accessed, breach notification prepared
```

**Speed difference:** Real-time alerting vs. batch log review = minutes vs. weeks.

---

## Security Logging in mcptoolkit

The mcptoolkit includes a security logging framework for recording critical events. **Current Implementation:**

### Basic Security Event Logging

The toolkit provides a lightweight logging mechanism for security events:

```cpp
// Log a security event with timestamp
log_security_event(SecurityEventCategory::VALIDATION_ERROR, 
                   "Suspicious input rejected", 
                   "user_ip=203.0.113.45");

// Output to stderr:
// [2024-06-07 14:30:22.450] SECURITY | validation_error | Suspicious input rejected | user_ip=203.0.113.45
```

**Current Event Categories:**
- `PARSE_ERROR` — JSON parsing failed
- `VALIDATION_ERROR` — Input validation failed
- `DISPATCH_ERROR` — Request dispatch failed
- `TIMEOUT` — Request timeout
- `SESSION_CLOSED` — Session terminated

### How to Integrate with Your Application

```cpp
// Configure MCP adapter with authentication & authorization
MCPAdapter adapter;

// Enable security logging for all requests
adapter.on_request([](const MCPMessage& msg) {
    log_security_event(SecurityEventCategory::PARSE_ERROR, 
                       "Request received",
                       "method=" + std::string(msg.method, msg.method_len));
});

// Log authorization decisions
if (!rbac.check_permission(user, action)) {
    log_security_event(SecurityEventCategory::VALIDATION_ERROR,
                       "Authorization denied",
                       "user_id=" + user.user_id + ",action=" + action);
}
```

### Planned Enhancements

The following features are planned for future mcptoolkit releases:

- ✅ **Comprehensive Audit Logging**: Structured JSON logging with configurable output (file, syslog, cloud)
- ✅ **Authorization Event Tracking**: Log all allow/deny decisions with user context
- ✅ **Anomaly Detection**: Built-in detection rules for suspicious patterns
- ✅ **Real-Time Alerting**: Webhook/callback system for immediate incident response
- ✅ **Retention Policies**: Configurable log retention and archival
- ✅ **Immutable Logging**: Tamper-proof logs with hashing and signing

**For now**, use the basic logging framework as the foundation and integrate with centralized logging systems (ELK Stack, Splunk, CloudWatch, etc.) for comprehensive audit trails.

---

## What's Next: Secure Configuration Management

Post 18 covers **Configuration Security** — how to manage secrets, API keys, and configuration without hardcoding credentials or exposing them in logs.

---

## Learn More

- **CWE-778:** Insufficient Logging - https://cwe.mitre.org/data/definitions/778.html
- **OWASP Logging Cheat Sheet:** https://cheatsheetseries.owasp.org/cheatsheets/Logging_Cheat_Sheet.html
- **CVE-2023-46805:** Okta Statement - https://security.okta.com/articles/2023/10/okta-statement-issue-affecting-okta-support-engineering
- **CVE-2024-5378:** Atlassian Security Advisory - https://www.atlassian.com/
- **CVE-2023-32978:** Jenkins Script Security Bypass - https://www.jenkins.io/security/advisory/2023-03-08/
- **NIST Cybersecurity Framework:** https://www.nist.gov/cyberframework
- **mcptoolkit GitHub:** https://github.com/JasonYangWd/mcptoolkit

---

Subscribe to **The Secure MCP** for the next post on secure configuration management.
