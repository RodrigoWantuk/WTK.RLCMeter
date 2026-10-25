# Installed production resources

[production-resources.zip](production-resources.zip) contains the22891-byte
`product.wrp2` physically installed on the first board, its entry summary, source
manifest and SIL Open Font License. It includes EN/PT-BR text, three bitmap font
roles and optional READY artwork. Schema2/API4 matches the current production
resource contract. Source manifest and six source JSON hashes are recorded in
[the physical installation evidence](../../../docs/review/bringup-2026-10-24/resource-install.json).

Pack SHA256:
`1048f3baa7942354e1c3d84b9b5cb361e7e3bd7347f221fdc2d3b2660b6ee9b4`.
ZIP SHA256:
`bf7427a2e4f8e000ba839801f327b25c978cf02d466f7a5641fcc971a4b3a582`.

This is external W25Q data, separate from the internal STM32 firmware. The
existing PRODUCT PLC1 receiver writes only the capacity-relative resource
partition. On the fitted4MiB part, this pack used six sectors at0x000000 and
did not alter the32KiB mutable tail starting0x3F8000. Full physical pack readback
matched exactly; PRODUCT admitted resources again after reset, with calibration
still required. The standard BRINGUP image was restored afterward.

Rebuild from the repository root:

```powershell
python Firmware/tools/resource_pack_tool.py bundle Firmware/assets/resource_manifest.json -o Firmware/build/resources/product.wrp2 --summary Firmware/build/resources/product.json
python Firmware/tools/resource_pack_tool.py inspect Firmware/build/resources/product.wrp2
```

For this board's polled-UART receiver, the successful transfer used byte flushing,
10ms per byte and10s settling after BEGIN, with one validated ACK per frame.
An earlier5ms attempt lost bytes during reception. The generic unpaced CLI
upload path is not bench-validated by this installation; retain the bounded
paced procedure and independent byte readback. See the
[operation report](../../../docs/review/bringup-2026-10-24/Resource-Install.md).
