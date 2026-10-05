import sys
from pathlib import Path

# make `import evaluation` work from the tests, whatever the working directory
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
