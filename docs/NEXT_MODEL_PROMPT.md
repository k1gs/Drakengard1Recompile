# Промпт продолжения

Продолжи Drakengard1 Recompile, PS2 Europe, D:\proj\drakengard 1 recompile.
Прочитай AGENTS.md, docs/CONTINUE.md, последние CURRENT_STATE.md и верхний handoff;
проверь HEAD/status/реальные процессы. Без субагентов; сохраняй .vscode/Splash.

Текущий приоритет — производительность меню. Предыдущие sparse lookup/SIMD bilinear
fixes сохранены. Новый --gs-metrics default off: bounded private separate UIcalls,
observedVBlankTicks, validlatchedcopies, RGBchanges, completeddraws/wall, CLUTloads,
statebins. Семантика/keylayout в docs/GS_PRODUCTION_METRICS.md. Не называй все
счётчики FPS: partial/missed/identical frames и overhead существенны.

253baseline/254fastpath по24 settled5s windows (~120s): UI59.22->58.82/с,
observedticks/readbacks6.72788->7.15342/с (+6.325% в этой паре), RGB5.66->5.28/с
(не FPS!). Rasterwall54.22%->51.60%. Count/time conservation PASS, overflow0.
Главные два T8/CT32/CSA0 состояния были58.19% rasterwall. Новый narrow cache-free
путь читает live CLUT два halfwords: обаCSM дают index, TEXA CT32 identity;
allotherstates originalLookupCLUT. Никакой stale palette/cache или guest timing.

464 actual-raster A/B syntheticdraws с полным4MiB VRAM равенством, в том числе
палитра writes/TEXFLUSH/LoadClut повторно; synthetic T8batch17.27% меньше времени
только на этот workload. Bootstrap14/14, Python56, actualRelease/PCH PASS.
Native253/254300s/1024M/frame600/--gs-metrics watchdogtrue/exitnull. Обычный
Start11/SIO2 pressrelease, menu640x511 просмотрено и pixel-identical к252.
ТочныеSHA/privateevidence/limits в topHANDOFF, work/gs-clut-254/report.md/json.

Следующий255 (проверь unused): profile remaining T8 texture/page-cache reads и
per-pixel blending с этими counters, затем один safe fastpath с fullraster pixel
equality и comparable native before/after. No palette cache без invalidation
proof/quality/timer/IOP skipping. Runner /Od, GS/runtime /O2 unchanged. Startup
helper после pad-control.txt, один controller за раз. Сохранённая игровая граница
ниже отложена до performance; не возвращай старые scheduler blockers.

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
