# Nominal 0.47-uH sensitivity check

After two successful nominal 470-uH checks, the owner substituted an isolated
nominal 0.47-uH inductor and triggered one short-OK PRODUCT measurement. SAFE
preflight, exact running-image verification and compiler-derived context layout
were preserved without reset or firmware/calibration writes.

Session 5 completed with one attempt at 1 kHz, RREF 10 ohm, HG return:

- R = 0.235328 ohm; X = -0.0100988 ohm.
- Classification: RESISTIVE. Published L and C are both invalid.
- No frequency refinement, clipping, safety abort or DSP error occurred.
- Active calibration remains sequence 1, all 33 records.

The completed measurement does not resolve the nominal inductance. In the
current engine, resistive classification skips the second-frequency refinement
(`measurement_engine.c`, `decide_next` policy). The observed result therefore
also exposes an automatic-search limitation: this session never tried 10 kHz.
This does not establish the hardware's minimum measurable inductance.

For scale only, ideal 0.47 uH has reactance 0.00295 ohm at 1 kHz and 0.0295 ohm
at 10 kHz, calculated from X = 2*pi*f*L. These are theoretical values, not
measured DUT results. Actual reactance was small and negative; assigning a
positive inductance to it or treating the internal candidate C field as a valid
capacitor reading would be unsupported. Published C remains correctly invalid.
Resolving this region needs frequency-dependent phase/fixture evidence and a
reviewed search policy. Lowering thresholds alone is not qualification.

Final metadata confirms K1 commanded SAFE, range/excitation OFF, quiet released,
zero safety faults and unchanged active calibration. No resistor campaign was
repeated and no battery voltage was read via SWD. No firmware code changed;
relevant checks are physical measurement, image/ABI match and SAFE teardown.
Software tests were not rerun. See physical-evidence.zip for raw context,
Flash-image verification, transcripts and read-only procedures.

Status: REQUIRES_BENCH_VALIDATION for sub-microhenry capability, independent
accuracy, phase and parasitic compensation. The successful 470-uH functional
checks remain valid as previously documented and are not extended to 0.47 uH.
