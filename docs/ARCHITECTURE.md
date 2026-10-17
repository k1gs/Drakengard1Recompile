# Архитектура и открытые решения

Целевая цепочка: локальный образ → извлечённый boot ELF и ресурсы → карта функций →
статическая трансляция EE → native host runtime → реальные GS/audio/input/file services.
Реализованы intake, Ghidra function-map export, guarded lift и diagnostic runner glue.
Для исследования закреплён PS2Recomp; полнота его runtime для игры ещё не подтверждена.

## Что проверить до интеграции

- **EE/R5900**: 128-bit GPR/MMI, COP1 FP, COP2 macro, branch-likely/delay slots,
  адресные алиасы, signedness, исключения, stack/ABI, static/indirect jumps.
- **Загрузка**: PT_LOAD, BSS, relocation assumptions, overlays/self-modifying code.
- **Kernel**: threads, priorities, interrupts, semaphores/events, timers и порядок пробуждения.
- **IOP**: выбирать HLE сервисов или исполнение IRX только по обнаруженным зависимостям;
  SIF/RPC, DMA и завершение операций должны соответствовать наблюдаемому протоколу.
- **Graphics**: VIF unpack, VU0/VU1 microcode, GIF tags, GS registers, VRAM/CLUT,
  blending, depth, framebuffer и timing. Одного EE recompiler недостаточно.
- **Media/input**: CD streaming, SPU2, FMV, pad и memory-card contracts.

Windows x64 — первая платформа. C++20/CMake — предполагаемый стек при выборе PS2Recomp.
Первый diagnostic host использует upstream raylib/OpenGL и audio backend. Это проверка
bring-up, а не окончательный выбор renderer/audio architecture. Runner не возвращает
код успеха как доказательство working port; остановки и unsupported paths диагностируются отдельно.

## Карта исходников runtime

| Область | Ответственность |
| --- | --- |
| `native/runner/main.cpp`, `verify_input.*` | Приёмка точных входов и выбор режима запуска |
| `native/runner/iop_boot.*` | Проверенные входы IOP, владение сессией и явный источник RTC |
| `native/runner/iop_bridge.cpp` | Один владелец состояния EE/IOP: режим запуска, SIF и экспериментальный DMA; синхронизация внешних hooks |
| `native/iop_probe/bootstrap_session.h` | Размещение модулей, переход SYSMEM → LOADCORE и сохраняемое состояние CPU |
| `native/iop_probe/bare_call.h` | Ограниченный цикл CPU, владение кодом, доставка исключений и возобновление |
| `native/iop_probe/iop_data_bus.h` | Именованная конфигурация устройств, проверка доступа, staging до инструкции и эффекты после неё |
| `native/iop_probe/iop_diagnostics.h` | Описания остановок CPU, unresolved imports и исчерпания бюджета |
| `native/runner/sif_channel.*`, `sif_transport.cpp` | Протокол пакетов SIF и адаптация к upstream runtime/VFS |
| `native/runner/fileio_rpc.*`, `cdvd_rpc.*`, `cdvd_device.*`, `iso_media.*` | Сервисы файлов и диска, регистры контроллера и чтение ISO |
| `native/runner/overlay.cmake`, `cmake/*.cmake` | Сборка host glue и отдельные проверяемые адаптации syscalls, dispatch, SIF и memory |

IOP получает устройства через `IopDevices`: например, `{.interrupts = &interrupts, .dma = &dma}`.
Отсутствующий указатель оставляет устройство неподдержанным. Шина сохраняет порядок операций:
подготовка данных → инструкция CPU → восстановление временных ROM/SIF значений → acknowledge
контроллеров → события CDVD и tick таймеров. Проверки исключений остаются в цикле CPU.

Внешний API моста SIF сохранён. Внутренний `CooperativeIopBridge` хранит режимы Prepared,
Reboot и Firmware вместо независимых флагов; входы и переходы остаются прежними.
Четыре CMake-адаптации выполняются в прежнем порядке и scope, а upstream checkout не меняется.
Сгенерированный guest code и производные прошивки остаются в локальных ignored-каталогах.

## Контракты диагностики

Каждый неподдержанный путь завершает ограниченный запуск с ненулевым кодом и описанием:
ELF SHA-256, guest PC, функция/сервис, аргументы, ожидаемый эффект. Не возвращать успех,
не подменять размеры ресурсов, не увеличивать память для скрытия ошибок протокола.
Game-specific патчи отделять от общего runtime и привязывать к точной ревизии ELF.

## Критерий выбора PS2Recomp

Закрепить commit и submodule revisions, воспроизвести C++20 build, проверить небольшой
synthetic R5900 sample, затем функции целевого ELF. Оценить доступный GS/VU/IOP путь,
unsupported calls и лицензию итогового linked runtime. Если ограничения мешают даже
вертикальному срезу, записать ADR с альтернативами до создания большого fork.
