# Security policy

## Supported versions

Only the latest [release](https://github.com/gufranco/fd3206-write-unlock/releases/latest) receives fixes.

## What counts as a security issue here

The firmware runs on an ATtiny inside a disk drive, with no network, user data or secrets. A report is in scope when it shows that the firmware or a published image can:

- drive a write line high, or pull one low while /READY, /WRITABLE MEDIA or /WRITE GATE is high, in a way that can damage the drive or erase a disk;
- be replaced or altered between the release and the user, for example a release image whose SHA-256 or build-provenance attestation does not verify;
- run build or release automation in this repository with more privilege than its workflow declares.

Ordinary defects belong in a [bug report](https://github.com/gufranco/fd3206-write-unlock/issues/new?template=bug.yml), and results from a real drive in a [drive result](https://github.com/gufranco/fd3206-write-unlock/issues/new?template=drive-result.yml).

## Reporting a vulnerability

Report it privately through [GitHub private vulnerability reporting](https://github.com/gufranco/fd3206-write-unlock/security/advisories/new), not in a public issue. Include the release version, what you observed and how to reproduce it. You get a first answer within seven days.

## Verifying a release

Each release attaches one image per chip, `fd3206-write-unlock-attiny2313a.hex` and `fd3206-write-unlock-attiny4313.hex`, each with its SHA-256, plus the Sigstore bundle of their signed build provenance and an SPDX SBOM attested to both images. Before flashing a chip, check the checksum and the provenance of its image:

```sh
sha256sum -c fd3206-write-unlock-attiny2313a.hex.sha256
gh attestation verify fd3206-write-unlock-attiny2313a.hex --repo gufranco/fd3206-write-unlock
```
