# Product status, lead-menu discovery and backlight feedback

STATUS: IN_PROGRESS

Prerequisites: plans 30/31 are implemented and deployed. The owner reports FAULT 00000000 entering About, NTC not in degrees C, undiscoverable quick lead compensation, and reduced brightness after reset despite 100% selection. The display occupies the UART connector; ST-Link is available. Preserve the installed API 5 pack, splash, factory OSL, settings and experimental NTC model.

## Work and expected files

1. Reproduce About with the real font/catalog and strict display bounds, including both languages and emergency rendering. Correct the layout in `ui/ui_product.c`; do not hide display errors or weaken safety fault handling.
2. Show actual fresh NTC temperature in degrees Celsius with sensible decimal precision and locale; retain stale/invalid sentinels and age. Cover signed values and changes in renderer/controller tests.
3. Make quick lead OPEN/SHORT an explicit selectable action under Calibration using existing resource IDs and the existing capture/storage service. No automatic capture on menu entry, no LOAD campaign, no charger bypass. Cover navigation/return and admission in app/UI tests.
4. Read actual TIM3/PB0/AFIO and applied brightness without halting or resetting. Trace startup/default/editor/persistence/timeout ownership before changing PWM. If 100% reaches the pad but light remains dim, record the electrical unknown and request only the necessary hardware measurement; do not invent a polarity/current fix.
5. Extend actual-font host preview coverage and relevant regressions, build/map the affected profiles, deploy a sealed verified image using existing authorization, update docs/evidence and the follow-up PR. Do not require another resource upload unless the existing catalog proves insufficient.

## Acceptance

About and diagnostics render within 240x320 using real API 5 resources and fallback. NTC is explicitly degrees C with fresh evidence; lead compensation is visible and can be entered/canceled without energizing K1. Acquisition/safety math and mutable record formats are unchanged. Brightness conclusions identify actual commanded PWM separately from physical LED current/brightness. Remaining physical checks are REQUIRES_BENCH_VALIDATION until observed by the owner.

## Checkpoint

About overflow is reproduced with the actual API 5 font at x=240, y=113 (4x12 glyph window); the full hardware string now uses the small face and renders successfully. NTC displays rounded localized tenths of a degree, including negative sub-degree values, and preserves unavailable/stale evidence. Calibration now opens the existing five-action lead menu directly (new OPEN/SHORT, toggle, clear, factory status, back), without automatic capture. The API 5 pack is unchanged.

66/66 host entries and focused app/UI tests with provisioning OFF pass. Actual controller/renderer integration covers 76 bilingual/fallback measurement renders plus 28 status-page renders. A new unit assertion initially lacked matching fake-provider labels; the fixture was corrected and the full suite passed. Initial failed logs remain distinct.

Actual non-halting register reads at the owner's 100% setting show applied 100, TIM3 ARR 999 / CCR3 1000, PWM1, CH3 enabled, no remap, PB0 high, zero electrical/display faults and SAFE command outputs. The module has a transistor according to the owner. LED/VCC voltage cannot be measured now. Physical brightness diagnosis remains open; no speculative PWM/polarity change. Final sealed build/deployment and owner visual acceptance are next.
