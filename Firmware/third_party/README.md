# `third_party`

External dependencies used by firmware.

## Rules

- keep each dependency isolated and identifiable;
- record upstream version/commit;
- preserve required license and notice files;
- do not copy third-party code into project-owned modules without attribution/history;
- prefer small, permissively licensed dependencies appropriate for embedded firmware.

CMSIS/HAL may be provided through a reproducible dependency mechanism instead of vendored source, provided command-line builds remain reproducible.

## ST firmware components

Official ST components are tracked as Git submodules under `st/`.

Initialize them after cloning:

```bash
git submodule update --init --recursive
```

Pinned Phase 01 baseline:

```text
STM32CubeF1 compatibility package: v1.8.7

st/cmsis_core              afc5ca6af0a49232fde7eb4548dd0962d119ce14
st/cmsis_device_f1         c8e9a4a4f16b6d2cb2a2083cbe5161025280fb22
st/stm32f1xx_hal_driver    fee494a92b5ad331f92ad21f76c66a5cb83773ee
```

Do not replace these with a developer-local STM32CubeIDE workspace or an unpinned `master` checkout. Update the SHAs only as an intentional dependency update with build validation and documentation.
