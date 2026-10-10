# Host QR adapter dependency

Nayuki QR-Code-generator, official C implementation:
https://github.com/nayuki/QR-Code-generator/tree/master/c

Fetched for the host simulator only. Original MIT license notices are preserved in qrcodegen.c and qrcodegen.h. Files are byte-identical to fetched upstream blobs:

- qrcodegen.c: 34f1002501fa2bcb0a054f4311795b8cbb977f5b
- qrcodegen.h: 6bbc15762a8af18c792a344632e2e538974df1a0

Firmware continues to use its existing ESP QR component. The adapter does not change firmware dependencies.
