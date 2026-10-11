# Workbench dependency and packaging license review

Reviewed 2026-10-11 for the pinned Windows x64 build. Runtime: PySide6-Essentials
6.8.3 + shiboken6 6.8.3, pyserial 3.5 and Python 3.9+. The Essentials wheel
supports CPython 3.9 ABI and includes Core/Gui/Widgets; avoiding Addons excludes
unneeded browser/chart modules and reduces deployment size. Tkinter has a smaller
installation footprint, but Qt supplies maintained forms/tables, signals, worker
ownership and headless interaction tests for this interface without a web server.

- [Qt for Python](https://doc.qt.io/qtforpython-6/) offers LGPLv3/GPLv3 or commercial
  licensing. [Qt 6.8 licensing](https://doc.qt.io/qt-6.8/licensing.html) separately
  lists GPL-only modules and third-party code. This app imports only Core, Gui,
  Widgets and QtTest (tests); no GPL-only Charts/Graphs module is imported.
- [PySide6-Essentials 6.8.3](https://pypi.org/project/PySide6-Essentials/6.8.3/)
  is the official minimal wheel. Its installed metadata/license notices must be
  retained; wheel metadata alone is not the complete set of Qt third-party notices.
- pyserial uses the BSD license (installed package metadata). Python uses the PSF
  license and bundled dependencies retain their individual notices.
- [PyInstaller 6.13 license](https://pyinstaller.org/en/v6.13.0/license.html) is
  GPLv2 with the executable-bundling exception; select runtime files use Apache 2.0.
  The exception permits distribution of generated applications under another
  license subject to their dependencies. PyInstaller is a build dependency only.

The onedir recipe keeps Qt DLLs replaceable, copies installed license/metadata
files and application Python source, and includes this review. It neither statically
links Qt nor introduces EULA restrictions on reverse engineering for LGPL debugging.
Before distributing a portable release externally, retain the applicable full
LGPL/GPL and Qt third-party notices and supply corresponding Qt/PySide source under
the LGPL's allowed mechanism, plus Python/native-fixture dependencies and notices.
The [Qt LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations)
describe corresponding source, replaceable libraries, user rights and prominent notices.
Corresponding upstream source is available at
[Qt 6.8.3](https://download.qt.io/archive/qt/6.8/6.8.3/) and
[PySide 6.8.3](https://download.qt.io/official_releases/QtForPython/pyside6/PySide6-6.8.3-src/).
Links do not replace required source delivery/offer obligations. No modified Qt or
PySide source is used. The repository has no root LICENSE at this starting SHA;
the owner must choose the application/fixture distribution terms before a public
binary release. This PR builds/tests a local bundle, not a legally cleared release.

Use the pinned build manifest for repeatable dependency versions; binary hashes may
vary with Python, native compiler, paths and timestamps. Archive build environment,
wheel/source hashes and appropriate notices when making a release. Windows 10/11
clean-machine testing, code signing and physical COM validation remain release gates.
