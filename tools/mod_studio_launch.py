"""Start ISSD Mod Studio.

    python tools/mod_studio_launch.py

This is also what the packaged .exe runs. It is a separate file from the
package so PyInstaller has a plain script to start from while `mod_studio`
stays an ordinary importable package.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from mod_studio.app import main   # noqa: E402

if __name__ == "__main__":
    main()
