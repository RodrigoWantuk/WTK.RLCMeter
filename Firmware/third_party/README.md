# `third_party`

External dependencies used by firmware.

## Rules

- keep each dependency isolated and identifiable;
- record upstream version/commit;
- preserve required license and notice files;
- do not copy third-party code into project-owned modules without attribution/history;
- prefer small, permissively licensed dependencies appropriate for embedded firmware.

CMSIS/HAL may be provided through a reproducible dependency mechanism instead of vendored source, provided command-line builds remain reproducible.
