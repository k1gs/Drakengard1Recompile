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
