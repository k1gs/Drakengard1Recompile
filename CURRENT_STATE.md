# Состояние проекта

Дата: 2026-10-17. Работа возобновлена пользователем.
M3: native diagnostic runtime собран и исполняет EE startup; меню ещё не получено.

## Последний runtime checkpoint

- Source-only public checkpoint: `41dfcbf`; CI прошёл четыре комбинации Windows/Linux × Python 3.10/3.13.
- Native build успешно слинкован без `/FORCE:MULTIPLE`: пустая upstream registration unit
  заменена нашим input verifier, generated registration используется единожды.
- Native runner сам проверяет точный ELF SHA-256 до инициализации окна. Неверный ELF
  отвергнут с exit 2; неподдерживаемая платформа также не обходит проверку.
- Native-run-002: за 40 секунд scheduler snapshots оставались на PC `0x003281E0`, RA `0x003281F4`.
  Причина: guest SetSyscall устанавливал handlers `0x003280D0` и `0x00328098`, отсутствовавшие
  в generated map; upstream возвращал ошибку при вызове и guest зацикливался на поиске таблицы.
- Эти два фактических входа добавлены в Ghidra. Lift-003/native build-008 успешны.
  Native-run-003 вышел с exit 5 за ~1 секунду: следующий установленный syscall `0x5A`,
  handler `0x00328700`, PC `0x003286E8`. Установку неизвестного handler теперь останавливает guard.
  Binary SHA-256: `79b684f98cbbf3f8fe1c720e55d2c6a76876705ebf0e987399295603277388b0`.
- `SeedAddressFunctions.java` добавляет подтверждённые runtime-входы; `SeedReferencedCode.java`
  добавил 115 кандидатов по существующим data references. Это кандидаты, не доказанные API.
- Автоанализ также создал пять ложных code bodies в `.data`; exporter ограничен initialized
  executable Ghidra memory. Export-005: 7 924 functions, 8 338 fragments, 2 178 284 bytes.
- Lift-004: exit 0, 8 337 fragments, 120 416 resumable entries, 0 stubs/skips/decode failures;
  1 540 indirect warnings. Upstream исключил кандидат `0x00342E00` из `.vutext`: это VU, не EE code.
- Синтетический discontinuous-body regression повторно прошёл после фильтра памяти;
  15 Python tests прошли. Build-009 успешно слинкован.
- Native-run-004: exit 5 без timeout. Следующий blocker: syscall `0x5B`, handler `0x80076000`,
  PC `0x003286E8`. Адрес вне исходного ELF-code; нужно исследовать источник kernel patch/relocation.
  Binary SHA-256: `2478b06c40466e7de0145c2f5c2526004df44b5c6535bb35d537e9f9df2f6c4e`.
- Native-run-005 подтвердил prefix kernel handler из `0x0051BFA0`. Исследованы копии
  `0x740` bytes → `0x80076000`, `0x330` → `0x80075000`, `0x28` → `0x00082000`.
- Создан отдельный private kernel translation image (work/kernel-image-002), точный SHA-256
  закреплён в scripts/kernel_image.py. Kernel lift-004: 16 functions, 2 440 code bytes,
  21 resumable entries, 0 warnings/errors/stubs/skips/decode failures.
- Sparse kernel lookup содержит 37 точных входов. Wrapper проверяет переведённые instruction
  ranges текущего patch-блока в реальной EE RAM перед dispatch и перед прямыми generated calls.
  Data tables могут изменяться; instruction mutation остаётся явным blocker.
- Native-run-009 прошёл оба kernel patch-блока, остановился на JALR `0x0027FA9C` →
  `0x0027C630`, RA `0x0027FAA4`, SP `0x0011FF40`, GP `0x00572C70`, A0 `0x0048AC68`.
  Binary SHA-256: `c0d7cd084bba214070cc6b107c5ba2ea49bafebf22a8b7ab2b1a0575cf4d80ef`.
- Этот фактический callback добавлен в основную Ghidra map: 7 925 functions → 8 339 records,
  2 178 292 covered bytes; lift-005 перевёл 8 338 EE fragments (ещё один — `.vutext`).
  Build-019 успешно собран. Native-run-010 прошёл callback `0x0027C630`, затем остановился
  на JALR `0x002625FC` → `0x002625D0`, RA `0x00262604`, SP `0x0011FF70`,
  A0 `0x004849F0`, A1 `1`, A2 `0`, A3 `0x0011FF70`; exit 5 без timeout.
  Binary SHA-256: `3e3f39427cfa1ad2299892346ac3b4641256d8e04d45987e6c1acf4d45908564`.
  Следующий эксперимент: выяснить отсутствующие references этого callback и расширить map.
- 19 Python tests и C++ synthetic kernel-dispatch smoke прошли: exact lookup, отказ до copy,
  исполнение после copy, отказ после byte mutation, независимые patch-регионы, invalid address/null RAM.
- Подробная процедура и ограничения: docs/KERNEL_PATCH.md. CI расширен synthetic C++ smoke;
  новая удалённая проверка будет запущена после публикации checkpoint.
- Все game-derived maps, generated code, snapshots/logs и binaries остаются ignored.
  Меню, кадр игры, звук и геймплей не подтверждены.

## Предыдущий checkpoint инструментов (история lift-002)

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
- Native build-005 использовал /MP4 и исправленный lift-002; последующие результаты см. выше.
- Source/docs tooling входят в текущий source-only checkpoint; предыдущий intake checkpoint — 6875bbd.
  Точный актуальный commit смотреть в Git main, без game-derived code или binaries.
  Не публиковать game/, generated/, tools/, build/ или work/.
- Перед следующей публикацией проверить новые исходники, public tree и staged diff.
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
- Полнота runtime и native boot; PS2Recomp revision закреплён в docs/TOOLCHAIN.md.
- ELF загружен и EE startup исполняется. Кадр игры, меню, звук и геймплей не получены.

## Следующее действие

1. Завершить native build и ограниченный запуск, зафиксировать первый runtime-блокер.
2. Проверить его guest PC/аргументы и первопричину; не включать skip или success stubs.
3. Дополнить визуальный PCSX2 baseline; уточнить indirect targets и VU/DVP coverage.
4. Проверить public diff, опубликовать focused source/docs checkpoint.

M2 частично выполнен: визуальный baseline ещё не принят. Подробный план: docs/ROADMAP.md.

Проверки и публичный checkpoint фиксируются в Git и CI; отчёты с личными путями и игровые
артефакты остаются в work/. Это подготовка исследовательского проекта, не результат порта.
