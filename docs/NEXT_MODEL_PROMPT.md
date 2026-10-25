# Промпт продолжения

Продолжи Drakengard1 Recompile, PS2 Europe, D:\proj\drakengard 1 recompile.
Прочитай AGENTS.md, docs/CONTINUE.md, последние CURRENT_STATE.md и верхний handoff.
Проверь git HEAD/status и реальные процессы. Без субагентов/ComputerUse/OS input.
Сохраняй .vscode/Splash и все старые private artifacts. Inputs ранее подтверждены
как полные; исходники игры/BIOS/upstream/generated/maps read-only.

Пользователь ЯВНО отложил FPS меню — сейчас продолжать ИГРУ, не оптимизацию.
255/256: после обычного Start15 на подтверждённой первой странице пролога
отрисован "Chapter I - Beginnings", GS640x511, приватно просмотрен. Это не
movement/combat/gameplay/audio/saves/recovery. Cross16 на256 принят SIO2 почти в
конце300s; его эффект не проверен. Не объявляй зависание/успех по одному PC/ACK.

255 упёрся в глобальный1024M IOP allowance (total1038404316), exit6/no timeout,
IOPpcbb9c Jself idle. Добавлен ЯВНЫЙ фиксированный2048M пресет, default256 и
512/1024 сохранены; no poll/idle renewal. Worst deadline2,063,999,999 fits32bits,
static guard, all-preset/invalid/renewal regression, bootstrap14/14, Python57 PASS.
256 с2048M прошёл прежнюю границу,300s watchdogtrue/exitnull, no new explicit
scheduler/device refusal. Ни один неподдержанный сервис не подменён success.

Пользователь попросил плавно остановиться до257. Последний native-tested256 EXE
c312c31d...; текущий rebuilt106577eb... НЕ запускался. Новый opt-in capturelimit
DRAKENGARD_FRAME_LIMIT1..64 (default32), parser/test/realbuild PASS, native64ещё
не проверен. Exact SHA/артефакты и ограничения — верхний handoff/локальный журнал.

Следующий257 ещё unused:300s/2048M, FRAME_INTERVAL300,FRAME_LIMIT64, metricsOFF.
Команда/порядок запуска в docs/CONTINUE.md. Готовы private work/chapter1-257/
menu.py (arg257), start-prologue.py и chapter-input.py, только py_compile checked.
Startup helper после pad-control.txt; menu/prologue/chapter helpers ждут завершения
предыдущей фазы, один input controller за раз. Chapter helper распознаёт свежий
титул (ROI bound8, positive6.83/negatives24+), Cross, затем Start только если тот же
титул после600 UI observations. Если ROI изменился — визуально проверь actualGS,
не объявляй новую сцену по нормализованному crop. Проверяй SIO2 press/release.

Цель: пройти дальше титула главы I, найти первый реальный blocker или отрисовать
следующую сцену, затем фактическое движение/бой. Не меняй timer/ready/PC/RAM,
не патчь пропуск фильмов, не делай unlimited/renewable budgets. Повтор27f1b8 и
Timer1 getter не подтверждают timer fault: наблюдавшийся callback127068 есть
исходный JRRA/NOP. Отдельно установи actual application/input/progress contract,
если обычный ввод не продвинет сцену.

Map011/lift010/все3 recovered leaves и36resume labels сохранены. Будущий Ghidra
export должен повторить ВСЕ3 verified recoveries из ANALYSIS_REGRESSIONS.md.
Обновляй CURRENT_STATE/CONTINUE/topHANDOFF/промпт. Focused validated commit/push
разрешены после staged review/check_public_tree.py. Game/BIOS/frames/private
traces/helpers не публикуй; только originalsource/synthetic/revieweddocs.
