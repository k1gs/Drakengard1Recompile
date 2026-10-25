# Промпт продолжения

Продолжи Drakengard1 Recompile, PS2 Europe, D:\proj\drakengard 1 recompile.
Сначала AGENTS.md, docs/CONTINUE.md, latest CURRENT_STATE.md и самый верх
HANDOFF_2026-10-24_CONTINUATION.md. Проверь actual HEAD/status/processes, fresh run
number. Без субагентов/ComputerUse/OS input. FPS меню отложен; цель — дальше игру.
Inputs ранее подтверждены complete, read-only. Сохрани unrelated .vscode/Splash,
upstream/generated/maps и все private evidence. Только original source/synthetic
tests/reviewed docs в Git, staged review и check_public_tree.py перед публикацией.
Focused validated commit/normal push разрешены. Каждый native/reference <=300s,
последовательно; без PC/RAM/ready/timer/movie patches, fake completions/renewal.

Runtime83e1557 не менялся; EXE SHA256
0d24cede033b00c545772545212b273d56994514e03b794834285d6c6db397f1.
Первый проверенный шаг281: UI readiness2->3, один обычный Cross принят,
closing fader60->0 за9.297s;61observations,60decrements ровно1,CPU10.140625s,
median0.156s. Это измерение counter cadence, НЕ прямой trace регистраa1.
Все4input phases281exit0, nativewatchdog300s/exitnull; targetresource789
LSN1485718 и final1485920 реально читались. Gameplay/world/audio/saves нет.

ВАЖНО: исправлена прежняя ошибка анализа83e1557. Switch table566090 maps
UIstate4 to2c6e80, НЕ2c6930. State4 waits remaining48a434 via341804, calls2b02ac,
then clears enable503008 at2c6eac and keeps UIstate4. Поэтому UI4/remaining0
НЕ является зависанием. 2c6930->UI0x12 — другая ветка. Source contract281 private.

281 read-only PC sampling:3809samples/59.422s,27f4c4=1272,2fdd08=469. Dispatch
sets targetPC BEFORE scheduler/device safepoint; это НЕ host stack profile и
НЕ доказательство spin-loop. Unique main GuestThread verified by id1/entry120008/
stack100000,size20000/GP572c70/context1648. Guard/narrow-region failed attempts
остались private, ничего не изменяли. Fader observer9248samples/0readerrors.

Последний282 завершён300s watchdog/exitnull;274captures,last16560black.
Startup/intro/menu0,progression1 at295s. Firstobserver1 earlyIOPidentity0;
свежий observe2exit0,но0payloadattempts/delivery_verifiedfalse: targetread не
успел,никакой lateRAM/payload не сохранён. Actual UI4/enable0/fader255,255,0,
IOP991711232,oldjob19/status3/LSN867382,dest4b1c00,131072B; это НЕ target789.
Нет нового runtime/device fault; targetdelivery всё ещё не доказана.

283 — ТОЛЬКО ПОДГОТОВЛЕННЫЙ, НЕЗАПУЩЕННЫЙ draft одного600s наблюдения.
Текущее правило проекта <=300s сохранено, publiclauncher НЕ изменён.
Требуется ЯВНОЕ разрешение пользователя на один600s прогон прежде запуска:
тот жеEXE/gameclocks,IOP2048M безrenewal; screenshotinterval120/cap320 покрывает
600s,progression595,observerchildbirth599.5. Синтаксис6privatehelpers PASS.
Draftlaunch.py откажет без DRAKENGARD_USER_APPROVED_600=1; не выставляй его без
прямого humanapproval. При отказе сохрани300s; не обходи правило копиейlauncher.
Цель разрешённого600s: послеfinalread дважды сравнить actualEEb7db40/481792bytes
с originalISO1485718*2048, expectedSHA
cbf01937e8b52bc842272646c53743ab4791cefb5b19e787dc0453396c46b5ba.
Только при полном совпадении сохранить privateNONATOMIC liveRAM с IOPcounters
before/after; это НЕ atomicfullprocesssnapshot. Первое чтение слишком поздно
для прежнего тяжёлого ProcDump; read-onlyVM_READ не приостанавливает потоки.
Сейчас native/reference/observer процессов нет. Проверь actual state ещё раз.

Если payload действительно совпал, CDVD->EEdelivery этого ресурса подтверждена;
анализируй следующий consumer/scene state по lateRAM и источнику. Если mismatch
или targetread не успел, это НЕ доказанная потеряDMA. Сохрани реальные counters,
jobID/status/node+14LSN/+18destination/+1csize; jobs переиспользуются. Старый273
snapshot предшествовал targetread, а job19/LSN867318 был другой завершённый запрос.
Нельзя объявлять lostDMA по устаревшим байтам/общемуUIframecounter.
Все3 recovered leaves/36resume labels уже исправлены; futureGhidraexport должен
сохранять3recoveries из ANALYSIS_REGRESSIONS.md. Не регрессируйscheduler/IPU/SPRadapters.
Обнови CURRENT_STATE/topHANDOFF/CONTINUE/этотprompt с доказанным итогом и следующим
конкретным экспериментом. До настоящего мира/движения/боя gameplay не заявлять.
