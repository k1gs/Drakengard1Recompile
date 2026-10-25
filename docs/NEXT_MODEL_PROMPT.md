# Промпт продолжения

Продолжи Drakengard1 Recompile, PS2 Europe, D:\proj\drakengard 1 recompile.
Прочитай AGENTS.md, docs/CONTINUE.md, последние CURRENT_STATE.md и верхний handoff.
Проверь git HEAD/status и реальные процессы. Без субагентов/ComputerUse/OS input.
Сохраняй .vscode/Splash и все старые private artifacts. Inputs ранее подтверждены
как полные; исходники игры/BIOS/upstream/generated/maps read-only.

Пользователь ЯВНО отложил FPS меню — сейчас продолжать ИГРУ, не оптимизацию.
258 после обычного Start на титуле главы отрисовал настоящий экран задания
"Chapter1 Verse2" с Weapon/Continue, GS640x511. Это ещё не movement/combat/
gameplay/audio/saves/recovery.260 повторил briefing раньше с обычными Start
на логотипах; Cross23 имел SIO2 ACK, но actual страница осталась.

255 упёрся в глобальный1024M IOP allowance (total1038404316), exit6/no timeout,
IOPpcbb9c Jself idle. Добавлен ЯВНЫЙ фиксированный2048M пресет, default256 и
512/1024 сохранены; no poll/idle renewal. Worst deadline2,063,999,999 fits32bits,
static guard, all-preset/invalid/renewal regression, bootstrap14/14, Python57 PASS.
256 с2048M прошёл прежнюю границу,300s watchdogtrue/exitnull, no new explicit
scheduler/device refusal. Ни один неподдержанный сервис не подменён success.

После ec9325d исправлен подтверждённый host/UI exception lifetime bug.
Контрольный265: invalid HOST command после реального UI frame120 →0xC0000409:
исключение разрушало joinable game thread раньше внешнего Native blocker.
GameThreadJoinGuard теперь stop/join при unwind, сохраняет исходную ошибку.
Контрольный266: тот же fault →exit6, invalid pad command prefix, final GS,
окно закрыто. Это не доказывает причину всех спонтанных C0000409.
Bootstrap15/15, Python57, настоящий Release runner PASS. Точный текущий SHA:
f926bb78f868c4da72f16c6a979fb02bb6a4c432c8e931cac2b14bdb6dcedd00.
Исходный upstream и старые generated/maps не менялись.

263 с700ms Start и3000ms briefing Cross: два реальных press/release, затем
last15600 серый без briefing, но мира нет.264 оборвалсяC0000409 до input phase.
267 normal на новом runner с3000ms game-phase Start/Cross завершён watchdog300s.
Briefing15900 Cross23 ACK, same16200 Cross24; реальные BFFF/FFFF видны в runtime
78768/79980, хотя helper дошёл295s раньше собственного ACK24. Last16500 всё ещё
briefing; явного нового device/unsupported/budget отказа нет. Не объявляй
ускорение от долгих Start или успех gameplay по этим данным.

Следующий268 проверен unused, профиль максимум300s/фиксированный2048M/
frame300/cap64/metricsOFF. Reuse263 progression: Start700ms, briefing Cross3000ms,
max2 Cross/свежая та же страница/ROI12. native_start --attempts5;260/intro.py268;
257/menu.py268 ТОЛЬКО после intro0;263/progression.py268 ждёт menu finish.
Нужен actual application input contract, не ещё одно слепое время удержания.
После первого briefing Cross ACK возьми ОДИН private full snapshot своего
native PID существующим ProcDump -ma <PID> <ignored output> (проверь свободное
место), не длительный -e debugger. Следи за GS до300s, исследуй EE context/
pad-consumption read-only. Ранний EE_RAM_OUTPUT не есть поздний briefing dump.
Не меняй timer/ready/PC/RAM, не патчь пропуск фильмов, не продлевай бюджеты.
Цель: первая настоящая сцена миссии и затем движение/бой. Не выдавай ACK/серый
кадр/отсутствие краша за gameplay или полное исправление спонтанного сбоя.

Map011/lift010/все3 recovered leaves и36resume labels сохранены. Будущий Ghidra
export должен повторить ВСЕ3 verified recoveries из ANALYSIS_REGRESSIONS.md.
Обновляй CURRENT_STATE/CONTINUE/topHANDOFF/промпт. Focused validated commit/push
разрешены после staged review/check_public_tree.py. Game/BIOS/frames/private
traces/helpers не публикуй; только originalsource/synthetic/revieweddocs.
