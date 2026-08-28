# Security and Safety Policy

## Supported version

Security and safety reports are accepted for the current V26 alpha package and
the current default-branch source. Older rollback packages are retained for
recovery but do not receive normal feature work.

## Reporting privately

Do not publish credentials, device identifiers, private show files, client
information, or an exploitable vulnerability in a public issue.

Use GitHub's **Report a vulnerability** option on the repository Security page
when it is available. If that option is unavailable, open a minimal issue that
requests private maintainer contact without including sensitive details.

Include the affected commit or package, exact QLC+ build, operating system,
impact, and safe reproduction conditions.

## Live-output safety reports

Unexpected DMX output, failed blackout, stale output after disconnect, unsafe
reconnect behavior, or control of the wrong universe/port is treated as a
safety-critical defect.

Stop testing, disconnect hazardous loads, record only non-sensitive evidence,
and report the issue. Do not attempt repeated live reproduction with fog,
sparks, lasers, pyrotechnics, or other high-consequence devices attached.

## Scope

This policy covers source and packages produced by this repository. It does not
cover vulnerabilities in QLC+, Qt, Windows, VirtualDJ, manufacturer drivers, or
SoundSwitch itself, although a compatibility impact may still be documented
here.
