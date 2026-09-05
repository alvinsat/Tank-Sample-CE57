from pathlib import Path
import re
path = Path(__file__).with_name('TankComponent.cpp')
text = path.read_text(encoding='utf-8')
pattern = re.compile(r"\t\t\t\tif \(gEnv && gEnv->pSystem && gEnv->pSystem->GetILog\(\)\)\s*\{.*?\n\t\t\t\tif \(!pPhysics \|\| !pPhysics->GetStatus\(&m_vehicleStatus\)\)\s*\n\t\t\t\t\treturn;.*?\n\t\t\}\s*\n\t\}\s*\n\t//\s*\n",
                     re.S)
match = pattern.search(text)
if not match:
    print('no regex match found')
    raise SystemExit(1)
text = text[:match.start()] + '\t\t}\n\t}\n\t//\n' + text[match.end():]
path.write_text(text, encoding='utf-8')
print('patched regex')
