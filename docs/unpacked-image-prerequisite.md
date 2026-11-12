# Supported private analysis-image prerequisite

This source build accepts one specific original-layout decrypted analysis image:

- Original game AoT.exe SHA256:
  67fc8121f65efb7f72274f7ca168ce9e30ad68b03d8e9f05a3d56c3b9ccce917
- Required private analysis-image SHA256:
  0a2d9e152db3bfe298017029804fe172d2a20bc51573feee874040ca3b1684d7

Check a locally prepared input before building:

```sh
sha256sum /path/to/private/supported-unpacked.exe
```

Do not launch this image or replace the installed game executable with it.
Ordinary unpackers that rewrite headers, entrypoint, sections or .bind do not
necessarily produce this required hash. Supplying the original packed EXE fails.

Actual preparation evidence from the development session: an earlier normally
running game supplied decrypted .text in its original file layout. A later private
offline recovery independently reproduced the exact full-file hash using format
logic reviewed against atom0s/Steamless v3.1.0.5, commit
cd770bf9749d3e4f438d23ac643917ad1a804257:
https://github.com/atom0s/Steamless/tree/cd770bf9749d3e4f438d23ac643917ad1a804257

The recorded recovery kept all original bytes/headers/entrypoint/section mapping
outside raw offset0x400, length0x863d9f, and replaced that span with its decrypted
original .text contents. It did not rebuild a runnable unpacked executable.
Private recovery used Python3 with the installed cryptography package for AES;
no upstream release executable was downloaded or run. Upstream CLI targets
.NETFramework4.5.2, but that CLI was not used. Historical Python/cryptography
versions were not pinned in the recovery record, so no untested version claim
is made. This package neither contains nor runs that private recovery code.

Upstream source had CC BY-NC-ND4.0 terms recorded in the local provenance notes.
No rights to distribute a private adaptation, decrypted image or original game
bytes are implied. No unpacker/source adaptation is bundled here.

Residual gap: this is a precise supported-input contract and actual preparation
provenance, not a tested clean-room unpacking tutorial. Users need a legitimately
prepared image matching the required hash. Preparing it from a fresh installation
is not currently a self-contained supported command in this draft. Do not bypass
the hash checks or substitute a differently laid-out image.
