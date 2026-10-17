# Состояние проекта

Дата: 2026-10-17. Работа возобновлена пользователем.
M3 EE startup выполнен до service layer. M4: SIF channel и read-only FileIO backend; меню ещё не получено.

## Сейчас

- Native-run-014 прошёл SIF/RPC initialization через реальный SET_SREG reply и гостевой SIF0 handler;
  ready flag напрямую не менялся. Следующий FileIO SID `0x80000001` изначально отсутствовал.
- Реализован packet channel: negotiated EE receive buffer, init, bind, call, other-data, обычное
  и no-interrupt completion, reset. EE/IOP address spaces сохраняются раздельными; IRQ зависит от reply.
- FileIO backend использует настоящий runtime VFS для open/read/seek/close в read-only mode.
  Синтетический file test проверил точные байты, unaligned buffers, EOF, seek, close и отказы.
- Native-run-015 остановился на FileIO extension `0xFF`. Этот Europe client проверяет reply word 1
  на версию 2. Backend явно возвращает отрицательный результат расширения, не объявляет версию,
  не регистрирует переданные callback buffers; legacy read protocol реализован настоящими операциями.
- Native-run-016 принял этот отказ, прошёл FileIO bind/probe, затем за 40 секунд остался на повторном
  bind CDVD SID `0x80000592`, available=false. Последние snapshots: PC `0x0030AE18`, RA `0x0030AD64`,
  SP `0x0011FD70`, thread 1. Exit — supervisor timeout, без подтверждённого кадра/меню.
  Binary SHA-256: `41fbeb10835e7c9185d6d42c815dde0067f330608087888e65642ab1daaa58fa`.
- Первый текущий blocker: отсутствует native CDVD RPC backend `0x80000592`. Следующий эксперимент:
  проверить wire ABI команды, подключить SHA-verified original ISO read-only и обслужить реальные
  CDVD requests. Не использовать fake disc-ready, fabricated LSN/TOC или guessed timing.
- Проверено: 19 Python tests, C++ exact kernel dispatch smoke, C++ SIF packet smoke, C++ real-file
  FileIO smoke. Actual game file/sector reads, графика, звук, ввод и сохранения ещё не подтверждены.
- Build-028 пересобирает тот же source checkpoint после дополнительной проверки metadata/data overlap.

## История runtime checkpoint

- Source-only public checkpoint: `b666696`; CI прошёл четыре комбинации Windows/Linux × Python 3.10/3.13,
  включая C++ kernel-dispatch smoke: https://github.com/k1gs/Drakengard1Recompile/actions/runs/39009134734.
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
  ELF содержит оба последних callback pointers как raw data words без Ghidra references.
- `SeedDataPointers.java` восстановил 241 кандидата из initialized non-executable data;
  известный реальный target `0x002625D0` найден по слову `0x0054B1CC`. Кандидаты не считаются символами/API.
  Lift-006/build-021 успешны; native-run-011 прошёл callback и остановился на `0x00288AB8`.
- Выяснено: `0x00288AB4`/`0x00288AB8` — существующие инструкции общего эпилога без function owner,
  на них ведут реальные ветки. Exporter следует explicit owned flow edges и существующим unowned
  instructions/fallthrough/delay slots, добавляя 252 code bytes, сохраняя остальные holes.
  Map-008: 8 172 functions, 8 585 records, 2 191 156 covered bytes.
- Lift-007/build-022: 8 584 EE fragments, 120 366 resumable entries, 1 538 indirect warnings,
  0 stubs/skips/decode failures/errors; кандидат `.vutext` исключён upstream.
- Native-run-012: 40-second timeout; snapshots постоянно PC `0x003223E0`, RA `0x003239D4`,
  SP `0x0011FCC0`, thread 1. Guest читает SREG[0] по `0x006B0B40`, ожидая SIF RPC initialization.
- Build-023/native-run-013 повторил этот wait за 20 секунд и записал реальные SIF DMA packets:
  `0x006B0980` → IOP `0`, size `0x14`, CID `0x80000002`, opt `0`, EE receive buffer `0x006B08C0`;
  затем `0x006B0C00` → IOP `0`, size `0x10`, тот же CID, opt `1`. Оба AfterCopy подтверждены.
  Binary SHA-256: `62086f6f712f06381529caa4b51a671862fba5f90024e5823a86368b8e3265a0`.
- Исторический blocker run-013: native SIF command/RPC protocol не обслуживался после реального DMA copy.
  IopRpcBridge::onSifTransfer не формировал response. Этот этап пройден packet-level channel,
  реальными ответами через guest SIF0 handler и bind/call к backend services; см. текущий статус выше.
- Новые Ghidra synthetic checks прошли: raw pointer positive/negative cases, reachable shared epilogue
  с сохранением gap, прежний discontinuous-body hole. Воспроизведение: docs/ANALYSIS_REGRESSIONS.md.
- 19 Python tests и C++ synthetic kernel-dispatch smoke прошли: exact lookup, отказ до copy,
  исполнение после copy, отказ после byte mutation, независимые patch-регионы, invalid address/null RAM.
- Подробные процедуры и ограничения: docs/KERNEL_PATCH.md и docs/SIF_PROTOCOL.md.
- PCSX2 baseline-003 создал targetable окно Drakengard в isolated profile и завершён по 60-second timeout.
  Computer Use capture не состоялся: app approval timed out. Визуальный baseline остаётся неподтверждённым.
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
- Публичная репа: https://github.com/k1gs/Drakengard1Recompile, ветка main.
- Bootstrap checkpoint: b776414. Первый CI прошёл все 4 комбинации Windows/Linux ×
  Python 3.10/3.13: https://github.com/k1gs/Drakengard1Recompile/actions/runs/38931534901.
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
