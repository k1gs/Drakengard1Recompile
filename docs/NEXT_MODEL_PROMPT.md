# Промпт продолжения

Продолжи Drakengard1 Recompile, PS2 Europe, D:\proj\drakengard 1 recompile.
Прочитай AGENTS.md, docs/CONTINUE.md, последние CURRENT_STATE.md и верхний handoff;
проверь HEAD/status/реальные процессы. Без субагентов; сохраняй .vscode/Splash.

Текущий приоритет — производительность меню. Sparse lookup fix bb4ba5b сохранён.
Новый проверенный фикс: gs_bilinear.h считает RGBA bilinear вместе SSE2 с тем же
порядком float и lround ties; не add0.5/truncate, не FMA. Scalar fallback nonSSE2.
Координаты/текстуры/CLUT/TEXA/качество/таймеры/compilerflags не менялись. Overlay
проверяет SHA полного прежнего math body; upstream/generated read-only.

Public2,048,594 exact math comparisons, bootstrap13/13, Python55 и настоящий
Release/PCH PASS. Private actual old/new raster416 paired synthetic draws, каждый
раз все4MiB VRAM одинаковы. Synthetic128 T8linear256x128 sprites median0.301296s
->0.254510s,15.53% меньше времени только на этот workload, не игра/FPS.
Native252300s/1024M/frame600 watchdogtrue/exitnull, обычный Start11/SIO2 ACK,
menu8400 GS640x511 просмотрено и весьRGB пиксельно совпал с251menu9600.
В новом30s профиле math встроился в SampleTexture; lerp/ucrtbase IP не встретились,
но это nearestMAP/RIP, не stacks/weightedETW и не измеренный whole-game speedup.
Точные SHA/evidence/ограничения в верхнем handoff, work/gs-bilinear-252/report.md.

Следующий253 (проверь unused): trustworthy guest GS frame/draw-production counter
отдельно от UI callbacks и bounded indexed-texture/CLUT draw-state statistics на
том же меню. После этого выбирай следующий fast path; cache требует CLUT/TEXA/
state invalidation tests. Не снижай качество/не пропускай IOP/таймеры/не мигрируй
renderer. Runner /Od, GS/runtime /O2 — unchanged, оптимизация flags отдельным опытом.
Unique guest FPS пока не измерен. Startup helper только после pad-control.txt,
один controller за раз. Ниже сохранена игровая граница, отложенная после FPS.

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
