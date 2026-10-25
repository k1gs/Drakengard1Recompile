# Промпт продолжения

Продолжи Drakengard1 Recompile, PS2 Europe, D:\proj\drakengard 1 recompile.
Прочитай AGENTS.md, docs/CONTINUE.md, последние CURRENT_STATE.md и верхний handoff;
проверь HEAD/status/реальные процессы. Без субагентов; сохраняй .vscode/Splash.

Текущий приоритет — низкий FPS меню. Native249: поток EE/IOP/softwareGS почти
занимает одно ядро; GS крупнейшая измеренная группа (~46% raw samples).
Validated фикс sparse_dispatch.h отсекает PCs вне реальных sorted nonempty таблиц
ядра И firmware до binary search, сохраняя exact hits и instruction verification.
Synthetic boundary/holes/singleton/UINT_MAX/mutation tests, Python55 и actual
Release/PCH PASS. Native251300s/1024M/frame600 watchdogtrue/exitnull: Start11 с
SIO2 press/release, меню9600 GS640x511 просмотрено, ROI MAE0. Combined lookup
141/1910=7.38% samples249 ->26/1906=1.36%251. Это не stacks/weightedETW/FPS.
ICF объединял lower_bound kernel/firmware, поэтому baseline6.54% не kernel-only.
Synthetic3.88x относится только к заданному workload поиска, не игре. Точные SHA
и private work/menu-perf-251/{report,comparison,icf-proof} в верхнем handoff.

Следующий252 (проверь unused): профиль dominant GS sprite/texture/filter на том же
меню, один безопасный fast path из реального draw state, synthetic pixel equality
и bounded native до/после. Без снижения качества/пропуска IOP или таймеров/миграции
renderer. Флаги unchanged: runner /Od, GS/runtime /O2; изменение оптимизации
отдельным проверяемым опытом. Unique guest FPS пока не измерен. Startup helper
запускай после появления pad-control.txt; один контроллер за раз. Следующие
пункты про пролог — сохранённая игровая граница и отложенная задача после FPS.

Checkpoint: все3 truncated leaves297E44,29322C,297878 восстановлены по реальным
инструкциям/epilogues. Map011/lift010,36resume labels сверены, fresh Ghidra
regression/full native Release PASS. Native246/247/248 прошли старые missing targets;
нового явного scheduler/device отказа нет. После New Game реально отрисована первая
текстовая страница предыстории (GS640x511). Это не gameplay/audio/saves/recovery.

Все runs300s/1024M; watchdog timed_out=true,exit=null. В248 на подтверждённой странице
Cross15 принят/отпущен SIO2, но следующий кадр показывает тот же текст. Пиксели
меняются; не объявляй зависание или успешное перелистывание. Exact evidence/hashes
в верхнем handoff/work/leaf297878-contract-246.json.

После performance-этапа используй свежий номер: установи ожидаемый способ продолжения пролога,
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
