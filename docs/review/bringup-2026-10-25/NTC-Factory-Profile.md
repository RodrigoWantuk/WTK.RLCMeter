# Experimental NTC profile applied — October 25, 2026

Status: EXPERIMENTAL_ADJUSTMENT_DEPLOYED; NOT_PHYSICALLY_QUALIFIED.

At the owner's explicit direction, use the supplied 30.1-degC reference despite
possible hand/probe contact interference. The fit anchors are 23.5 degC/ADC1952
and sustained warm 30.1 degC/ADC1856. The earlier ADC1877 sample remains transient
diagnostic evidence. The monitoring log contains both stable windows and later
movement; no stabilization or contact uncertainty is relabelled qualification.
The thermometer model is not requested or stored. Its uncertainty is unknown.

The versioned profile is bound to UID `54ff70068370525760380567`. Effective Beta is
1284.690748 K and effective R25/Rtop is
0.891241456. These describe the supplied observations,
not verified intrinsic thermistor parameters. Wider-range extrapolation and
independent held-out temperatures remain unqualified. Original nominal builds
are unchanged; this unit's updates must select its factory profile explicitly.

All nine unit profiles compiled with strict warnings and existing C8 memory
gates. BRINGUP remains 65,512 bytes; BRINGUP_CAL remains 35,864 bytes. All 46
host CTests pass, including the actual C conversion at both anchors; deployment
tests report 54 passes and five optional integration skips. Manifests use schema2
to reject old tools, verify the LUT in the linked BIN, and bind programming to UID.

Source `0da88bfd9bf032da48e317f7ece9a03e9d719d5f` was programmed as BRINGUP_CAL and read
back byte-for-byte. The post-boot identity matches, SAFE/idle is confirmed,
safety faults zero and protocol counter zero. A fresh raw1975 snapshot reports
21.99884 degC; host interpolation of the embedded table agrees within 0.001 degC.
There was no simultaneous external reference for that post-boot reading, so this
verifies the installed conversion, not physical temperature accuracy.

Battery was 3.55546 V before programming and 3.54740 V afterward, still OK but
close to the 3.50-V LOW threshold; the owner was asked to recharge before the next
bench stage. No external W25Q settings/resources/calibration were programmed.
OSL remains absent: all 99 original captures and diagnostic repeats are preserved,
but three high-Z/10-kHz fits still lack observed gain overlap. The temperature
profile does not complete or physically qualify RLC calibration.

See [structured delivery/readback evidence](NTC-Factory-Profile.json),
[unit firmware and raw measurements](../../../Firmware/releases/2026-10-25/ntc-unit-profile/README.md),
[rebuild instructions](../../../Firmware/tools/NTC_FACTORY_PROFILE.md) and
[OSL status](OSL-All-99.md).

The owner later disconnected ST-Link to attach the charger while keeping TTL.
Read-only COM4 status confirmed charger present, SAFE/idle, zero faults and
blockers 0x09. No capture was started. At that checkpoint exact battery voltage
was unavailable through serial. [The subsequent telemetry update](../../../Firmware/releases/2026-10-25/battery-serial-telemetry/README.md)
now reports fresh nominal voltage over COM4 without SWD; the post-charge reading
is 3.949 V. Primary BRINGUP, BRINGUP_CAL
and FACTORY_PRODUCT schema2 deployment packages are included under deployable/.
Factory-product OSL provisioning prerequisites remain unchanged.
