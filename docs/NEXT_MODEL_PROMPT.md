# Промпт продолжения

Продолжи Drakengard 1 Recompile, PS2 Europe, в D:\proj\drakengard 1 recompile.
Прочитай AGENTS.md, docs/CONTINUE.md, последние CURRENT_STATE.md и верхний handoff.
Проверь Git HEAD/status/процессы. Без субагентов; сохраняй .vscode/ и native/Splash.png.

Старый missing scheduler target298154 уже исправлен cf3692d и повторно проверен:
owner297E44 восстановлен, все12 resume labels зарегистрированы. Следующий normal
VU0-data→SPR DMA9 контракт реализован и прошёл native244 (минимум16 реальных
completion с совпадающими ненулевыми source/destination hashes). Tests12/12 и
native Release/PCH build PASS. Никакого read-side STR clear/фиктивного IRQ/нуля.

Текущий blocker: native244 exit5/no timeout, missing EE scheduler target29353C,
RA2984EC, owning leaf29322C, map ends29353C. Generated branch293524→293538 входит
в delay instruction другого jump, затем выходит за обрезанный конец. Это
подтверждённая новая граница, ещё не исправленный leaf. FinalGS серый640x511,
реальное gameplay/audio/saves/recovery не доказаны. Меню и NORMAL dialog работают.

Следующий245 (сначала проверь unused): исследуй реальные инструкции/epilogue
этого owner, восстанови только подтверждённый bounded flow через существующие
read-only Ghidra scripts (docs/ANALYSIS_REGRESSIONS.md), экспортируй новую карту
и fresh lift, не меняя old lift008. Сверь все resume labels с registration table,
проверь delay-slot/yield/resume и настоящий native245 максимум300s/1024M.
Ordinary menu observer/GS refs и evidence paths указаны в handoff/CONTINUE.
Не добавляй произвольные aliases, RA/PC patches, padding, ready flags или skips.

Реализуй и проверь установленную причину, не ограничивайся планом. Inputs read-only;
upstream/generated вручную не редактируй. Private captures/BIOS/maps/lifts не
публикуй. Обнови CURRENT_STATE, верхний handoff и CONTINUE. Focused validated
commits/normal push разрешены по AGENTS; перед публикацией staged review и
scripts/check_public_tree.py. Сплешскрин остаётся отложенной задачей.
