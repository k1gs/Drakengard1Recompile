# Состояние проекта

Дата: 2026-10-16. Этап: M1 — приёмка предоставленного Europe-образа выполнена;
далее M2 — эталонный запуск и feasibility инструментов.

## Подтверждено

- Рабочая цель: Drakengard 1, PS2, Europe, первоначально Windows x64.
- Пользователь предоставил ISO в game/; выполнена read-only приёмка, оригинал не изменился
  (SHA-256 повторно проверен после извлечения).
- ISO: 4 445 044 736 байт; boot SLES_523.22, SYSTEM.CNF VER=1.00, VMODE=PAL.
- ELF32 little-endian MIPS, entry 0x00120008, два валидных PT_LOAD.
- ELF SHA-256: `065a94591d295bb936a8dca92e722c3bd72cd05848c7274b26d572b0f782e918`.
- 17 извлечённых файлов: boot ELF, SYSTEM.CNF, 8 IRX, IOPRP280.IMG и 6 крупных BIN.
- Обычных SHT_SYMTAB/SHT_DYNSYM нет. Сохранены 1 346 именованных .gnu.linkonce.t.* секций
  и 53 .DVP.overlay.* секции; это материал для анализа, не готовая карта всех функций.
- Подробности: docs/INPUT_EUROPE.md. Рабочая копия и reports находятся в ignored work/.
- game/ целиком исключён из Git; исходный образ и извлечённые байты не публикуются.
- Инструменты приёмки работают только по явному пути после `--input-complete`.
- Локальные тесты используют исключительно синтетический ELF.
- Локально: Python 3.10.0, 8 synthetic tests прошли; doctor выполнен.
- Публичная репа: https://github.com/k1gs/drakengard1-recomp, ветка main.
- Bootstrap checkpoint: b776414. Первый CI прошёл все 4 комбинации Windows/Linux ×
  Python 3.10/3.13: https://github.com/k1gs/drakengard1-recomp/actions/runs/38931534901.
- В текущем shell найдены Git и CMake; MSVC/clang-cl/Ninja в PATH не обнаружены.
  Это не проверка отсутствия установленного Visual Studio: перед M2 проверить developer shell.

## Ещё не подтверждено

- Соответствие хеша внешнему эталонному dump/revision. VER=1.00 — только значение SYSTEM.CNF.
- Форматы BIN, фактические языки, overlays/self-modifying EE code, содержимое VU programs.
- PCSX2 cold boot, конфигурация и наличие подходящего локального BIOS для эталона.
- Подходящая ревизия PS2Recomp, покрытие runtime, результат native boot.
- Ни загрузка игры, ни кадр, меню, звук или геймплей не проверялись.

## Следующее действие

1. Подготовить local tools, проверить MSVC developer shell и закрепить PS2Recomp commit.
2. Зафиксировать версии PCSX2/Ghidra и снять cold-boot baseline при наличии BIOS.
3. Импортировать ELF с точным хешем в Ghidra, используя именованные секции как подсказки;
   восстановить границы .text функций, indirect targets и возможные оверлеи.
4. Synthetic smoke recompiler, затем минимальный lift и первый реальный runtime-блокер.

M2 и native runtime в этом checkpoint ещё не выполнены. Подробный план: docs/ROADMAP.md.

Проверки и публичный checkpoint фиксируются в Git и CI; отчёты с личными путями и игровые
артефакты остаются в work/. Это подготовка исследовательского проекта, не результат порта.
