# Security policy

## Supported versions

Only the latest [release](https://github.com/gufranco/fdswriteunlock/releases/latest) receives fixes.

## Reporting a vulnerability

Report it privately through [GitHub private vulnerability reporting](https://github.com/gufranco/fdswriteunlock/security/advisories/new), not in a public issue. Include the release version, what you observed and how to reproduce it. You get a first answer within seven days.

## Verifying a release

Each release attaches `fdswriteunlock.hex`, its SHA-256, the Sigstore bundle of its signed build provenance and an SPDX SBOM attested to the same hex. Before flashing a chip, check the checksum and the provenance:

```sh
sha256sum -c fdswriteunlock.hex.sha256
gh attestation verify fdswriteunlock.hex --repo gufranco/fdswriteunlock
```
