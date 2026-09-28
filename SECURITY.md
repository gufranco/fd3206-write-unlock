# Security policy

## Supported versions

Only the latest [release](https://github.com/gufranco/fd3206-write-unlock/releases/latest) receives fixes.

## Reporting a vulnerability

Report it privately through [GitHub private vulnerability reporting](https://github.com/gufranco/fd3206-write-unlock/security/advisories/new), not in a public issue. Include the release version, what you observed and how to reproduce it. You get a first answer within seven days.

## Verifying a release

Each release attaches `fd3206-write-unlock.hex`, its SHA-256, the Sigstore bundle of its signed build provenance and an SPDX SBOM attested to the same hex. Before flashing a chip, check the checksum and the provenance:

```sh
sha256sum -c fd3206-write-unlock.hex.sha256
gh attestation verify fd3206-write-unlock.hex --repo gufranco/fd3206-write-unlock
```
