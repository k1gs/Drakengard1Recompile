# Продолжение работы

## Latest: closing fade completes; target delivery under observation (2026-10-25)

Runtime unchanged from83e1557; actual EXE SHA
0d24cede033b00c545772545212b273d56994514e03b794834285d6c6db397f1.
Native281 used the same300s/2048M/interval60/cap320/metricsOFF/watch4fbd30;
all4input phases0,watchdogtrue/exitnull. No new explicit native/device refusal.
A read-only owned-process observer verified unique EE RAM with the original ELF
hash and3code probes; no WriteProcessMemory, debugger or thread suspension.
Actual closing60->0:61observations,all60net decrements1,9.297wall seconds,
10.140625process CPU seconds,median0.156s between decrements. This proves
progress, not an exact trace of the341698a1 register. Target first1485718 and
final1485920 CDVD reads were reached; no gameplay/world proof.

Critical source-analysis correction: original UI switch table566090 maps state4
to2c6e80, not2c6930. It waits remaining48a434, calls2b02ac and clears503008 via
2c6eac, while leaving state4 intact. Do not infer a stuck fade from UI4 alone.
Earlier83e1557 notes attributing UI0x12 to state4 were incorrect and are corrected.

A separate read-only main GuestThread PC sample found3809samples/59.422s:
27f4c4=1272,2fdd08=469. The dispatch target is set BEFORE scheduler safe-point/
device work, so these are not host stack timings or proof those functions spin.
Two initial PC locator attempts failed closed (guard page / narrow region cap);
a fresh valid locator scanned74,469,376bytes in0.110s and verified one actual
id1/entry120008/stack100000,size20000/GP572c70/context1648. All helpers stay ignored.

Native282 completed300s watchdog/exitnull:274captures,last16560 black;
startup/intro/menu0,progression1(deadline295s). First observer1 on early unmatched
IOP; separate fresh read-only observer2 exited0, but NO targetread occurred.
Payload attempts0,delivery_verifiedfalse,NO RAM/payload saved. Final actual UI4/
enable0/fader255,255,0; old completedjob19/status3/LSN867382,dest4b1c00,131072B,
IOP991711232. This independently confirms that UI4 persists after fade completion.
No new native/device refusal; target CDVD->EE delivery remains unverified.

Next283 is ONLY A PREPARED DRAFT, NOT AUTHORIZED OR RUN. Current project limit
is300s per native/reference experiment. Proposed ONE600s observation preserves
sameEXE,original game clocks and fixed2048M without renewal. Private parser copy,
frameinterval120/cap320,progression595s,observerchildbirth+599.5s are syntaxchecked.
The draft refuses launch without explicit human600s approval; do not widen public
launcher or run it until the user agrees. Original300s launcher remains unchanged.
It will compare EEb7db40/481792bytes twice against ISO LSN1485718*2048, then save
one private NONATOMIC liveRAM copy with before/after counters only on a match.
No guest flags/PC/RAM/timers/movie changes, fake completions or budget renewal.
All native/observer processes stopped. Menu-FPS work remains postponed; preserve
unrelated .vscode/Splash, inputs/upstream/generated/maps and private evidence.

Этот файл — постоянная точка входа для любого агента с доступом к репозиторию.
Запрос «продолжи» означает: восстановить актуальное состояние из файлов и
продолжить реализацию первого подтверждённого блокера с проверкой результата.
Не ограничивайся пересказом журнала или планом. При этом такой запрос сам по себе
не разрешает force-push, публикацию приватных данных или обход поведения игры.

## Порядок чтения

1. `AGENTS.md`.
2. Последние разделы `CURRENT_STATE.md`, если файл доступен. Это локальный
   игнорируемый журнал; в свежем клоне его может не быть.
3. Верхний актуальный раздел `docs/HANDOFF_2026-10-24_CONTINUATION.md`.
4. Последние актуальные разделы `docs/IOP_BOOTSTRAP.md` и
   `docs/RESEARCH_WORKFLOW.md`.
5. `docs/ROADMAP.md`, если он существует. Отсутствие этого файла не блокирует
   продолжение: используй журнал и актуальный handoff.

Сначала проверь `git status`, текущий HEAD и реальные процессы native/reference.
Более новые результаты журнала и явные указания пользователя важнее checkpoint
ниже. Не запускай старый номер эксперимента повторно и не перезаписывай evidence.


## Сохранённый checkpoint пролога: 2026-10-25

- Все три подтверждённых truncated leaves восстановлены:297E44..2983E8,
  29322C..2937D0,297878..297E44. Последний имеет реальный JR297E3C/SP+60
  delay297E40,1484 actual bytes,+388 coverage. Нет aliases/пустых returns.
- Fresh map011/lift010 сохраняет старые исправления и unrelated rows. Все36
  resume labels зарегистрированы.8578compiled/0stubs/skips/decodefailures.
  Fresh Ghidra behavioral regression и full real Release/PCH build PASS.
- Native246/247/248 на одном binary проходят прежний297BB0 и выполняют хвост
  с реальным VU0-data4860→SPR transfer. Явных новых scheduler/device отказов нет.
- Новый визуальный milestone: после New Game появилась ПЕРВАЯ ТЕКСТОВАЯ СТРАНИЦА
  ПРЕДЫСТОРИИ, GS640x511,246frame15600/248frames14400,15000, приватно просмотрены.
  Это ещё не gameplay/host audio/saves/recovery. Все прогоны300s watchdog,
  timed_out=true/exit=null, IOP/SIF/callback processing активны. Нет final-on-exit GS.
- Чистый ввод246: Start11/Cross12/Left13/Cross14, все SIO2 press/release.248 так же,
  затем один Cross15 на подтверждённом прологе (SIO2 ACK). После него текст страницы
  тот же; пиксели отличаются. Семантический переход страницы НЕ подтверждён.
- Старый ROI oracle в247 поздно признал корректный диалог; это не отказ игры.
  Private248 oracle проверяет menu MAE2, dialog text/buttons15/8 и prologue
  normalizedROI15. Это checkpoint-specific detection; raw evidence не изменено.
- Точные hashes/ограничения/evidence — верхний handoff. Source input/upstream/
  old generated/maps не менялись. .vscode/Splash сохрани; сплешскрин отложен.

## История: прежняя граница пролога

Исторически границей была первая страница пролога; теперь продолжай главу I выше,
а НЕ missing scheduler297BB0. Установи ожидаемое поведение страницы: ordinary
Start/Cross после проверки готовности; реальный callback/input handler и, при
необходимости, read-only Timer1 COUNT/MODE/софт-прогресс. Повторяющийся PC27F1B8
и getter1215F4(10000800) не доказывают timer bug. При необходимости сравни
эквивалентное состояние owned reference, последовательно с native.

Не форсируй ready/time/PC/RAM. Пройди следующую реальную сцену и проверь движение/
бой прежде, чем заявлять gameplay. Каждый native/reference run максимум300s.
Private248 menu script accepts run ID; prologue script hardcodes248/output/seq14,
поэтому используй новую копию с проверенными именами/sequence, не overwrite.
Один controller за раз, actual GS state и SIO2 press/release; ACK не равен UI action.

Для будущего полного Ghidra export из read-only проекта повтори ВСЕ ТРИ verified
recoveries выше, чтобы не регрессировать coverage. Workflow в ANALYSIS_REGRESSIONS.
Old maps/lifts сохраняй; сначала проверь новые имена. Промпт — NEXT_MODEL_PROMPT.

## Инструменты и границы

- Computer Use не используй. Управление — штатный host pad protocol,
  кадры — прямые GS-захваты. Не внедряй OS input и не меняй guest ready flags.
- Если доступны навыки `ps2-recomp-research` и `ps2-frame-investigation`, прочитай
  и используй их. Если другая среда не поддерживает эти навыки, следуй правилам
  этого репозитория; недоступность локального навыка сама по себе не причина
  прекращать работу.
- Субагенты необязательны. Если делегируешь, пользователь разрешил только
  GPT-5.6 Luna; проверь их выводы самостоятельно. Если такой модели нет,
  работай самостоятельно.
- Vulkan — будущий backend. Сейчас продолжай фактический следующий blocker из нового checkpoint;
  ADMA/FX уже прошли прежние границы. Не начинай миграцию рендера вместо
  исправления установленного контракта.
- Upstream в `tools/ps2recomp` и generated guest code не редактируй. Исправляй
  оригинальные адаптеры/генераторы/overlays. Сохраняй `.vscode/` и чужие изменения.
- Исходные уже проверенные inputs ранее были подтверждены как полные. Сохраняй
  их read-only и проверяй закреплённые hashes существующими средствами. Новые
  поступившие файлы не исследуй до подтверждения их полноты пользователем.
- Pinned PCSX2 v2.8.2 reference находится локально в
  `work/ipu-reference-001/`. Используй его для сверки контрактов, не копируй
  реализацию декодера в публичный исходник.
- Никаких success stubs, выдуманных completion/IRQ, подстановок start-code,
  исправления RA или патчей ready/input flags ради продвижения.
- Не пропускай фильм патчем гостевого кода. Автоматизация предупреждения карты
  памяти обычными кнопками уже разрешена и реализована.

## Следующий ограниченный прогон

Номер сверяй с верхним checkpoint, каталогом work/ и реальными процессами.
Пример ниже — только обычный300s профиль; НЕ запускай вместо ожидающего
разрешения600s draft283. Номер283 прежде сверь с каталогами. После изменения
runtime сначала собери настоящий runner, проверь exit code и SHA-256; smoke.exe
не заменяет ps2EntryRunner. Сборка при живом native процессе запрещена.

```powershell
$nativeRun=283
$env:DRAKENGARD_FRAME_INTERVAL="60"
$env:DRAKENGARD_FRAME_LIMIT="320"
python scripts/run_native.py --input-complete --run $nativeRun --seconds 300 --iop-bootstrap --ee-firmware-entry --iop-video-events --virtual-pad --pad-control --iop-runtime-millions 2048 --ee-write-watch 0x4fbd30
```

Для обычного ввода используй новую private копию последнего controllers.py с
правильным run ID. Не жди конца launcher: controller должен дождаться реального
pad-control.txt и запустить native_start --timeout300 --attempts5 --after-frame1200.
Последний1200 откалиброван actual271; более старые214–216 имели другое состояние.
260/intro.py принимает run CLI;279/menu.py запускается ТОЛЬКО после intro0;
279/progression.py принимает run CLI, ждёт menu.log completion marker и UI state3.
Один controller на фазу; корректные startup.log/intro.log/menu.log обязательны.

DRAKENGARD_FRAME_LIMIT1..320 — opt-in конечный cap, default32. Интервал60..3600
измеряется в host UI observations, не игровых кадрах/FPS. Interval60 даёт более
частое распознавание состояния и добавляет capture I/O overhead. Сохраняй actual
config line, EXE hash и число файлов в evidence. Ни rawGS, ни metadata не заменяют
проверку gameplay. Native/reference последовательно, каждый максимум300s.

## Проверка и завершение этапа

Отложенная задача пользователя: `native/Splash.png` должен показываться в host
окне сразу после запуска executable вместо начального розового фона. Asset
сохранён без изменений; интеграция ещё не выполнена. Host splash нельзя выдавать
за кадр игры. При публикации сначала проверь правила assets/public-tree.

Для исправленного контракта добавь детерминированные поведенческие регрессии,
собери соответствующие targets, затем проверь путь настоящим bounded native
прогоном. Синтетические тесты не доказывают меню или gameplay.

Перед остановкой обязательно обнови:

- `CURRENT_STATE.md`: изменения, команды/версии, hashes, evidence paths,
  exit vs timeout, первый оставшийся блокер и один следующий эксперимент.
- Верхний актуальный раздел handoff: то, что сможет прочитать агент в свежем
  клоне без приватного журнала и истории чата.
- Этот файл, если изменились точка входа, команда или актуальный checkpoint.

Не записывай новый прогресс в глобальную память ассистента без отдельной просьбы.
Публикуй только разрешённый source/docs/tests checkpoint, проверяй staged diff и
`python scripts/check_public_tree.py`. Запрос «продолжи» не отменяет явного
ограничения пользователя на commit/push; текущий пользователь разрешил push
проверенного checkpoint и дальнейшее продолжение. Публикуй focused validated
изменения обычным push, без повторного запроса уже данного разрешения.
