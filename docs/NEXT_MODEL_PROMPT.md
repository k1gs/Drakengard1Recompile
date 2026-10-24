# Промпт продолжения

Продолжи Drakengard1 Recompile, PS2 Europe, D:\proj\drakengard 1 recompile.
Прочитай AGENTS.md, docs/CONTINUE.md, последние CURRENT_STATE.md и верхний handoff;
проверь HEAD/status/реальные процессы. Без субагентов; сохраняй .vscode/Splash.

Checkpoint: все3 truncated leaves297E44,29322C,297878 восстановлены по реальным
инструкциям/epilogues. Map011/lift010,36resume labels сверены, fresh Ghidra
regression/full native Release PASS. Native246/247/248 прошли старые missing targets;
нового явного scheduler/device отказа нет. После New Game реально отрисована первая
текстовая страница предыстории (GS640x511). Это не gameplay/audio/saves/recovery.

Все runs300s/1024M; watchdog timed_out=true,exit=null. В248 на подтверждённой странице
Cross15 принят/отпущен SIO2, но следующий кадр показывает тот же текст. Пиксели
меняются; не объявляй зависание или успешное перелистывание. Exact evidence/hashes
в верхнем handoff/work/leaf297878-contract-246.json.

Следующий249 (проверь unused): установи ожидаемый способ продолжения пролога,
обычным Start/Cross в готовом GS-состоянии. Проследи actual callback/input handler;
если нужно, read-only Timer1 COUNT/MODE и software progress. RepeatedPC27F1B8/
getter1215F4(10000800) не доказывают timer defect. При необходимости последовательно
сравни эквивалентное owned reference состояние. Цель — следующая реальная сцена,
затем движение/бой; не исправляй предполагаемую причину до её наблюдения.

Private menu observer248 калиброван на просмотренных GS-состояниях; prologue script
hardcodes248/output/sequence14. Для нового run сделай свежую проверенную копию,
один controller за раз и actual SIO2 press/release. ACK не означает UI принятие.
Не делай OS input/Computer Use, timer/ready/PC/RAM patches, skip stubs/aliases.
Input/upstream/old generated read-only. Будущий полный Ghidra export должен
повторить ВСЕ3 verified recoveries из docs/ANALYSIS_REGRESSIONS.md.

Реализуй/проверь подтверждённый контракт, а не только план. Обнови CURRENT_STATE,
CONTINUE/top handoff/промпт. Focused validated commits/normal push разрешены по
AGENTS после staged review и check_public_tree.py. Private maps/lifts/BIOS/game/
frames не публикуй. Сплешскрин остаётся отложенным.
