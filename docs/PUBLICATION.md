# Публикация

Публичны только собственные source/scripts, synthetic tests, шаблоны и проверенная документация.
Игровые образы, ELF/IRX, BIOS, assets, generated lifts, disassembly, Ghidra databases,
save files, captures и private reports остаются локальными. .gitignore — первый барьер;
`scripts/check_public_tree.py` проверяет tracked/index paths, размер и текстовый формат.
Он не заменяет ручную проверку на игровые данные, credentials или чужой код.

Перед focused commit/push:

```powershell
git status --short
git diff
python -m unittest discover -s tests -v
# Stage only reviewed project files, then:
python scripts/check_public_tree.py
git diff --cached --check
git diff --cached
```

CI повторяет synthetic tests и проверку public tree на Windows/Linux. Никакой CI job
не скачивает игру или BIOS и не запускает game code. Не добавлять generated guest code
даже как debug artifact. Нормальные commits/push авторизованы пользователем для этого проекта.
Сторонние библиотеки/runtime требуют отдельного учёта provenance/license и pinned revisions.
