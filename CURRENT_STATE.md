# Состояние проекта

Дата: 2026-10-17. Работа возобновлена пользователем.
M2: инструменты и исправленный lift-002 готовы. Native diagnostic build в работе.

## Текущий checkpoint инструментов

- PS2Recomp commit `eb8688b2fd191129f103a39543002c005b33409c`, analyzer/recompiler собраны MSVC x64.
- Ghidra 12.1.4 + Emotion Engine Reloaded v2.1.38: 1 346 seeds, 7 796 functions;
  исправленный exporter выдаёт 8 212 disjoint code fragments, 2 169 136 covered bytes.
- Synthetic generated C++ скомпилирован и исполнен: v0=42, pc=ra, zero/s0 сохранены.
- Lift-001 заменён: bounding ranges ошибочно включали дыры разрозненных тел функций,
  что породило два 10-МБ C++ файла и MSVC C1002. Подробности: docs/PROGRESS_2026-10-17.md.
- Lift-002 exit 0: 8 212 fragments, 0 stubs/skip/decode failures; 1 536 indirect warnings.
- 15 Python tests прошли; Ghidra synthetic regression подтвердил сохранение разрыва тела.
- Изолированный PCSX2 распознал SLES-52322/version 1.00/CRC 79585776; меню визуально не подтверждено.
  Reference process завершён, пользовательский профиль не менялся.
- Native runner overlay настроен; неизвестный syscall останавливается exception вместо fake success.
- Native build использует /MP4 и исправленный lift-002.
  Текущий лог: work/build-native-005.log; успешная линковка и native game execution пока не подтверждены.
- Source/docs tooling входят в текущий source-only checkpoint; предыдущий intake checkpoint — 6875bbd.
  Точный актуальный commit смотреть в Git main, без game-derived code или binaries.
  Не публиковать game/, generated/, tools/, build/ или work/.
- После сборки: проверить результат,
  затем `python scripts/run_native.py --input-complete --seconds 20 --run 1`, разобрать первый blocker.
  Повторно проверить новые исходники, public tree и focused diff перед commit/push.
- Подробные версии, команды и ограничения записаны в новом docs/TOOLCHAIN.md.

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
- Локально: Python 3.10.0; 15 synthetic tests прошли, native return42 smoke исполнен.
- Публичная репа: https://github.com/k1gs/drakengard1-recomp, ветка main.
- Bootstrap checkpoint: b776414. Первый CI прошёл все 4 комбинации Windows/Linux ×
  Python 3.10/3.13: https://github.com/k1gs/drakengard1-recomp/actions/runs/38931534901.
- Найден и использован MSVC 19.41 x64 (VS 2022), Windows SDK 10.0.22621.0.

## Ещё не подтверждено

- Соответствие хеша внешнему эталонному dump/revision. VER=1.00 — только значение SYSTEM.CNF.
- Форматы BIN, фактические языки, overlays/self-modifying EE code, содержимое VU programs.
- Визуальное меню PCSX2 и интерактивный baseline; BIOS и isolated startup уже проверены по логам.
- Покрытие runtime и результат native boot; PS2Recomp revision закреплён в docs/TOOLCHAIN.md.
- Ни загрузка игры, ни кадр, меню, звук или геймплей не проверялись.

## Следующее действие

1. Завершить native build и ограниченный запуск, зафиксировать первый runtime-блокер.
2. Проверить его guest PC/аргументы и первопричину; не включать skip или success stubs.
3. Дополнить визуальный PCSX2 baseline; уточнить indirect targets и VU/DVP coverage.
4. Проверить public diff, опубликовать focused source/docs checkpoint.

M2 частично выполнен: визуальный baseline ещё не принят. Подробный план: docs/ROADMAP.md.

Проверки и публичный checkpoint фиксируются в Git и CI; отчёты с личными путями и игровые
артефакты остаются в work/. Это подготовка исследовательского проекта, не результат порта.
