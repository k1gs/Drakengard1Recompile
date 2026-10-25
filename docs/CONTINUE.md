# Продолжение работы

## Latest: CVT.W.S sort fixed and native verified; missing owner tail2ad344 (2026-10-25)

User approved600s diagnostic execution and delegated routine choices. Keep finite
600s maximum for this validation profile and fixed2048M IOP without renewal;
public launcher remains300s, private verified extended launcher stays ignored.
Native283 on old0d24cede...EXE completed600s watchdog/exitnull, allinput phases0,
observer0. Exact481792B file789 from ISO1485718*2048 matched EEb7db40 TWICE.
SHA cbf01937e8b52bc842272646c53743ab4791cefb5b19e787dc0453396c46b5ba.
IOP999749747->999756219 during reads; job34/status2 then actualstatus3/object6.
Live RAM copy is NONATOMIC (IOP999768532->999932807), not a full-process snapshot.
Next actual readLSN1482591/6402419B reached final1485664/54sectors; node48fd10
was reused, so oldjob34/status3 must NOT be attributed to this later request.
Late scene handler143194 at3b4c20 advanced substate+5c2->4. No rendered world.

Confirmed next defect: original comb-sort24e4d8 repeatedly has gap2/no swaps,
51entries in scratchpad70000000. Constant56e8c0=1.2999999523162842. The emitted
CVT.W.S24e50c called hostnearbyintf, giving2 for2/1.3; gap never reaches1 and
original backedge24e584 never terminates. Source/runtime cause, no gap patch.
Pinned PCSX2v2.8.2 CVT_W truncates towardzero and saturates by sign; recompiler
usesCVTTSS2SI. Original ee_fpu_convert.h + shared macro/PCH overlay now implements
that RESULT contract, independent of host rounding; no broader FCR31 flag model.
Upstream/generated/gamecode unchanged.18/18 tests PASS:boundaries/all4rounding
modes, fail-before nearbyint2/1.3=2, bounded synthetic51entry comb-sort termination.
Actual fullPCH Release build PASS; newEXE SHA
81886f391519c3acb95180d215c43d93e20e91b7e72a4ae658f2d5543b6f3583.
Native284 VERIFIED advancement beyond the old sort: nativeexit5/timed_out=false,
all4input phases0, same600s/2048M profile. Firstobserver1 on transient nullCIMpath;
freshobserver2 waited for validpath, verified owned EE/IOP, exited0 and proved
payload delivery again (two attempts, second matched). LiveRAM hash
bcf501867cf345a8de911c553a84f994cad7f57914f2b2a3c035979d2a5cddfd.
Reviewed final640x511 GS shows "Loading...", not a rendered world/gameplay.
All native/observer processes stopped. Next285 unused (recheck).

FIRST REMAINING BLOCKER: scheduler target2ad344,RA1a22d4,SP11f8d0,GP572c70.
Owning exported function2aca3c has exclusive end2ad344, so the real instruction
SRLa1,a1,1 at2ad344 is missing. BEQ2ad334 targets2ad340 (VNOP, also the delay
slot of J2ad33c), then should execute2ad344 and BNEZ2ad348 back to2acc08.
The export instead splits2ad348 into a separate owner which branches back inside
2aca3c; registration omits2ad344. Recover the actual owning body/control-flow
and resume set in original analysis/generator/overlay, preserve all3prior
recoveries, use fresh map/lift and bounded native285. Do NOT add a success stub,
blind alias, guestPC change or a second prologue. Original tail disassembly and
before/after evidence are private chapter1-284; next cause is ownership/export,
not a new DMA defect. No gameplay claim.
No subagents/ComputerUse/OS input, gameclock/PC/RAM patches, fake completions or
budget renewal. Preserve .vscode/Splash/upstream/generated/maps/private evidence.

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
бой прежде, чем заявлять gameplay. Обычный предел native/reference300s; текущий600s профиль явно одобрен пользователем.
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
Пример ниже показывает обычный publiclauncher300s. Для текущей600s проверки
используй свежую копию последнего privateextendedlauncher/controllers, учитывая
исправленный observe2 startup. Следующий285 прежде сверь с каталогами. После изменения
runtime сначала собери настоящий runner, проверь exit code и SHA-256; smoke.exe
не заменяет ps2EntryRunner. Сборка при живом native процессе запрещена.

```powershell
$nativeRun=285
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
проверку gameplay. Native/reference последовательно: обычный300s, текущий одобренный профиль600s.

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
